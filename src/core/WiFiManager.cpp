#include "config.h"
#if FEATURE_WIFI

#include "core/WiFiManager.h"
#include "core/SettingsManager.h"
#include <Preferences.h>
#include <SD.h>


WiFiManager wifiManager;

static const char* NVS_NAMESPACE = "sumi_wifi";
static const char* NVS_KEY_SSID = "ssid";
static const char* NVS_KEY_PASS = "password";
static const byte DNS_PORT = 53;

WiFiManager::WiFiManager() 
    : _state(WiFiState::IDLE)
    , _connectStartTime(0)
    , _scanResultCount(0)
    , _mdnsRunning(false)
    , _dnsRunning(false)
    , _reconnecting(false)
    , _lastReconnectAttempt(0)
    , _timeSynced(false)
{
    generateAPName();
}

void WiFiManager::begin() {
    Serial.println("[WIFI] Initializing...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    if (loadCredentials()) {
        Serial.printf("[WIFI] Found saved network: %s\n", _savedSSID.c_str());
    }
    Serial.println("[WIFI] Ready");
}

bool WiFiManager::startAP() {
    Serial.printf("[WIFI] Starting AP: %s\n", _apName.c_str());
    
    // Use AP+STA mode for captive portal to work properly
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    delay(100);
    
    // Configure AP with static IP
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    
    // Start AP with NO PASSWORD (open network for easy connection)
    // Channel 6, not hidden, max 4 connections
    bool success = WiFi.softAP(_apName.c_str(), NULL, 6, 0, 4);
    
    if (success) {
        _state = WiFiState::AP_MODE;
        Serial.printf("[WIFI] AP started. IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("[WIFI] Setup URL: %s\n", getSetupURL().c_str());
        
        // Start DNS server for captive portal - redirects ALL DNS to us
        // Required for automatic portal detection on phones/computers
        _dnsServer.start(DNS_PORT, "*", local_IP);
        _dnsRunning = true;
        Serial.println("[WIFI] DNS server started (captive portal)");
        
        startMDNS("sumi");
        return true;
    }
    
    _state = WiFiState::FAILED;
    return false;
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    Serial.printf("[WIFI] Connecting to: %s\n", ssid);
    
    // Keep AP running during connection attempt so portal stays accessible
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    WiFi.begin(ssid, password);
    
    _state = WiFiState::CONNECTING;
    _connectStartTime = millis();
    
    while (WiFi.status() != WL_CONNECTED && (millis() - _connectStartTime) < 15000) {
        delay(250);
        Serial.print(".");
        // Keep processing DNS requests during connection
        if (_dnsRunning) {
            _dnsServer.processNextRequest();
        }
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        _state = WiFiState::CONNECTED;
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        startMDNS("sumi");
        return true;
    }
    
    _state = WiFiState::FAILED;
    return false;
}

bool WiFiManager::connectSaved() {
    if (!hasCredentials()) return false;
    return connect(_savedSSID.c_str(), _savedPassword.c_str());
}

void WiFiManager::disconnect() {
    stopMDNS();
    WiFi.disconnect();
    _state = WiFiState::IDLE;
}

void WiFiManager::stopAP() {
    if (_state == WiFiState::AP_MODE) {
        // Stop DNS server
        if (_dnsRunning) {
            _dnsServer.stop();
            _dnsRunning = false;
            Serial.println("[WIFI] DNS server stopped");
        }
        stopMDNS();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _state = WiFiState::IDLE;
    }
}

// =============================================================================
// mDNS
// =============================================================================
bool WiFiManager::startMDNS(const char* hostname) {
    if (_mdnsRunning) MDNS.end();
    
    if (MDNS.begin(hostname)) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("sumi", "tcp", 80);
        _mdnsRunning = true;
        Serial.printf("[WIFI] mDNS: %s.local\n", hostname);
        return true;
    }
    _mdnsRunning = false;
    return false;
}

void WiFiManager::stopMDNS() {
    if (_mdnsRunning) {
        MDNS.end();
        _mdnsRunning = false;
    }
}

// =============================================================================
// Portal URLs
// =============================================================================
String WiFiManager::getSetupURL() const {
    return "http://192.168.4.1";
}

String WiFiManager::getPortalURL() const {
    if (_mdnsRunning) return "http://sumi.local";
    return "http://" + getIP();
}

// =============================================================================
// Status
// =============================================================================
String WiFiManager::getIP() const {
    if (_state == WiFiState::AP_MODE) return WiFi.softAPIP().toString();
    if (_state == WiFiState::CONNECTED) return WiFi.localIP().toString();
    return "0.0.0.0";
}

String WiFiManager::getSSID() const {
    if (_state == WiFiState::CONNECTED) return WiFi.SSID();
    if (_state == WiFiState::AP_MODE) return _apName;
    return "";
}

int WiFiManager::getRSSI() const {
    return (_state == WiFiState::CONNECTED) ? WiFi.RSSI() : 0;
}

// =============================================================================
// Credentials
// =============================================================================
bool WiFiManager::hasCredentials() const { return _savedSSID.length() > 0; }

String WiFiManager::getSavedSSID() const { return _savedSSID; }

bool WiFiManager::saveCredentials(const char* ssid, const char* password) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(NVS_KEY_SSID, ssid);
    prefs.putString(NVS_KEY_PASS, password);
    prefs.end();
    _savedSSID = ssid;
    _savedPassword = password;
    return true;
}

bool WiFiManager::clearCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    _savedSSID = "";
    _savedPassword = "";
    return true;
}

bool WiFiManager::loadCredentials() {
    // First, try loading from /wifi.txt on SD card (for easy setup without portal)
    if (SD.exists("/wifi.txt")) {
        File f = SD.open("/wifi.txt", FILE_READ);
        if (f) {
            String ssid = f.readStringUntil('\n');
            String pass = f.readStringUntil('\n');
            f.close();
            
            ssid.trim();
            pass.trim();
            
            if (ssid.length() > 0) {
                Serial.printf("[WIFI] Loaded from /wifi.txt: %s\n", ssid.c_str());
                _savedSSID = ssid;
                _savedPassword = pass;
                // Save to NVS for future boots
                saveCredentials(_savedSSID.c_str(), _savedPassword.c_str());
                // Optionally delete wifi.txt after reading
                // SD.remove("/wifi.txt");
                return true;
            }
        }
    }
    
    // Fall back to NVS
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    _savedSSID = prefs.getString(NVS_KEY_SSID, "");
    _savedPassword = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();
    return _savedSSID.length() > 0;
}

// =============================================================================
// Network Scanning
// =============================================================================
int WiFiManager::scanNetworks() {
    Serial.println("[WIFI] Scanning...");
    
    // If in AP mode, temporarily enable STA for scanning
    wifi_mode_t currentMode = WiFi.getMode();
    bool wasAPOnly = (currentMode == WIFI_AP);
    
    if (wasAPOnly) {
        WiFi.mode(WIFI_AP_STA);
        delay(50);
    }
    
    WiFi.scanDelete();
    _scanResultCount = WiFi.scanNetworks(false, false, false, 300);  // Faster scan timeout
    
    // Restore AP-only mode if configured
    if (wasAPOnly) {
        WiFi.mode(WIFI_AP);
    }
    
    if (_scanResultCount < 0) _scanResultCount = 0;
    Serial.printf("[WIFI] Found %d networks\n", _scanResultCount);
    return _scanResultCount;
}

String WiFiManager::getScannedSSID(int index) {
    return (index >= 0 && index < _scanResultCount) ? WiFi.SSID(index) : "";
}

int WiFiManager::getScannedRSSI(int index) {
    return (index >= 0 && index < _scanResultCount) ? WiFi.RSSI(index) : 0;
}

bool WiFiManager::isScannedSecure(int index) {
    return (index >= 0 && index < _scanResultCount) ? WiFi.encryptionType(index) != WIFI_AUTH_OPEN : true;
}

int WiFiManager::getScannedChannel(int index) {
    return (index >= 0 && index < _scanResultCount) ? WiFi.channel(index) : 0;
}

// =============================================================================
// Reconnection Logic
// =============================================================================
void WiFiManager::attemptReconnect() {
    // Don't reconnect if no credentials
    if (!hasCredentials()) return;
    
    // Don't reconnect if already connected or in AP mode
    if (_state == WiFiState::CONNECTED) return;
    if (_state == WiFiState::AP_MODE) return;
    if (_state == WiFiState::CONNECTING) return;
    
    // Rate limit: wait 60 seconds between attempts
    if (millis() - _lastReconnectAttempt < 60000) return;
    
    Serial.println("[WIFI] Attempting reconnection...");
    _lastReconnectAttempt = millis();
    _reconnecting = true;
    
    // Non-blocking connection attempt
    WiFi.mode(WIFI_STA);
    WiFi.begin(_savedSSID.c_str(), _savedPassword.c_str());
    _state = WiFiState::CONNECTING;
    _connectStartTime = millis();
}

// =============================================================================
// Update Loop - MUST BE CALLED IN loop() FOR CAPTIVE PORTAL TO WORK
// =============================================================================
void WiFiManager::update() {
    // Process DNS requests for captive portal - THIS IS CRITICAL
    if (_dnsRunning) {
        _dnsServer.processNextRequest();
    }
    
    // Handle connection state changes
    if (WiFi.status() == WL_CONNECTED && _state != WiFiState::CONNECTED) {
        _state = WiFiState::CONNECTED;
        _reconnecting = false;
        if (!_mdnsRunning) startMDNS("sumi");
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else if (WiFi.status() != WL_CONNECTED && _state == WiFiState::CONNECTED) {
        _state = WiFiState::IDLE;
        _mdnsRunning = false;
        Serial.println("[WIFI] Disconnected");
    }
    
    // Handle reconnection timeout (15 seconds)
    if (_state == WiFiState::CONNECTING && _reconnecting) {
        if (millis() - _connectStartTime > 15000) {
            Serial.println("[WIFI] Reconnection timeout");
            _state = WiFiState::IDLE;
            _reconnecting = false;
            WiFi.disconnect();
        }
    }
}

void WiFiManager::generateAPName() {
    uint32_t chipId = (uint32_t)(ESP.getEfuseMac() >> 32);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%04X", WIFI_AP_SSID_PREFIX, chipId & 0xFFFF);
    _apName = buf;
}

// =============================================================================
// Brief WiFi Connection (for background tasks)
// =============================================================================
bool WiFiManager::connectBriefly(int timeoutMs) {
    if (!hasCredentials()) {
        Serial.println("[WIFI] No credentials for brief connect");
        return false;
    }
    
    // Already connected?
    if (isConnected()) {
        Serial.println("[WIFI] Already connected");
        return true;
    }
    
    Serial.printf("[WIFI] Brief connect to: %s\n", _savedSSID.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(_savedSSID.c_str(), _savedPassword.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < (unsigned long)timeoutMs) {
        delay(100);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Brief connect success, IP: %s\n", WiFi.localIP().toString().c_str());
        _state = WiFiState::CONNECTED;
        return true;
    }
    
    Serial.println("[WIFI] Brief connect failed");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    _state = WiFiState::IDLE;
    return false;
}

void WiFiManager::disconnectBriefly() {
    Serial.println("[WIFI] Brief disconnect");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    _state = WiFiState::IDLE;
}

// =============================================================================
// NTP Time Sync
// =============================================================================
bool WiFiManager::syncTime() {
    Serial.println("[WIFI] Starting time sync...");
    
    // Connect if not already connected
    bool wasConnected = isConnected();
    if (!wasConnected) {
        if (!connectBriefly(10000)) {
            Serial.println("[WIFI] Time sync failed - could not connect");
            return false;
        }
    }
    
    // If timezone offset is 0 (default/UTC), try to detect it from IP
    int32_t tzOffset = settingsManager.weather.timezoneOffset;
    if (tzOffset == 0) {
        Serial.println("[WIFI] Timezone not set, detecting from IP...");
        tzOffset = fetchTimezoneFromIP();
        if (tzOffset != 0) {
            settingsManager.weather.timezoneOffset = tzOffset;
            settingsManager.markDirty();
            Serial.printf("[WIFI] Detected timezone offset: %d seconds\n", tzOffset);
        }
    }
    
    Serial.printf("[WIFI] Using timezone offset: %d seconds\n", tzOffset);
    
    // Configure NTP with timezone offset
    // First param: GMT offset in seconds, Second param: DST offset (0 since our offset includes DST)
    configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    
    // Wait for time to be set (up to 10 seconds)
    Serial.println("[WIFI] Waiting for NTP sync...");
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo, 1000) && attempts < 10) {
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (attempts < 10) {
        _timeSynced = true;
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.printf("[WIFI] Time synced (local): %s\n", timeStr);
        
        // Disconnect if we connected just for this
        if (!wasConnected) {
            disconnectBriefly();
        }
        return true;
    }
    
    Serial.println("[WIFI] Time sync failed - NTP timeout");
    if (!wasConnected) {
        disconnectBriefly();
    }
    return false;
}

int32_t WiFiManager::fetchTimezoneFromIP() {
    WiFiClient client;
    if (!client.connect("ip-api.com", 80)) {
        Serial.println("[WIFI] Could not connect to ip-api.com");
        return 0;
    }
    
    client.print("GET /json/?fields=offset HTTP/1.1\r\n");
    client.print("Host: ip-api.com\r\n");
    client.print("Connection: close\r\n\r\n");
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            client.stop();
            return 0;
        }
    }
    
    // Skip headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }
    
    String payload = client.readString();
    client.stop();
    
    // Simple parse for "offset":-25200 (Mountain time is -7 hours = -25200 seconds)
    int idx = payload.indexOf("\"offset\":");
    if (idx < 0) return 0;
    
    int start = idx + 9;
    int end = payload.indexOf(',', start);
    if (end < 0) end = payload.indexOf('}', start);
    if (end < 0) return 0;
    
    String offsetStr = payload.substring(start, end);
    int32_t offset = offsetStr.toInt();
    
    Serial.printf("[WIFI] Got timezone offset from IP: %d\n", offset);
    return offset;
}

// =============================================================================
// Fast Time Sync (for wake from sleep - non-blocking)
// =============================================================================
bool WiFiManager::syncTimeFast() {
    Serial.println("[WIFI] Fast time sync...");
    
    // Quick connect - only 3 seconds max
    bool wasConnected = isConnected();
    if (!wasConnected) {
        if (!connectBriefly(3000)) {
            Serial.println("[WIFI] Fast sync: could not connect quickly");
            return false;
        }
    }
    
    // Use saved timezone - skip the slow IP lookup if we have one
    int32_t tzOffset = settingsManager.weather.timezoneOffset;
    if (tzOffset == 0) {
        // No timezone saved - use UTC for now, full sync later
        Serial.println("[WIFI] No timezone saved, using UTC");
    }
    
    // Kick off NTP (non-blocking call)
    configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov");
    
    // Brief wait - just 2 seconds
    struct tm timeinfo;
    for (int i = 0; i < 4; i++) {
        if (getLocalTime(&timeinfo, 500)) {
            _timeSynced = true;
            char timeStr[32];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
            Serial.printf("[WIFI] Fast sync OK: %s\n", timeStr);
            
            if (!wasConnected) {
                disconnectBriefly();
            }
            return true;
        }
    }
    
    // Didn't sync yet but NTP is running - it'll catch up
    Serial.println("[WIFI] Fast sync: NTP kicked off, will sync soon");
    if (!wasConnected) {
        disconnectBriefly();
    }
    return false;
}

#endif // FEATURE_WIFI

// Stub instance when WiFi is disabled
#if !FEATURE_WIFI
#include "core/WiFiManager.h"
WiFiManager wifiManager;
#endif
