/**
 * @file Weather.cpp
 * @brief Weather widget implementation - Improved UI
 * @version 2.1.28
 * 
 * Features:
 * - 7-day forecast with day navigation
 * - Large weather icons that fill the screen
 * - Big temperature display
 * - Comprehensive weather data
 */

#include "plugins/Weather.h"
#include "core/WiFiManager.h"
#include "core/HomeScreen.h"  // For saveWeatherCache

#if FEATURE_WEATHER

WeatherApp weatherApp;

WeatherApp::WeatherApp() { 
    reset(); 
    needsFullRedraw = true;
}

void WeatherApp::reset() {
    _hasData = false;
    _locationSet = false;
    _useCelsius = false;
    _selectedDay = 0;
    _currentTemp = 0;
    _currentHumidity = 0;
    _currentWind = 0;
    _currentWeatherCode = 0;
    memset(_location, 0, sizeof(_location));
    memset(_forecast, 0, sizeof(_forecast));
}

void WeatherApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _lastUpdate = 0;
    _hasData = false;
    _locationSet = false;
    _selectedDay = 0;
    _state = STATE_WEATHER;
    _zipLen = 0;
    _zipCursor = 0;
    needsFullRedraw = true;
    memset(_zipCode, 0, sizeof(_zipCode));
    
    // Check for saved location
    if (settingsManager.weather.latitude != 0.0f || 
        settingsManager.weather.longitude != 0.0f) {
        _locationSet = true;
        strncpy(_location, settingsManager.weather.location, sizeof(_location) - 1);
    }
    
    // Fetch weather if we have credentials and location
    if (wifiManager.hasCredentials()) {
        if (!_locationSet) {
            fetchLocation();
        }
        if (_locationSet) {
            fetchWeather();
        }
        wifiManager.disconnectBriefly();
    }
}

bool WeatherApp::handleInput(Button btn) {
    if (_state == STATE_ZIP_ENTRY) {
        switch (btn) {
            case BTN_UP:
                if (_zipLen > 0) {
                    char& c = _zipCode[_zipLen - 1];
                    if (c < '9') c++; else c = '0';
                }
                needsFullRedraw = false;  // Just update digits
                return true;
            case BTN_DOWN:
                if (_zipLen > 0) {
                    char& c = _zipCode[_zipLen - 1];
                    if (c > '0') c--; else c = '9';
                }
                needsFullRedraw = false;  // Just update digits
                return true;
            case BTN_RIGHT:
                if (_zipLen < 5) {
                    _zipCode[_zipLen++] = '0';
                    _zipCode[_zipLen] = '\0';
                }
                needsFullRedraw = false;  // Just update digits
                return true;
            case BTN_LEFT:
                if (_zipLen > 0) {
                    _zipLen--;
                    _zipCode[_zipLen] = '\0';
                }
                needsFullRedraw = false;  // Just update digits
                return true;
            case BTN_CONFIRM:
                if (_zipLen == 5) {
                    needsFullRedraw = true;
                    fetchLocationFromZip(_zipCode);
                    if (_locationSet) {
                        fetchWeather();
                        wifiManager.disconnectBriefly();
                    }
                    _state = STATE_WEATHER;
                }
                return true;
            case BTN_BACK:
                _state = STATE_WEATHER;
                needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    
    // Weather view always needs full redraw
    needsFullRedraw = true;
    
    switch (btn) {
        case BTN_LEFT:
            if (_selectedDay > 0) _selectedDay--;
            return true;
        case BTN_RIGHT:
            if (_selectedDay < FORECAST_DAYS - 1) _selectedDay++;
            return true;
        case BTN_UP:
            _state = STATE_ZIP_ENTRY;
            _zipLen = 0;
            memset(_zipCode, 0, sizeof(_zipCode));
            return true;
        case BTN_DOWN:
            _useCelsius = !_useCelsius;
            settingsManager.weather.celsius = _useCelsius;
            settingsManager.markDirty();
            if (_locationSet) {
                fetchWeather();
                wifiManager.disconnectBriefly();
            }
            return true;
        case BTN_CONFIRM:
            if (wifiManager.hasCredentials()) {
                if (!_locationSet) fetchLocation();
                if (_locationSet) fetchWeather();
                wifiManager.disconnectBriefly();
            }
            return true;
        case BTN_BACK:
            return false;
        default:
            return true;
    }
}

bool WeatherApp::update() {
    return false;
}

void WeatherApp::draw() {
    // Full screen refresh
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        
        if (_state == STATE_ZIP_ENTRY) {
            drawZipEntry();
        } else {
            drawWeatherView();
        }
    } while (display.nextPage());
}

void WeatherApp::drawPartial() {
    // Partial refresh for ZIP digit entry only
    if (_state != STATE_ZIP_ENTRY) {
        // Fall back to full draw for other states
        draw();
        return;
    }
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    int boxW = 55;
    int boxH = 70;
    int gap = 12;
    int totalW = 5 * boxW + 4 * gap;
    int startX = centerX - totalW / 2;
    int startY = centerY - 35;
    
    // Set partial window for just the digits area
    display.setPartialWindow(startX - 10, startY - 10, totalW + 20, boxH + 30);
    display.firstPage();
    do {
        display.fillRect(startX - 10, startY - 10, totalW + 20, boxH + 30, GxEPD_WHITE);
        drawZipDigitsOnly(startX, startY, boxW, boxH, gap);
    } while (display.nextPage());
    
    // Reset to full window
    display.setFullWindow();
}

// Draw just the ZIP digit boxes (for partial refresh)
void WeatherApp::drawZipDigitsOnly(int startX, int startY, int boxW, int boxH, int gap) {
    int16_t tx, ty; uint16_t tw, th;
    
    for (int i = 0; i < 5; i++) {
        int x = startX + i * (boxW + gap);
        int y = startY;
        
        display.drawRect(x, y, boxW, boxH, GxEPD_BLACK);
        if (i == _zipLen - 1 && _zipLen > 0) {
            display.drawRect(x + 1, y + 1, boxW - 2, boxH - 2, GxEPD_BLACK);
        }
        
        if (i < _zipLen) {
            display.setFont(&FreeSansBold12pt7b);
            display.setTextSize(2);
            display.setTextColor(GxEPD_BLACK);
            char digit[2] = {_zipCode[i], '\0'};
            display.getTextBounds(digit, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x + (boxW - tw) / 2, y + boxH - 15);
            display.print(digit);
            display.setTextSize(1);
        }
        
        // Cursor indicator under active digit
        if (i == _zipLen - 1 && _zipLen > 0) {
            display.fillRect(x + 5, y + boxH + 8, boxW - 10, 4, GxEPD_BLACK);
        }
    }
}

void WeatherApp::drawWeatherView() {
    PluginUI::drawHeader("Weather", _screenW);
    
    // No WiFi
    if (!wifiManager.hasCredentials()) {
        display.setFont(&FreeSansBold12pt7b);
        int16_t tx, ty; uint16_t tw, th;
        const char* msg1 = "WiFi not configured";
        display.getTextBounds(msg1, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((_screenW - tw) / 2, _screenH / 2 - 30);
        display.print(msg1);
        
        display.setFont(&FreeSans9pt7b);
        const char* msg2 = "Set up WiFi in Settings first";
        display.getTextBounds(msg2, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((_screenW - tw) / 2, _screenH / 2 + 20);
        display.print(msg2);
        PluginUI::drawFooter("", "BACK:Exit", _screenW, _screenH);
        return;
    }
    
    // No location
    if (!_locationSet) {
        display.setFont(&FreeSansBold12pt7b);
        int16_t tx, ty; uint16_t tw, th;
        const char* msg1 = "Location not set";
        display.getTextBounds(msg1, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((_screenW - tw) / 2, _screenH / 2 - 60);
        display.print(msg1);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(40, _screenH / 2 - 10);
        display.print("OK: Auto-detect from IP");
        display.setCursor(40, _screenH / 2 + 25);
        display.print("UP: Enter ZIP code manually");
        PluginUI::drawFooter("", "OK:Detect UP:ZIP", _screenW, _screenH);
        return;
    }
    
    // No data
    if (!_hasData) {
        display.setFont(&FreeSansBold12pt7b);
        int16_t tx, ty; uint16_t tw, th;
        const char* msg = "Press OK to fetch weather";
        display.getTextBounds(msg, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((_screenW - tw) / 2, _screenH / 2);
        display.print(msg);
        PluginUI::drawFooter(_location, "OK:Fetch UP:ZIP", _screenW, _screenH);
        return;
    }
    
    // =========================================================================
    // MAIN WEATHER DISPLAY
    // =========================================================================
    
    // Draw 7-day selector bar at top
    drawDaySelector();
    
    int startY = PLUGIN_HEADER_H + 75;  // Below day selector
    int centerX = _screenW / 2;
    
    // Get data for selected day
    DayForecast& day = _forecast[_selectedDay];
    int weatherCode = (_selectedDay == 0) ? _currentWeatherCode : day.weatherCode;
    int tempMain = (_selectedDay == 0) ? (int)_currentTemp : (int)day.tempHigh;
    
    // --- LARGE WEATHER ICON ---
    int iconSize = _landscape ? 120 : 160;  // Big icons!
    int iconX = centerX - iconSize / 2;
    int iconY = startY + 10;
    drawWeatherIcon(weatherCode, iconX, iconY, iconSize);
    
    // --- BIG TEMPERATURE ---
    int tempY = iconY + iconSize + 50;
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%d°%s", tempMain, _useCelsius ? "C" : "F");
    
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(3);  // 3x size for big temp
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(tempStr, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, tempY);
    display.print(tempStr);
    display.setTextSize(1);
    
    // --- WEATHER DESCRIPTION ---
    char desc[32];
    descriptionFromCode(weatherCode, desc, sizeof(desc));
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(desc, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, tempY + 50);
    display.print(desc);
    
    // --- HIGH / LOW ---
    display.setFont(&FreeSans9pt7b);
    char hiloStr[32];
    snprintf(hiloStr, sizeof(hiloStr), "High: %d°   Low: %d°", 
             (int)day.tempHigh, (int)day.tempLow);
    display.getTextBounds(hiloStr, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, tempY + 90);
    display.print(hiloStr);
    
    // --- DETAILS SECTION (only for today) ---
    if (_selectedDay == 0) {
        int detailY = tempY + 130;
        int col1X = 40;
        int col2X = centerX + 20;
        
        display.setFont(&FreeSans9pt7b);
        
        // Column 1
        display.setCursor(col1X, detailY);
        display.printf("Humidity: %d%%", _currentHumidity);
        
        display.setCursor(col1X, detailY + 35);
        display.printf("Wind: %d mph", (int)_currentWind);
        
        // Column 2 - Feels like
        int feelsLike = (int)_currentTemp;
        if (_currentWind > 5 && _currentTemp < 50) {
            feelsLike = (int)(_currentTemp - (_currentWind * 0.3));
        } else if (_currentHumidity > 70 && _currentTemp > 75) {
            feelsLike = (int)(_currentTemp + (_currentHumidity - 70) * 0.1);
        }
        display.setCursor(col2X, detailY);
        display.printf("Feels: %d°", feelsLike);
        
        display.setCursor(col2X, detailY + 35);
        display.print(day.date);
    } else {
        // For future days, show the full date
        int detailY = tempY + 130;
        display.setFont(&FreeSansBold9pt7b);
        display.getTextBounds(day.date, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw/2, detailY);
        display.print(day.date);
    }
    
    // --- LOCATION ---
    display.setFont(&FreeSans9pt7b);
    display.setCursor(PLUGIN_MARGIN, _screenH - PLUGIN_FOOTER_H - 30);
    display.print(_location);
    
    PluginUI::drawFooter("L/R:Days UP:ZIP DN:Units", "OK:Refresh", _screenW, _screenH);
}

void WeatherApp::drawDaySelector() {
    int y = PLUGIN_HEADER_H + 5;
    int dayW = _screenW / FORECAST_DAYS;
    int boxH = 60;  // Taller boxes
    
    // Get current day of week
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    int todayWday = timeinfo ? timeinfo->tm_wday : 0;
    
    // Day abbreviations: Su M Tu W Th F Sa
    const char* dayAbbr[] = {"Su", "M", "Tu", "W", "Th", "F", "Sa"};
    
    for (int i = 0; i < FORECAST_DAYS; i++) {
        int x = i * dayW;
        bool selected = (i == _selectedDay);
        
        // Draw box
        if (selected) {
            display.fillRect(x + 2, y, dayW - 4, boxH, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRect(x + 2, y, dayW - 4, boxH, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        
        // Day label
        const char* label;
        if (i == 0) {
            label = "Today";
        } else {
            int wday = (todayWday + i) % 7;
            label = dayAbbr[wday];
        }
        
        display.setFont(&FreeSans9pt7b);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(x + (dayW - tw) / 2, y + 22);
        display.print(label);
        
        // Temperature underneath
        if (_hasData) {
            char t[8];
            snprintf(t, sizeof(t), "%d°", (int)_forecast[i].tempHigh);
            display.setFont(&FreeSansBold9pt7b);
            display.getTextBounds(t, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x + (dayW - tw) / 2, y + 48);
            display.print(t);
        }
    }
    
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);
}

void WeatherApp::drawZipEntry() {
    PluginUI::drawHeader("Enter ZIP Code", _screenW);
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    
    display.setFont(&FreeSansBold12pt7b);
    int16_t tx, ty; uint16_t tw, th;
    const char* title = "Enter 5-digit ZIP code:";
    display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, centerY - 100);
    display.print(title);
    
    // ZIP code boxes
    int boxW = 55;
    int boxH = 70;
    int gap = 12;
    int totalW = 5 * boxW + 4 * gap;
    int startX = centerX - totalW / 2;
    
    for (int i = 0; i < 5; i++) {
        int x = startX + i * (boxW + gap);
        int y = centerY - 35;
        
        display.drawRect(x, y, boxW, boxH, GxEPD_BLACK);
        if (i == _zipLen - 1 && _zipLen > 0) {
            display.drawRect(x + 1, y + 1, boxW - 2, boxH - 2, GxEPD_BLACK);
        }
        
        if (i < _zipLen) {
            display.setFont(&FreeSansBold12pt7b);
            display.setTextSize(2);
            char digit[2] = {_zipCode[i], '\0'};
            display.getTextBounds(digit, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x + (boxW - tw) / 2, y + boxH - 15);
            display.print(digit);
            display.setTextSize(1);
        }
        
        // Cursor indicator
        if (i == _zipLen - 1 && _zipLen > 0) {
            display.fillRect(x + 5, y + boxH + 8, boxW - 10, 4, GxEPD_BLACK);
        }
    }
    
    // Help text
    display.setFont(&FreeSans9pt7b);
    int helpY = centerY + 70;
    
    const char* h1 = "UP/DOWN: Change digit";
    display.getTextBounds(h1, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, helpY);
    display.print(h1);
    
    const char* h2 = "LEFT: Delete   RIGHT: Add";
    display.getTextBounds(h2, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, helpY + 30);
    display.print(h2);
    
    const char* h3 = "OK: Search   BACK: Cancel";
    display.getTextBounds(h3, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, helpY + 60);
    display.print(h3);
    
    PluginUI::drawFooter("", "", _screenW, _screenH);
}

// =============================================================================
// WEATHER ICONS - Larger and more detailed
// =============================================================================

void WeatherApp::drawWeatherIcon(int code, int x, int y, int size) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    
    if (code == 0) {
        // Clear sky - big sun
        drawSunIcon(cx, cy, size * 2 / 5);
    } else if (code <= 3) {
        // Partly cloudy - sun + cloud
        drawSunIcon(cx - size/6, cy - size/6, size / 4);
        drawCloudIcon(x + size/6, cy - size/8, size * 2/3, size / 2);
    } else if (code <= 49) {
        // Fog
        drawFogIcon(x, y, size, size);
    } else if (code <= 69) {
        // Rain
        drawCloudIcon(x + size/8, y + size/8, size * 3/4, size / 2);
        drawRainIcon(x + size/8, y + size/2, size * 3/4, size / 2);
    } else if (code <= 79) {
        // Snow
        drawCloudIcon(x + size/8, y + size/8, size * 3/4, size / 2);
        drawSnowIcon(x + size/8, y + size/2, size * 3/4, size / 2);
    } else if (code <= 94) {
        // Showers
        drawCloudIcon(x + size/8, y + size/8, size * 3/4, size / 2);
        drawRainIcon(x + size/8, y + size/2, size * 3/4, size / 2);
    } else {
        // Thunderstorm
        drawStormIcon(x, y, size, size);
    }
}

void WeatherApp::drawSunIcon(int cx, int cy, int r) {
    // Filled sun circle
    display.fillCircle(cx, cy, r, GxEPD_BLACK);
    display.fillCircle(cx, cy, r - 3, GxEPD_WHITE);
    display.drawCircle(cx, cy, r, GxEPD_BLACK);
    display.drawCircle(cx, cy, r - 1, GxEPD_BLACK);
    
    // Rays - thicker and longer
    int rayLen = r * 2 / 3;
    int rayGap = 6;
    for (int i = 0; i < 8; i++) {
        float angle = i * PI / 4;
        int x1 = cx + cos(angle) * (r + rayGap);
        int y1 = cy + sin(angle) * (r + rayGap);
        int x2 = cx + cos(angle) * (r + rayGap + rayLen);
        int y2 = cy + sin(angle) * (r + rayGap + rayLen);
        display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
        display.drawLine(x1+1, y1, x2+1, y2, GxEPD_BLACK);
        display.drawLine(x1, y1+1, x2, y2+1, GxEPD_BLACK);
    }
}

void WeatherApp::drawCloudIcon(int x, int y, int w, int h) {
    int r1 = h * 2 / 5;
    int r2 = h / 3;
    int r3 = h / 4;
    
    int baseY = y + h - r2;
    
    // Multiple overlapping circles for fluffy cloud
    display.fillCircle(x + r1, baseY, r1, GxEPD_WHITE);
    display.drawCircle(x + r1, baseY, r1, GxEPD_BLACK);
    
    display.fillCircle(x + w/2, baseY - r1/2, r1 + 5, GxEPD_WHITE);
    display.drawCircle(x + w/2, baseY - r1/2, r1 + 5, GxEPD_BLACK);
    
    display.fillCircle(x + w - r2, baseY, r2, GxEPD_WHITE);
    display.drawCircle(x + w - r2, baseY, r2, GxEPD_BLACK);
    
    display.fillCircle(x + w - r2 - r3, baseY - r2 + 5, r3, GxEPD_WHITE);
    display.drawCircle(x + w - r2 - r3, baseY - r2 + 5, r3, GxEPD_BLACK);
    
    // Fill base
    display.fillRect(x + r1/2, baseY, w - r1/2 - r2/2, r2, GxEPD_WHITE);
    display.drawLine(x + r1/2, y + h, x + w - r2/2, y + h, GxEPD_BLACK);
}

void WeatherApp::drawRainIcon(int x, int y, int w, int h) {
    int drops = 6;
    int dropLen = h * 2 / 3;
    
    for (int i = 0; i < drops; i++) {
        int dx = x + 15 + i * (w - 30) / (drops - 1);
        int dy = y + (i % 2) * 12;
        
        // Thicker rain lines
        for (int t = -1; t <= 1; t++) {
            display.drawLine(dx + t, dy, dx - 8 + t, dy + dropLen, GxEPD_BLACK);
        }
    }
}

void WeatherApp::drawSnowIcon(int x, int y, int w, int h) {
    int flakes = 5;
    int flakeR = 8;
    
    for (int i = 0; i < flakes; i++) {
        int cx = x + 20 + i * (w - 40) / (flakes - 1);
        int cy = y + 15 + (i % 2) * 15;
        
        // 6-pointed snowflake
        for (int j = 0; j < 6; j++) {
            float angle = j * PI / 3;
            int x2 = cx + cos(angle) * flakeR;
            int y2 = cy + sin(angle) * flakeR;
            display.drawLine(cx, cy, x2, y2, GxEPD_BLACK);
            display.drawLine(cx+1, cy, x2+1, y2, GxEPD_BLACK);
        }
        display.fillCircle(cx, cy, 2, GxEPD_BLACK);
    }
}

void WeatherApp::drawStormIcon(int x, int y, int w, int h) {
    // Dark cloud
    drawCloudIcon(x + w/8, y, w * 3/4, h / 2);
    
    // Lightning bolt
    int boltX = x + w / 2;
    int boltY = y + h / 2 - 10;
    int boltH = h / 2;
    
    // Draw thick lightning bolt
    for (int t = -2; t <= 2; t++) {
        display.drawLine(boltX + t, boltY, boltX - 15 + t, boltY + boltH/3, GxEPD_BLACK);
        display.drawLine(boltX - 15 + t, boltY + boltH/3, boltX + 5 + t, boltY + boltH/3, GxEPD_BLACK);
        display.drawLine(boltX + 5 + t, boltY + boltH/3, boltX - 10 + t, boltY + boltH, GxEPD_BLACK);
    }
}

void WeatherApp::drawFogIcon(int x, int y, int w, int h) {
    int lineSpacing = h / 6;
    int startY = y + h / 4;
    
    for (int i = 0; i < 4; i++) {
        int ly = startY + i * lineSpacing;
        int thick = (i == 1 || i == 2) ? 3 : 2;
        
        for (int t = 0; t < thick; t++) {
            for (int px = x + 20; px < x + w - 20; px += 2) {
                int wave = sin((px - x) * 0.05) * 3;
                display.drawPixel(px, ly + wave + t, GxEPD_BLACK);
                display.drawPixel(px + 1, ly + wave + t, GxEPD_BLACK);
            }
        }
    }
}

// =============================================================================
// NETWORK FUNCTIONS
// =============================================================================

void WeatherApp::fetchLocation() {
    Serial.println("[WEATHER] Fetching location from IP...");
    
    bool wasConnected = WiFi.status() == WL_CONNECTED;
    if (!wasConnected) {
        if (!wifiManager.connectBriefly(10000)) {
            Serial.println("[WEATHER] Could not connect to WiFi");
            return;
        }
    }
    
    WiFiClient client;
    if (!client.connect("ip-api.com", 80)) {
        Serial.println("[WEATHER] Could not connect to ip-api.com");
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
    
    if (strlen(country) > 0 && strlen(city) + strlen(country) < 60) {
        snprintf(settingsManager.weather.location, sizeof(settingsManager.weather.location),
                 "%s, %s", city, country);
    } else {
        strncpy(settingsManager.weather.location, city, sizeof(settingsManager.weather.location) - 1);
    }
    
    settingsManager.markDirty();
    
    _locationSet = true;
    strncpy(_location, settingsManager.weather.location, sizeof(_location) - 1);
    
    Serial.printf("[WEATHER] Location: %s (%.4f, %.4f)\n", _location, lat, lon);
    
    configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov");
}

void WeatherApp::fetchLocationFromZip(const char* zip) {
    Serial.printf("[WEATHER] Looking up ZIP: %s\n", zip);
    
    bool wasConnected = WiFi.status() == WL_CONNECTED;
    if (!wasConnected) {
        if (!wifiManager.connectBriefly(10000)) {
            Serial.println("[WEATHER] Could not connect to WiFi");
            return;
        }
    }
    
    WiFiClient client;
    if (!client.connect("api.zippopotam.us", 80)) {
        Serial.println("[WEATHER] ZIP API connection failed");
        return;
    }
    
    char request[128];
    snprintf(request, sizeof(request), "GET /us/%s HTTP/1.1\r\n", zip);
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
    if (deserializeJson(doc, payload)) {
        Serial.println("[WEATHER] ZIP JSON parse failed");
        return;
    }
    
    JsonArray places = doc["places"];
    if (places.size() == 0) {
        Serial.println("[WEATHER] ZIP not found");
        return;
    }
    
    JsonObject place = places[0];
    const char* city = place["place name"] | "Unknown";
    const char* state = place["state abbreviation"] | "";
    float lat = atof(place["latitude"] | "0");
    float lon = atof(place["longitude"] | "0");
    
    if (lat == 0.0f && lon == 0.0f) return;
    
    settingsManager.weather.latitude = lat;
    settingsManager.weather.longitude = lon;
    
    snprintf(settingsManager.weather.location, sizeof(settingsManager.weather.location),
             "%s, %s %s", city, state, zip);
    strncpy(settingsManager.weather.zipCode, zip, sizeof(settingsManager.weather.zipCode) - 1);
    
    settingsManager.markDirty();
    settingsManager.save();  // Save immediately so portal can see it
    
    _locationSet = true;
    strncpy(_location, settingsManager.weather.location, sizeof(_location) - 1);
    
    Serial.printf("[WEATHER] ZIP location: %s (%.4f, %.4f)\n", _location, lat, lon);
}

void WeatherApp::fetchWeather() {
    if (!_locationSet) return;
    
    Serial.println("[WEATHER] Fetching 7-day forecast...");
    
    bool wasConnected = WiFi.status() == WL_CONNECTED;
    if (!wasConnected) {
        if (!wifiManager.connectBriefly(10000)) return;
    }
    
    if (!wifiManager.isTimeSynced()) {
        configTime(settingsManager.weather.timezoneOffset, 0, "pool.ntp.org");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 3000)) {
            wifiManager.setTimeSynced(true);
        }
    }
    
    float lat = settingsManager.weather.latitude;
    float lon = settingsManager.weather.longitude;
    _useCelsius = settingsManager.weather.celsius;
    
    WiFiClient client;
    if (!client.connect("api.open-meteo.com", 80)) return;
    
    char request[384];
    snprintf(request, sizeof(request),
        "GET /v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
        "&daily=temperature_2m_max,temperature_2m_min,weather_code"
        "&temperature_unit=%s&wind_speed_unit=mph"
        "&forecast_days=7&timezone=auto HTTP/1.0\r\n",
        lat, lon, _useCelsius ? "celsius" : "fahrenheit");
    
    client.print(request);
    client.print("Host: api.open-meteo.com\r\n");
    client.print("Connection: close\r\n\r\n");
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 10000) {
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
    
    // Current conditions
    JsonObject current = doc["current"];
    _currentTemp = current["temperature_2m"] | 0.0f;
    _currentHumidity = current["relative_humidity_2m"] | 0;
    _currentWind = current["wind_speed_10m"] | 0.0f;
    _currentWeatherCode = current["weather_code"] | 0;
    
    // Daily forecast
    JsonObject daily = doc["daily"];
    JsonArray dates = daily["time"];
    JsonArray maxTemps = daily["temperature_2m_max"];
    JsonArray minTemps = daily["temperature_2m_min"];
    JsonArray codes = daily["weather_code"];
    
    for (int i = 0; i < FORECAST_DAYS && i < (int)dates.size(); i++) {
        _forecast[i].tempHigh = maxTemps[i] | 0.0f;
        _forecast[i].tempLow = minTemps[i] | 0.0f;
        _forecast[i].weatherCode = codes[i] | 0;
        
        const char* dateStr = dates[i] | "0000-00-00";
        int year, month, day;
        sscanf(dateStr, "%d-%d-%d", &year, &month, &day);
        
        struct tm tm = {0};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        mktime(&tm);
        
        const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        snprintf(_forecast[i].date, sizeof(_forecast[i].date), "%s %d/%d", 
                 dayNames[tm.tm_wday], month, day);
    }
    
    _hasData = true;
    _lastUpdate = millis();
    
    // Save weather to cache for home screen widget
    saveWeatherCache(_currentTemp, _currentWeatherCode, _currentHumidity, 
                     _currentWind, _location, _useCelsius);
    
    Serial.printf("[WEATHER] Got %d day forecast, current: %d, %.1f\n", 
                  FORECAST_DAYS, _currentWeatherCode, _currentTemp);
}

void WeatherApp::descriptionFromCode(int code, char* buf, int bufLen) {
    const char* desc;
    if (code == 0) desc = "Clear sky";
    else if (code <= 3) desc = "Partly cloudy";
    else if (code <= 49) desc = "Foggy";
    else if (code <= 59) desc = "Drizzle";
    else if (code <= 69) desc = "Rain";
    else if (code <= 79) desc = "Snow";
    else if (code <= 84) desc = "Rain showers";
    else if (code <= 94) desc = "Snow showers";
    else desc = "Thunderstorm";
    
    strncpy(buf, desc, bufLen - 1);
    buf[bufLen - 1] = '\0';
}

#endif // FEATURE_WEATHER
