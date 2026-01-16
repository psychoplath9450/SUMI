#ifndef SUMI_WIFI_MANAGER_H
#define SUMI_WIFI_MANAGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// WiFi States
// =============================================================================
enum class WiFiState {
    IDLE,
    AP_MODE,        // Setup mode - device creates hotspot
    CONNECTING,     // Attempting to connect to saved network
    CONNECTED,      // Connected to WiFi network
    FAILED          // Connection failed
};

#if !FEATURE_WIFI
// =============================================================================
// Stub WiFi Manager when WiFi is disabled (saves ~25KB RAM)
// =============================================================================
class WiFiManager {
public:
    void begin() {}
    bool startAP() { return false; }
    void stopAP() {}
    bool connect(const char*, const char*) { return false; }
    bool connectSaved() { return false; }
    void disconnect() {}
    WiFiState getState() const { return WiFiState::IDLE; }
    bool isConnected() const { return false; }
    bool isAPMode() const { return false; }
    String getIP() const { return "0.0.0.0"; }
    String getSSID() const { return ""; }
    int getRSSI() const { return 0; }
    String getAPName() const { return ""; }
    String getSetupURL() const { return ""; }
    String getPortalURL() const { return ""; }
    bool startMDNS(const char* = "sumi") { return false; }
    void stopMDNS() {}
    bool isMDNSRunning() const { return false; }
    bool hasCredentials() const { return false; }
    bool saveCredentials(const char*, const char*) { return false; }
    bool clearCredentials() { return false; }
    int scanNetworks() { return 0; }
    String getScannedSSID(int) { return ""; }
    int getScannedRSSI(int) { return 0; }
    bool isScannedSecure(int) { return false; }
    int getScannedChannel(int) { return 0; }
    int getScanResultCount() const { return 0; }
    void update() {}
    bool connectBriefly(int = 10000) { return false; }
    void disconnectBriefly() {}
    bool syncTime() { return false; }
    bool syncTimeFast() { return false; }
    bool isTimeSynced() const { return false; }
    void setTimeSynced(bool) {}
};
extern WiFiManager wifiManager;

#else // FEATURE_WIFI enabled

#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

// =============================================================================
// WiFi Manager Class
// =============================================================================
class WiFiManager {
public:
    WiFiManager();
    
    // Initialization
    void begin();
    
    // Mode control
    bool startAP();
    void stopAP();
    bool connect(const char* ssid, const char* password);
    bool connectSaved();
    void disconnect();
    
    // Status
    WiFiState getState() const { return _state; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    bool isAPMode() const { return _state == WiFiState::AP_MODE; }
    String getIP() const;
    String getAPIP() const { return "192.168.4.1"; }  // Default AP IP
    String getSSID() const;
    int getRSSI() const;
    String getAPName() const { return _apName; }
    
    // Portal URLs for QR codes
    String getSetupURL() const;
    String getPortalURL() const;
    
    // mDNS
    bool startMDNS(const char* hostname = "sumi");
    void stopMDNS();
    bool isMDNSRunning() const { return _mdnsRunning; }
    
    // Credentials management
    bool hasCredentials() const;
    String getSavedSSID() const;  // Get stored SSID (even if not connected)
    bool saveCredentials(const char* ssid, const char* password);
    bool clearCredentials();
    
    // Network scanning
    int scanNetworks();
    String getScannedSSID(int index);
    int getScannedRSSI(int index);
    bool isScannedSecure(int index);
    int getScannedChannel(int index);
    int getScanResultCount() const { return _scanResultCount; }
    
    // Update loop - MUST be called in loop() for captive portal to work
    void update();
    
    // Reconnection
    void attemptReconnect();
    bool isReconnecting() const { return _reconnecting; }
    
    // Brief WiFi connect for background tasks (time sync, weather)
    // Connects, performs task, then disconnects
    bool connectBriefly(int timeoutMs = 10000);
    void disconnectBriefly();
    
    // NTP Time Sync
    bool syncTime();                      // Connect briefly and sync time via NTP
    bool syncTimeFast();                  // Fast sync for wake (shorter timeout, no TZ lookup)
    bool isTimeSynced() const { return _timeSynced; }
    void setTimeSynced(bool synced) { _timeSynced = synced; }

private:
    WiFiState _state;
    String _apName;
    String _savedSSID;
    String _savedPassword;
    unsigned long _connectStartTime;
    int _scanResultCount;
    bool _mdnsRunning;
    DNSServer _dnsServer;
    bool _dnsRunning;
    
    // Reconnection
    bool _reconnecting;
    unsigned long _lastReconnectAttempt;
    
    // Time sync tracking
    bool _timeSynced;
    
    void generateAPName();
    bool loadCredentials();
    int32_t fetchTimezoneFromIP();  // Auto-detect timezone from IP geolocation
};

extern WiFiManager wifiManager;

#endif // FEATURE_WIFI
#endif // SUMI_WIFI_MANAGER_H
