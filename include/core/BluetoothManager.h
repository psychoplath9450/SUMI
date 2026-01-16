#ifndef SUMI_BLUETOOTH_MANAGER_H
#define SUMI_BLUETOOTH_MANAGER_H

#include "config.h"

#if FEATURE_BLUETOOTH

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <Preferences.h>

// =============================================================================
// Bluetooth Device Types
// =============================================================================
enum class BTDeviceType {
    UNKNOWN,
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    AUDIO,
    OTHER
};

// =============================================================================
// Bluetooth Device State
// =============================================================================
enum class BTDeviceState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    PAIRED_OFFLINE,  // Paired but not currently connected
    FAILED
};

// =============================================================================
// Bluetooth Device Info
// =============================================================================
struct BTDevice {
    char name[64];
    char address[18];       // "AA:BB:CC:DD:EE:FF"
    BTDeviceType type;
    BTDeviceState state;
    int rssi;               // Signal strength
    bool paired;            // Is this device paired/saved?
    bool autoConnect;       // Auto-connect on boot?
    unsigned long lastSeen;
    
    BTDevice() {
        memset(name, 0, sizeof(name));
        memset(address, 0, sizeof(address));
        type = BTDeviceType::UNKNOWN;
        state = BTDeviceState::DISCONNECTED;
        rssi = -100;
        paired = false;
        autoConnect = false;
        lastSeen = 0;
    }
    
    void copyFrom(const BTDevice& other) {
        strncpy(name, other.name, sizeof(name) - 1);
        strncpy(address, other.address, sizeof(address) - 1);
        type = other.type;
        state = other.state;
        rssi = other.rssi;
        paired = other.paired;
        autoConnect = other.autoConnect;
        lastSeen = other.lastSeen;
    }
};

// =============================================================================
// Keyboard Layout
// =============================================================================
enum class KeyboardLayout {
    US,
    UK,
    DE,
    FR,
    ES,
    IT
};

// =============================================================================
// Key Event
// =============================================================================
struct KeyEvent {
    uint8_t keyCode;        // HID key code
    uint8_t modifiers;      // Shift, Ctrl, Alt, etc.
    bool pressed;           // true = pressed, false = released
    char character;         // Translated character (if printable)
    
    // Modifier flags
    static const uint8_t MOD_CTRL  = 0x01;
    static const uint8_t MOD_SHIFT = 0x02;
    static const uint8_t MOD_ALT   = 0x04;
    static const uint8_t MOD_GUI   = 0x08;  // Windows/Command key
    
    bool isCtrl() const { return modifiers & MOD_CTRL; }
    bool isShift() const { return modifiers & MOD_SHIFT; }
    bool isAlt() const { return modifiers & MOD_ALT; }
    bool isGui() const { return modifiers & MOD_GUI; }
    bool isPrintable() const { return character >= 32 && character < 127; }
};

// =============================================================================
// Scan Result Callback
// =============================================================================
using BTScanCallback = void(*)(const BTDevice& device);
using BTKeyCallback = void(*)(const KeyEvent& event);
using BTStateCallback = void(*)(BTDeviceState state, const char* address);

// =============================================================================
// Bluetooth Manager Class
// =============================================================================
class BluetoothManager {
public:
    BluetoothManager();
    
    // Lifecycle
    void begin();
    void end();
    void update();  // Call in main loop
    
    // Enable/Disable
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    
    // Scanning
    bool startScan(uint32_t durationMs = 10000);
    void stopScan();
    bool isScanning() const { return _scanning; }
    void setScanCallback(BTScanCallback cb) { _scanCallback = cb; }
    
    // Get discovered devices
    int getDiscoveredCount() const { return _discoveredCount; }
    const BTDevice* getDiscoveredDevice(int index) const;
    void clearDiscovered();
    
    // Pairing
    bool pair(const char* address);
    bool unpair(const char* address);
    bool isPaired(const char* address) const;
    
    // Connection
    bool connect(const char* address);
    bool disconnect(const char* address);
    bool isConnected(const char* address) const;
    bool hasConnectedKeyboard() const { return _keyboardConnected; }
    
    // Auto-connect
    void setAutoConnect(const char* address, bool autoConnect);
    bool getAutoConnect(const char* address) const;
    void autoConnectPairedDevices();
    
    // Paired devices
    int getPairedCount() const { return _pairedCount; }
    const BTDevice* getPairedDevice(int index) const;
    void loadPairedDevices();
    void savePairedDevices();
    
    // Keyboard input
    void setKeyCallback(BTKeyCallback cb) { _keyCallback = cb; }
    void setKeyboardLayout(KeyboardLayout layout) { _keyboardLayout = layout; }
    KeyboardLayout getKeyboardLayout() const { return _keyboardLayout; }
    
    // State callback
    void setStateCallback(BTStateCallback cb) { _stateCallback = cb; }
    
    // Status
    BTDeviceState getDeviceState(const char* address) const;
    const char* getConnectedKeyboardName() const;
    
    // For API/JSON
    void getStatusJSON(String& json) const;
    void getDevicesJSON(String& json) const;
    void getPairedDevices(JsonArray& arr) const;
    void getDiscoveredDevices(JsonArray& arr) const;

private:
    bool _enabled;
    bool _initialized;
    bool _scanning;
    bool _keyboardConnected;
    
    // Discovered devices (from scanning)
    static const int MAX_DISCOVERED = 20;
    BTDevice _discovered[MAX_DISCOVERED];
    int _discoveredCount;
    
    // Paired devices (saved)
    static const int MAX_PAIRED = 8;
    BTDevice _paired[MAX_PAIRED];
    int _pairedCount;
    
    // Connected keyboard info
    char _connectedKeyboardName[64];
    char _connectedKeyboardAddr[18];
    
    // Keyboard
    KeyboardLayout _keyboardLayout;
    uint8_t _lastModifiers;
    
    // Callbacks
    BTScanCallback _scanCallback;
    BTKeyCallback _keyCallback;
    BTStateCallback _stateCallback;
    
    // BLE objects
    BLEScan* _bleScan;
    BLEClient* _bleClient;
    
    // Scan timing
    unsigned long _scanStartTime;
    uint32_t _scanDuration;
    
    // Internal helpers
    BTDeviceType detectDeviceType(BLEAdvertisedDevice& device);
    void onDeviceDiscovered(BLEAdvertisedDevice& device);
    int findDiscoveredIndex(const char* address) const;
    int findPairedIndex(const char* address) const;
    char translateKeyCode(uint8_t keyCode, uint8_t modifiers);
    void processKeyReport(uint8_t* data, size_t length);
    
    // BLE callbacks (static for ESP32 BLE library)
    static BluetoothManager* _instance;
    static void scanResultCallback(BLEScanResults results);
    
    // NVS storage
    void savePairedDevice(int index);
    void loadPairedDevice(int index);
    void clearPairedDevice(int index);
};

// Global instance
extern BluetoothManager bluetoothManager;

#endif // FEATURE_BLUETOOTH
#endif // SUMI_BLUETOOTH_MANAGER_H
