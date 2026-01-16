/**
 * @file Weather.h
 * @brief Weather display widget for Sumi e-reader
 * @version 2.1.26
 * 
 * Features:
 * - 7-day forecast with LEFT/RIGHT navigation
 * - Weather icons for different conditions
 * - Manual zip code entry for custom locations
 * - Auto IP-based geolocation
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
#include "core/PluginHelpers.h"
#include "core/SettingsManager.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class WeatherApp {
public:
    static const int FORECAST_DAYS = 7;
    
    enum State {
        STATE_WEATHER,
        STATE_ZIP_ENTRY,
        STATE_LOADING
    };
    
    // For partial refresh support
    bool needsFullRedraw;
    
    WeatherApp();
    
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();
    void drawPartial();
    bool update();
    
private:
    // Forecast data for each day
    struct DayForecast {
        float tempHigh;
        float tempLow;
        int weatherCode;
        char date[12];  // "Mon 1/15"
    };
    
    DayForecast _forecast[FORECAST_DAYS];
    float _currentTemp;
    int _currentHumidity;
    float _currentWind;
    int _currentWeatherCode;
    
    char _location[64];
    bool _hasData;
    bool _locationSet;
    bool _useCelsius;
    unsigned long _lastUpdate;
    
    int _selectedDay;  // 0 = today, 1 = tomorrow, etc.
    State _state;
    
    // Zip code entry
    char _zipCode[6];
    int _zipLen;
    int _zipCursor;
    
    int _screenW, _screenH;
    bool _landscape;
    
    void reset();
    void fetchLocation();
    void fetchLocationFromZip(const char* zip);
    void fetchWeather();
    void descriptionFromCode(int code, char* buf, int bufLen);
    
    // Drawing helpers
    void drawWeatherView();
    void drawZipEntry();
    void drawZipDigitsOnly(int startX, int startY, int boxW, int boxH, int gap);
    void drawWeatherIcon(int code, int x, int y, int size);
    void drawSunIcon(int x, int y, int r);
    void drawCloudIcon(int x, int y, int w, int h);
    void drawRainIcon(int x, int y, int w, int h);
    void drawSnowIcon(int x, int y, int w, int h);
    void drawStormIcon(int x, int y, int w, int h);
    void drawFogIcon(int x, int y, int w, int h);
    void drawDaySelector();
};

extern WeatherApp weatherApp;

#endif // FEATURE_WEATHER
#endif // SUMI_PLUGIN_WEATHER_H
