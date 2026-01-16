/**
 * @file BluetoothManager.cpp
 * @brief BLE HID keyboard manager for Sumi e-ink device
 * 
 * Supports BLE HID keyboards for text input and navigation.
 * Works on ESP32-C3 (BLE only) and ESP32/S3 (BLE + Classic).
 */

#include "config.h"

#if FEATURE_BLUETOOTH

#include "core/BluetoothManager.h"
#include <Preferences.h>

// Global instance
BluetoothManager bluetoothManager;

// Static instance pointer for callbacks
BluetoothManager* BluetoothManager::_instance = nullptr;

// BLE HID Service and Characteristic UUIDs
static BLEUUID hidServiceUUID("1812");           // HID Service
static BLEUUID hidReportUUID("2a4d");            // HID Report
static BLEUUID hidReportMapUUID("2a4b");         // HID Report Map
static BLEUUID batteryServiceUUID("180f");       // Battery Service

// =============================================================================
// Constructor
// =============================================================================
BluetoothManager::BluetoothManager() 
    : _enabled(false), _initialized(false), _scanning(false), _keyboardConnected(false),
      _discoveredCount(0), _pairedCount(0), _keyboardLayout(KeyboardLayout::US),
      _lastModifiers(0), _scanCallback(nullptr), _keyCallback(nullptr), 
      _stateCallback(nullptr), _bleScan(nullptr), _bleClient(nullptr),
      _scanStartTime(0), _scanDuration(0) {
    
    _instance = this;
    memset(_connectedKeyboardName, 0, sizeof(_connectedKeyboardName));
    memset(_connectedKeyboardAddr, 0, sizeof(_connectedKeyboardAddr));
    memset(_discovered, 0, sizeof(_discovered));
    memset(_paired, 0, sizeof(_paired));
}

// =============================================================================
// Lifecycle
// =============================================================================
void BluetoothManager::begin() {
    if (_initialized) return;
    
    loadPairedDevices();
    _initialized = true;
    Serial.println("[BT] BluetoothManager initialized");
}

void BluetoothManager::end() {
    if (!_initialized) return;
    
    if (_enabled) {
        setEnabled(false);
    }
    
    _initialized = false;
    Serial.println("[BT] BluetoothManager stopped");
}

void BluetoothManager::update() {
    if (!_enabled) return;
    
    // Check if scan completed
    if (_scanning && _bleScan) {
        if (!_bleScan->isScanning()) {
            _scanning = false;
            Serial.printf("[BT] Scan complete, found %d devices\n", _discoveredCount);
        }
    }
    
    // Auto-reconnect logic could go here
    // Check connection status and reconnect if needed
}

// =============================================================================
// Enable/Disable
// =============================================================================
void BluetoothManager::setEnabled(bool enabled) {
    if (enabled == _enabled) return;
    
    _enabled = enabled;
    
    if (enabled) {
        Serial.println("[BT] Initializing BLE...");
        BLEDevice::init("Sumi");
        _bleScan = BLEDevice::getScan();
        _bleScan->setActiveScan(true);
        _bleScan->setInterval(100);
        _bleScan->setWindow(99);
        Serial.println("[BT] Bluetooth enabled");
        
        // Notify state change
        if (_stateCallback) {
            _stateCallback(BTDeviceState::DISCONNECTED, "");
        }
    } else {
        Serial.println("[BT] Disabling BLE...");
        
        // Disconnect any connected device
        if (_keyboardConnected) {
            disconnect(_connectedKeyboardAddr);
        }
        
        // Stop scanning
        if (_scanning) {
            stopScan();
        }
        
        BLEDevice::deinit(true);
        _bleScan = nullptr;
        Serial.println("[BT] Bluetooth disabled");
    }
}

// =============================================================================
// Scanning
// =============================================================================
bool BluetoothManager::startScan(uint32_t durationMs) {
    if (!_enabled || _scanning) return false;
    
    Serial.println("[BT] Starting scan...");
    
    // Clear previous results
    clearDiscovered();
    
    _scanStartTime = millis();
    _scanDuration = durationMs;
    _scanning = true;
    
    // Start async scan
    _bleScan->start(durationMs / 1000, [](BLEScanResults results) {
        if (_instance) {
            for (int i = 0; i < results.getCount() && _instance->_discoveredCount < MAX_DISCOVERED; i++) {
                BLEAdvertisedDevice device = results.getDevice(i);
                _instance->onDeviceDiscovered(device);
            }
            _instance->_scanning = false;
        }
    }, false);
    
    return true;
}

void BluetoothManager::stopScan() {
    if (!_scanning || !_bleScan) return;
    
    _bleScan->stop();
    _scanning = false;
    Serial.println("[BT] Scan stopped");
}

void BluetoothManager::clearDiscovered() {
    memset(_discovered, 0, sizeof(_discovered));
    _discoveredCount = 0;
}

const BTDevice* BluetoothManager::getDiscoveredDevice(int index) const {
    if (index < 0 || index >= _discoveredCount) return nullptr;
    return &_discovered[index];
}

// =============================================================================
// Device Discovery Callback
// =============================================================================
void BluetoothManager::onDeviceDiscovered(BLEAdvertisedDevice& device) {
    // Check for HID device (keyboard)
    if (!device.haveServiceUUID()) return;
    if (!device.isAdvertisingService(hidServiceUUID)) return;
    
    // Check if already in list
    const char* addr = device.getAddress().toString().c_str();
    if (findDiscoveredIndex(addr) >= 0) return;
    
    if (_discoveredCount >= MAX_DISCOVERED) return;
    
    BTDevice& d = _discovered[_discoveredCount];
    strncpy(d.address, addr, sizeof(d.address) - 1);
    
    if (device.haveName()) {
        strncpy(d.name, device.getName().c_str(), sizeof(d.name) - 1);
    } else {
        strcpy(d.name, "Unknown Device");
    }
    
    d.type = detectDeviceType(device);
    d.rssi = device.getRSSI();
    d.state = BTDeviceState::DISCONNECTED;
    d.lastSeen = millis();
    
    // Check if this device is already paired
    int pairedIdx = findPairedIndex(addr);
    if (pairedIdx >= 0) {
        d.paired = true;
        d.autoConnect = _paired[pairedIdx].autoConnect;
    }
    
    _discoveredCount++;
    
    Serial.printf("[BT] Found: %s (%s) RSSI: %d\n", d.name, d.address, d.rssi);
    
    // Notify callback
    if (_scanCallback) {
        _scanCallback(d);
    }
}

BTDeviceType BluetoothManager::detectDeviceType(BLEAdvertisedDevice& device) {
    // Check for HID device type based on appearance or name
    if (device.haveServiceUUID() && device.isAdvertisingService(hidServiceUUID)) {
        String name = device.haveName() ? String(device.getName().c_str()) : "";
        name.toLowerCase();
        
        if (name.indexOf("keyboard") >= 0 || name.indexOf("key") >= 0) {
            return BTDeviceType::KEYBOARD;
        }
        if (name.indexOf("mouse") >= 0) {
            return BTDeviceType::MOUSE;
        }
        if (name.indexOf("gamepad") >= 0 || name.indexOf("controller") >= 0) {
            return BTDeviceType::GAMEPAD;
        }
        
        // Default to keyboard for HID devices (most common case for Sumi)
        return BTDeviceType::KEYBOARD;
    }
    
    return BTDeviceType::UNKNOWN;
}

// =============================================================================
// Pairing
// =============================================================================
bool BluetoothManager::pair(const char* address) {
    if (!_enabled) return false;
    
    // Check if already paired
    if (isPaired(address)) {
        Serial.printf("[BT] Device %s already paired\n", address);
        return true;
    }
    
    // Check limit
    if (_pairedCount >= MAX_PAIRED) {
        Serial.println("[BT] Max paired devices reached");
        return false;
    }
    
    // Find in discovered list
    int discIdx = findDiscoveredIndex(address);
    if (discIdx < 0) {
        Serial.println("[BT] Device not in discovered list");
        return false;
    }
    
    // Copy to paired list
    _paired[_pairedCount].copyFrom(_discovered[discIdx]);
    _paired[_pairedCount].paired = true;
    _paired[_pairedCount].autoConnect = true;  // Enable auto-connect by default
    _pairedCount++;
    
    // Also update discovered device
    _discovered[discIdx].paired = true;
    
    savePairedDevices();
    
    Serial.printf("[BT] Paired with %s\n", address);
    return true;
}

bool BluetoothManager::unpair(const char* address) {
    int idx = findPairedIndex(address);
    if (idx < 0) return false;
    
    // Disconnect if connected
    if (strcmp(_connectedKeyboardAddr, address) == 0) {
        disconnect(address);
    }
    
    // Remove from list (shift remaining)
    for (int i = idx; i < _pairedCount - 1; i++) {
        _paired[i].copyFrom(_paired[i + 1]);
    }
    _pairedCount--;
    
    savePairedDevices();
    
    Serial.printf("[BT] Unpaired %s\n", address);
    return true;
}

bool BluetoothManager::isPaired(const char* address) const {
    return findPairedIndex(address) >= 0;
}

// =============================================================================
// Connection
// =============================================================================
bool BluetoothManager::connect(const char* address) {
    if (!_enabled) return false;
    
    Serial.printf("[BT] Connecting to %s...\n", address);
    
    // Disconnect current if any
    if (_keyboardConnected) {
        disconnect(_connectedKeyboardAddr);
    }
    
    // Find device info
    int idx = findPairedIndex(address);
    if (idx < 0) {
        idx = findDiscoveredIndex(address);
        if (idx < 0) {
            Serial.println("[BT] Device not found");
            return false;
        }
    }
    
    BTDevice* device = (findPairedIndex(address) >= 0) ? &_paired[findPairedIndex(address)] : &_discovered[idx];
    
    // Notify connecting state
    device->state = BTDeviceState::CONNECTING;
    if (_stateCallback) {
        _stateCallback(BTDeviceState::CONNECTING, address);
    }
    
    // Create BLE client
    _bleClient = BLEDevice::createClient();
    
    BLEAddress bleAddr(address);
    if (!_bleClient->connect(bleAddr)) {
        Serial.printf("[BT] Failed to connect to %s\n", address);
        device->state = BTDeviceState::FAILED;
        if (_stateCallback) {
            _stateCallback(BTDeviceState::FAILED, address);
        }
        return false;
    }
    
    // Get HID service
    BLERemoteService* hidService = _bleClient->getService(hidServiceUUID);
    if (!hidService) {
        Serial.println("[BT] HID service not found");
        _bleClient->disconnect();
        device->state = BTDeviceState::FAILED;
        if (_stateCallback) {
            _stateCallback(BTDeviceState::FAILED, address);
        }
        return false;
    }
    
    // Get Report characteristic and subscribe to notifications
    BLERemoteCharacteristic* reportChar = hidService->getCharacteristic(hidReportUUID);
    if (reportChar && reportChar->canNotify()) {
        reportChar->registerForNotify([](BLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
            if (_instance && len > 0) {
                _instance->processKeyReport(data, len);
            }
        });
        Serial.println("[BT] Subscribed to HID reports");
    } else {
        Serial.println("[BT] Warning: Could not subscribe to HID reports");
    }
    
    // Connection established
    _keyboardConnected = true;
    strncpy(_connectedKeyboardAddr, address, sizeof(_connectedKeyboardAddr) - 1);
    strncpy(_connectedKeyboardName, device->name, sizeof(_connectedKeyboardName) - 1);
    
    device->state = BTDeviceState::CONNECTED;
    if (_stateCallback) {
        _stateCallback(BTDeviceState::CONNECTED, address);
    }
    
    Serial.printf("[BT] Connected to %s (%s)\n", _connectedKeyboardName, address);
    return true;
}

bool BluetoothManager::disconnect(const char* address) {
    if (!_keyboardConnected) return false;
    if (strcmp(_connectedKeyboardAddr, address) != 0) return false;
    
    if (_bleClient) {
        _bleClient->disconnect();
    }
    
    _keyboardConnected = false;
    
    // Update device state
    int idx = findPairedIndex(address);
    if (idx >= 0) {
        _paired[idx].state = BTDeviceState::PAIRED_OFFLINE;
    }
    
    if (_stateCallback) {
        _stateCallback(BTDeviceState::DISCONNECTED, address);
    }
    
    memset(_connectedKeyboardAddr, 0, sizeof(_connectedKeyboardAddr));
    memset(_connectedKeyboardName, 0, sizeof(_connectedKeyboardName));
    
    Serial.println("[BT] Disconnected");
    return true;
}

bool BluetoothManager::isConnected(const char* address) const {
    return _keyboardConnected && strcmp(_connectedKeyboardAddr, address) == 0;
}

// =============================================================================
// Auto-connect
// =============================================================================
void BluetoothManager::setAutoConnect(const char* address, bool autoConnect) {
    int idx = findPairedIndex(address);
    if (idx >= 0) {
        _paired[idx].autoConnect = autoConnect;
        savePairedDevices();
    }
}

bool BluetoothManager::getAutoConnect(const char* address) const {
    int idx = findPairedIndex(address);
    if (idx >= 0) {
        return _paired[idx].autoConnect;
    }
    return false;
}

void BluetoothManager::autoConnectPairedDevices() {
    if (!_enabled) return;
    
    for (int i = 0; i < _pairedCount; i++) {
        if (_paired[i].autoConnect && !_keyboardConnected) {
            Serial.printf("[BT] Auto-connecting to %s...\n", _paired[i].name);
            if (connect(_paired[i].address)) {
                break;  // Connected to one device
            }
        }
    }
}

// =============================================================================
// Paired Device Management
// =============================================================================
const BTDevice* BluetoothManager::getPairedDevice(int index) const {
    if (index < 0 || index >= _pairedCount) return nullptr;
    return &_paired[index];
}

void BluetoothManager::loadPairedDevices() {
    Preferences prefs;
    prefs.begin("sumi_bt", true);
    
    _pairedCount = prefs.getUChar("count", 0);
    if (_pairedCount > MAX_PAIRED) _pairedCount = MAX_PAIRED;
    
    for (int i = 0; i < _pairedCount; i++) {
        loadPairedDevice(i);
    }
    
    prefs.end();
    Serial.printf("[BT] Loaded %d paired devices\n", _pairedCount);
}

void BluetoothManager::savePairedDevices() {
    Preferences prefs;
    prefs.begin("sumi_bt", false);
    
    prefs.putUChar("count", _pairedCount);
    
    for (int i = 0; i < _pairedCount; i++) {
        savePairedDevice(i);
    }
    
    prefs.end();
    Serial.println("[BT] Saved paired devices");
}

void BluetoothManager::savePairedDevice(int index) {
    if (index < 0 || index >= _pairedCount) return;
    
    Preferences prefs;
    prefs.begin("sumi_bt", false);
    
    String prefix = "d" + String(index) + "_";
    prefs.putString((prefix + "addr").c_str(), _paired[index].address);
    prefs.putString((prefix + "name").c_str(), _paired[index].name);
    prefs.putBool((prefix + "auto").c_str(), _paired[index].autoConnect);
    prefs.putUChar((prefix + "type").c_str(), (uint8_t)_paired[index].type);
    
    prefs.end();
}

void BluetoothManager::loadPairedDevice(int index) {
    if (index < 0 || index >= MAX_PAIRED) return;
    
    Preferences prefs;
    prefs.begin("sumi_bt", true);
    
    String prefix = "d" + String(index) + "_";
    
    String addr = prefs.getString((prefix + "addr").c_str(), "");
    String name = prefs.getString((prefix + "name").c_str(), "Unknown");
    bool autoConn = prefs.getBool((prefix + "auto").c_str(), true);
    uint8_t type = prefs.getUChar((prefix + "type").c_str(), 0);
    
    if (addr.length() > 0) {
        strncpy(_paired[index].address, addr.c_str(), sizeof(_paired[index].address) - 1);
        strncpy(_paired[index].name, name.c_str(), sizeof(_paired[index].name) - 1);
        _paired[index].autoConnect = autoConn;
        _paired[index].type = (BTDeviceType)type;
        _paired[index].paired = true;
        _paired[index].state = BTDeviceState::PAIRED_OFFLINE;
    }
    
    prefs.end();
}

void BluetoothManager::clearPairedDevice(int index) {
    if (index < 0 || index >= MAX_PAIRED) return;
    
    Preferences prefs;
    prefs.begin("sumi_bt", false);
    
    String prefix = "d" + String(index) + "_";
    prefs.remove((prefix + "addr").c_str());
    prefs.remove((prefix + "name").c_str());
    prefs.remove((prefix + "auto").c_str());
    prefs.remove((prefix + "type").c_str());
    
    prefs.end();
}

// =============================================================================
// Keyboard Input Processing
// =============================================================================
void BluetoothManager::processKeyReport(uint8_t* data, size_t length) {
    if (length < 3) return;
    
    // HID Keyboard Report format:
    // Byte 0: Modifier keys (Ctrl, Shift, Alt, GUI)
    // Byte 1: Reserved (always 0)
    // Bytes 2-7: Key codes (up to 6 simultaneous keys)
    
    uint8_t modifiers = data[0];
    
    // Process each key in the report
    for (size_t i = 2; i < length && i < 8; i++) {
        uint8_t keyCode = data[i];
        if (keyCode == 0) continue;  // No key
        
        KeyEvent event;
        event.keyCode = keyCode;
        event.modifiers = modifiers;
        event.pressed = true;
        event.character = translateKeyCode(keyCode, modifiers);
        
        // Notify callback
        if (_keyCallback) {
            _keyCallback(event);
        }
    }
    
    _lastModifiers = modifiers;
}

char BluetoothManager::translateKeyCode(uint8_t keyCode, uint8_t modifiers) {
    bool shift = (modifiers & KeyEvent::MOD_SHIFT) != 0;
    
    // Basic US keyboard layout translation
    // HID Usage Table for Keyboard/Keypad Page (0x07)
    
    // Letters (0x04-0x1D = a-z)
    if (keyCode >= 0x04 && keyCode <= 0x1D) {
        char base = 'a' + (keyCode - 0x04);
        return shift ? (base - 32) : base;  // Uppercase if shift
    }
    
    // Numbers (0x1E-0x27 = 1-0)
    if (keyCode >= 0x1E && keyCode <= 0x27) {
        if (shift) {
            // Shift + number = symbol
            const char symbols[] = "!@#$%^&*()";
            return symbols[keyCode - 0x1E];
        }
        if (keyCode == 0x27) return '0';
        return '1' + (keyCode - 0x1E);
    }
    
    // Special keys
    switch (keyCode) {
        case 0x28: return '\n';  // Enter
        case 0x29: return 0x1B;  // Escape
        case 0x2A: return '\b';  // Backspace
        case 0x2B: return '\t';  // Tab
        case 0x2C: return ' ';   // Space
        case 0x2D: return shift ? '_' : '-';
        case 0x2E: return shift ? '+' : '=';
        case 0x2F: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x31: return shift ? '|' : '\\';
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x35: return shift ? '~' : '`';
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
    }
    
    // Non-printable or unhandled
    return 0;
}

// =============================================================================
// Status & Search Helpers
// =============================================================================
BTDeviceState BluetoothManager::getDeviceState(const char* address) const {
    if (_keyboardConnected && strcmp(_connectedKeyboardAddr, address) == 0) {
        return BTDeviceState::CONNECTED;
    }
    
    int idx = findPairedIndex(address);
    if (idx >= 0) {
        return _paired[idx].state;
    }
    
    idx = findDiscoveredIndex(address);
    if (idx >= 0) {
        return _discovered[idx].state;
    }
    
    return BTDeviceState::DISCONNECTED;
}

const char* BluetoothManager::getConnectedKeyboardName() const {
    return _keyboardConnected ? _connectedKeyboardName : nullptr;
}

int BluetoothManager::findDiscoveredIndex(const char* address) const {
    for (int i = 0; i < _discoveredCount; i++) {
        if (strcmp(_discovered[i].address, address) == 0) {
            return i;
        }
    }
    return -1;
}

int BluetoothManager::findPairedIndex(const char* address) const {
    for (int i = 0; i < _pairedCount; i++) {
        if (strcmp(_paired[i].address, address) == 0) {
            return i;
        }
    }
    return -1;
}

// =============================================================================
// JSON API Support
// =============================================================================
void BluetoothManager::getStatusJSON(String& json) const {
    JsonDocument doc;
    
    doc["enabled"] = _enabled;
    doc["scanning"] = _scanning;
    doc["connected"] = _keyboardConnected;
    doc["pairedCount"] = _pairedCount;
    doc["discoveredCount"] = _discoveredCount;
    
    if (_keyboardConnected) {
        doc["connectedDevice"] = _connectedKeyboardName;
        doc["connectedAddress"] = _connectedKeyboardAddr;
    }
    
    serializeJson(doc, json);
}

void BluetoothManager::getDevicesJSON(String& json) const {
    JsonDocument doc;
    
    JsonArray paired = doc["paired"].to<JsonArray>();
    for (int i = 0; i < _pairedCount; i++) {
        JsonObject dev = paired.add<JsonObject>();
        dev["address"] = _paired[i].address;
        dev["name"] = _paired[i].name;
        dev["connected"] = isConnected(_paired[i].address);
        dev["autoConnect"] = _paired[i].autoConnect;
    }
    
    JsonArray discovered = doc["discovered"].to<JsonArray>();
    for (int i = 0; i < _discoveredCount; i++) {
        JsonObject dev = discovered.add<JsonObject>();
        dev["address"] = _discovered[i].address;
        dev["name"] = _discovered[i].name;
        dev["rssi"] = _discovered[i].rssi;
        dev["paired"] = _discovered[i].paired;
    }
    
    serializeJson(doc, json);
}

void BluetoothManager::getPairedDevices(JsonArray& arr) const {
    for (int i = 0; i < _pairedCount; i++) {
        JsonObject dev = arr.add<JsonObject>();
        dev["address"] = _paired[i].address;
        dev["name"] = _paired[i].name;
        dev["connected"] = isConnected(_paired[i].address);
        dev["autoConnect"] = _paired[i].autoConnect;
    }
}

void BluetoothManager::getDiscoveredDevices(JsonArray& arr) const {
    for (int i = 0; i < _discoveredCount; i++) {
        JsonObject dev = arr.add<JsonObject>();
        dev["address"] = _discovered[i].address;
        dev["name"] = _discovered[i].name;
        dev["rssi"] = _discovered[i].rssi;
        dev["paired"] = _discovered[i].paired;
    }
}

#endif // FEATURE_BLUETOOTH
