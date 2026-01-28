/**
 * @file Weather.h
 * @brief Enhanced Weather app for Sumi e-reader
 * @version 2.0.0
 *
 * Features:
 * - Current conditions with "feels like", UV, cloud cover
 * - 7-day forecast with precipitation %, sunrise/sunset
 * - Day detail view with comprehensive data
 * - Improved ZIP code entry with location preview
 * - Settings for units and display options
 * - Uses Open-Meteo free API (no key required)
 */

#ifndef SUMI_PLUGIN_WEATHER_H
#define SUMI_PLUGIN_WEATHER_H

#include "config.h"
#if FEATURE_WEATHER

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "core/PluginHelpers.h"
#include "core/SettingsManager.h"
#include "core/WiFiManager.h"
#include "core/HomeScreen.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// Constants
// =============================================================================
#ifndef WEATHER_CACHE_PATH
#define WEATHER_CACHE_PATH "/.sumi/weather_cache.bin"
#endif

enum WeatherScreen : uint8_t {
    WX_SCREEN_MAIN,
    WX_SCREEN_FORECAST,
    WX_SCREEN_DAY_DETAIL,
    WX_SCREEN_LOCATION,
    WX_SCREEN_SETTINGS
};

// =============================================================================
// Data Structures
// =============================================================================
struct CurrentWeather {
    float temp;
    float feelsLike;
    int humidity;
    float windSpeed;
    int windDirection;
    float windGusts;
    int cloudCover;
    int weatherCode;
    bool isDay;
};

struct DayForecast {
    char date[12];          // "Mon 1/27"
    char fullDate[16];      // "Monday, Jan 27"
    int weatherCode;
    float tempHigh;
    float tempLow;
    float feelsHigh;
    float feelsLow;
    char sunrise[8];        // "7:12 AM"
    char sunset[8];         // "5:15 PM"
    float uvIndex;
    float precipAmount;     // inches or mm
    int precipChance;       // 0-100%
    float windMax;
    float gustMax;
};

struct ZipPreview {
    bool valid;
    char city[32];
    char state[4];
    float lat;
    float lon;
};

// =============================================================================
// Weather App Class
// =============================================================================
class WeatherApp {
public:
    static const int FORECAST_DAYS = 7;
    
    // Screen dimensions
    int screenW, screenH;
    
    // State
    WeatherScreen currentScreen;
    int menuCursor;
    int selectedDay;
    bool needsFullRedraw;
    
    // Weather data
    CurrentWeather current;
    DayForecast forecast[FORECAST_DAYS];
    char location[64];
    bool hasData;
    bool locationSet;
    unsigned long lastUpdate;
    
    // ZIP entry
    char zipCode[6];
    int zipLen;
    int zipCursor;
    ZipPreview zipPreview;
    
    // Settings
    bool useCelsius;
    bool showFeelsLike;
    bool showUV;
    bool showSunTimes;
    
    // ==========================================================================
    // Constructor
    // ==========================================================================
    WeatherApp() {
        currentScreen = WX_SCREEN_MAIN;
        menuCursor = 0;
        selectedDay = 0;
        needsFullRedraw = true;
        hasData = false;
        locationSet = false;
        lastUpdate = 0;
        zipLen = 0;
        zipCursor = 0;
        memset(zipCode, 0, sizeof(zipCode));
        memset(location, 0, sizeof(location));
        memset(&current, 0, sizeof(current));
        memset(forecast, 0, sizeof(forecast));
        zipPreview.valid = false;
        
        // Default settings
        useCelsius = false;
        showFeelsLike = true;
        showUV = true;
        showSunTimes = true;
    }
    
    // ==========================================================================
    // Init
    // ==========================================================================
    void loadFromHomeCache() {
        // Try to load basic weather data from home screen's cache as fallback
        // Matches WeatherWidget struct in HomeScreen.cpp
        File f = SD.open(WEATHER_CACHE_PATH, FILE_READ);
        if (!f) return;
        
        struct WeatherCache {
            uint32_t magic;
            uint32_t timestamp;
            float temperature;
            int weatherCode;
            int humidity;
            float windSpeed;
            char location[48];
            bool useCelsius;
            float high;
            float low;
            float forecastHigh[3];
            float forecastLow[3];
            char forecastDay[3][4];
        } cache;
        
        size_t read = f.read((uint8_t*)&cache, sizeof(cache));
        f.close();
        
        if (read != sizeof(cache) || cache.magic != 0x57545853) return;
        
        // Load basic data into our structures
        current.temp = cache.temperature;
        current.weatherCode = cache.weatherCode;
        current.humidity = cache.humidity;
        current.windSpeed = cache.windSpeed;
        useCelsius = cache.useCelsius;
        strncpy(location, cache.location, sizeof(location) - 1);
        locationSet = (location[0] != '\0');
        
        // Load today's forecast
        forecast[0].tempHigh = cache.high;
        forecast[0].tempLow = cache.low;
        forecast[0].weatherCode = cache.weatherCode;
        strncpy(forecast[0].date, "Today", sizeof(forecast[0].date));
        
        // Load 3-day forecast
        for (int i = 0; i < 3; i++) {
            forecast[i+1].tempHigh = cache.forecastHigh[i];
            forecast[i+1].tempLow = cache.forecastLow[i];
            strncpy(forecast[i+1].date, cache.forecastDay[i], sizeof(forecast[i+1].date));
        }
        
        hasData = true;
        Serial.printf("[WEATHER] Loaded from cache: %.1f°, H:%.0f L:%.0f\n", 
                      cache.temperature, cache.high, cache.low);
    }
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
        
        // Load settings
        useCelsius = settingsManager.weather.celsius;
        
        // Check for saved location
        if (settingsManager.weather.latitude != 0.0f || 
            settingsManager.weather.longitude != 0.0f) {
            locationSet = true;
            strncpy(location, settingsManager.weather.location, sizeof(location) - 1);
        }
        
        // First try to load from cache (instant)
        loadFromHomeCache();
        
        // Then try to fetch fresh data if we have WiFi and location
        if (wifiManager.hasCredentials()) {
            if (!locationSet) {
                fetchLocation();
            }
            if (locationSet) {
                fetchWeather();
            }
            wifiManager.disconnectBriefly();
        }
        
        currentScreen = WX_SCREEN_MAIN;
        menuCursor = 0;
        needsFullRedraw = true;
    }
    
    // ==========================================================================
    // Input Handling
    // ==========================================================================
    bool handleInput(Button btn) {
        switch (currentScreen) {
            case WX_SCREEN_MAIN: return handleMainInput(btn);
            case WX_SCREEN_FORECAST: return handleForecastInput(btn);
            case WX_SCREEN_DAY_DETAIL: return handleDayDetailInput(btn);
            case WX_SCREEN_LOCATION: return handleLocationInput(btn);
            case WX_SCREEN_SETTINGS: return handleSettingsInput(btn);
            default: return false;
        }
    }
    
    bool handleMainInput(Button btn) {
        switch (btn) {
            case BTN_UP:
            case BTN_DOWN:
                menuCursor = 1 - menuCursor;
                // Partial refresh for cursor
                return true;
            case BTN_CONFIRM:
                if (menuCursor == 0) {
                    // 7-Day Forecast
                    selectedDay = 0;
                    currentScreen = WX_SCREEN_FORECAST;
                } else {
                    // Change Location
                    zipLen = 0;
                    zipCursor = 0;
                    memset(zipCode, 0, sizeof(zipCode));
                    zipPreview.valid = false;
                    currentScreen = WX_SCREEN_LOCATION;
                }
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_LEFT:
                // Refresh weather
                if (wifiManager.hasCredentials() && locationSet) {
                    fetchWeather();
                    wifiManager.disconnectBriefly();
                    needsFullRedraw = true;  // Data change
                }
                return true;
            case BTN_RIGHT:
                // Settings
                menuCursor = 0;
                currentScreen = WX_SCREEN_SETTINGS;
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                return false;
            default:
                return true;
        }
    }
    
    bool handleForecastInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (selectedDay > 0) selectedDay--;
                // Partial refresh for cursor
                return true;
            case BTN_DOWN:
                if (selectedDay < FORECAST_DAYS - 1) selectedDay++;
                // Partial refresh for cursor
                return true;
            case BTN_CONFIRM:
                currentScreen = WX_SCREEN_DAY_DETAIL;
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                menuCursor = 0;
                currentScreen = WX_SCREEN_MAIN;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleDayDetailInput(Button btn) {
        switch (btn) {
            case BTN_LEFT:
                if (selectedDay > 0) selectedDay--;
                // Partial refresh - just day change
                return true;
            case BTN_RIGHT:
                if (selectedDay < FORECAST_DAYS - 1) selectedDay++;
                // Partial refresh - just day change
                return true;
            case BTN_BACK:
                currentScreen = WX_SCREEN_FORECAST;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleLocationInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (zipLen > 0) {
                    char& c = zipCode[zipCursor];
                    if (c < '9') c++; else c = '0';
                    lookupZipPreview();
                }
                // Partial refresh
                return true;
            case BTN_DOWN:
                if (zipLen > 0) {
                    char& c = zipCode[zipCursor];
                    if (c > '0') c--; else c = '9';
                    lookupZipPreview();
                }
                return true;
            case BTN_RIGHT:
                if (zipLen < 5) {
                    zipCode[zipLen] = '0';
                    zipCursor = zipLen;
                    zipLen++;
                    zipCode[zipLen] = '\0';
                    if (zipLen == 5) lookupZipPreview();
                } else if (zipCursor < 4) {
                    zipCursor++;
                }
                return true;
            case BTN_LEFT:
                if (zipCursor > 0) {
                    zipCursor--;
                } else if (zipLen > 0) {
                    zipLen--;
                    zipCode[zipLen] = '\0';
                    zipPreview.valid = false;
                }
                return true;
            case BTN_CONFIRM:
                if (zipLen == 5 && zipPreview.valid) {
                    // Confirm location
                    settingsManager.weather.latitude = zipPreview.lat;
                    settingsManager.weather.longitude = zipPreview.lon;
                    snprintf(settingsManager.weather.location, sizeof(settingsManager.weather.location),
                             "%s, %s %s", zipPreview.city, zipPreview.state, zipCode);
                    strncpy(settingsManager.weather.zipCode, zipCode, 5);
                    settingsManager.markDirty();
                    settingsManager.save();
                    
                    locationSet = true;
                    strncpy(location, settingsManager.weather.location, sizeof(location) - 1);
                    
                    // Fetch weather for new location
                    fetchWeather();
                    wifiManager.disconnectBriefly();
                    
                    menuCursor = 0;
                    currentScreen = WX_SCREEN_MAIN;
                    needsFullRedraw = true;  // Screen change
                }
                return true;
            case BTN_BACK:
                menuCursor = 1;
                currentScreen = WX_SCREEN_MAIN;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleSettingsInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (menuCursor > 0) menuCursor--;
                // Partial refresh for cursor
                return true;
            case BTN_DOWN:
                if (menuCursor < 3) menuCursor++;
                // Partial refresh for cursor
                return true;
            case BTN_CONFIRM:
            case BTN_LEFT:
            case BTN_RIGHT:
                if (menuCursor == 0) {
                    useCelsius = !useCelsius;
                    settingsManager.weather.celsius = useCelsius;
                    settingsManager.markDirty();
                    // Refetch with new units
                    if (locationSet && wifiManager.hasCredentials()) {
                        fetchWeather();
                        wifiManager.disconnectBriefly();
                        needsFullRedraw = true;  // Data change
                    }
                } else if (menuCursor == 1) {
                    showFeelsLike = !showFeelsLike;
                } else if (menuCursor == 2) {
                    showUV = !showUV;
                } else if (menuCursor == 3) {
                    showSunTimes = !showSunTimes;
                }
                // Partial refresh for toggles
                return true;
            case BTN_BACK:
                menuCursor = 0;
                currentScreen = WX_SCREEN_MAIN;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    // ==========================================================================
    // Update (for periodic refresh)
    // ==========================================================================
    bool update() {
        return false;
    }
    
    // ==========================================================================
    // Drawing
    // ==========================================================================
    void draw() {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            
            switch (currentScreen) {
                case WX_SCREEN_MAIN: drawMainScreen(); break;
                case WX_SCREEN_FORECAST: drawForecastScreen(); break;
                case WX_SCREEN_DAY_DETAIL: drawDayDetailScreen(); break;
                case WX_SCREEN_LOCATION: drawLocationScreen(); break;
                case WX_SCREEN_SETTINGS: drawSettingsScreen(); break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawPartial() {
        // Use partial window for smoother updates
        display.setPartialWindow(0, 0, screenW, screenH);
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            
            switch (currentScreen) {
                case WX_SCREEN_MAIN: drawMainScreen(); break;
                case WX_SCREEN_FORECAST: drawForecastScreen(); break;
                case WX_SCREEN_DAY_DETAIL: drawDayDetailScreen(); break;
                case WX_SCREEN_LOCATION: drawLocationScreen(); break;
                case WX_SCREEN_SETTINGS: drawSettingsScreen(); break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawFullScreen() {
        needsFullRedraw = true;
        draw();
    }
    
    // --------------------------------------------------------------------------
    // Main Screen
    // --------------------------------------------------------------------------
    void drawMainScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("Weather", screenW / 2, 26);
        
        if (locationSet) {
            display.setFont(&FreeSans9pt7b);
            centerText(location, screenW / 2, 42);
        }
        
        int y = 60;
        
        // No WiFi warning
        if (!wifiManager.hasCredentials()) {
            display.setTextColor(GxEPD_BLACK);
            display.setFont(&FreeSansBold12pt7b);
            centerText("WiFi not configured", screenW / 2, screenH / 2 - 20);
            display.setFont(&FreeSans9pt7b);
            centerText("Set up WiFi in Settings", screenW / 2, screenH / 2 + 10);
            return;
        }
        
        // No location
        if (!locationSet) {
            display.setTextColor(GxEPD_BLACK);
            display.setFont(&FreeSansBold12pt7b);
            centerText("No location set", screenW / 2, screenH / 2 - 20);
            display.setFont(&FreeSans9pt7b);
            centerText("Press OK to set location", screenW / 2, screenH / 2 + 10);
            
            // Show menu anyway
            y = screenH / 2 + 60;
            drawMainMenu(y);
            return;
        }
        
        // Current conditions card
        display.setTextColor(GxEPD_BLACK);
        display.drawRoundRect(16, y, screenW - 32, 150, 8, GxEPD_BLACK);
        
        // Weather icon
        drawWeatherIcon(current.weatherCode, 40, y + 20, 70);
        
        // Temperature
        display.setFont(&FreeSansBold12pt7b);
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%d%s", (int)round(current.temp), useCelsius ? "C" : "F");
        display.setTextSize(2);
        display.setCursor(130, y + 60);
        display.print(tempStr);
        display.setTextSize(1);
        
        // Feels like
        if (showFeelsLike) {
            display.setFont(&FreeSans9pt7b);
            snprintf(tempStr, sizeof(tempStr), "Feels like %d%s", (int)round(current.feelsLike), useCelsius ? "C" : "F");
            display.setCursor(130, y + 85);
            display.print(tempStr);
        }
        
        // Condition
        char desc[32];
        descriptionFromCode(current.weatherCode, desc, sizeof(desc));
        display.setCursor(130, y + 108);
        display.print(desc);
        
        // Stats row
        int statsY = y + 125;
        display.drawFastHLine(24, statsY, screenW - 48, GxEPD_BLACK);
        statsY += 8;
        
        int statW = (screenW - 48) / 4;
        const char* labels[] = { "Humidity", "Wind", "UV", "Cloud" };
        char values[4][16];
        snprintf(values[0], 16, "%d%%", current.humidity);
        snprintf(values[1], 16, "%d mph", (int)round(current.windSpeed));
        snprintf(values[2], 16, "%.1f", forecast[0].uvIndex);
        snprintf(values[3], 16, "%d%%", current.cloudCover);
        
        display.setFont(&FreeSans9pt7b);
        for (int i = 0; i < 4; i++) {
            int x = 24 + i * statW + statW / 2;
            display.setTextColor(GxEPD_BLACK);
            
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(labels[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x - tw / 2, statsY + 12);
            display.print(labels[i]);
            
            display.setFont(&FreeSansBold9pt7b);
            display.getTextBounds(values[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x - tw / 2, statsY + 30);
            display.print(values[i]);
            display.setFont(&FreeSans9pt7b);
        }
        
        y += 165;
        
        // Today's details card
        display.drawRoundRect(16, y, screenW - 32, 100, 8, GxEPD_BLACK);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(28, y + 22);
        display.print("Today's Details");
        
        display.setFont(&FreeSans9pt7b);
        int detailY = y + 42;
        int col1 = 28, col2 = screenW / 2 + 10;
        
        // High/Low
        display.setCursor(col1, detailY);
        display.print("High / Low");
        snprintf(tempStr, sizeof(tempStr), "%d / %d", (int)round(forecast[0].tempHigh), (int)round(forecast[0].tempLow));
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(tempStr, 0, 0, &tx, &ty, &tw, &th);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(screenW / 2 - 28 - tw, detailY);
        display.print(tempStr);
        
        // Precip
        display.setFont(&FreeSans9pt7b);
        display.setCursor(col2, detailY);
        display.print("Precip");
        snprintf(tempStr, sizeof(tempStr), "%d%%", forecast[0].precipChance);
        display.getTextBounds(tempStr, 0, 0, &tx, &ty, &tw, &th);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(screenW - 28 - tw, detailY);
        display.print(tempStr);
        
        detailY += 24;
        
        // Sunrise
        display.setFont(&FreeSans9pt7b);
        display.setCursor(col1, detailY);
        display.print("Sunrise");
        display.setFont(&FreeSansBold9pt7b);
        display.getTextBounds(forecast[0].sunrise, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW / 2 - 28 - tw, detailY);
        display.print(forecast[0].sunrise);
        
        // Sunset
        display.setFont(&FreeSans9pt7b);
        display.setCursor(col2, detailY);
        display.print("Sunset");
        display.setFont(&FreeSansBold9pt7b);
        display.getTextBounds(forecast[0].sunset, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - 28 - tw, detailY);
        display.print(forecast[0].sunset);
        
        y += 115;
        
        // Menu
        drawMainMenu(y);
        
        // Footer
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        char updateStr[32];
        unsigned long ago = (millis() - lastUpdate) / 60000;
        if (ago < 1) {
            strcpy(updateStr, "Just updated");
        } else if (ago < 60) {
            snprintf(updateStr, sizeof(updateStr), "Updated %lu min ago", ago);
        } else {
            snprintf(updateStr, sizeof(updateStr), "Updated %lu hr ago", ago / 60);
        }
        
        display.setCursor(20, screenH - 12);
        display.print(updateStr);
        
        display.setCursor(screenW - 140, screenH - 12);
        display.print("< Refresh  > Settings");
    }
    
    void drawMainMenu(int y) {
        const char* items[] = { "7-Day Forecast", "Change Location" };
        const char* descs[] = { "Detailed daily forecasts", "Enter ZIP code" };
        
        for (int i = 0; i < 2; i++) {
            int itemY = y + i * 58;
            bool sel = (menuCursor == i);
            
            if (sel) {
                display.fillRoundRect(16, itemY, screenW - 32, 52, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(16, itemY, screenW - 32, 52, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(32, itemY + 22);
            display.print(items[i]);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(32, itemY + 42);
            display.print(descs[i]);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(screenW - 50, itemY + 32);
            display.print(">");
        }
        
        display.setTextColor(GxEPD_BLACK);
    }
    
    // --------------------------------------------------------------------------
    // Forecast Screen
    // --------------------------------------------------------------------------
    void drawForecastScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("7-Day Forecast", screenW / 2, 26);
        display.setFont(&FreeSans9pt7b);
        centerText(location, screenW / 2, 42);
        
        int y = 56;
        int itemH = 68;
        
        display.setTextColor(GxEPD_BLACK);
        
        for (int i = 0; i < FORECAST_DAYS; i++) {
            int itemY = y + i * itemH;
            bool sel = (selectedDay == i);
            
            if (sel) {
                display.fillRoundRect(12, itemY, screenW - 24, itemH - 4, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(12, itemY, screenW - 24, itemH - 4, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Day/Date
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(24, itemY + 24);
            display.print(i == 0 ? "Today" : forecast[i].date);
            
            // Icon
            drawWeatherIcon(forecast[i].weatherCode, 100, itemY + 8, 48);
            
            // Temps
            display.setFont(&FreeSansBold12pt7b);
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%d", (int)round(forecast[i].tempHigh));
            display.setCursor(170, itemY + 32);
            display.print(tempStr);
            
            display.setFont(&FreeSans9pt7b);
            snprintf(tempStr, sizeof(tempStr), "/ %d", (int)round(forecast[i].tempLow));
            display.setCursor(205, itemY + 32);
            display.print(tempStr);
            
            // Precip chance
            if (forecast[i].precipChance > 0) {
                snprintf(tempStr, sizeof(tempStr), "%d%%", forecast[i].precipChance);
                
                int badgeW = 50;
                int badgeX = screenW - 28 - badgeW;
                int badgeY = itemY + 14;
                
                if (sel) {
                    display.fillRoundRect(badgeX, badgeY, badgeW, 24, 12, GxEPD_WHITE);
                    display.setTextColor(GxEPD_BLACK);
                } else {
                    display.fillRoundRect(badgeX, badgeY, badgeW, 24, 12, GxEPD_BLACK);
                    display.setTextColor(GxEPD_WHITE);
                }
                
                display.setFont(&FreeSansBold9pt7b);
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(tempStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(badgeX + (badgeW - tw) / 2, badgeY + 18);
                display.print(tempStr);
                
                display.setTextColor(sel ? GxEPD_WHITE : GxEPD_BLACK);
            }
            
            // Description
            display.setFont(&FreeSans9pt7b);
            char desc[24];
            descriptionFromCode(forecast[i].weatherCode, desc, sizeof(desc));
            display.setCursor(24, itemY + 48);
            display.print(desc);
        }
        
        // Footer
        display.setTextColor(GxEPD_BLACK);
        display.fillRect(0, screenH - 36, screenW, 36, GxEPD_WHITE);
        display.drawFastHLine(0, screenH - 36, screenW, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        centerText("Up/Down: Select  •  OK: Details  •  Back: Return", screenW / 2, screenH - 12);
    }
    
    // --------------------------------------------------------------------------
    // Day Detail Screen
    // --------------------------------------------------------------------------
    void drawDayDetailScreen() {
        DayForecast& day = forecast[selectedDay];
        
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText(day.fullDate, screenW / 2, 26);
        display.setFont(&FreeSans9pt7b);
        centerText(location, screenW / 2, 42);
        
        int y = 60;
        
        // Main condition
        display.setTextColor(GxEPD_BLACK);
        
        drawWeatherIcon(day.weatherCode, 40, y, 90);
        
        char desc[32];
        descriptionFromCode(day.weatherCode, desc, sizeof(desc));
        display.setFont(&FreeSans9pt7b);
        display.setCursor(150, y + 20);
        display.print(desc);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(2);
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%d", (int)round(day.tempHigh));
        display.setCursor(150, y + 65);
        display.print(tempStr);
        display.setTextSize(1);
        
        display.setFont(&FreeSans9pt7b);
        snprintf(tempStr, sizeof(tempStr), "High: %d / Low: %d", (int)round(day.tempHigh), (int)round(day.tempLow));
        display.setCursor(150, y + 90);
        display.print(tempStr);
        
        y += 110;
        
        // Details card
        display.drawRoundRect(16, y, screenW - 32, 280, 8, GxEPD_BLACK);
        
        struct DetailItem {
            const char* icon;
            const char* label;
            char value[24];
        };
        
        DetailItem items[8];
        items[0].icon = ""; items[0].label = "Feels Like";
        snprintf(items[0].value, 24, "%d / %d", (int)round(day.feelsHigh), (int)round(day.feelsLow));
        
        items[1].icon = ""; items[1].label = "Precipitation";
        snprintf(items[1].value, 24, "%d%% chance", day.precipChance);
        
        items[2].icon = ""; items[2].label = "Precip Amount";
        snprintf(items[2].value, 24, "%.2f in", day.precipAmount);
        
        items[3].icon = ""; items[3].label = "Wind";
        snprintf(items[3].value, 24, "Up to %d mph", (int)round(day.windMax));
        
        items[4].icon = ""; items[4].label = "Wind Gusts";
        snprintf(items[4].value, 24, "%d mph", (int)round(day.gustMax));
        
        items[5].icon = ""; items[5].label = "UV Index";
        const char* uvDesc = day.uvIndex < 3 ? "Low" : (day.uvIndex < 6 ? "Moderate" : "High");
        snprintf(items[5].value, 24, "%.1f (%s)", day.uvIndex, uvDesc);
        
        items[6].icon = ""; items[6].label = "Sunrise";
        strncpy(items[6].value, day.sunrise, 23);
        
        items[7].icon = ""; items[7].label = "Sunset";
        strncpy(items[7].value, day.sunset, 23);
        
        int itemY = y + 8;
        int col1 = 28, col2 = screenW / 2 + 10;
        
        for (int i = 0; i < 8; i++) {
            int x = (i % 2 == 0) ? col1 : col2;
            int iy = itemY + (i / 2) * 68;
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(x, iy + 16);
            display.print(items[i].label);
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(x, iy + 38);
            display.print(items[i].value);
            
            // Divider
            if (i < 6 && i % 2 == 1) {
                display.drawFastHLine(28, iy + 52, screenW - 56, GxEPD_BLACK);
            }
        }
        
        // Wind advisory alert
        if (day.gustMax > 40) {
            int alertY = y + 290;
            display.fillRoundRect(16, alertY, screenW - 32, 50, 8, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(50, alertY + 22);
            display.print("Wind Advisory");
            display.setFont(&FreeSans9pt7b);
            char alertStr[48];
            snprintf(alertStr, sizeof(alertStr), "Gusts up to %d mph expected", (int)round(day.gustMax));
            display.setCursor(50, alertY + 40);
            display.print(alertStr);
        }
        
        // Footer
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        centerText("Left/Right: Prev/Next  •  Back: Forecast", screenW / 2, screenH - 12);
    }
    
    // --------------------------------------------------------------------------
    // Location Screen
    // --------------------------------------------------------------------------
    void drawLocationScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("Set Location", screenW / 2, 34);
        
        int y = 70;
        
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(24, y);
        display.print("ENTER ZIP CODE");
        y += 20;
        
        // ZIP entry card
        display.drawRoundRect(16, y, screenW - 32, 150, 12, GxEPD_BLACK);
        display.drawRoundRect(18, y + 2, screenW - 36, 146, 10, GxEPD_BLACK);
        
        // Digit boxes
        int boxW = 60;
        int boxH = 80;
        int gap = 12;
        int totalW = 5 * boxW + 4 * gap;
        int startX = (screenW - totalW) / 2;
        int boxY = y + 20;
        
        for (int i = 0; i < 5; i++) {
            int x = startX + i * (boxW + gap);
            bool active = (i == zipCursor && i < zipLen);
            bool filled = (i < zipLen);
            
            if (active) {
                display.fillRoundRect(x, boxY, boxW, boxH, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(x, boxY, boxW, boxH, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            if (filled) {
                display.setFont(&FreeSansBold12pt7b);
                display.setTextSize(2);
                char digit[2] = { zipCode[i], '\0' };
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(digit, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(x + (boxW - tw) / 2, boxY + boxH - 20);
                display.print(digit);
                display.setTextSize(1);
            }
        }
        
        display.setTextColor(GxEPD_BLACK);
        
        // Control hints
        display.setFont(&FreeSans9pt7b);
        int hintY = boxY + boxH + 20;
        centerText("Up/Down: Change  •  Left/Right: Move", screenW / 2, hintY);
        
        y += 170;
        
        // Preview card
        display.drawRoundRect(16, y, screenW - 32, 80, 8, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(28, y + 22);
        display.print("Preview");
        
        if (zipPreview.valid) {
            display.setFont(&FreeSansBold12pt7b);
            char previewStr[48];
            snprintf(previewStr, sizeof(previewStr), "%s, %s %s", zipPreview.city, zipPreview.state, zipCode);
            display.setCursor(28, y + 48);
            display.print(previewStr);
            
            display.setFont(&FreeSans9pt7b);
            snprintf(previewStr, sizeof(previewStr), "Lat: %.4f  Lon: %.4f", zipPreview.lat, zipPreview.lon);
            display.setCursor(28, y + 68);
            display.print(previewStr);
        } else if (zipLen == 5) {
            display.setFont(&FreeSans9pt7b);
            display.setCursor(28, y + 48);
            display.print("Looking up ZIP code...");
        } else {
            display.setFont(&FreeSans9pt7b);
            display.setCursor(28, y + 48);
            display.print("Enter 5 digits to preview");
        }
        
        y += 95;
        
        // Confirm button
        if (zipLen == 5 && zipPreview.valid) {
            display.fillRoundRect(16, y, screenW - 32, 54, 8, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(16, y, screenW - 32, 54, 8, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setFont(&FreeSansBold12pt7b);
        centerText("Confirm Location", screenW / 2, y + 36);
        
        display.setTextColor(GxEPD_BLACK);
        
        // Auto-detect option
        y += 70;
        display.drawRoundRect(16, y, screenW - 32, 54, 8, GxEPD_BLACK);
        display.setFont(&FreeSansBold9pt7b);
        centerText("Use Current Location", screenW / 2, y + 24);
        display.setFont(&FreeSans9pt7b);
        centerText("Auto-detect from IP address", screenW / 2, y + 44);
    }
    
    // --------------------------------------------------------------------------
    // Settings Screen
    // --------------------------------------------------------------------------
    void drawSettingsScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("Weather Settings", screenW / 2, 34);
        
        int y = 64;
        
        // Current location card
        display.setTextColor(GxEPD_BLACK);
        display.drawRoundRect(16, y, screenW - 32, 60, 8, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(28, y + 18);
        display.print("Current Location");
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(28, y + 42);
        display.print(locationSet ? location : "Not set");
        
        y += 80;
        
        // Units section
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y);
        display.print("UNITS & DISPLAY");
        y += 20;
        
        struct SettingItem {
            const char* label;
            const char* valueOn;
            const char* valueOff;
            bool* setting;
        };
        
        SettingItem items[] = {
            { "Temperature", "Celsius (C)", "Fahrenheit (F)", nullptr },
            { "Show \"Feels Like\"", "ON", "OFF", &showFeelsLike },
            { "Show UV Index", "ON", "OFF", &showUV },
            { "Show Sunrise/Sunset", "ON", "OFF", &showSunTimes }
        };
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 58;
            bool sel = (menuCursor == i);
            bool enabled = (i == 0) ? useCelsius : *(items[i].setting);
            
            if (sel) {
                display.drawRoundRect(14, itemY - 2, screenW - 28, 54, 8, GxEPD_BLACK);
                display.drawRoundRect(15, itemY - 1, screenW - 30, 52, 7, GxEPD_BLACK);
            }
            
            display.drawRoundRect(16, itemY, screenW - 32, 50, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(28, itemY + 30);
            display.print(items[i].label);
            
            if (i == 0) {
                // Temperature toggle shows current value
                display.setFont(&FreeSansBold9pt7b);
                const char* val = useCelsius ? items[i].valueOn : items[i].valueOff;
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(val, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(screenW - 40 - tw, itemY + 30);
                display.print(val);
            } else {
                // Toggle switch
                int sw = 44, sh = 24;
                int sx = screenW - 70;
                int sy = itemY + 13;
                
                if (enabled) {
                    display.fillRoundRect(sx, sy, sw, sh, sh / 2, GxEPD_BLACK);
                    display.fillCircle(sx + sw - sh / 2, sy + sh / 2, 8, GxEPD_WHITE);
                } else {
                    display.drawRoundRect(sx, sy, sw, sh, sh / 2, GxEPD_BLACK);
                    display.fillCircle(sx + sh / 2, sy + sh / 2, 8, GxEPD_BLACK);
                }
            }
        }
        
        // Footer
        display.setFont(&FreeSans9pt7b);
        centerText("Up/Down: Select  •  OK: Toggle  •  Back: Return", screenW / 2, screenH - 12);
    }
    
    // ==========================================================================
    // Weather Icon Drawing
    // ==========================================================================
    void drawWeatherIcon(int code, int x, int y, int size) {
        int cx = x + size / 2;
        int cy = y + size / 2;
        
        if (code == 0) {
            // Clear - Sun
            drawSun(cx, cy, size / 3);
        } else if (code <= 3) {
            // Partly cloudy
            drawSun(cx - size / 6, cy - size / 6, size / 4);
            drawCloud(cx + size / 8, cy + size / 8, size / 2, size / 3);
        } else if (code <= 49) {
            // Fog
            drawFog(cx, cy, size / 2, size / 3);
        } else if (code <= 69) {
            // Rain
            drawCloud(cx, cy - size / 6, size / 2, size / 3);
            drawRain(cx, cy + size / 4, size / 3);
        } else if (code <= 79) {
            // Snow
            drawCloud(cx, cy - size / 6, size / 2, size / 3);
            drawSnow(cx, cy + size / 4, size / 3);
        } else if (code <= 84) {
            // Rain showers
            drawCloud(cx, cy - size / 6, size / 2, size / 3);
            drawRain(cx, cy + size / 4, size / 3);
        } else if (code <= 94) {
            // Snow showers
            drawCloud(cx, cy - size / 6, size / 2, size / 3);
            drawSnow(cx, cy + size / 4, size / 3);
        } else {
            // Thunderstorm
            drawCloud(cx, cy - size / 6, size / 2, size / 3);
            drawLightning(cx, cy + size / 6, size / 4);
        }
    }
    
    void drawSun(int cx, int cy, int r) {
        display.fillCircle(cx, cy, r, GxEPD_BLACK);
        // Rays
        for (int i = 0; i < 8; i++) {
            float angle = i * PI / 4;
            int x1 = cx + cos(angle) * (r + 4);
            int y1 = cy + sin(angle) * (r + 4);
            int x2 = cx + cos(angle) * (r + 10);
            int y2 = cy + sin(angle) * (r + 10);
            display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
            display.drawLine(x1 + 1, y1, x2 + 1, y2, GxEPD_BLACK);
        }
    }
    
    void drawCloud(int cx, int cy, int w, int h) {
        int r = h / 2;
        display.fillCircle(cx - w / 4, cy, r, GxEPD_BLACK);
        display.fillCircle(cx + w / 4, cy, r, GxEPD_BLACK);
        display.fillCircle(cx, cy - r / 2, r, GxEPD_BLACK);
        display.fillRect(cx - w / 3, cy, w * 2 / 3, h / 2 + 2, GxEPD_BLACK);
    }
    
    void drawRain(int cx, int cy, int size) {
        for (int i = -1; i <= 1; i++) {
            int x = cx + i * (size / 3);
            display.drawLine(x, cy, x - 4, cy + size / 2, GxEPD_BLACK);
            display.drawLine(x + 1, cy, x - 3, cy + size / 2, GxEPD_BLACK);
        }
    }
    
    void drawSnow(int cx, int cy, int size) {
        for (int i = -1; i <= 1; i++) {
            int x = cx + i * (size / 3);
            int y = cy + (i == 0 ? 0 : size / 4);
            display.fillCircle(x, y, 3, GxEPD_BLACK);
        }
    }
    
    void drawLightning(int cx, int cy, int size) {
        int x1 = cx - size / 4;
        int x2 = cx + size / 8;
        int x3 = cx - size / 8;
        int x4 = cx + size / 4;
        
        display.drawLine(x1, cy, x2, cy + size / 2, GxEPD_BLACK);
        display.drawLine(x2, cy + size / 2, x3, cy + size / 2, GxEPD_BLACK);
        display.drawLine(x3, cy + size / 2, x4, cy + size, GxEPD_BLACK);
    }
    
    void drawFog(int cx, int cy, int w, int h) {
        for (int i = 0; i < 3; i++) {
            int y = cy - h / 2 + i * (h / 2);
            display.drawFastHLine(cx - w / 2, y, w, GxEPD_BLACK);
            display.drawFastHLine(cx - w / 2, y + 1, w, GxEPD_BLACK);
        }
    }
    
    // ==========================================================================
    // API Functions
    // ==========================================================================
    void fetchLocation() {
        Serial.println("[WEATHER] Fetching location from IP...");
        
        if (!wifiManager.connectBriefly(10000)) {
            Serial.println("[WEATHER] WiFi connect failed");
            return;
        }
        
        WiFiClient client;
        if (!client.connect("ip-api.com", 80)) {
            Serial.println("[WEATHER] IP API connection failed");
            return;
        }
        
        client.print("GET /json/?fields=status,city,country,lat,lon,offset HTTP/1.1\r\n");
        client.print("Host: ip-api.com\r\n");
        client.print("Connection: close\r\n\r\n");
        
        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                client.stop();
                return;
            }
        }
        
        // Skip headers
        while (client.available()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") break;
        }
        
        String payload = client.readString();
        client.stop();
        
        JsonDocument doc;
        if (deserializeJson(doc, payload)) return;
        
        if (strcmp(doc["status"] | "fail", "success") != 0) return;
        
        float lat = doc["lat"] | 0.0f;
        float lon = doc["lon"] | 0.0f;
        const char* city = doc["city"] | "Unknown";
        const char* country = doc["country"] | "";
        int32_t tzOffset = doc["offset"] | 0;
        
        if (lat == 0.0f && lon == 0.0f) return;
        
        settingsManager.weather.latitude = lat;
        settingsManager.weather.longitude = lon;
        settingsManager.weather.timezoneOffset = tzOffset;
        
        snprintf(settingsManager.weather.location, sizeof(settingsManager.weather.location),
                 "%s, %s", city, country);
        settingsManager.markDirty();
        
        locationSet = true;
        strncpy(location, settingsManager.weather.location, sizeof(location) - 1);
        
        configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov");
        
        Serial.printf("[WEATHER] Location: %s (%.4f, %.4f)\n", location, lat, lon);
    }
    
    void lookupZipPreview() {
        if (zipLen != 5) {
            zipPreview.valid = false;
            return;
        }
        
        Serial.printf("[WEATHER] Preview ZIP: %s\n", zipCode);
        
        if (!wifiManager.connectBriefly(10000)) return;
        
        WiFiClient client;
        if (!client.connect("api.zippopotam.us", 80)) return;
        
        char request[64];
        snprintf(request, sizeof(request), "GET /us/%s HTTP/1.1\r\n", zipCode);
        client.print(request);
        client.print("Host: api.zippopotam.us\r\n");
        client.print("Connection: close\r\n\r\n");
        
        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                client.stop();
                return;
            }
        }
        
        while (client.available()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") break;
        }
        
        String payload = client.readString();
        client.stop();
        
        JsonDocument doc;
        if (deserializeJson(doc, payload)) return;
        
        JsonArray places = doc["places"];
        if (places.size() == 0) return;
        
        JsonObject place = places[0];
        strncpy(zipPreview.city, place["place name"] | "Unknown", sizeof(zipPreview.city) - 1);
        strncpy(zipPreview.state, place["state abbreviation"] | "", sizeof(zipPreview.state) - 1);
        zipPreview.lat = atof(place["latitude"] | "0");
        zipPreview.lon = atof(place["longitude"] | "0");
        zipPreview.valid = (zipPreview.lat != 0.0f || zipPreview.lon != 0.0f);
        
        Serial.printf("[WEATHER] Preview: %s, %s (%.4f, %.4f)\n", 
                      zipPreview.city, zipPreview.state, zipPreview.lat, zipPreview.lon);
    }
    
    void fetchWeather() {
        if (!locationSet) {
            Serial.println("[WEATHER] No location set");
            return;
        }
        
        Serial.println("[WEATHER] Fetching 7-day forecast...");
        
        if (!wifiManager.connectBriefly(10000)) {
            Serial.println("[WEATHER] WiFi connect failed");
            return;
        }
        
        // Sync time if needed
        if (!wifiManager.isTimeSynced()) {
            configTime(settingsManager.weather.timezoneOffset, 0, "pool.ntp.org");
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 3000)) {
                wifiManager.setTimeSynced(true);
            }
        }
        
        float lat = settingsManager.weather.latitude;
        float lon = settingsManager.weather.longitude;
        
        WiFiClient client;
        if (!client.connect("api.open-meteo.com", 80)) {
            Serial.println("[WEATHER] Failed to connect to API");
            return;
        }
        
        // Request weather data - use larger buffer for full URL
        char request[640];
        int reqLen = snprintf(request, sizeof(request),
            "GET /v1/forecast?latitude=%.4f&longitude=%.4f"
            "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,"
            "weather_code,cloud_cover,wind_speed_10m"
            "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
            "apparent_temperature_max,apparent_temperature_min,"
            "sunrise,sunset,uv_index_max,precipitation_probability_max"
            "&temperature_unit=%s&wind_speed_unit=mph"
            "&timezone=auto&forecast_days=7 HTTP/1.0\r\n",
            lat, lon, useCelsius ? "celsius" : "fahrenheit");
        
        Serial.printf("[WEATHER] Request len: %d\n", reqLen);
        
        client.print(request);
        client.print("Host: api.open-meteo.com\r\n");
        client.print("Connection: close\r\n\r\n");
        
        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 10000) {
                Serial.println("[WEATHER] Timeout waiting for response");
                client.stop();
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
        
        Serial.printf("[WEATHER] Received %d bytes\n", payload.length());
        
        int jsonStart = payload.indexOf('{');
        if (jsonStart < 0) {
            Serial.println("[WEATHER] No JSON found in response");
            return;
        }
        
        String jsonPayload = payload.substring(jsonStart);
        
        // Use JsonDocument with capacity hint for 7-day forecast (~6KB typical)
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, jsonPayload);
        if (err) {
            Serial.printf("[WEATHER] JSON parse failed: %s\n", err.c_str());
            return;
        }
        
        // Parse current conditions
        JsonObject curr = doc["current"];
        current.temp = curr["temperature_2m"] | 0.0f;
        current.feelsLike = curr["apparent_temperature"] | current.temp;
        current.humidity = curr["relative_humidity_2m"] | 0;
        current.weatherCode = curr["weather_code"] | 0;
        current.cloudCover = curr["cloud_cover"] | 0;
        current.windSpeed = curr["wind_speed_10m"] | 0.0f;
        current.windDirection = curr["wind_direction_10m"] | 0;
        current.windGusts = curr["wind_gusts_10m"] | 0.0f;
        current.isDay = curr["is_day"] | 1;
        
        // Parse daily forecast
        JsonObject daily = doc["daily"];
        JsonArray dates = daily["time"];
        JsonArray codes = daily["weather_code"];
        JsonArray maxTemps = daily["temperature_2m_max"];
        JsonArray minTemps = daily["temperature_2m_min"];
        JsonArray feelsMax = daily["apparent_temperature_max"];
        JsonArray feelsMin = daily["apparent_temperature_min"];
        JsonArray sunrise = daily["sunrise"];
        JsonArray sunset = daily["sunset"];
        JsonArray uvMax = daily["uv_index_max"];
        JsonArray precipSum = daily["precipitation_sum"];
        JsonArray precipProb = daily["precipitation_probability_max"];
        JsonArray windMax = daily["wind_speed_10m_max"];
        JsonArray gustMax = daily["wind_gusts_10m_max"];
        
        const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        const char* fullDayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        
        for (int i = 0; i < FORECAST_DAYS && i < (int)dates.size(); i++) {
            const char* dateStr = dates[i] | "0000-00-00";
            int year, month, day;
            sscanf(dateStr, "%d-%d-%d", &year, &month, &day);
            
            struct tm tm = {0};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            mktime(&tm);
            
            snprintf(forecast[i].date, sizeof(forecast[i].date), "%s %d/%d", 
                     dayNames[tm.tm_wday], month, day);
            snprintf(forecast[i].fullDate, sizeof(forecast[i].fullDate), "%s, %s %d",
                     fullDayNames[tm.tm_wday], monthNames[month - 1], day);
            
            forecast[i].weatherCode = codes[i] | 0;
            forecast[i].tempHigh = maxTemps[i] | 0.0f;
            forecast[i].tempLow = minTemps[i] | 0.0f;
            forecast[i].feelsHigh = feelsMax[i] | forecast[i].tempHigh;
            forecast[i].feelsLow = feelsMin[i] | forecast[i].tempLow;
            forecast[i].uvIndex = uvMax[i] | 0.0f;
            forecast[i].precipAmount = precipSum[i] | 0.0f;
            forecast[i].precipChance = precipProb[i] | 0;
            forecast[i].windMax = windMax[i] | 0.0f;
            forecast[i].gustMax = gustMax[i] | 0.0f;
            
            // Parse sunrise/sunset times
            const char* sunriseStr = sunrise[i] | "T00:00";
            const char* sunsetStr = sunset[i] | "T00:00";
            
            // Find time part after 'T'
            const char* srt = strchr(sunriseStr, 'T');
            const char* sst = strchr(sunsetStr, 'T');
            
            if (srt) {
                int h, m;
                sscanf(srt + 1, "%d:%d", &h, &m);
                const char* ampm = h >= 12 ? "PM" : "AM";
                if (h > 12) h -= 12;
                if (h == 0) h = 12;
                snprintf(forecast[i].sunrise, sizeof(forecast[i].sunrise), "%d:%02d %s", h, m, ampm);
            }
            
            if (sst) {
                int h, m;
                sscanf(sst + 1, "%d:%d", &h, &m);
                const char* ampm = h >= 12 ? "PM" : "AM";
                if (h > 12) h -= 12;
                if (h == 0) h = 12;
                snprintf(forecast[i].sunset, sizeof(forecast[i].sunset), "%d:%02d %s", h, m, ampm);
            }
        }
        
        hasData = true;
        lastUpdate = millis();
        
        // Update home screen widget cache
        float forecastHigh[3] = {0, 0, 0};
        float forecastLow[3] = {0, 0, 0};
        char forecastDay[3][4] = {"", "", ""};
        
        for (int i = 0; i < 3 && i + 1 < FORECAST_DAYS; i++) {
            forecastHigh[i] = forecast[i + 1].tempHigh;
            forecastLow[i] = forecast[i + 1].tempLow;
            strncpy(forecastDay[i], forecast[i + 1].date, 3);
            forecastDay[i][3] = '\0';
        }
        
        saveWeatherCache(current.temp, current.weatherCode, current.humidity,
                         current.windSpeed, location, useCelsius,
                         forecast[0].tempHigh, forecast[0].tempLow,
                         forecastHigh, forecastLow, forecastDay,
                         forecast[0].sunrise, forecast[0].sunset);
        
        Serial.printf("[WEATHER] Got %d day forecast, current: %.1f, code %d\n",
                      FORECAST_DAYS, current.temp, current.weatherCode);
    }
    
    void descriptionFromCode(int code, char* buf, int bufLen) {
        const char* desc;
        if (code == 0) desc = "Clear sky";
        else if (code == 1) desc = "Mainly clear";
        else if (code == 2) desc = "Partly cloudy";
        else if (code == 3) desc = "Overcast";
        else if (code <= 49) desc = "Fog";
        else if (code <= 55) desc = "Drizzle";
        else if (code <= 59) desc = "Freezing drizzle";
        else if (code <= 65) desc = "Rain";
        else if (code <= 69) desc = "Freezing rain";
        else if (code <= 75) desc = "Snow";
        else if (code <= 79) desc = "Snow grains";
        else if (code <= 82) desc = "Rain showers";
        else if (code <= 86) desc = "Snow showers";
        else if (code == 95) desc = "Thunderstorm";
        else if (code <= 99) desc = "T-storm + hail";
        else desc = "Unknown";
        
        strncpy(buf, desc, bufLen - 1);
        buf[bufLen - 1] = '\0';
    }
    
    // ==========================================================================
    // Helpers
    // ==========================================================================
    void centerText(const char* text, int x, int y) {
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(x - tw / 2, y);
        display.print(text);
    }
};

#endif // FEATURE_WEATHER
#endif // SUMI_PLUGIN_WEATHER_H
