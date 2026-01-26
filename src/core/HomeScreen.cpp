/**
 * @file HomeScreen.cpp
 * @brief Home screen display and navigation implementation
 * @version 1.3.0
 * 
 * Community-inspired features:
 * - Continue Reading widget showing current book with cover
 * - Weather widget showing current conditions
 * - Cleaner, more minimal interface
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

// Cover drawing offsets (set before drawing cover)
static int _widgetCoverX = 0;
static int _widgetCoverY = 0;
static int _widgetCoverMaxW = 50;
static int _widgetCoverMaxH = 70;
static int _widgetBoundsX = 0;  // Widget left edge for clipping
static int _widgetBoundsY = 0;  // Widget top edge for clipping
static float _coverScaleFactor = 1.0f;  // Software scale factor (0.75 for 3/8 effective)

// TJpgDec callback for widget cover with software scaling
static bool _widgetCoverCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // Widget clip bounds
    int clipLeft = _widgetBoundsX;
    int clipTop = _widgetBoundsY;
    int clipRight = _widgetBoundsX + _widgetCoverMaxW;
    int clipBottom = _widgetBoundsY + _widgetCoverMaxH;
    
    // Bayer 4x4 dithering matrix (values 0-255)
    static const uint8_t bayer[4][4] = {
        {  15, 135,  45, 165 },
        { 195,  75, 225, 105 },
        {  60, 180,  30, 150 },
        { 240, 120, 210,  90 }
    };
    
    // Apply software scaling
    float scale = _coverScaleFactor;
    
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            // Get RGB565 pixel and convert to grayscale
            uint16_t pixel = bitmap[j * w + i];
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;
            uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
            
            // Calculate output rectangle for this source pixel
            int px1 = _widgetCoverX + (int)((x + i) * scale);
            int py1 = _widgetCoverY + (int)((y + j) * scale);
            int px2 = _widgetCoverX + (int)((x + i + 1) * scale);
            int py2 = _widgetCoverY + (int)((y + j + 1) * scale);
            
            // Ensure at least 1 pixel is drawn
            if (px2 <= px1) px2 = px1 + 1;
            if (py2 <= py1) py2 = py1 + 1;
            
            // Fill the entire scaled pixel area
            for (int fy = py1; fy < py2 && fy < clipBottom; fy++) {
                if (fy < clipTop) continue;
                for (int fx = px1; fx < px2 && fx < clipRight; fx++) {
                    if (fx < clipLeft) continue;
                    if (fx < 0 || fy < 0 || fx >= display.width() || fy >= display.height()) continue;
                    
                    // Use position-based dithering
                    uint8_t threshold = bayer[fy & 3][fx & 3];
                    display.drawPixel(fx, fy, (gray > threshold) ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
        }
    }
    return true;
}

// Draw cover in widget area - scales to FIT exactly within widget bounds
// Global flag to track cover decode state across paged drawing
static bool _coverDecodedThisRefresh = false;
static String _lastDecodedCoverPath = "";

static void resetCoverDecodeFlag() {
    _coverDecodedThisRefresh = false;
}

static bool drawWidgetCover(const char* coverPath, int x, int y, int maxW, int maxH) {
    // Check if this is a different cover
    if (_lastDecodedCoverPath != coverPath) {
        _lastDecodedCoverPath = coverPath;
    }
    
    if (!coverPath || strlen(coverPath) < 5) return false;
    
    // Check if file exists by trying to open it
    File testFile = SD.open(coverPath);
    if (!testFile) {
        Serial.printf("[HOME] Cover not found: %s\n", coverPath);
        return false;
    }
    testFile.close();;
    
    // Store widget bounds for clipping in callback
    _widgetBoundsX = x;
    _widgetBoundsY = y;
    _widgetCoverMaxW = maxW;
    _widgetCoverMaxH = maxH;
    
    TJpgDec.setCallback(_widgetCoverCallback);
    
    // Get image size
    uint16_t jpgW, jpgH;
    if (TJpgDec.getFsJpgSize(&jpgW, &jpgH, coverPath, SD) != JDR_OK) {
        return false;  // Cover doesn't exist or can't be read
    }
    
    // Calculate exact scale to FIT image in widget (maintain aspect ratio)
    float scaleW = (float)maxW / jpgW;
    float scaleH = (float)maxH / jpgH;
    float targetScale = min(scaleW, scaleH);  // Use smaller to ensure it fits
    
    // Choose JPEG scale (1, 2, 4, or 8) that gets us closest without going too small
    // We want decoded size to be >= target size so software scale is <= 1.0
    int jpgScale = 1;
    if (targetScale < 0.125f) jpgScale = 8;
    else if (targetScale < 0.25f) jpgScale = 4;
    else if (targetScale < 0.5f) jpgScale = 2;
    else jpgScale = 1;
    
    // Calculate software scale to achieve exact target size
    int decodedW = jpgW / jpgScale;
    int decodedH = jpgH / jpgScale;
    _coverScaleFactor = targetScale * jpgScale;  // Software scale on top of JPEG scale
    
    // Final dimensions
    int finalW = (int)(decodedW * _coverScaleFactor);
    int finalH = (int)(decodedH * _coverScaleFactor);
    
    TJpgDec.setJpgScale(jpgScale);
    
    // Center the cover in the widget area
    _widgetCoverX = x + (maxW - finalW) / 2;
    _widgetCoverY = y + (maxH - finalH) / 2;
    
    // Only log once per refresh cycle
    if (!_coverDecodedThisRefresh) {
        Serial.printf("[HOME] Cover: %dx%d -> %dx%d at (%d,%d)\n", 
                      jpgW, jpgH, finalW, finalH, _widgetCoverX, _widgetCoverY);
    }
    
    JRESULT result = TJpgDec.drawFsJpg(0, 0, coverPath, SD);
    
    if (result == JDR_OK) {
        _coverDecodedThisRefresh = true;  // Mark for logging control
    }
    
    return (result == JDR_OK);
}

// =============================================================================
// Home Screen State
// =============================================================================
uint8_t enabledItemIndices[MAX_HOME_ITEMS];
int enabledItemCount = 0;
int homeSelection = 0;
int homePageIndex = 0;
int itemsPerPage = 8;
int homeCols = 2;
int homeRows = 4;
int widgetSelection = -1;  // -1 = grid, 0 = book widget, 1 = weather widget

// Internal state
static bool needsFullRedraw = true;
static bool _skipCoverRedecode = false;  // Skip JPEG decode when just updating selection
static int previousSelection = -1;
static int previousPage = -1;

// =============================================================================
// Widget Data Cache Files
// =============================================================================
#define LAST_BOOK_PATH "/.sumi/lastbook.bin"
#define WEATHER_CACHE_PATH "/.sumi/weather.bin"

// Continue Reading widget data - MUST match LastBookInfo in Library.h
struct LastBookWidget {
    uint32_t magic;
    char title[64];
    char author[48];
    char coverPath[96];
    char bookPath[128];    // Must include to match Library.h LastBookInfo
    int chapter;
    int page;
    int totalPages;
    float progress;
};

// Weather widget data (cached from Weather app)
struct WeatherWidget {
    uint32_t magic;          // 0x57545852 "WTHR"
    uint32_t timestamp;      // When data was fetched
    float temperature;
    int weatherCode;
    int humidity;
    float windSpeed;
    char location[48];
    bool useCelsius;
};

static LastBookWidget lastBookWidget;
static WeatherWidget weatherWidget;
static bool hasLastBook = false;
static bool hasWeather = false;

// =============================================================================
// Widget Loading
// =============================================================================
void loadLastBookWidget() {
    hasLastBook = false;
    
    // Try loading saved last book first
    if (SD.exists(LAST_BOOK_PATH)) {
        File file = SD.open(LAST_BOOK_PATH, FILE_READ);
        if (file) {
            size_t read = file.read((uint8_t*)&lastBookWidget, sizeof(LastBookWidget));
            file.close();
            
            if (read == sizeof(LastBookWidget) && lastBookWidget.magic == 0x4C415354) {
                hasLastBook = true;
                Serial.printf("[HOME] Last book loaded: %s\n", lastBookWidget.title);
                
                // Verify cover exists, or try to find portal-uploaded cover
                if (lastBookWidget.coverPath[0] == '\0' || !SD.exists(lastBookWidget.coverPath)) {
                    // Try to find portal cover using book path filename
                    const char* bookPath = lastBookWidget.bookPath;
                    const char* justFilename = strrchr(bookPath, '/');
                    if (justFilename) justFilename++; else justFilename = bookPath;
                    
                    uint32_t hash = 0;
                    for (const char* p = justFilename; *p; p++) {
                        uint32_t charCode = (uint8_t)*p;
                        hash = ((hash << 5) - hash) + charCode;
                    }
                    hash = hash & 0xFFFFFFFF;
                    
                    // Portal saves covers to /.sumi/books/{hash}/cover_full.jpg
                    char coverPath[96];
                    snprintf(coverPath, sizeof(coverPath), "/.sumi/books/%08x/cover_full.jpg", hash);
                    File coverFile = SD.open(coverPath);
                    if (coverFile) {
                        coverFile.close();
                        strncpy(lastBookWidget.coverPath, coverPath, sizeof(lastBookWidget.coverPath) - 1);
                        Serial.printf("[HOME] Found portal cover: %s\n", coverPath);
                    } else {
                        lastBookWidget.coverPath[0] = '\0';
                    }
                }
                return;
            }
        }
    }
    
    // No last book - try to find first book in library as fallback
    if (!hasLastBook && settingsManager.display.showBookWidget) {
        File booksDir = SD.open("/books");
        if (booksDir && booksDir.isDirectory()) {
            File entry = booksDir.openNextFile();
            while (entry) {
                if (!entry.isDirectory()) {
                    String name = entry.name();
                    if (name.endsWith(".epub") || name.endsWith(".EPUB")) {
                        // Found a book - use it as placeholder
                        memset(&lastBookWidget, 0, sizeof(lastBookWidget));
                        lastBookWidget.magic = 0x4C415354;
                        
                        // Extract title from filename (remove .epub and path)
                        int lastSlash = name.lastIndexOf('/');
                        String filename = (lastSlash >= 0) ? name.substring(lastSlash + 1) : name;
                        filename = filename.substring(0, filename.length() - 5); // Remove .epub
                        
                        // Try to make title nicer (remove " - Author" part)
                        int dashPos = filename.lastIndexOf(" - ");
                        if (dashPos > 0) {
                            filename = filename.substring(0, dashPos);
                        }
                        
                        strncpy(lastBookWidget.title, filename.c_str(), sizeof(lastBookWidget.title) - 1);
                        snprintf(lastBookWidget.bookPath, sizeof(lastBookWidget.bookPath), 
                                 "/books/%s", entry.name());
                        
                        // Check for cached cover (hash filename only, matching portal)
                        const char* justFilename = entry.name();
                        // entry.name() may include path prefix, strip it
                        const char* slashPtr = strrchr(justFilename, '/');
                        if (slashPtr) justFilename = slashPtr + 1;
                        
                        uint32_t hash = 0;
                        for (const char* p = justFilename; *p; p++) {
                            uint32_t charCode = (uint8_t)*p;
                            hash = ((hash << 5) - hash) + charCode;
                        }
                        hash = hash & 0xFFFFFFFF;
                        char coverPath[96];
                        // Portal saves covers to /.sumi/books/{hash}/cover_full.jpg
                        snprintf(coverPath, sizeof(coverPath), "/.sumi/books/%08x/cover_full.jpg", hash);
                        Serial.printf("[HOME] Looking for cover: %s (hash=%08x from '%s')\n", coverPath, hash, justFilename);
                        
                        // Use file open instead of SD.exists() - more reliable
                        File coverFile = SD.open(coverPath);
                        if (coverFile) {
                            coverFile.close();
                            strncpy(lastBookWidget.coverPath, coverPath, sizeof(lastBookWidget.coverPath) - 1);
                            Serial.printf("[HOME] Found cover: %s\n", coverPath);
                        } else {
                            // Also try without leading slash
                            snprintf(coverPath, sizeof(coverPath), ".sumi/books/%08x/cover_full.jpg", hash);
                            coverFile = SD.open(coverPath);
                            if (coverFile) {
                                coverFile.close();
                                strncpy(lastBookWidget.coverPath, coverPath, sizeof(lastBookWidget.coverPath) - 1);
                                Serial.printf("[HOME] Found cover (alt path): %s\n", coverPath);
                            } else {
                                Serial.printf("[HOME] Cover not found at: /.sumi/books/%08x/cover_full.jpg\n", hash);
                            }
                        }
                        
                        hasLastBook = true;
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
    hasWeather = false;
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
            
            if (read == sizeof(WeatherWidget) && weatherWidget.magic == 0x57545852) {
                // Check if data is still fresh
                // Use timestamp=0 to mean "valid until refresh requested"
                // Otherwise, timestamp is seconds since boot when cached
                // After reboot, millis resets, so just accept cache if it has valid data
                uint32_t uptimeSec = millis() / 1000;
                uint32_t age = (uptimeSec > weatherWidget.timestamp) ? 
                               (uptimeSec - weatherWidget.timestamp) : 0;
                
                // Accept cache if: timestamp is 0, age < 2 hours, or we just rebooted (uptime < 60s)
                if (weatherWidget.timestamp == 0 || age < 7200 || uptimeSec < 60) {
                    hasWeather = true;
                    Serial.printf("[HOME] Weather loaded: %.1f° at %s\n", 
                                  weatherWidget.temperature, weatherWidget.location);
                } else {
                    Serial.println("[HOME] Weather data stale, will fetch");
                    needsFetch = true;
                }
            } else {
                needsFetch = true;
            }
        }
    }
    
    // Fetch fresh data if widget is enabled and data is missing/stale
    if (needsFetch && settingsManager.display.showWeatherWidget) {
        fetchWeatherForWidget();
    }
}

// Save weather data (called from Weather app after fetch)
void saveWeatherCache(float temp, int code, int humidity, float wind, 
                      const char* location, bool celsius) {
    weatherWidget.magic = 0x57545852;
    weatherWidget.timestamp = millis() / 1000;
    weatherWidget.temperature = temp;
    weatherWidget.weatherCode = code;
    weatherWidget.humidity = humidity;
    weatherWidget.windSpeed = wind;
    weatherWidget.useCelsius = celsius;
    strncpy(weatherWidget.location, location, sizeof(weatherWidget.location) - 1);
    
    File file = SD.open(WEATHER_CACHE_PATH, FILE_WRITE);
    if (file) {
        file.write((uint8_t*)&weatherWidget, sizeof(WeatherWidget));
        file.close();
        Serial.println("[HOME] Weather cache saved");
    }
}

// Fetch weather data independently for the widget
// This allows the widget to update without opening the Weather app
void fetchWeatherForWidget() {
    // Check if location is configured
    if (settingsManager.weather.latitude == 0 && settingsManager.weather.longitude == 0) {
        Serial.println("[HOME] Weather: No location configured");
        return;
    }
    
    // Check if WiFi is available
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
    
    // Simple request for current weather only
    char request[256];
    snprintf(request, sizeof(request),
        "GET /v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
        "&temperature_unit=%s&wind_speed_unit=mph HTTP/1.0\r\n",
        lat, lon, useCelsius ? "celsius" : "fahrenheit");
    
    client.print(request);
    client.print("Host: api.open-meteo.com\r\n");
    client.print("Connection: close\r\n\r\n");
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 8000) {
            client.stop();
            Serial.println("[HOME] Weather: Timeout");
            return;
        }
    }
    
    // Skip headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) break;
    }
    
    // Read body
    String payload = "";
    timeout = millis();
    while (client.available() || millis() - timeout < 2000) {
        if (client.available()) {
            payload += (char)client.read();
            timeout = millis();
        }
    }
    client.stop();
    
    // Parse JSON
    int jsonStart = payload.indexOf('{');
    if (jsonStart < 0) {
        Serial.println("[HOME] Weather: No JSON found");
        return;
    }
    
    String jsonPayload = payload.substring(jsonStart);
    
    JsonDocument doc;
    if (deserializeJson(doc, jsonPayload)) {
        Serial.println("[HOME] Weather: JSON parse failed");
        return;
    }
    
    // Extract current conditions
    JsonObject current = doc["current"];
    float temp = current["temperature_2m"] | 0.0f;
    int humidity = current["relative_humidity_2m"] | 0;
    float wind = current["wind_speed_10m"] | 0.0f;
    int weatherCode = current["weather_code"] | 0;
    
    // Get location name from settings
    const char* location = settingsManager.weather.location;
    if (location[0] == '\0') {
        location = "Unknown";
    }
    
    // Save to cache
    saveWeatherCache(temp, weatherCode, humidity, wind, location, useCelsius);
    hasWeather = true;
    
    Serial.printf("[HOME] Weather updated: %.1f° at %s\n", temp, location);
}

// =============================================================================
// Widget Helper Functions
// =============================================================================
bool hasBookWidget() {
    return hasLastBook && settingsManager.display.showBookWidget && (homePageIndex == 0);
}

bool hasWeatherWidget() {
    return hasWeather && settingsManager.display.showWeatherWidget && (homePageIndex == 0);
}

bool hasOrientWidget() {
    return settingsManager.display.showOrientWidget && (homePageIndex == 0);
}

int getWidgetCount() {
    if (homePageIndex != 0) return 0;
    int count = 0;
    if (hasLastBook && settingsManager.display.showBookWidget) count++;
    if (hasWeather && settingsManager.display.showWeatherWidget) count++;
    if (settingsManager.display.showOrientWidget) count++;
    return count;
}

void toggleOrientation() {
    // Toggle between portrait (1) and landscape (0)
    settingsManager.display.orientation = 1 - settingsManager.display.orientation;
    settingsManager.save();
    
    // Apply new orientation - landscape=0, portrait=3
    bool isHorizontal = (settingsManager.display.orientation == 0);
    display.setRotation(isHorizontal ? 0 : 3);
    setButtonOrientation(isHorizontal);
    
    Serial.printf("[HOME] Toggled orientation to %s\n", 
                  isHorizontal ? "Landscape" : "Portrait");
}

void activateWidget(int widget) {
    int currentIdx = 0;
    
    // Book widget - launches Library directly and resumes last book
    if (hasLastBook && settingsManager.display.showBookWidget) {
        if (widget == currentIdx) {
            Serial.printf("[HOME] Activating book widget: %s\n", lastBookWidget.title);
            // Launch Library directly (don't require it in grid)
            extern void openAppByItemIndex(uint8_t itemIndex);
            openAppByItemIndex(HOME_ITEM_LIBRARY);
            return;
        }
        currentIdx++;
    }
    
    // Weather widget - launches Weather app directly
    if (hasWeather && settingsManager.display.showWeatherWidget) {
        if (widget == currentIdx) {
            Serial.println("[HOME] Activating weather widget - opening forecast");
            // Launch Weather app directly (don't require it in grid)
            extern void openAppByItemIndex(uint8_t itemIndex);
            openAppByItemIndex(HOME_ITEM_WEATHER);
            return;
        }
        currentIdx++;
    }
    
    // Orientation widget
    if (settingsManager.display.showOrientWidget) {
        if (widget == currentIdx) {
            Serial.println("[HOME] Activating orientation widget - toggling");
            toggleOrientation();
            // Trigger full redraw with new orientation
            extern bool needsFullRedraw;
            needsFullRedraw = true;
            showHomeScreen();
            return;
        }
    }
}

// =============================================================================
// Building Home Screen Items
// =============================================================================
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
        Serial.println("[HOME] Added Settings to home screen");
    }
    
    if (enabledItemCount == 0) {
        Serial.println("[HOME] WARNING: No home items configured! Adding Settings only.");
        enabledItemIndices[enabledItemCount++] = HOME_ITEM_SETTINGS;
    }
    
    // Load widgets
    loadLastBookWidget();
    loadWeatherWidget();
    
    Serial.printf("[HOME] Final: %d items, hasBook=%d, hasWeather=%d\n", 
                  enabledItemCount, hasLastBook, hasWeather);
}

void updateGridLayout() {
    // Grid layout is now calculated dynamically in calculateCellGeometry()
    // based on actual items on current page
    // Just update itemsPerPage for page calculations
    bool isHorizontal = (settingsManager.display.orientation == 0);
    
    // Set maximum items per page
    if (isHorizontal) {
        itemsPerPage = 8;  // 4x2 max
    } else {
        itemsPerPage = 9;  // 3x3 max  
    }
    
    // Update globals for compatibility
    homeCols = isHorizontal ? 4 : 3;
    homeRows = isHorizontal ? 2 : 3;
    
    if (homeSelection >= getItemsOnCurrentPage()) homeSelection = 0;
    if (homePageIndex >= getTotalPages()) homePageIndex = 0;
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
    
    // Reserve space for widgets on first page
    int widgetCount = getWidgetCount();
    bool isHorizontal = (settingsManager.display.orientation == 0);
    
    if (widgetCount > 0 && !isHorizontal) {
        // Portrait: widget area height depends on what's shown
        bool showsBook = hasLastBook && settingsManager.display.showBookWidget;
        bool showsWeather = hasWeather && settingsManager.display.showWeatherWidget;
        bool showsOrient = settingsManager.display.showOrientWidget;
        
        int widgetAreaH;
        if (!showsBook && !showsWeather && showsOrient) {
            widgetAreaH = 50;  // Orient only - slim
        } else if (!showsBook && showsWeather && !showsOrient) {
            widgetAreaH = 140; // Weather only
        } else if (showsBook) {
            widgetAreaH = 340; // Book shown - tall for large cover
        } else {
            widgetAreaH = 180; // Weather + Orient without book
        }
        geo.gridTop += widgetAreaH + 10;
    }
    
    int pageIndicatorHeight = (getTotalPages() > 1) ? 30 : 10;
    int gridHeight = h - geo.gridTop - pageIndicatorHeight;
    
    // Calculate actual items on current page
    int itemsOnPage = getItemsOnCurrentPage();
    if (itemsOnPage == 0) itemsOnPage = 1;  // Avoid division by zero
    
    // Calculate optimal grid dimensions based on actual items
    // Try to make cells as large as possible while fitting all items
    int optimalCols, optimalRows;
    
    if (isHorizontal) {
        // Landscape: prefer wide grid
        int availW = w - geo.gridPadding * 2;
        if (widgetCount > 0) availW -= 230;  // Reserve for widgets
        
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
        // Portrait: always 2 columns, rows expand to fit
        // This matches the portal preview layout
        optimalCols = 2;
        if (itemsOnPage <= 2) {
            optimalRows = 1;
        } else if (itemsOnPage <= 4) {
            optimalRows = 2;
        } else if (itemsOnPage <= 6) {
            optimalRows = 3;
        } else if (itemsOnPage <= 8) {
            optimalRows = 4;
        } else {
            optimalCols = 3;  // Only go to 3 cols if more than 8
            optimalRows = 3;
        }
    }
    
    geo.cols = optimalCols;
    geo.rows = optimalRows;
    
    // Calculate actual rows needed for items on this page
    int actualRows = (itemsOnPage + geo.cols - 1) / geo.cols;
    if (actualRows < 1) actualRows = 1;
    
    // Use the smaller of optimal rows and actual rows needed
    // This ensures cells fill available space when we have fewer items
    geo.rows = min(geo.rows, actualRows);
    
    geo.cellWidth = (w - geo.gridPadding * 2 - geo.cellGap * (geo.cols - 1)) / geo.cols;
    
    // For landscape with widgets, adjust cell width for remaining space
    if (isHorizontal && widgetCount > 0) {
        int availW = w - 230 - geo.gridPadding * 2;
        geo.cellWidth = (availW - geo.cellGap * (geo.cols - 1)) / geo.cols;
    }
    
    // Calculate cell height to fill available grid space
    geo.cellHeight = (gridHeight - geo.cellGap * (geo.rows - 1)) / geo.rows;
    
    // Cap cell height to reasonable maximum for usability
    // Portrait can have taller cells since we're stacking vertically
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
    
    Serial.printf("[HOME] Grid: %dx%d, items=%d, cellW=%d, cellH=%d, gridTop=%d, gridH=%d\n",
                  geo.cols, geo.rows, itemsOnPage, geo.cellWidth, geo.cellHeight, geo.gridTop, gridHeight);
}

void getCellPosition(const CellGeometry& geo, int cellIndex, int& cellX, int& cellY) {
    int row = cellIndex / geo.cols;
    int col = cellIndex % geo.cols;
    
    // In landscape with widgets, offset grid to right
    int offsetX = 0;
    int widgetCount = getWidgetCount();
    if (widgetCount > 0) {
        bool isHorizontal = (settingsManager.display.orientation == 0);
        if (isHorizontal) {
            offsetX = 220;  // Wider widgets for better cover display
        }
    }
    
    cellX = geo.gridPadding + offsetX + col * (geo.cellWidth + geo.cellGap);
    cellY = geo.gridTop + row * (geo.cellHeight + geo.cellGap);
}

// =============================================================================
// Weather Icon Drawing
// =============================================================================
void drawWeatherIcon(int code, int x, int y, int size, uint16_t color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 3;
    
    if (code == 0) {
        // Clear - sun
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
        // Partly cloudy - sun with cloud
        display.drawCircle(cx - 5, cy - 5, r - 3, color);
        // Cloud
        display.fillCircle(cx - 2, cy + 3, 6, color);
        display.fillCircle(cx + 6, cy + 3, 6, color);
        display.fillCircle(cx + 2, cy - 2, 5, color);
        display.fillRect(cx - 8, cy + 3, 20, 8, color);
    } else if (code <= 49) {
        // Fog - horizontal lines
        for (int i = 0; i < 4; i++) {
            display.drawLine(x + 5, y + 8 + i * 8, x + size - 5, y + 8 + i * 8, color);
        }
    } else if (code <= 69) {
        // Rain - cloud with drops
        display.fillCircle(cx - 5, cy - 5, 8, color);
        display.fillCircle(cx + 5, cy - 5, 8, color);
        display.fillCircle(cx, cy - 10, 6, color);
        display.fillRect(cx - 13, cy - 5, 26, 10, color);
        // Rain drops
        for (int i = 0; i < 3; i++) {
            display.drawLine(cx - 8 + i * 8, cy + 8, cx - 10 + i * 8, cy + 15, color);
        }
    } else if (code <= 79) {
        // Snow - cloud with snowflakes
        display.fillCircle(cx - 5, cy - 5, 8, color);
        display.fillCircle(cx + 5, cy - 5, 8, color);
        display.fillRect(cx - 13, cy - 5, 26, 10, color);
        // Snowflakes (asterisks)
        for (int i = 0; i < 3; i++) {
            int sx = cx - 8 + i * 8;
            int sy = cy + 12;
            display.drawLine(sx - 3, sy, sx + 3, sy, color);
            display.drawLine(sx, sy - 3, sx, sy + 3, color);
        }
    } else {
        // Storm - cloud with lightning
        display.fillCircle(cx - 5, cy - 8, 8, color);
        display.fillCircle(cx + 5, cy - 8, 8, color);
        display.fillRect(cx - 13, cy - 8, 26, 10, color);
        // Lightning bolt
        display.drawLine(cx, cy + 2, cx - 4, cy + 10, color);
        display.drawLine(cx - 4, cy + 10, cx + 2, cy + 10, color);
        display.drawLine(cx + 2, cy + 10, cx - 2, cy + 18, color);
    }
}

// =============================================================================
// Widget Drawing - Simplified Design
// =============================================================================
void drawWidgets(const CellGeometry& geo) {
    if (homePageIndex != 0) return;
    
    int widgetCount = getWidgetCount();
    if (widgetCount == 0) return;
    
    // Only log once per refresh, not every paged iteration
    static bool _widgetLoggedThisRefresh = false;
    if (!_coverDecodedThisRefresh && !_widgetLoggedThisRefresh) {
        Serial.printf("[HOME] drawWidgets: count=%d, hasBook=%d, hasWeather=%d\n",
                      widgetCount, hasLastBook, hasWeather);
        _widgetLoggedThisRefresh = true;
    }
    if (_coverDecodedThisRefresh) {
        _widgetLoggedThisRefresh = false;  // Reset for next refresh
    }
    
    int w = display.width();
    int h = display.height();
    bool isLandscape = (settingsManager.display.orientation == 0);
    
    // Check which widgets are enabled
    bool showBook = hasLastBook && settingsManager.display.showBookWidget;
    bool showWeather = hasWeather && settingsManager.display.showWeatherWidget;
    bool showOrient = settingsManager.display.showOrientWidget;
    
    int currentWidgetIndex = 0;
    
    if (isLandscape) {
        // === LANDSCAPE MODE ===
        // TOP: Book cover (large, as wide as possible)
        // BOTTOM: Weather + Orientation side by side
        
        int widgetAreaX = geo.gridPadding;
        int widgetAreaY = geo.gridTop;
        int widgetAreaW = 220;  // Total width for widget area (wider for better cover)
        int widgetAreaH = h - geo.gridTop - 40;  // Use most of available height
        
        // Calculate row heights
        int bottomRowH = 50;  // Height for weather + orientation row
        int topRowH = showBook ? (widgetAreaH - bottomRowH - 8) : 0;  // Book gets most of height
        int bottomRowY = widgetAreaY + (showBook ? topRowH + 8 : 0);
        
        // --- Book Cover Widget (TOP, Large) ---
        if (showBook) {
            bool isSelected = (widgetSelection == currentWidgetIndex);
            uint16_t fg = isSelected ? geo.bgColor : geo.fgColor;
            uint16_t bg = isSelected ? geo.fgColor : geo.bgColor;
            
            int bookX = widgetAreaX;
            int bookY = widgetAreaY;
            int bookW = widgetAreaW;
            int bookH = topRowH;
            
            // Selection highlight (subtle border only when selected)
            if (isSelected) {
                display.fillRoundRect(bookX - 3, bookY - 3, bookW + 6, bookH + 6, 6, geo.fgColor);
            }
            
            // Only decode JPEG if not skipping (cover already on screen from previous draw)
            if (!_skipCoverRedecode) {
                // Draw cover directly - no border, no progress bar
                bool hasCover = false;
                if (lastBookWidget.coverPath[0] != '\0') {
                    hasCover = drawWidgetCover(lastBookWidget.coverPath, bookX, bookY, bookW, bookH);
                }
                
                // Fallback if no cover image
                if (!hasCover) {
                    display.fillRoundRect(bookX, bookY, bookW, bookH, 4, bg);
                    display.drawRoundRect(bookX, bookY, bookW, bookH, 4, fg);
                    // Simple book lines as placeholder
                    display.setFont(&FreeSans9pt7b);
                    display.setTextColor(fg);
                    for (int i = 0; i < 5; i++) {
                        int lineY = bookY + 20 + i * (bookH - 40) / 5;
                        display.drawLine(bookX + 15, lineY, bookX + bookW - 15, lineY, fg);
                    }
                }
            }
            
            currentWidgetIndex++;
        }
        
        // --- BOTTOM ROW: Weather + Orientation side by side ---
        if (showWeather || showOrient) {
            int weatherW = 0, orientW = 0;
            if (showWeather && showOrient) {
                weatherW = (widgetAreaW - 8) * 2 / 3;  // Weather gets 2/3 width
                orientW = widgetAreaW - weatherW - 8;
            } else if (showWeather) {
                weatherW = widgetAreaW;
            } else {
                orientW = widgetAreaW;
            }
            
            int currentX = widgetAreaX;
            
            // --- Weather Widget (icon + temp, horizontal layout) ---
            if (showWeather) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t textColor = isSelected ? geo.bgColor : geo.fgColor;
                
                if (isSelected) {
                    display.fillRoundRect(currentX, bottomRowY, weatherW, bottomRowH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(currentX, bottomRowY, weatherW, bottomRowH, 6, geo.fgColor);
                }
                
                // Weather icon on left
                int iconSize = bottomRowH - 20;
                if (iconSize < 20) iconSize = 20;
                drawWeatherIcon(weatherWidget.weatherCode, 
                               currentX + 8, bottomRowY + 10, iconSize, textColor);
                
                // Temperature to the right of icon
                display.setFont(&FreeSansBold12pt7b);
                display.setTextColor(textColor);
                char tempStr[12];
                snprintf(tempStr, sizeof(tempStr), "%.0f%c", weatherWidget.temperature, 
                         weatherWidget.useCelsius ? 'C' : 'F');
                display.setCursor(currentX + iconSize + 12, bottomRowY + 22);
                display.print(tempStr);
                
                // Location/humidity below temp (smaller font)
                display.setFont(&FreeSans9pt7b);
                if (weatherWidget.location[0] != '\0') {
                    // Show abbreviated location
                    char locStr[12];
                    strncpy(locStr, weatherWidget.location, 11);
                    locStr[11] = '\0';
                    display.setCursor(currentX + iconSize + 12, bottomRowY + bottomRowH - 10);
                    display.print(locStr);
                }
                
                currentX += weatherW + 8;
                currentWidgetIndex++;
            }
            
            // --- Orientation Toggle (Slim switch) ---
            if (showOrient) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t fg = isSelected ? geo.bgColor : geo.fgColor;
                
                if (isSelected) {
                    display.fillRoundRect(currentX, bottomRowY, orientW, bottomRowH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(currentX, bottomRowY, orientW, bottomRowH, 6, geo.fgColor);
                }
                
                // Slim toggle switch centered
                int switchW = min(orientW - 12, 50);
                int switchH = min(bottomRowH - 16, 24);
                int switchX = currentX + (orientW - switchW) / 2;
                int switchY = bottomRowY + (bottomRowH - switchH) / 2;
                
                // Switch track
                display.drawRoundRect(switchX, switchY, switchW, switchH, switchH/2, fg);
                
                // Switch knob (left = landscape, right = portrait)
                int knobW = switchW / 2 - 4;
                int knobX = isLandscape ? (switchX + 3) : (switchX + switchW/2 + 1);
                display.fillRoundRect(knobX, switchY + 3, knobW, switchH - 6, (switchH - 6)/2, fg);
            }
        }
        
    } else {
        // === PORTRAIT MODE ===
        // Layout depends on which widgets are shown:
        // - All 3: Book left (55%) | Weather + Orient stacked right (45%)
        // - Book + Weather: Book left | Weather right
        // - Book + Orient: Book left | Orient right (slim)
        // - Weather + Orient: Weather + Orient stacked full width
        // - Just Book: Book centered
        // - Just Weather: Weather centered
        // - Just Orient: Small slim toggle bar centered
        
        int widgetAreaY = geo.statusBarHeight + 8;
        int widgetAreaW = w - geo.gridPadding * 2;
        
        // Calculate widget area height based on what's shown
        int widgetAreaH;
        if (!showBook && !showWeather && showOrient) {
            // Orient only - slim bar
            widgetAreaH = 50;
        } else if (!showBook && showWeather && !showOrient) {
            // Weather only
            widgetAreaH = 140;
        } else if (showBook) {
            // Book is shown - TALL height for large cover (340px for maximum cover size)
            widgetAreaH = 340;
        } else {
            // Weather + Orient without book
            widgetAreaH = 180;
        }
        
        // Calculate layout - book gets 60% width for larger cover display
        int leftColW = showBook ? (widgetAreaW * 60 / 100) : 0;
        int rightColW = showBook ? (widgetAreaW - leftColW - 10) : widgetAreaW;
        int leftColX = geo.gridPadding;
        int rightColX = showBook ? (geo.gridPadding + leftColW + 10) : geo.gridPadding;
        
        // --- Book Cover Widget (Left, Large) ---
        if (showBook) {
            bool isSelected = (widgetSelection == currentWidgetIndex);
            uint16_t fg = isSelected ? geo.bgColor : geo.fgColor;
            uint16_t bg = isSelected ? geo.fgColor : geo.bgColor;
            
            // Selection highlight border (always draw for selection state)
            if (isSelected) {
                display.fillRoundRect(leftColX - 3, widgetAreaY - 3, leftColW + 6, widgetAreaH + 6, 6, geo.fgColor);
            }
            
            // Only decode JPEG if not skipping (cover already on screen from previous draw)
            if (!_skipCoverRedecode) {
                // Draw cover directly - no border, no progress bar, no title
                bool hasCover = false;
                if (lastBookWidget.coverPath[0] != '\0') {
                    hasCover = drawWidgetCover(lastBookWidget.coverPath, leftColX, widgetAreaY, leftColW, widgetAreaH);
                }
                
                // Fallback if no cover
                if (!hasCover) {
                    display.fillRoundRect(leftColX, widgetAreaY, leftColW, widgetAreaH, 4, bg);
                    display.drawRoundRect(leftColX, widgetAreaY, leftColW, widgetAreaH, 4, fg);
                    // Book lines placeholder
                    for (int i = 0; i < 7; i++) {
                        int lineY = widgetAreaY + 20 + i * (widgetAreaH - 40) / 7;
                        display.drawLine(leftColX + 15, lineY, leftColX + leftColW - 15, lineY, fg);
                    }
                }
            }
            
            currentWidgetIndex++;
        }
        
        // --- Right Column: Weather + Orientation stacked ---
        if (showWeather || showOrient) {
            int rightY = widgetAreaY;
            
            // Calculate heights - weather gets more space now for extra info
            int weatherH = 0, orientH = 0;
            if (showWeather && showOrient) {
                weatherH = widgetAreaH * 70 / 100;  // Weather gets 70%
                orientH = widgetAreaH - weatherH - 8;
            } else if (showWeather) {
                weatherH = widgetAreaH;
            } else {
                orientH = widgetAreaH;
            }
            
            // --- Weather Widget (with location, temp, humidity, date) ---
            if (showWeather) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t textColor = isSelected ? geo.bgColor : geo.fgColor;
                
                if (isSelected) {
                    display.fillRoundRect(rightColX, rightY, rightColW, weatherH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(rightColX, rightY, rightColW, weatherH, 6, geo.fgColor);
                }
                
                int contentX = rightColX + 10;
                int contentY = rightY + 6;
                int lineY = contentY;
                
                // Location at top (small font)
                display.setFont(&FreeSans9pt7b);
                display.setTextColor(textColor);
                if (weatherWidget.location[0] != '\0') {
                    // Truncate location if too long
                    char locStr[20];
                    strncpy(locStr, weatherWidget.location, 19);
                    locStr[19] = '\0';
                    display.setCursor(contentX, lineY + 12);
                    display.print(locStr);
                    lineY += 18;
                }
                
                // Weather icon + Temperature on same line
                int iconSize = min(40, (weatherH - 80) / 2);
                if (iconSize < 20) iconSize = 20;
                drawWeatherIcon(weatherWidget.weatherCode, contentX, lineY + 2, iconSize, textColor);
                
                // Temperature (large, right of icon)
                display.setFont(&FreeSansBold12pt7b);
                display.setTextColor(textColor);
                char tempStr[12];
                snprintf(tempStr, sizeof(tempStr), "%.0f%c", weatherWidget.temperature, 
                         weatherWidget.useCelsius ? 'C' : 'F');
                display.setCursor(contentX + iconSize + 8, lineY + iconSize - 5);
                display.print(tempStr);
                lineY += iconSize + 8;
                
                // Humidity
                display.setFont(&FreeSans9pt7b);
                char humStr[16];
                snprintf(humStr, sizeof(humStr), "%d%% humidity", weatherWidget.humidity);
                display.setCursor(contentX, lineY + 12);
                display.print(humStr);
                lineY += 20;
                
                // Date and day of week (fill remaining space)
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 100)) {  // 100ms timeout
                    // Day of week on its own line
                    display.setFont(&FreeSansBold9pt7b);
                    char dayStr[12];
                    strftime(dayStr, sizeof(dayStr), "%A", &timeinfo);  // Full day name
                    display.setCursor(contentX, lineY + 14);
                    display.print(dayStr);
                    lineY += 18;
                    
                    // Date below
                    display.setFont(&FreeSans9pt7b);
                    char dateStr[16];
                    strftime(dateStr, sizeof(dateStr), "%B %d", &timeinfo);  // Month Day
                    display.setCursor(contentX, lineY + 12);
                    display.print(dateStr);
                } else {
                    // Time not synced - show placeholder
                    display.setFont(&FreeSans9pt7b);
                    display.setCursor(contentX, lineY + 14);
                    display.print("Time not set");
                }
                
                rightY += weatherH + 8;
                currentWidgetIndex++;
            }
            
            // --- Orientation Toggle (Slim switch) ---
            if (showOrient) {
                bool isSelected = (widgetSelection == currentWidgetIndex);
                uint16_t fg = isSelected ? geo.bgColor : geo.fgColor;
                
                if (isSelected) {
                    display.fillRoundRect(rightColX, rightY, rightColW, orientH, 6, geo.fgColor);
                } else {
                    display.drawRoundRect(rightColX, rightY, rightColW, orientH, 6, geo.fgColor);
                }
                
                // Slim toggle centered
                int switchW = min(rightColW - 30, 80);
                int switchH = min(orientH - 12, 28);
                int switchX = rightColX + (rightColW - switchW) / 2;
                int switchY = rightY + (orientH - switchH) / 2;
                
                // Switch track
                display.drawRoundRect(switchX, switchY, switchW, switchH, switchH/2, fg);
                
                // Switch knob
                int knobW = switchW / 2 - 4;
                int knobX = isLandscape ? (switchX + 3) : (switchX + switchW/2 + 1);
                display.fillRoundRect(knobX, switchY + 3, knobW, switchH - 6, (switchH - 6)/2, fg);
            }
        }
    }
}

// =============================================================================
// Drawing Functions
// =============================================================================
void showHomeScreen() {
    needsFullRedraw = true;
    loadLastBookWidget();
    loadWeatherWidget();
    showHomeScreenPartial(false);
    refreshManager.recordFullRefresh();
    previousSelection = homeSelection;
    previousPage = homePageIndex;
    needsFullRedraw = false;
}

void refreshChangedCells(int oldSel, int newSel) {
    if (refreshManager.mustFullRefresh() && refreshManager.canFullRefresh()) {
        showHomeScreen();
        return;
    }
    
    int w = display.width();
    
    CellGeometry geo;
    calculateCellGeometry(geo);
    
    // Calculate adjusted cell width for landscape with widgets (same as showHomeScreenPartial)
    int actualCellWidth = geo.cellWidth;
    bool showWidgets = (getWidgetCount() > 0) && (homePageIndex == 0);
    bool isLandscape = (settingsManager.display.orientation == 0);
    
    if (showWidgets && isLandscape) {
        int availableWidth = w - 220 - geo.gridPadding * 2;
        actualCellWidth = (availableWidth - geo.cellGap * (geo.cols - 1)) / geo.cols;
    }
    
    // Calculate positions using adjusted dimensions
    int oldX, oldY, newX, newY;
    
    if (showWidgets && isLandscape) {
        // Manual position calculation for landscape with widgets
        int oldCol = oldSel % geo.cols;
        int oldRow = oldSel / geo.cols;
        int newCol = newSel % geo.cols;
        int newRow = newSel / geo.cols;
        
        oldX = geo.gridPadding + 220 + oldCol * (actualCellWidth + geo.cellGap);
        oldY = geo.gridTop + oldRow * (geo.cellHeight + geo.cellGap);
        newX = geo.gridPadding + 220 + newCol * (actualCellWidth + geo.cellGap);
        newY = geo.gridTop + newRow * (geo.cellHeight + geo.cellGap);
    } else {
        getCellPosition(geo, oldSel, oldX, oldY);
        getCellPosition(geo, newSel, newX, newY);
    }
    
    int margin = 2;
    int minX = min(oldX, newX) - margin;
    int minY = min(oldY, newY) - margin;
    int maxX = max(oldX, newX) + actualCellWidth + margin;
    int maxY = max(oldY, newY) + geo.cellHeight + margin;
    
    display.setPartialWindow(minX, minY, maxX - minX, maxY - minY);
    
    display.firstPage();
    do {
        display.fillRect(minX, minY, maxX - minX, maxY - minY, geo.bgColor);
        
        // Old cell (deselected)
        display.fillRoundRect(oldX, oldY, actualCellWidth, geo.cellHeight, geo.cornerRadius, geo.bgColor);
        display.drawRoundRect(oldX, oldY, actualCellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.fgColor);
        
        uint8_t oldIdx = getItemIndexForPosition(oldSel);
        const HomeItemInfo* oldInfo = getHomeItemByIndex(oldIdx);
        const char* oldLabel = oldInfo ? oldInfo->label : "???";
        
        // Use smaller font if cells are narrow
        if (actualCellWidth < 80) {
            display.setFont(&FreeSansBold9pt7b);
        } else {
            display.setFont(&FreeSansBold12pt7b);
        }
        
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(oldLabel, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(oldX + (actualCellWidth - tw) / 2, oldY + (geo.cellHeight + th) / 2);
        display.print(oldLabel);
        
        // New cell (selected)
        display.fillRoundRect(newX, newY, actualCellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.bgColor);
        
        uint8_t newIdx = getItemIndexForPosition(newSel);
        const HomeItemInfo* newInfo = getHomeItemByIndex(newIdx);
        const char* newLabel = newInfo ? newInfo->label : "???";
        
        display.getTextBounds(newLabel, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(newX + (actualCellWidth - tw) / 2, newY + (geo.cellHeight + th) / 2);
        display.print(newLabel);
        
    } while (display.nextPage());
    
    refreshManager.recordPartialRefresh();
}

void showHomeScreenPartial(bool partialRefresh) {
    int w = display.width();
    int h = display.height();
    
    CellGeometry geo;
    calculateCellGeometry(geo);
    
    int totalPages = getTotalPages();
    int pageIndicatorHeight = (totalPages > 1) ? 30 : 10;
    int gridHeight = h - geo.gridTop - pageIndicatorHeight;
    
    // Calculate the area that needs updating
    // Include widget area which can be above or beside the grid
    int refreshTop = geo.statusBarHeight;  // Start after status bar to include widgets
    int refreshHeight = h - refreshTop;
    
    if (partialRefresh) {
        display.setPartialWindow(0, refreshTop, w, refreshHeight);
    } else {
        display.setFullWindow();
    }
    
    // Reset cover decode flag for new refresh cycle
    resetCoverDecodeFlag();
    
    display.firstPage();
    do {
        display.fillScreen(geo.bgColor);
        
        // Status bar
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
        
        // Draw widgets (if on first page) - always draw to show selection state
        drawWidgets(geo);
        
        // Calculate adjusted cell dimensions for widget area
        int actualCellWidth = geo.cellWidth;
        bool showWidgets = (getWidgetCount() > 0);
        if (showWidgets && settingsManager.display.orientation == 0) {
            int availableWidth = w - 220 - geo.gridPadding * 2;  // Narrower with wider widget column
            actualCellWidth = (availableWidth - geo.cellGap * (geo.cols - 1)) / geo.cols;
        }
        
        // Draw cells
        int itemsOnPage = getItemsOnCurrentPage();
        for (int i = 0; i < itemsOnPage; i++) {
            int cellX, cellY;
            getCellPosition(geo, i, cellX, cellY);
            
            // Recalculate cell position if widgets present
            if (showWidgets && settingsManager.display.orientation == 0) {
                int col = i % geo.cols;
                cellX = geo.gridPadding + 220 + col * (actualCellWidth + geo.cellGap);
            }
            
            uint8_t itemIndex = getItemIndexForPosition(i);
            const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
            const char* label = itemInfo ? itemInfo->label : "???";
            
            // Only show cell as selected if we're not in widget mode
            bool selected = (i == homeSelection) && (widgetSelection < 0);
            bool highContrast = (settingsManager.display.bgTheme == 2);
            
            int cellW = showWidgets && settingsManager.display.orientation == 0 
                        ? actualCellWidth : geo.cellWidth;
            
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
            
            uint8_t fontSize = settingsManager.display.fontSize;
            if (fontSize <= 14 || cellW < 80) {
                display.setFont(&FreeSansBold9pt7b);
            } else {
                display.setFont(&FreeSansBold12pt7b);
            }
            
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
            
            char truncLabel[16];
            strncpy(truncLabel, label, sizeof(truncLabel) - 1);
            truncLabel[sizeof(truncLabel) - 1] = '\0';
            while (tw > cellW - 10 && strlen(truncLabel) > 3) {
                truncLabel[strlen(truncLabel) - 1] = '\0';
                display.getTextBounds(truncLabel, 0, 0, &tx, &ty, &tw, &th);
            }
            
            display.setCursor(cellX + (cellW - tw) / 2, cellY + (geo.cellHeight + th) / 2);
            display.print(truncLabel);
        }
        
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
}

void drawSingleCell(int cellIndex, bool selected) {
    CellGeometry geo;
    calculateCellGeometry(geo);
    
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

// =============================================================================
// Widget Selection Refresh
// =============================================================================
void refreshWidgetSelection(int oldWidget, int newWidget) {
    // Fast refresh - skip JPEG decoding since cover is already on screen
    _skipCoverRedecode = true;
    showHomeScreenPartial(true);
    _skipCoverRedecode = false;
}

// Fast partial refresh that skips cover redecode
void showHomeScreenPartialFast() {
    _skipCoverRedecode = true;
    showHomeScreenPartial(true);
    _skipCoverRedecode = false;
}
