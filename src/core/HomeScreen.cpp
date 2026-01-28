/**
 * @file HomeScreen.cpp
 * @brief Home screen display and navigation - Optimized for instant rendering
 * @version 1.5.0
 * 
 * Optimizations implemented:
 * 1. Pre-cached scaled cover image on SD card (eliminates JPEG decode on each draw)
 * 2. Persistent HomeState struct (no recalculation on navigation)
 * 3. Pre-render during deploy screen (instant home screen display)
 * 4. Per-widget partial refresh (minimal screen updates)
 * 5. Fixed plugin disappearing bug (always draw ALL grid items)
 */

#include "core/HomeScreen.h"
#include "core/SettingsManager.h"
#include "core/BatteryMonitor.h"
#include "core/RefreshManager.h"
#include "core/ButtonInput.h"
#include "core/WiFiManager.h"
#include <SD.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <ArduinoJson.h>

extern WiFiManager wifiManager;

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern RefreshManager refreshManager;

// =============================================================================
// Global State
// =============================================================================
uint8_t enabledItemIndices[MAX_HOME_ITEMS];
int enabledItemCount = 0;
int homeSelection = 0;
int homePageIndex = 0;
int itemsPerPage = 8;
int homeCols = 2;
int homeRows = 4;
int widgetSelection = -1;

// Persistent home state - survives across function calls
HomeState homeState = {0};

// =============================================================================
// Internal State
// =============================================================================
static bool _skipCoverDraw = false;  // Skip cover drawing during selection-only updates

// Continue Reading widget data - MUST match LastBookInfo in Library.h
struct LastBookWidget {
    uint32_t magic;
    char title[64];
    char author[48];
    char coverPath[96];
    char bookPath[128];
    int chapter;
    int page;
    int totalPages;
    float progress;
};

// Weather widget data
struct WeatherWidget {
    uint32_t magic;
    uint32_t timestamp;
    float temperature;
    int weatherCode;
    int humidity;
    float windSpeed;
    char location[48];
    bool useCelsius;
    float high;                 // Today's high
    float low;                  // Today's low
    float forecastHigh[3];      // Next 3 days high temps
    float forecastLow[3];       // Next 3 days low temps
    char forecastDay[3][4];     // Day names
    char sunrise[12];           // e.g. "6:45 AM"
    char sunset[12];            // e.g. "5:30 PM"
};

static LastBookWidget lastBookWidget;
static WeatherWidget weatherWidget;

// =============================================================================
// Cover Cache - Pre-scaled 1-bit bitmap stored on SD
// =============================================================================
// Format: 4-byte width, 4-byte height, then packed 1-bit pixels (8 pixels per byte)
// This is MUCH faster than JPEG decode (direct memory copy vs decode+scale+dither)

// Temporary buffer for cover caching (used during JPEG decode)
static int _cacheCoverX = 0;
static int _cacheCoverY = 0;
static int _cacheCoverW = 0;
static int _cacheCoverH = 0;
static uint8_t* _cacheCoverBits = nullptr;
static float _cacheCoverScale = 1.0f;

// TJpgDec callback for creating cached cover (dithers and stores to RAM buffer)
static bool _cacheCoverCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_cacheCoverBits) return false;
    
    // Bayer 4x4 dithering matrix
    static const uint8_t bayer[4][4] = {
        {  15, 135,  45, 165 },
        { 195,  75, 225, 105 },
        {  60, 180,  30, 150 },
        { 240, 120, 210,  90 }
    };
    
    float scale = _cacheCoverScale;
    int stride = (_cacheCoverW + 7) / 8;  // Bytes per row
    
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            // Convert RGB565 to grayscale
            uint16_t pixel = bitmap[j * w + i];
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;
            uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
            
            // Calculate output pixel position with scaling
            int px = (int)((x + i) * scale);
            int py = (int)((y + j) * scale);
            
            // Skip if outside bounds
            if (px < 0 || px >= _cacheCoverW || py < 0 || py >= _cacheCoverH) continue;
            
            // Dither
            uint8_t threshold = bayer[py & 3][px & 3];
            bool isWhite = (gray > threshold);
            
            // Set bit in packed array (1 = white, 0 = black)
            int byteIdx = py * stride + (px / 8);
            int bitIdx = 7 - (px % 8);  // MSB first
            
            if (isWhite) {
                _cacheCoverBits[byteIdx] |= (1 << bitIdx);
            } else {
                _cacheCoverBits[byteIdx] &= ~(1 << bitIdx);
            }
        }
    }
    return true;
}

bool createCachedCover(const char* sourcePath, int targetW, int targetH) {
    if (!sourcePath || strlen(sourcePath) < 5) return false;
    
    // Check source exists
    File srcFile = SD.open(sourcePath);
    if (!srcFile) {
        Serial.printf("[HOME] Cover source not found: %s\n", sourcePath);
        return false;
    }
    srcFile.close();
    
    // Get JPEG dimensions
    uint16_t jpgW, jpgH;
    if (TJpgDec.getFsJpgSize(&jpgW, &jpgH, sourcePath, SD) != JDR_OK) {
        Serial.println("[HOME] Failed to get JPEG size");
        return false;
    }
    
    // Calculate scale to fit
    float scaleW = (float)targetW / jpgW;
    float scaleH = (float)targetH / jpgH;
    float targetScale = min(scaleW, scaleH);
    
    // Choose JPEG hardware scale
    int jpgScale = 1;
    if (targetScale < 0.125f) jpgScale = 8;
    else if (targetScale < 0.25f) jpgScale = 4;
    else if (targetScale < 0.5f) jpgScale = 2;
    
    // Calculate final dimensions
    int decodedW = jpgW / jpgScale;
    int decodedH = jpgH / jpgScale;
    _cacheCoverScale = targetScale * jpgScale;
    
    int finalW = (int)(decodedW * _cacheCoverScale);
    int finalH = (int)(decodedH * _cacheCoverScale);
    
    // Allocate buffer for 1-bit image
    int stride = (finalW + 7) / 8;
    int bufSize = stride * finalH;
    
    _cacheCoverBits = (uint8_t*)malloc(bufSize);
    if (!_cacheCoverBits) {
        Serial.printf("[HOME] Failed to alloc %d bytes for cover cache\n", bufSize);
        return false;
    }
    
    // Initialize to white
    memset(_cacheCoverBits, 0xFF, bufSize);
    
    _cacheCoverW = finalW;
    _cacheCoverH = finalH;
    _cacheCoverX = 0;
    _cacheCoverY = 0;
    
    // Set up decoder
    TJpgDec.setJpgScale(jpgScale);
    TJpgDec.setCallback(_cacheCoverCallback);
    
    // Decode JPEG into buffer
    JRESULT result = TJpgDec.drawFsJpg(0, 0, sourcePath, SD);
    
    if (result != JDR_OK) {
        Serial.println("[HOME] JPEG decode failed for cache");
        free(_cacheCoverBits);
        _cacheCoverBits = nullptr;
        return false;
    }
    
    // Save to SD card
    File cacheFile = SD.open(HOME_COVER_CACHE_PATH, FILE_WRITE);
    if (!cacheFile) {
        Serial.println("[HOME] Failed to create cover cache file");
        free(_cacheCoverBits);
        _cacheCoverBits = nullptr;
        return false;
    }
    
    // Write header: width, height
    cacheFile.write((uint8_t*)&finalW, 4);
    cacheFile.write((uint8_t*)&finalH, 4);
    // Write pixel data
    cacheFile.write(_cacheCoverBits, bufSize);
    cacheFile.close();
    
    Serial.printf("[HOME] Cover cached: %dx%d -> %dx%d (%d bytes)\n", 
                  jpgW, jpgH, finalW, finalH, bufSize + 8);
    
    // Store in homeState
    homeState.cachedCoverW = finalW;
    homeState.cachedCoverH = finalH;
    homeState.coverCached = true;
    
    free(_cacheCoverBits);
    _cacheCoverBits = nullptr;
    
    return true;
}

bool drawCachedCover(int x, int y) {
    File cacheFile = SD.open(HOME_COVER_CACHE_PATH, FILE_READ);
    if (!cacheFile) {
        return false;
    }
    
    // Read header
    int32_t w, h;
    cacheFile.read((uint8_t*)&w, 4);
    cacheFile.read((uint8_t*)&h, 4);
    
    if (w <= 0 || h <= 0 || w > 500 || h > 500) {
        cacheFile.close();
        return false;
    }
    
    int stride = (w + 7) / 8;
    
    // Read and draw row by row (memory efficient)
    uint8_t rowBuf[64];  // Max 512 pixels wide
    
    for (int row = 0; row < h; row++) {
        cacheFile.read(rowBuf, stride);
        
        for (int col = 0; col < w; col++) {
            int byteIdx = col / 8;
            int bitIdx = 7 - (col % 8);
            bool isWhite = (rowBuf[byteIdx] >> bitIdx) & 1;
            
            display.drawPixel(x + col, y + row, isWhite ? GxEPD_WHITE : GxEPD_BLACK);
        }
    }
    
    cacheFile.close();
    return true;
}

// =============================================================================
// Widget Loading - Same as before but updates HomeState
// =============================================================================
void loadLastBookWidget() {
    homeState.hasBook = false;
    
    // Try loading saved last book first
    if (SD.exists(LAST_BOOK_PATH)) {
        File file = SD.open(LAST_BOOK_PATH, FILE_READ);
        if (file) {
            size_t read = file.read((uint8_t*)&lastBookWidget, sizeof(LastBookWidget));
            file.close();
            
            if (read == sizeof(LastBookWidget) && lastBookWidget.magic == 0x4C415354) {
                homeState.hasBook = true;
                strncpy(homeState.bookTitle, lastBookWidget.title, sizeof(homeState.bookTitle) - 1);
                strncpy(homeState.bookCoverPath, lastBookWidget.coverPath, sizeof(homeState.bookCoverPath) - 1);
                homeState.bookProgress = lastBookWidget.progress;
                Serial.printf("[HOME] Last book loaded: %s\n", lastBookWidget.title);
                
                // Verify cover exists
                if (lastBookWidget.coverPath[0] == '\0' || !SD.exists(lastBookWidget.coverPath)) {
                    // Try portal cover path
                    const char* bookPath = lastBookWidget.bookPath;
                    const char* justFilename = strrchr(bookPath, '/');
                    if (justFilename) justFilename++; else justFilename = bookPath;
                    
                    uint32_t hash = 0;
                    for (const char* p = justFilename; *p; p++) {
                        hash = ((hash << 5) - hash) + (uint8_t)*p;
                    }
                    
                    char coverPath[96];
                    snprintf(coverPath, sizeof(coverPath), "/.sumi/books/%08x/cover_full.jpg", hash);
                    if (SD.exists(coverPath)) {
                        strncpy(lastBookWidget.coverPath, coverPath, sizeof(lastBookWidget.coverPath) - 1);
                        strncpy(homeState.bookCoverPath, coverPath, sizeof(homeState.bookCoverPath) - 1);
                    }
                }
                return;
            }
        }
    }
    
    // No last book - try to find first book in library
    if (!homeState.hasBook && settingsManager.display.showBookWidget) {
        File booksDir = SD.open("/books");
        if (booksDir && booksDir.isDirectory()) {
            File entry = booksDir.openNextFile();
            while (entry) {
                if (!entry.isDirectory()) {
                    String name = entry.name();
                    if (name.endsWith(".epub") || name.endsWith(".EPUB")) {
                        memset(&lastBookWidget, 0, sizeof(lastBookWidget));
                        lastBookWidget.magic = 0x4C415354;
                        
                        int lastSlash = name.lastIndexOf('/');
                        String filename = (lastSlash >= 0) ? name.substring(lastSlash + 1) : name;
                        filename = filename.substring(0, filename.length() - 5);
                        
                        int dashPos = filename.lastIndexOf(" - ");
                        if (dashPos > 0) filename = filename.substring(0, dashPos);
                        
                        strncpy(lastBookWidget.title, filename.c_str(), sizeof(lastBookWidget.title) - 1);
                        strncpy(homeState.bookTitle, filename.c_str(), sizeof(homeState.bookTitle) - 1);
                        snprintf(lastBookWidget.bookPath, sizeof(lastBookWidget.bookPath), "/books/%s", entry.name());
                        
                        // Find cover
                        const char* justFilename = entry.name();
                        const char* slashPtr = strrchr(justFilename, '/');
                        if (slashPtr) justFilename = slashPtr + 1;
                        
                        uint32_t hash = 0;
                        for (const char* p = justFilename; *p; p++) {
                            hash = ((hash << 5) - hash) + (uint8_t)*p;
                        }
                        
                        char coverPath[96];
                        snprintf(coverPath, sizeof(coverPath), "/.sumi/books/%08x/cover_full.jpg", hash);
                        Serial.printf("[HOME] Looking for cover: %s\n", coverPath);
                        
                        if (SD.exists(coverPath)) {
                            strncpy(lastBookWidget.coverPath, coverPath, sizeof(lastBookWidget.coverPath) - 1);
                            strncpy(homeState.bookCoverPath, coverPath, sizeof(homeState.bookCoverPath) - 1);
                            Serial.printf("[HOME] Found cover: %s\n", coverPath);
                        }
                        
                        homeState.hasBook = true;
                        Serial.printf("[HOME] Using first book: %s\n", lastBookWidget.title);
                        entry.close();
                        booksDir.close();
                        return;
                    }
                }
                entry.close();
                entry = booksDir.openNextFile();
            }
            booksDir.close();
        }
    }
}

// Forward declaration
void fetchWeatherForWidget();

void loadWeatherWidget() {
    homeState.hasWeather = false;
    bool needsFetch = false;
    
    if (!SD.exists(WEATHER_CACHE_PATH)) {
        Serial.println("[HOME] Weather: No cache file, will fetch");
        needsFetch = true;
    } else {
        File file = SD.open(WEATHER_CACHE_PATH, FILE_READ);
        if (!file) {
            needsFetch = true;
        } else {
            size_t read = file.read((uint8_t*)&weatherWidget, sizeof(WeatherWidget));
            file.close();
            
            if (read == sizeof(WeatherWidget) && weatherWidget.magic == 0x57545853) {
                uint32_t uptimeSec = millis() / 1000;
                uint32_t age = (uptimeSec > weatherWidget.timestamp) ? 
                               (uptimeSec - weatherWidget.timestamp) : 0;
                
                if (weatherWidget.timestamp == 0 || age < 7200 || uptimeSec < 60) {
                    homeState.hasWeather = true;
                    homeState.weatherTemp = weatherWidget.temperature;
                    homeState.weatherCode = weatherWidget.weatherCode;
                    homeState.weatherHumidity = weatherWidget.humidity;
                    homeState.weatherCelsius = weatherWidget.useCelsius;
                    homeState.weatherHigh = weatherWidget.high;
                    homeState.weatherLow = weatherWidget.low;
                    strncpy(homeState.weatherLocation, weatherWidget.location, sizeof(homeState.weatherLocation) - 1);
                    strncpy(homeState.sunrise, weatherWidget.sunrise, sizeof(homeState.sunrise) - 1);
                    strncpy(homeState.sunset, weatherWidget.sunset, sizeof(homeState.sunset) - 1);
                    
                    // Load forecast data
                    for (int i = 0; i < 3; i++) {
                        homeState.forecastHigh[i] = weatherWidget.forecastHigh[i];
                        homeState.forecastLow[i] = weatherWidget.forecastLow[i];
                        strncpy(homeState.forecastDay[i], weatherWidget.forecastDay[i], 3);
                        homeState.forecastDay[i][3] = '\0';
                    }
                    
                    Serial.printf("[HOME] Weather loaded: %.1f° (H:%.0f L:%.0f) at %s\n", 
                                  weatherWidget.temperature, weatherWidget.high, weatherWidget.low, weatherWidget.location);
                } else {
                    Serial.println("[HOME] Weather data stale, will fetch");
                    needsFetch = true;
                }
            } else {
                needsFetch = true;
            }
        }
    }
    
    if (needsFetch && settingsManager.display.showWeatherWidget) {
        fetchWeatherForWidget();
    }
}

void saveWeatherCache(float temp, int code, int humidity, float wind, 
                      const char* location, bool celsius,
                      float high, float low,
                      float forecastHigh[3], float forecastLow[3],
                      const char forecastDay[3][4],
                      const char* sunrise, const char* sunset) {
    weatherWidget.magic = 0x57545853;
    weatherWidget.timestamp = millis() / 1000;
    weatherWidget.temperature = temp;
    weatherWidget.weatherCode = code;
    weatherWidget.humidity = humidity;
    weatherWidget.windSpeed = wind;
    weatherWidget.useCelsius = celsius;
    weatherWidget.high = high;
    weatherWidget.low = low;
    strncpy(weatherWidget.location, location, sizeof(weatherWidget.location) - 1);
    strncpy(weatherWidget.sunrise, sunrise ? sunrise : "", sizeof(weatherWidget.sunrise) - 1);
    strncpy(weatherWidget.sunset, sunset ? sunset : "", sizeof(weatherWidget.sunset) - 1);
    
    for (int i = 0; i < 3; i++) {
        weatherWidget.forecastHigh[i] = forecastHigh[i];
        weatherWidget.forecastLow[i] = forecastLow[i];
        strncpy(weatherWidget.forecastDay[i], forecastDay[i], 3);
        weatherWidget.forecastDay[i][3] = '\0';
    }
    
    // Update homeState
    homeState.hasWeather = true;
    homeState.weatherTemp = temp;
    homeState.weatherCode = code;
    homeState.weatherHumidity = humidity;
    homeState.weatherCelsius = celsius;
    homeState.weatherHigh = high;
    homeState.weatherLow = low;
    strncpy(homeState.weatherLocation, location, sizeof(homeState.weatherLocation) - 1);
    strncpy(homeState.sunrise, sunrise ? sunrise : "", sizeof(homeState.sunrise) - 1);
    strncpy(homeState.sunset, sunset ? sunset : "", sizeof(homeState.sunset) - 1);
    
    for (int i = 0; i < 3; i++) {
        homeState.forecastHigh[i] = forecastHigh[i];
        homeState.forecastLow[i] = forecastLow[i];
        strncpy(homeState.forecastDay[i], forecastDay[i], 3);
        homeState.forecastDay[i][3] = '\0';
    }
    
    File file = SD.open(WEATHER_CACHE_PATH, FILE_WRITE);
    if (file) {
        file.write((uint8_t*)&weatherWidget, sizeof(WeatherWidget));
        file.close();
        Serial.println("[HOME] Weather cache saved");
    }
}

void fetchWeatherForWidget() {
    // If no location configured, try to get from IP
    if (settingsManager.weather.latitude == 0 && settingsManager.weather.longitude == 0) {
        Serial.println("[HOME] Weather: No location - trying IP geolocation...");
        
        if (WiFi.status() != WL_CONNECTED) {
            if (!wifiManager.connectBriefly(8000)) {
                Serial.println("[HOME] Weather: WiFi not available for geolocation");
                return;
            }
        }
        
        WiFiClient geoClient;
        if (geoClient.connect("ip-api.com", 80)) {
            geoClient.print("GET /json/?fields=status,city,country,lat,lon,offset HTTP/1.1\r\n");
            geoClient.print("Host: ip-api.com\r\n");
            geoClient.print("Connection: close\r\n\r\n");
            
            unsigned long timeout = millis();
            while (geoClient.available() == 0 && millis() - timeout < 5000) {
                delay(10);
            }
            
            // Skip headers
            while (geoClient.available()) {
                String line = geoClient.readStringUntil('\n');
                if (line == "\r" || line.length() == 0) break;
            }
            
            String payload = geoClient.readString();
            geoClient.stop();
            
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                if (strcmp(doc["status"] | "fail", "success") == 0) {
                    float lat = doc["lat"] | 0.0f;
                    float lon = doc["lon"] | 0.0f;
                    if (lat != 0.0f || lon != 0.0f) {
                        settingsManager.weather.latitude = lat;
                        settingsManager.weather.longitude = lon;
                        const char* city = doc["city"] | "Unknown";
                        snprintf(settingsManager.weather.location, sizeof(settingsManager.weather.location), "%s", city);
                        settingsManager.markDirty();
                        Serial.printf("[HOME] Weather: Auto-detected location: %s (%.4f, %.4f)\n", city, lat, lon);
                    }
                }
            }
        }
        
        // If still no location, give up
        if (settingsManager.weather.latitude == 0 && settingsManager.weather.longitude == 0) {
            Serial.println("[HOME] Weather: Could not determine location");
            return;
        }
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        if (!wifiManager.connectBriefly(8000)) {
            Serial.println("[HOME] Weather: WiFi not available");
            return;
        }
    }
    
    Serial.println("[HOME] Fetching weather for widget...");
    
    float lat = settingsManager.weather.latitude;
    float lon = settingsManager.weather.longitude;
    bool useCelsius = settingsManager.weather.celsius;
    
    WiFiClient client;
    if (!client.connect("api.open-meteo.com", 80)) {
        Serial.println("[HOME] Weather: Connection failed");
        return;
    }
    
    // Request current weather + 4-day forecast + sunrise/sunset
    char request[448];
    snprintf(request, sizeof(request),
        "GET /v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
        "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset"
        "&forecast_days=4"
        "&temperature_unit=%s&wind_speed_unit=mph&timezone=auto HTTP/1.0\r\n",
        lat, lon, useCelsius ? "celsius" : "fahrenheit");
    
    client.print(request);
    client.print("Host: api.open-meteo.com\r\n");
    client.print("Connection: close\r\n\r\n");
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 8000) {
            client.stop();
            return;
        }
    }
    
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) break;
    }
    
    String payload = "";
    timeout = millis();
    while (client.available() || millis() - timeout < 2000) {
        if (client.available()) {
            payload += (char)client.read();
            timeout = millis();
        }
    }
    client.stop();
    
    int jsonStart = payload.indexOf('{');
    if (jsonStart < 0) return;
    
    String jsonPayload = payload.substring(jsonStart);
    JsonDocument doc;
    if (deserializeJson(doc, jsonPayload)) return;
    
    // Current weather
    JsonObject current = doc["current"];
    float temp = current["temperature_2m"] | 0.0f;
    int humidity = current["relative_humidity_2m"] | 0;
    float wind = current["wind_speed_10m"] | 0.0f;
    int weatherCode = current["weather_code"] | 0;
    
    // Daily forecast
    JsonObject daily = doc["daily"];
    JsonArray tempMax = daily["temperature_2m_max"];
    JsonArray tempMin = daily["temperature_2m_min"];
    JsonArray dates = daily["time"];
    JsonArray sunriseArr = daily["sunrise"];
    JsonArray sunsetArr = daily["sunset"];
    
    // Today's high/low (index 0)
    float high = tempMax[0] | temp;
    float low = tempMin[0] | temp;
    
    // Parse sunrise/sunset for today (format: "2024-01-15T06:45" -> "6:45 AM")
    char sunrise[12] = "";
    char sunset[12] = "";
    
    const char* sunriseStr = sunriseArr[0];
    const char* sunsetStr = sunsetArr[0];
    
    if (sunriseStr) {
        // Find 'T' and parse time after it
        const char* t = strchr(sunriseStr, 'T');
        if (t) {
            int hour, minute;
            if (sscanf(t + 1, "%d:%d", &hour, &minute) == 2) {
                const char* ampm = (hour < 12) ? "AM" : "PM";
                if (hour == 0) hour = 12;
                else if (hour > 12) hour -= 12;
                snprintf(sunrise, sizeof(sunrise), "%d:%02d %s", hour, minute, ampm);
            }
        }
    }
    
    if (sunsetStr) {
        const char* t = strchr(sunsetStr, 'T');
        if (t) {
            int hour, minute;
            if (sscanf(t + 1, "%d:%d", &hour, &minute) == 2) {
                const char* ampm = (hour < 12) ? "AM" : "PM";
                if (hour == 0) hour = 12;
                else if (hour > 12) hour -= 12;
                snprintf(sunset, sizeof(sunset), "%d:%02d %s", hour, minute, ampm);
            }
        }
    }
    
    // Next 3 days forecast (indices 1, 2, 3)
    float forecastHigh[3] = {0, 0, 0};
    float forecastLow[3] = {0, 0, 0};
    char forecastDay[3][4] = {"", "", ""};
    
    const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    for (int i = 0; i < 3 && i + 1 < tempMax.size(); i++) {
        forecastHigh[i] = tempMax[i + 1] | 0.0f;
        forecastLow[i] = tempMin[i + 1] | 0.0f;
        
        // Parse date to get day of week
        const char* dateStr = dates[i + 1];
        if (dateStr) {
            int year, month, day;
            if (sscanf(dateStr, "%d-%d-%d", &year, &month, &day) == 3) {
                // Zeller's formula for day of week
                if (month < 3) { month += 12; year--; }
                int q = day, m = month, k = year % 100, j = year / 100;
                int dow = (q + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
                dow = (dow + 6) % 7;  // Adjust: 0=Sun
                strncpy(forecastDay[i], dayNames[dow], 3);
                forecastDay[i][3] = '\0';
            }
        }
    }
    
    const char* location = settingsManager.weather.location;
    if (location[0] == '\0') location = "Unknown";
    
    saveWeatherCache(temp, weatherCode, humidity, wind, location, useCelsius,
                     high, low, forecastHigh, forecastLow, forecastDay, sunrise, sunset);
    Serial.printf("[HOME] Weather updated: %.1f° (H:%.0f L:%.0f) sunrise:%s sunset:%s\n", 
                  temp, high, low, sunrise, sunset);
}

// =============================================================================
// Widget Helper Functions
// =============================================================================
bool hasBookWidget() {
    return homeState.hasBook && settingsManager.display.showBookWidget && (homePageIndex == 0);
}

bool hasWeatherWidget() {
    return homeState.hasWeather && settingsManager.display.showWeatherWidget && (homePageIndex == 0);
}

bool hasOrientWidget() {
    return settingsManager.display.showOrientWidget && (homePageIndex == 0);
}

int getWidgetCount() {
    if (homePageIndex != 0) return 0;
    int count = 0;
    if (homeState.hasBook && settingsManager.display.showBookWidget) count++;
    if (homeState.hasWeather && settingsManager.display.showWeatherWidget) count++;
    if (settingsManager.display.showOrientWidget) count++;
    return count;
}

void toggleOrientation() {
    settingsManager.display.orientation = 1 - settingsManager.display.orientation;
    settingsManager.save();
    
    bool isHorizontal = (settingsManager.display.orientation == 0);
    display.setRotation(isHorizontal ? 0 : 3);
    setButtonOrientation(isHorizontal);
    
    homeState.isLandscape = isHorizontal;
    homeState.dirty = true;  // Force full recalculation
    homeState.coverCached = false;  // Cover size will change
    
    Serial.printf("[HOME] Toggled orientation to %s\n", isHorizontal ? "Landscape" : "Portrait");
}

void activateWidget(int widget) {
    int currentIdx = 0;
    
    if (homeState.hasBook && settingsManager.display.showBookWidget) {
        if (widget == currentIdx) {
            Serial.printf("[HOME] Activating book widget: %s\n", lastBookWidget.title);
            extern void openAppByItemIndex(uint8_t itemIndex);
            openAppByItemIndex(HOME_ITEM_LIBRARY);
            return;
        }
        currentIdx++;
    }
    
    if (homeState.hasWeather && settingsManager.display.showWeatherWidget) {
        if (widget == currentIdx) {
            Serial.println("[HOME] Activating weather widget - opening forecast");
            extern void openAppByItemIndex(uint8_t itemIndex);
            openAppByItemIndex(HOME_ITEM_WEATHER);
            return;
        }
        currentIdx++;
    }
    
    if (settingsManager.display.showOrientWidget) {
        if (widget == currentIdx) {
            Serial.println("[HOME] Activating orientation widget - toggling");
            toggleOrientation();
            showHomeScreen();
            return;
        }
    }
}

// =============================================================================
// Building Home Screen Items
// =============================================================================
void initHomeState() {
    memset(&homeState, 0, sizeof(homeState));
    homeState.initialized = false;
    homeState.dirty = true;
}

void buildHomeScreenItems() {
    enabledItemCount = 0;
    settingsManager.getEnabledHomeItems(enabledItemIndices, &enabledItemCount, MAX_HOME_ITEMS);
    
    Serial.printf("[HOME] Got %d items from settings\n", enabledItemCount);
    
    // Ensure Settings is always included
    bool hasSettings = false;
    for (int i = 0; i < enabledItemCount; i++) {
        if (enabledItemIndices[i] == HOME_ITEM_SETTINGS) {
            hasSettings = true;
            break;
        }
    }
    if (!hasSettings && enabledItemCount < MAX_HOME_ITEMS) {
        enabledItemIndices[enabledItemCount++] = HOME_ITEM_SETTINGS;
    }
    
    if (enabledItemCount == 0) {
        enabledItemIndices[enabledItemCount++] = HOME_ITEM_SETTINGS;
    }
    
    // Load widgets
    loadLastBookWidget();
    loadWeatherWidget();
    
    // Update homeState
    homeState.widgetCount = getWidgetCount();
    homeState.hasOrient = settingsManager.display.showOrientWidget;
    homeState.isLandscape = (settingsManager.display.orientation == 0);
    homeState.dirty = true;
    
    Serial.printf("[HOME] Final: %d items, hasBook=%d, hasWeather=%d\n", 
                  enabledItemCount, homeState.hasBook, homeState.hasWeather);
}

void prepareHomeScreen() {
    // Called during deploy screen - do all heavy work here
    Serial.println("[HOME] Preparing home screen (background)...");
    
    // Calculate geometry
    calculateCellGeometry(homeState.geo);
    homeState.totalPages = getTotalPages();
    homeState.itemsOnPage = getItemsOnCurrentPage();
    
    // Cache the book cover if we have one
    if (homeState.hasBook && homeState.bookCoverPath[0] != '\0') {
        // Calculate cover dimensions based on layout
        int coverW, coverH;
        if (homeState.isLandscape) {
            coverW = 200;
            coverH = display.height() - homeState.geo.gridTop - 100;
        } else {
            coverW = (display.width() - homeState.geo.gridPadding * 2) * 60 / 100;
            coverH = 340;
        }
        
        if (!homeState.coverCached || 
            homeState.cachedCoverW != coverW || 
            homeState.cachedCoverH != coverH) {
            createCachedCover(homeState.bookCoverPath, coverW, coverH);
        }
    }
    
    homeState.initialized = true;
    homeState.dirty = false;
    
    Serial.println("[HOME] Home screen prepared");
}

void updateGridLayout() {
    bool isHorizontal = (settingsManager.display.orientation == 0);
    
    if (isHorizontal) {
        itemsPerPage = 8;  // Landscape: 4x2 or smaller
    } else {
        itemsPerPage = 8;  // Portrait: 2x4 or smaller (consistent 2-column nav)
    }
    
    // Calculate actual geometry which will set homeCols/homeRows correctly
    calculateCellGeometry(homeState.geo);
    
    if (homeSelection >= getItemsOnCurrentPage()) homeSelection = 0;
    if (homePageIndex >= getTotalPages()) homePageIndex = 0;
    
    homeState.itemsOnPage = getItemsOnCurrentPage();
    homeState.totalPages = getTotalPages();
}

// =============================================================================
// Page/Item Calculations
// =============================================================================
int getTotalPages() {
    if (enabledItemCount == 0) return 1;
    return (enabledItemCount + itemsPerPage - 1) / itemsPerPage;
}

int getItemsOnCurrentPage() {
    int startIdx = homePageIndex * itemsPerPage;
    int remaining = enabledItemCount - startIdx;
    if (remaining <= 0) return 0;
    return min(remaining, itemsPerPage);
}

uint8_t getItemIndexForPosition(int position) {
    int actualIndex = homePageIndex * itemsPerPage + position;
    if (actualIndex >= 0 && actualIndex < enabledItemCount) {
        return enabledItemIndices[actualIndex];
    }
    return 0xFF;
}

// =============================================================================
// Cell Geometry
// =============================================================================
void calculateCellGeometry(CellGeometry& geo) {
    int w = display.width();
    int h = display.height();
    
    bool invertColors = settingsManager.display.invertColors;
    geo.bgColor = invertColors ? GxEPD_BLACK : GxEPD_WHITE;
    geo.fgColor = invertColors ? GxEPD_WHITE : GxEPD_BLACK;
    
    bool showStatusBar = settingsManager.display.showClockHome || 
                         settingsManager.display.showBatteryHome;
    geo.statusBarHeight = showStatusBar ? 30 : 0;
    geo.gridPadding = 12;
    geo.cellGap = 10;
    geo.gridTop = geo.statusBarHeight + 8;
    
    int widgetCount = getWidgetCount();
    bool isHorizontal = (settingsManager.display.orientation == 0);
    
    if (widgetCount > 0 && !isHorizontal) {
        bool showsBook = homeState.hasBook && settingsManager.display.showBookWidget;
        bool showsWeather = homeState.hasWeather && settingsManager.display.showWeatherWidget;
        bool showsOrient = settingsManager.display.showOrientWidget;
        
        int widgetAreaH;
        if (!showsBook && !showsWeather && showsOrient) {
            widgetAreaH = 50;
        } else if (!showsBook && showsWeather && !showsOrient) {
            widgetAreaH = 140;
        } else if (showsBook) {
            widgetAreaH = 340;
        } else {
            widgetAreaH = 180;
        }
        geo.gridTop += widgetAreaH + 10;
    }
    
    int pageIndicatorHeight = (getTotalPages() > 1) ? 30 : 10;
    int gridHeight = h - geo.gridTop - pageIndicatorHeight;
    
    int itemsOnPage = getItemsOnCurrentPage();
    if (itemsOnPage == 0) itemsOnPage = 1;
    
    int optimalCols, optimalRows;
    
    if (isHorizontal) {
        int availW = w - geo.gridPadding * 2;
        if (widgetCount > 0) availW -= 230;
        
        if (itemsOnPage <= 2) {
            optimalCols = 2; optimalRows = 1;
        } else if (itemsOnPage <= 4) {
            optimalCols = 2; optimalRows = 2;
        } else if (itemsOnPage <= 6) {
            optimalCols = 3; optimalRows = 2;
        } else {
            optimalCols = 4; optimalRows = 2;
        }
    } else {
        // Portrait mode - ALWAYS use 2 columns for consistent navigation
        // Max 8 items per page = 2x4 grid
        optimalCols = 2;
        if (itemsOnPage <= 2) {
            optimalRows = 1;
        } else if (itemsOnPage <= 4) {
            optimalRows = 2;
        } else if (itemsOnPage <= 6) {
            optimalRows = 3;
        } else {
            optimalRows = 4;  // Max 8 items = 2x4
        }
    }
    
    geo.cols = optimalCols;
    geo.rows = optimalRows;
    
    int actualRows = (itemsOnPage + geo.cols - 1) / geo.cols;
    if (actualRows < 1) actualRows = 1;
    geo.rows = min(geo.rows, actualRows);
    
    geo.cellWidth = (w - geo.gridPadding * 2 - geo.cellGap * (geo.cols - 1)) / geo.cols;
    
    if (isHorizontal && widgetCount > 0) {
        int availW = w - 230 - geo.gridPadding * 2;
        geo.cellWidth = (availW - geo.cellGap * (geo.cols - 1)) / geo.cols;
    }
    
    geo.cellHeight = (gridHeight - geo.cellGap * (geo.rows - 1)) / geo.rows;
    
    int maxCellHeight = isHorizontal ? 200 : 180;
    if (geo.cellHeight > maxCellHeight) {
        geo.cellHeight = maxCellHeight;
    }
    
    uint8_t buttonShape = settingsManager.display.buttonShape;
    switch (buttonShape) {
        case 1: geo.cornerRadius = min(geo.cellWidth, geo.cellHeight) / 2; break;
        case 2: geo.cornerRadius = 2; break;
        default: geo.cornerRadius = 10; break;
    }
    
    // CRITICAL: Sync global homeCols/homeRows with actual geometry for navigation
    homeCols = geo.cols;
    homeRows = geo.rows;
    
    Serial.printf("[HOME] Grid: %dx%d, items=%d, cellW=%d, cellH=%d, gridTop=%d, gridH=%d\n",
                  geo.cols, geo.rows, itemsOnPage, geo.cellWidth, geo.cellHeight, geo.gridTop, gridHeight);
}

void getCellPosition(const CellGeometry& geo, int cellIndex, int& cellX, int& cellY) {
    int row = cellIndex / geo.cols;
    int col = cellIndex % geo.cols;
    
    int offsetX = 0;
    int widgetCount = getWidgetCount();
    if (widgetCount > 0) {
        bool isHorizontal = (settingsManager.display.orientation == 0);
        if (isHorizontal) {
            offsetX = 220;
        }
    }
    
    cellX = geo.gridPadding + offsetX + col * (geo.cellWidth + geo.cellGap);
    cellY = geo.gridTop + row * (geo.cellHeight + geo.cellGap);
}

// =============================================================================
// Weather Icon Drawing
// =============================================================================
// Forward declaration
void drawWeatherIcon(int code, int x, int y, int size, uint16_t color);

// Draw weather icon from BMP file on SD card
bool drawWeatherIconBMP(int code, int x, int y, int size, uint16_t fgColor, uint16_t bgColor) {
    // Map WMO code to icon filename
    const char* iconName;
    if (code == 0) iconName = "sunny";
    else if (code <= 2) iconName = "partlycloudy";
    else if (code <= 3) iconName = "cloudy";
    else if (code <= 49) iconName = "fog";
    else if (code <= 69) iconName = "rain";
    else if (code <= 79) iconName = "snow";
    else if (code <= 86) iconName = "showers";
    else iconName = "thunderstorm";
    
    // Build path
    char path[64];
    snprintf(path, sizeof(path), "/weather/%s_%d.bmp", iconName, size <= 48 ? 48 : 64);
    
    File f = SD.open(path, FILE_READ);
    if (!f) {
        // Fall back to programmatic drawing
        drawWeatherIcon(code, x, y, size, fgColor);
        return false;
    }
    
    // Read BMP header
    uint8_t header[62];
    f.read(header, 62);
    
    // Parse BMP header
    int dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    int width = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    int height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    int bpp = header[28] | (header[29] << 8);
    
    if (bpp != 1 || width <= 0 || height <= 0) {
        f.close();
        drawWeatherIcon(code, x, y, size, fgColor);
        return false;
    }
    
    // Row size (padded to 4 bytes)
    int rowSize = ((width + 31) / 32) * 4;
    
    // Seek to pixel data
    f.seek(dataOffset);
    
    // BMP is stored bottom-up
    uint8_t rowBuf[16];  // Max 128 pixels wide for 1-bit
    
    // Scale factor if needed
    float scale = (float)size / width;
    
    for (int row = height - 1; row >= 0; row--) {
        f.read(rowBuf, rowSize);
        
        for (int col = 0; col < width; col++) {
            int byteIdx = col / 8;
            int bitIdx = 7 - (col % 8);
            bool bitSet = (rowBuf[byteIdx] >> bitIdx) & 1;
            
            // In standard 1-bit BMP: palette index 0=black, 1=white
            // Our BMPs have black icon on white background
            // Icon pixels = 0 (black), background = 1 (white)
            // Draw icon pixels (where bit is 0, not 1)
            int px = x + (int)(col * scale);
            int py = y + (int)((height - 1 - row) * scale);
            
            if (!bitSet) {
                display.drawPixel(px, py, fgColor);
            }
            // Don't draw background pixels - already transparent
        }
    }
    
    f.close();
    return true;
}

void drawWeatherIcon(int code, int x, int y, int size, uint16_t color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 3;
    
    if (code == 0) {
        display.drawCircle(cx, cy, r, color);
        for (int i = 0; i < 8; i++) {
            float angle = i * 3.14159 / 4;
            int x1 = cx + (r + 4) * cos(angle);
            int y1 = cy + (r + 4) * sin(angle);
            int x2 = cx + (r + 8) * cos(angle);
            int y2 = cy + (r + 8) * sin(angle);
            display.drawLine(x1, y1, x2, y2, color);
        }
    } else if (code <= 3) {
        display.drawCircle(cx - 5, cy - 5, r - 3, color);
        display.fillCircle(cx - 2, cy + 3, 6, color);
        display.fillCircle(cx + 6, cy + 3, 6, color);
        display.fillCircle(cx + 2, cy - 2, 5, color);
        display.fillRect(cx - 8, cy + 3, 20, 8, color);
    } else if (code <= 49) {
        for (int i = 0; i < 4; i++) {
            display.drawLine(x + 5, y + 8 + i * 8, x + size - 5, y + 8 + i * 8, color);
        }
    } else if (code <= 69) {
        display.fillCircle(cx - 5, cy - 5, 8, color);
        display.fillCircle(cx + 5, cy - 5, 8, color);
        display.fillCircle(cx, cy - 10, 6, color);
        display.fillRect(cx - 13, cy - 5, 26, 10, color);
        for (int i = 0; i < 3; i++) {
            display.drawLine(cx - 8 + i * 8, cy + 8, cx - 10 + i * 8, cy + 15, color);
        }
    } else if (code <= 79) {
        display.fillCircle(cx - 5, cy - 5, 8, color);
        display.fillCircle(cx + 5, cy - 5, 8, color);
        display.fillRect(cx - 13, cy - 5, 26, 10, color);
        for (int i = 0; i < 3; i++) {
            int sx = cx - 8 + i * 8;
            int sy = cy + 12;
            display.drawLine(sx - 3, sy, sx + 3, sy, color);
            display.drawLine(sx, sy - 3, sx, sy + 3, color);
        }
    } else {
        display.fillCircle(cx - 5, cy - 8, 8, color);
        display.fillCircle(cx + 5, cy - 8, 8, color);
        display.fillRect(cx - 13, cy - 8, 26, 10, color);
        display.drawLine(cx, cy + 2, cx - 4, cy + 10, color);
        display.drawLine(cx - 4, cy + 10, cx + 2, cy + 10, color);
        display.drawLine(cx + 2, cy + 10, cx - 2, cy + 18, color);
    }
}

// =============================================================================
// Widget Drawing - Uses cached cover for speed
// =============================================================================
static void drawWidgets(const CellGeometry& geo) {
    if (homePageIndex != 0) return;
    
    int widgetCount = getWidgetCount();
    if (widgetCount == 0) return;
    
    int w = display.width();
    int h = display.height();
    bool isLandscape = (settingsManager.display.orientation == 0);
    
    bool showBook = homeState.hasBook && settingsManager.display.showBookWidget;
    bool showWeather = homeState.hasWeather && settingsManager.display.showWeatherWidget;
    bool showOrient = settingsManager.display.showOrientWidget;
    
    int currentWidgetIndex = 0;
    
    if (isLandscape) {
        // === LANDSCAPE MODE ===
        int widgetAreaX = geo.gridPadding;
        int widgetAreaY = geo.gridTop;
        int widgetAreaW = 200;
        int widgetAreaH = h - geo.gridTop - 40;
        
        int bottomRowH = 50;
        int topRowH = showBook ? (widgetAreaH - bottomRowH - 8) : 0;
        int bottomRowY = widgetAreaY + (showBook ? topRowH + 8 : 0);
        
        // Book Widget
        if (showBook) {
            bool isSelected = (widgetSelection == currentWidgetIndex);
            
            if (isSelected) {
                display.fillRoundRect(widgetAreaX - 3, widgetAreaY - 3, widgetAreaW + 6, topRowH + 6, 6, geo.fgColor);
            }
            
            // Draw cached cover (fast!) or fallback
            if (!_skipCoverDraw) {
                bool drawn = false;
                if (homeState.coverCached) {
                    int coverX = widgetAreaX + (widgetAreaW - homeState.cachedCoverW) / 2;
                    int coverY = widgetAreaY + (topRowH - homeState.cachedCoverH) / 2;
                    drawn = drawCachedCover(coverX, coverY);
                }
                
                if (!drawn) {
                    // Fallback: placeholder
                    display.fillRoundRect(widgetAreaX, widgetAreaY, widgetAreaW, topRowH, 4, geo.bgColor);
                    display.drawRoundRect(widgetAreaX, widgetAreaY, widgetAreaW, topRowH, 4, geo.fgColor);
                    for (int i = 0; i < 5; i++) {
                        int lineY = widgetAreaY + 20 + i * (topRowH - 40) / 5;
                        display.drawLine(widgetAreaX + 15, lineY, widgetAreaX + widgetAreaW - 15, lineY, geo.fgColor);
                    }
                }
            } else {
                // When skipping cover draw, still draw border outline
                if (!isSelected) {
                    display.drawRoundRect(widgetAreaX, widgetAreaY, widgetAreaW, topRowH, 4, geo.fgColor);
                }
            }
            
            currentWidgetIndex++;
        }
        
        // Bottom row: Weather + Orientation
        if (showWeather || showOrient) {
            int weatherW = 0, orientW = 0;
            if (showWeather && showOrient) {
                weatherW = (widgetAreaW - 8) * 2 / 3;
                orientW = widgetAreaW - weatherW - 8;
            } else if (showWeather) {
                weatherW = widgetAreaW;
            } else {
                orientW = widgetAreaW;
            }
            
            int currentX = widgetAreaX;
            
            if (showWeather) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t textColor = isSelected ? geo.bgColor : geo.fgColor;
                uint16_t bgCol = isSelected ? geo.fgColor : geo.bgColor;
                
                if (isSelected) {
                    display.fillRoundRect(currentX, bottomRowY, weatherW, bottomRowH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(currentX, bottomRowY, weatherW, bottomRowH, 6, geo.fgColor);
                }
                
                int iconSize = bottomRowH - 20;
                if (iconSize < 20) iconSize = 20;
                drawWeatherIconBMP(homeState.weatherCode, currentX + 8, bottomRowY + 10, iconSize, textColor, bgCol);
                
                display.setFont(&FreeSansBold12pt7b);
                display.setTextColor(textColor);
                char tempStr[12];
                snprintf(tempStr, sizeof(tempStr), "%.0f%c", homeState.weatherTemp, 
                         homeState.weatherCelsius ? 'C' : 'F');
                display.setCursor(currentX + iconSize + 12, bottomRowY + 22);
                display.print(tempStr);
                
                display.setFont(&FreeSans9pt7b);
                if (homeState.weatherLocation[0] != '\0') {
                    char locStr[12];
                    strncpy(locStr, homeState.weatherLocation, 11);
                    locStr[11] = '\0';
                    display.setCursor(currentX + iconSize + 12, bottomRowY + bottomRowH - 10);
                    display.print(locStr);
                }
                
                currentX += weatherW + 8;
                currentWidgetIndex++;
            }
            
            if (showOrient) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t fg = isSelected ? geo.bgColor : geo.fgColor;
                
                if (isSelected) {
                    display.fillRoundRect(currentX, bottomRowY, orientW, bottomRowH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(currentX, bottomRowY, orientW, bottomRowH, 6, geo.fgColor);
                }
                
                int switchW = min(orientW - 12, 50);
                int switchH = min(bottomRowH - 16, 24);
                int switchX = currentX + (orientW - switchW) / 2;
                int switchY = bottomRowY + (bottomRowH - switchH) / 2;
                
                display.drawRoundRect(switchX, switchY, switchW, switchH, switchH/2, fg);
                
                int knobW = switchW / 2 - 4;
                int knobX = isLandscape ? (switchX + 3) : (switchX + switchW/2 + 1);
                display.fillRoundRect(knobX, switchY + 3, knobW, switchH - 6, (switchH - 6)/2, fg);
            }
        }
        
    } else {
        // === PORTRAIT MODE ===
        int widgetAreaY = geo.statusBarHeight + 8;
        int widgetAreaW = w - geo.gridPadding * 2;
        
        int widgetAreaH;
        if (!showBook && !showWeather && showOrient) {
            widgetAreaH = 50;
        } else if (!showBook && showWeather && !showOrient) {
            widgetAreaH = 140;
        } else if (showBook) {
            widgetAreaH = 340;
        } else {
            widgetAreaH = 180;
        }
        
        int leftColW = showBook ? (widgetAreaW * 60 / 100) : 0;
        int rightColW = showBook ? (widgetAreaW - leftColW - 10) : widgetAreaW;
        int leftColX = geo.gridPadding;
        int rightColX = showBook ? (geo.gridPadding + leftColW + 10) : geo.gridPadding;
        
        // Book Widget
        if (showBook) {
            bool isSelected = (widgetSelection == currentWidgetIndex);
            
            if (isSelected) {
                display.fillRoundRect(leftColX - 3, widgetAreaY - 3, leftColW + 6, widgetAreaH + 6, 6, geo.fgColor);
            }
            
            if (!_skipCoverDraw) {
                bool drawn = false;
                if (homeState.coverCached) {
                    int coverX = leftColX + (leftColW - homeState.cachedCoverW) / 2;
                    int coverY = widgetAreaY + (widgetAreaH - homeState.cachedCoverH) / 2;
                    drawn = drawCachedCover(coverX, coverY);
                }
                
                if (!drawn) {
                    display.fillRoundRect(leftColX, widgetAreaY, leftColW, widgetAreaH, 4, geo.bgColor);
                    display.drawRoundRect(leftColX, widgetAreaY, leftColW, widgetAreaH, 4, geo.fgColor);
                    for (int i = 0; i < 7; i++) {
                        int lineY = widgetAreaY + 20 + i * (widgetAreaH - 40) / 7;
                        display.drawLine(leftColX + 15, lineY, leftColX + leftColW - 15, lineY, geo.fgColor);
                    }
                }
            } else {
                // When skipping cover draw, still draw the border/placeholder outline
                // so the area doesn't get blanked
                if (!isSelected) {
                    display.drawRoundRect(leftColX, widgetAreaY, leftColW, widgetAreaH, 4, geo.fgColor);
                }
            }
            
            currentWidgetIndex++;
        }
        
        // Right Column: Weather + Orientation
        if (showWeather || showOrient) {
            int rightY = widgetAreaY;
            
            int weatherH = 0, orientH = 0;
            if (showWeather && showOrient) {
                weatherH = widgetAreaH * 70 / 100;
                orientH = widgetAreaH - weatherH - 8;
            } else if (showWeather) {
                weatherH = widgetAreaH;
            } else {
                orientH = widgetAreaH;
            }
            
            if (showWeather) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t textColor = isSelected ? geo.bgColor : geo.fgColor;
                uint16_t bgColor = isSelected ? geo.fgColor : geo.bgColor;
                
                // Draw widget background
                if (isSelected) {
                    display.fillRoundRect(rightColX, rightY, rightColW, weatherH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(rightColX, rightY, rightColW, weatherH, 6, geo.fgColor);
                }
                
                int16_t tx, ty;
                uint16_t tw, th;
                int pad = 8;
                
                // === ROW 1: LARGE Icon (left) + BIG Temp (right) ===
                int row1H = 100;  // Bigger row for larger icon
                int row1CenterY = rightY + row1H / 2 + 15;  // More top padding
                
                // LARGE weather icon - 80px (scaled from 64px BMP)
                int iconSize = 80;
                int iconX = rightColX + pad;
                int iconY = row1CenterY - iconSize/2;
                drawWeatherIconBMP(homeState.weatherCode, iconX, iconY, iconSize, textColor, bgColor);
                
                // BIG temperature RIGHT of icon - use 24pt bold
                display.setFont(&FreeSansBold24pt7b);
                display.setTextColor(textColor);
                char tempStr[16];
                snprintf(tempStr, sizeof(tempStr), "%.0f", homeState.weatherTemp);
                display.getTextBounds(tempStr, 0, 0, &tx, &ty, &tw, &th);
                int tempX = iconX + iconSize + 5;
                display.setCursor(tempX, row1CenterY + th/3);
                display.print(tempStr);
                int tempEndX = tempX + tw;
                
                // Degree symbol
                display.setFont(&FreeSansBold12pt7b);
                display.setCursor(tempEndX + 2, row1CenterY - th/3 + 8);
                display.print("o");
                
                // === ROW 2: High / Low centered ===
                int row2Y = rightY + row1H + 15;  // Match top padding
                
                display.setFont(&FreeSansBold12pt7b);
                snprintf(tempStr, sizeof(tempStr), "%d", (int)homeState.weatherHigh);
                display.getTextBounds(tempStr, 0, 0, &tx, &ty, &tw, &th);
                int highW = tw;
                
                display.setFont(&FreeSans12pt7b);
                char lowStr[8];
                snprintf(lowStr, sizeof(lowStr), "%d", (int)homeState.weatherLow);
                display.getTextBounds(lowStr, 0, 0, &tx, &ty, &tw, &th);
                int lowW = tw;
                
                int totalHiLoW = highW + 30 + lowW;
                int hiloStartX = rightColX + (rightColW - totalHiLoW) / 2;
                
                display.setFont(&FreeSansBold12pt7b);
                display.setCursor(hiloStartX, row2Y + 18);
                snprintf(tempStr, sizeof(tempStr), "%d", (int)homeState.weatherHigh);
                display.print(tempStr);
                
                display.setFont(&FreeSans12pt7b);
                display.setCursor(hiloStartX + highW + 8, row2Y + 18);
                display.print("/");
                display.setCursor(hiloStartX + highW + 22, row2Y + 18);
                snprintf(lowStr, sizeof(lowStr), "%d", (int)homeState.weatherLow);
                display.print(lowStr);
                
                // === ROW 3: Humidity centered ===
                int row3Y = row2Y + 28;
                display.setFont(&FreeSans9pt7b);
                char humStr[16];
                snprintf(humStr, sizeof(humStr), "Humidity: %d%%", homeState.weatherHumidity);
                display.getTextBounds(humStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(rightColX + (rightColW - tw) / 2, row3Y + 14);
                display.print(humStr);
                
                // === ROW 4: Sunrise / Sunset (if available) ===
                int row4Y = row3Y + 22;
                if (homeState.sunrise[0] != '\0' && homeState.sunset[0] != '\0') {
                    display.setFont(&FreeSans9pt7b);
                    char sunStr[32];
                    snprintf(sunStr, sizeof(sunStr), "%s - %s", homeState.sunrise, homeState.sunset);
                    display.getTextBounds(sunStr, 0, 0, &tx, &ty, &tw, &th);
                    display.setCursor(rightColX + (rightColW - tw) / 2, row4Y + 12);
                    display.print(sunStr);
                }
                
                // === Divider line before forecast ===
                int dividerY = row4Y + 22;
                display.drawLine(rightColX + pad, dividerY, rightColX + rightColW - pad, dividerY, textColor);
                
                // === FORECAST ROWS (remaining space) ===
                int forecastStartY = dividerY + 8;
                int forecastAreaH = weatherH - (forecastStartY - rightY) - 5;
                int rowH = forecastAreaH / 3;
                if (rowH > 26) rowH = 26;
                
                for (int i = 0; i < 3; i++) {
                    if (homeState.forecastDay[i][0] == '\0') continue;
                    
                    int rowY = forecastStartY + i * rowH + rowH/2 + 4;
                    
                    // Day name (left)
                    display.setFont(&FreeSansBold9pt7b);
                    display.setTextColor(textColor);
                    char dayStr[4];
                    strncpy(dayStr, homeState.forecastDay[i], 3);
                    dayStr[3] = '\0';
                    display.setCursor(rightColX + pad, rowY);
                    display.print(dayStr);
                    
                    // Low temp (center)
                    display.setFont(&FreeSans9pt7b);
                    snprintf(lowStr, sizeof(lowStr), "%d", (int)homeState.forecastLow[i]);
                    display.getTextBounds(lowStr, 0, 0, &tx, &ty, &tw, &th);
                    display.setCursor(rightColX + rightColW/2 - tw/2, rowY);
                    display.print(lowStr);
                    
                    // High temp (right)
                    display.setFont(&FreeSansBold9pt7b);
                    char highStr[8];
                    snprintf(highStr, sizeof(highStr), "%d", (int)homeState.forecastHigh[i]);
                    display.getTextBounds(highStr, 0, 0, &tx, &ty, &tw, &th);
                    display.setCursor(rightColX + rightColW - pad - tw, rowY);
                    display.print(highStr);
                }
                
                currentWidgetIndex++;
                rightY += weatherH + 8;
            }
            
            if (showOrient) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t fg = isSelected ? geo.bgColor : geo.fgColor;
                
                if (isSelected) {
                    display.fillRoundRect(rightColX, rightY, rightColW, orientH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(rightColX, rightY, rightColW, orientH, 6, geo.fgColor);
                }
                
                int switchW = min(rightColW - 30, 80);
                int switchH = min(orientH - 12, 28);
                int switchX = rightColX + (rightColW - switchW) / 2;
                int switchY = rightY + (orientH - switchH) / 2;
                
                display.drawRoundRect(switchX, switchY, switchW, switchH, switchH/2, fg);
                
                int knobW = switchW / 2 - 4;
                int knobX = isLandscape ? (switchX + 3) : (switchX + switchW/2 + 1);
                display.fillRoundRect(knobX, switchY + 3, knobW, switchH - 6, (switchH - 6)/2, fg);
            }
        }
    }
}

// =============================================================================
// Grid Drawing - ALWAYS draws ALL items (fixes disappearing bug)
// =============================================================================
static void drawGrid(const CellGeometry& geo) {
    int w = display.width();
    
    int widgetCount = getWidgetCount();
    bool showWidgets = (widgetCount > 0) && (homePageIndex == 0);
    bool isLandscape = (settingsManager.display.orientation == 0);
    
    // Calculate adjusted cell width for landscape with widgets
    int actualCellWidth = geo.cellWidth;
    if (showWidgets && isLandscape) {
        int availableWidth = w - 220 - geo.gridPadding * 2;
        actualCellWidth = (availableWidth - geo.cellGap * (geo.cols - 1)) / geo.cols;
    }
    
    // ALWAYS draw ALL items - this fixes the disappearing bug
    int itemsOnPage = getItemsOnCurrentPage();
    for (int i = 0; i < itemsOnPage; i++) {
        int cellX, cellY;
        getCellPosition(geo, i, cellX, cellY);
        
        // Recalculate cell position if widgets present in landscape
        if (showWidgets && isLandscape) {
            int col = i % geo.cols;
            cellX = geo.gridPadding + 220 + col * (actualCellWidth + geo.cellGap);
        }
        
        uint8_t itemIndex = getItemIndexForPosition(i);
        const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
        const char* label = itemInfo ? itemInfo->label : "???";
        
        // Only highlight selected cell if not in widget mode
        bool selected = (i == homeSelection) && (widgetSelection < 0);
        bool highContrast = (settingsManager.display.bgTheme == 2);
        
        int cellW = (showWidgets && isLandscape) ? actualCellWidth : geo.cellWidth;
        
        // ALWAYS draw the cell background and border
        if (selected) {
            display.fillRoundRect(cellX, cellY, cellW, geo.cellHeight, geo.cornerRadius, geo.fgColor);
            display.setTextColor(geo.bgColor);
        } else {
            display.fillRoundRect(cellX, cellY, cellW, geo.cellHeight, geo.cornerRadius, geo.bgColor);
            display.drawRoundRect(cellX, cellY, cellW, geo.cellHeight, geo.cornerRadius, geo.fgColor);
            if (highContrast) {
                display.drawRoundRect(cellX+1, cellY+1, cellW-2, geo.cellHeight-2, 
                                     geo.cornerRadius > 2 ? geo.cornerRadius-1 : geo.cornerRadius, geo.fgColor);
            }
            display.setTextColor(geo.fgColor);
        }
        
        // Font size based on cell width
        uint8_t fontSize = settingsManager.display.fontSize;
        if (fontSize <= 14 || cellW < 80) {
            display.setFont(&FreeSansBold9pt7b);
        } else {
            display.setFont(&FreeSansBold12pt7b);
        }
        
        // Measure and truncate label if needed
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
        
        char truncLabel[16];
        strncpy(truncLabel, label, sizeof(truncLabel) - 1);
        truncLabel[sizeof(truncLabel) - 1] = '\0';
        while (tw > cellW - 10 && strlen(truncLabel) > 3) {
            truncLabel[strlen(truncLabel) - 1] = '\0';
            display.getTextBounds(truncLabel, 0, 0, &tx, &ty, &tw, &th);
        }
        
        // Center text in cell
        display.setCursor(cellX + (cellW - tw) / 2, cellY + (geo.cellHeight + th) / 2);
        display.print(truncLabel);
    }
}

// =============================================================================
// Main Drawing Functions
// =============================================================================
void showHomeScreen() {
    Serial.println("[HOME] Full refresh");
    
    // Recalculate if needed
    if (homeState.dirty || !homeState.initialized) {
        calculateCellGeometry(homeState.geo);
        homeState.totalPages = getTotalPages();
        homeState.itemsOnPage = getItemsOnCurrentPage();
        homeState.dirty = false;
        homeState.initialized = true;
    }
    
    CellGeometry& geo = homeState.geo;
    int w = display.width();
    int h = display.height();
    int totalPages = homeState.totalPages;
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(geo.bgColor);
        
        // Status bar
        if (geo.statusBarHeight > 0) {
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(geo.fgColor);
            
            if (settingsManager.display.showClockHome) {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    char timeStr[16];
                    strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
                    display.setCursor(12, 22);
                    display.print(timeStr);
                }
            }
            
            if (settingsManager.display.showBatteryHome) {
                char battStr[8];
                snprintf(battStr, sizeof(battStr), "%d%%", batteryMonitor.getPercent());
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(battStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(w - tw - 10, 22);
                display.print(battStr);
            }
        }
        
        // Draw widgets
        drawWidgets(geo);
        
        // Draw grid (ALL items)
        drawGrid(geo);
        
        // Page indicator
        if (totalPages > 1) {
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(geo.fgColor);
            char pageStr[16];
            snprintf(pageStr, sizeof(pageStr), "Page %d/%d", homePageIndex + 1, totalPages);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(pageStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(w / 2 - tw / 2, h - 15);
            display.print(pageStr);
        }
        
    } while (display.nextPage());
    
    refreshManager.recordFullRefresh();
}

void showHomeScreenPartial(bool partialRefresh) {
    CellGeometry& geo = homeState.geo;
    int w = display.width();
    int h = display.height();
    int totalPages = homeState.totalPages;
    
    // Start partial refresh slightly above widgets to ensure borders aren't clipped
    int refreshTop = max(0, geo.statusBarHeight - 2);
    int refreshHeight = h - refreshTop;
    
    if (partialRefresh) {
        display.setPartialWindow(0, refreshTop, w, refreshHeight);
    } else {
        display.setFullWindow();
    }
    
    display.firstPage();
    do {
        display.fillScreen(geo.bgColor);
        
        // Status bar (only on full refresh)
        if (!partialRefresh && geo.statusBarHeight > 0) {
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(geo.fgColor);
            
            if (settingsManager.display.showClockHome) {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    char timeStr[16];
                    strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
                    display.setCursor(12, 22);
                    display.print(timeStr);
                }
            }
            
            if (settingsManager.display.showBatteryHome) {
                char battStr[8];
                snprintf(battStr, sizeof(battStr), "%d%%", batteryMonitor.getPercent());
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(battStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(w - tw - 10, 22);
                display.print(battStr);
            }
        }
        
        // Draw widgets
        drawWidgets(geo);
        
        // Draw grid (ALL items)
        drawGrid(geo);
        
        // Page indicator
        if (totalPages > 1) {
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(geo.fgColor);
            char pageStr[16];
            snprintf(pageStr, sizeof(pageStr), "Page %d/%d", homePageIndex + 1, totalPages);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(pageStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(w / 2 - tw / 2, h - 15);
            display.print(pageStr);
        }
        
    } while (display.nextPage());
    
    if (partialRefresh) {
        refreshManager.recordPartialRefresh();
    } else {
        refreshManager.recordFullRefresh();
    }
}

void showHomeScreenFast() {
    // Always draw cover - cached cover is fast enough
    showHomeScreenPartial(true);
}

void showHomeScreenPartialFast() {
    showHomeScreenFast();
}

// =============================================================================
// Selection-Only Refresh (minimal update)
// =============================================================================
void refreshCellSelection(int oldSel, int newSel) {
    if (refreshManager.mustFullRefresh() && refreshManager.canFullRefresh()) {
        showHomeScreen();
        return;
    }
    
    CellGeometry& geo = homeState.geo;
    int w = display.width();
    int h = display.height();
    
    bool showWidgets = (getWidgetCount() > 0) && (homePageIndex == 0);
    bool isLandscape = (settingsManager.display.orientation == 0);
    
    int actualCellWidth = geo.cellWidth;
    int gridOffsetX = geo.gridPadding;
    
    if (showWidgets && isLandscape) {
        int availableWidth = w - 220 - geo.gridPadding * 2;
        actualCellWidth = (availableWidth - geo.cellGap * (geo.cols - 1)) / geo.cols;
        gridOffsetX = geo.gridPadding + 220;
    }
    
    // Get cell positions
    int oldCol = oldSel % geo.cols;
    int oldRow = oldSel / geo.cols;
    int newCol = newSel % geo.cols;
    int newRow = newSel / geo.cols;
    
    int oldX = gridOffsetX + oldCol * (actualCellWidth + geo.cellGap);
    int oldY = geo.gridTop + oldRow * (geo.cellHeight + geo.cellGap);
    int newX = gridOffsetX + newCol * (actualCellWidth + geo.cellGap);
    int newY = geo.gridTop + newRow * (geo.cellHeight + geo.cellGap);
    
    // LARGER margin to ensure cell borders don't get clipped
    int margin = 12;
    
    // Calculate bounding box that covers ONLY the two cells we're updating
    int refreshX = min(oldX, newX) - margin;
    int refreshY = min(oldY, newY) - margin;
    int refreshW = max(oldX, newX) + actualCellWidth + margin * 2 - refreshX;
    int refreshH = max(oldY, newY) + geo.cellHeight + margin * 2 - refreshY;
    
    // Clamp to screen bounds
    if (refreshX < 0) refreshX = 0;
    if (refreshY < 0) refreshY = 0;
    if (refreshX + refreshW > w) refreshW = w - refreshX;
    if (refreshY + refreshH > h) refreshH = h - refreshY;
    
    display.setPartialWindow(refreshX, refreshY, refreshW, refreshH);
    
    display.firstPage();
    do {
        // Fill background
        display.fillRect(refreshX, refreshY, refreshW, refreshH, geo.bgColor);
        
        // Determine which cells overlap with refresh area and redraw them
        int itemsOnPage = getItemsOnCurrentPage();
        
        for (int i = 0; i < itemsOnPage; i++) {
            int cellRow = i / geo.cols;
            int cellCol = i % geo.cols;
            
            int cellX = gridOffsetX + cellCol * (actualCellWidth + geo.cellGap);
            int cellY = geo.gridTop + cellRow * (geo.cellHeight + geo.cellGap);
            
            // Check if this cell overlaps with the refresh window (any overlap)
            bool overlapsX = (cellX < refreshX + refreshW) && (cellX + actualCellWidth > refreshX);
            bool overlapsY = (cellY < refreshY + refreshH) && (cellY + geo.cellHeight > refreshY);
            if (!overlapsX || !overlapsY) continue;
            
            bool selected = (i == newSel);
            
            if (selected) {
                display.fillRoundRect(cellX, cellY, actualCellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
                display.setTextColor(geo.bgColor);
            } else {
                display.fillRoundRect(cellX, cellY, actualCellWidth, geo.cellHeight, geo.cornerRadius, geo.bgColor);
                display.drawRoundRect(cellX, cellY, actualCellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
                display.setTextColor(geo.fgColor);
            }
            
            uint8_t itemIdx = getItemIndexForPosition(i);
            const HomeItemInfo* info = getHomeItemByIndex(itemIdx);
            const char* label = info ? info->label : "???";
            
            if (actualCellWidth < 80) {
                display.setFont(&FreeSansBold9pt7b);
            } else {
                display.setFont(&FreeSansBold12pt7b);
            }
            
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(cellX + (actualCellWidth - tw) / 2, cellY + (geo.cellHeight + th) / 2);
            display.print(label);
        }
        
    } while (display.nextPage());
    
    refreshManager.recordPartialRefresh();
}

void refreshChangedCells(int oldSelection, int newSelection) {
    refreshCellSelection(oldSelection, newSelection);
}

void refreshWidgetSelection(int oldWidget, int newWidget) {
    // Full partial refresh to ensure borders are drawn correctly
    // The fill/border transitions need a clean background
    showHomeScreenPartial(true);
}

void drawSingleCell(int cellIndex, bool selected) {
    CellGeometry& geo = homeState.geo;
    
    int cellX, cellY;
    getCellPosition(geo, cellIndex, cellX, cellY);
    
    uint8_t itemIndex = getItemIndexForPosition(cellIndex);
    const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
    const char* label = itemInfo ? itemInfo->label : "???";
    
    if (selected) {
        display.fillRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.bgColor);
    } else {
        display.fillRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.bgColor);
        display.drawRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.fgColor);
    }
    
    display.setFont(&FreeSansBold12pt7b);
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(cellX + (geo.cellWidth - tw) / 2, cellY + (geo.cellHeight + th) / 2);
    display.print(label);
}

void refreshGridOnly() {
    CellGeometry& geo = homeState.geo;
    int w = display.width();
    int h = display.height();
    
    // Calculate grid area
    int gridLeft = geo.gridPadding;
    if (getWidgetCount() > 0 && settingsManager.display.orientation == 0) {
        gridLeft += 220;
    }
    
    int pageIndicatorHeight = (homeState.totalPages > 1) ? 30 : 10;
    int gridHeight = h - geo.gridTop - pageIndicatorHeight;
    
    display.setPartialWindow(gridLeft, geo.gridTop, w - gridLeft - geo.gridPadding, gridHeight);
    
    display.firstPage();
    do {
        display.fillRect(gridLeft, geo.gridTop, w - gridLeft - geo.gridPadding, gridHeight, geo.bgColor);
        drawGrid(geo);
    } while (display.nextPage());
    
    refreshManager.recordPartialRefresh();
}
