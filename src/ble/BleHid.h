#pragma once
#include <Arduino.h>

// BLE HID input events
enum class BleKey : uint8_t {
    NONE = 0,
    // Page turner / remote
    PAGE_NEXT,
    PAGE_PREV,
    ENTER,
    // Full keyboard
    KEY_CHAR,
    KEY_BACKSPACE,
    KEY_RETURN,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_TAB,
    KEY_ESCAPE,
    KEY_HOME,
    KEY_END,
    KEY_DELETE,
};

// Discovered device info
struct BleDevice {
    char name[32];
    char addr[18];   // "AA:BB:CC:DD:EE:FF"
    int  rssi;
    bool hasHID;
};

namespace ble {

// Initialize BLE (allocates ~40-50KB)
void init();

// Scanning
void startScan(int seconds = 10);
bool isScanning();
bool scanComplete();
int  scanResultCount();
const BleDevice* scanResult(int index);

// Connect to scanned device by index
bool connectTo(int index);

// Auto-reconnect to saved address
bool reconnect(const char* addr);

// Poll for input (non-blocking)
BleKey poll();

// Last character from KEY_CHAR event
char lastChar();

// Connection state
bool isConnected();
const char* connectedDevice();

// Disconnect current device
void disconnect();

// Shutdown BLE (frees ~40-50KB)
void deinit();

// Check if initialized
bool isReady();

}  // namespace ble
