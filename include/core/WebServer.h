#ifndef SUMI_WEB_SERVER_H
#define SUMI_WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "config.h"

#if FEATURE_WEBSERVER

// Global flags to signal main loop
extern volatile bool g_settingsDeployed;
extern volatile bool g_wifiJustConnected;

// =============================================================================
// Minimal Web Server - For ESP32-C3 Setup Portal
// =============================================================================
class SumiWebServer {
public:
    SumiWebServer();
    
    void begin();
    void stop();
    bool isRunning() const { return _running; }

private:
    AsyncWebServer _server;
    bool _running;
    
    // Route setup
    void setupRoutes();
    void setupCaptivePortalRoutes();  // Handles Windows/Apple/Android/Firefox portal detection
    void setupAPIRoutes();
    void setupPageRoutes();
    
    // Status & Settings
    void handleStatus(AsyncWebServerRequest* request);
    void handleGetSettings(AsyncWebServerRequest* request);
    void handleSaveSettings(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleSetupComplete(AsyncWebServerRequest* request);
    
    // WiFi
    void handleWiFiScan(AsyncWebServerRequest* request);
    void handleWiFiConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleWiFiDisconnect(AsyncWebServerRequest* request);
    
    // Files
    void handleFileList(AsyncWebServerRequest* request);
    void handleFileUpload(AsyncWebServerRequest* request, const String& filename, 
                          size_t index, uint8_t* data, size_t len, bool final);
    
    // System
    void handleReboot(AsyncWebServerRequest* request);
    
    // Weather
    void handleWeatherLocation(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleWeatherUnit(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTimezone(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    
    // Helpers
    void sendError(AsyncWebServerRequest* request, const char* message, int code = 400);
    void sendSuccess(AsyncWebServerRequest* request, const char* message = "ok");
    void addCORSHeaders(AsyncWebServerResponse* response);
};

extern SumiWebServer webServer;

#endif // FEATURE_WEBSERVER
#endif // SUMI_WEB_SERVER_H
