#include "BleHid.h"
#include "../config.h"

#if FEATURE_BLUETOOTH

#include <NimBLEDevice.h>

// HID Service/Characteristic UUIDs
static NimBLEUUID hidServiceUUID("1812");
static NimBLEUUID hidReportUUID("2A4D");

namespace {

NimBLEClient* _client = nullptr;
bool _connected = false;
bool _scanning = false;
bool _scanDone = false;
bool _initialized = false;
char _deviceName[32] = {0};
char _lastChar = 0;

// Scan results
constexpr int MAX_SCAN_RESULTS = 8;
BleDevice _scanDevices[MAX_SCAN_RESULTS];
NimBLEAddress _scanAddrs[MAX_SCAN_RESULTS];
int _scanCount = 0;

// Input ring buffer
constexpr int KEY_BUF_SIZE = 32;
struct BleKeyEvent { BleKey key; char ch; };
BleKeyEvent _keyBuf[KEY_BUF_SIZE];
volatile int _keyHead = 0;
volatile int _keyTail = 0;

void pushKey(BleKey key, char ch = 0) {
    int next = (_keyHead + 1) % KEY_BUF_SIZE;
    if (next == _keyTail) return;
    _keyBuf[_keyHead] = {key, ch};
    _keyHead = next;
}

bool popKey(BleKeyEvent& out) {
    if (_keyTail == _keyHead) return false;
    out = _keyBuf[_keyTail];
    _keyTail = (_keyTail + 1) % KEY_BUF_SIZE;
    return true;
}

// ── Key Repeat State ────────────────────────────────────────

constexpr unsigned long REPEAT_DELAY_MS  = 400;
constexpr unsigned long REPEAT_RATE_MS   = 60;

struct {
    uint8_t keycode = 0;
    uint8_t modifiers = 0;
    BleKey bleKey = BleKey::NONE;
    char    ch = 0;
    unsigned long pressedAt = 0;
    unsigned long lastRepeatAt = 0;
    bool repeating = false;
} _repeat;

void repeatClear() {
    _repeat.keycode = 0;
    _repeat.bleKey = BleKey::NONE;
    _repeat.ch = 0;
    _repeat.repeating = false;
}

void repeatTick() {
    if (_repeat.keycode == 0) return;
    unsigned long now = millis();
    if (!_repeat.repeating) {
        if (now - _repeat.pressedAt >= REPEAT_DELAY_MS) {
            _repeat.repeating = true;
            _repeat.lastRepeatAt = now;
            pushKey(_repeat.bleKey, _repeat.ch);
        }
    } else {
        if (now - _repeat.lastRepeatAt >= REPEAT_RATE_MS) {
            _repeat.lastRepeatAt = now;
            pushKey(_repeat.bleKey, _repeat.ch);
        }
    }
}

// ── HID Report Parsing ──────────────────────────────────────

BleKey parseConsumerControl(uint16_t usage) {
    switch (usage) {
        case 0x00E9: return BleKey::PAGE_NEXT;
        case 0x00EA: return BleKey::PAGE_PREV;
        case 0x00CD: return BleKey::ENTER;
        case 0x00B5: return BleKey::PAGE_NEXT;
        case 0x00B6: return BleKey::PAGE_PREV;
        default: return BleKey::NONE;
    }
}

static bool _capsLock = false;

BleKey parseKeyboard(uint8_t keycode, uint8_t modifiers) {
    switch (keycode) {
        case 0x00: return BleKey::NONE;
        case 0x28: return BleKey::KEY_RETURN;
        case 0x29: return BleKey::KEY_ESCAPE;
        case 0x2A: return BleKey::KEY_BACKSPACE;
        case 0x2B: return BleKey::KEY_TAB;
        case 0x39: _capsLock = !_capsLock; return BleKey::NONE;
        case 0x4A: return BleKey::KEY_HOME;
        case 0x4B: return BleKey::KEY_END;
        case 0x4C: return BleKey::KEY_DELETE;
        case 0x4F: return BleKey::KEY_RIGHT;
        case 0x50: return BleKey::KEY_LEFT;
        case 0x51: return BleKey::KEY_DOWN;
        case 0x52: return BleKey::KEY_UP;
        default: break;
    }

    char c = 0;
    bool shift = (modifiers & 0x22) != 0;

    if (keycode >= 0x04 && keycode <= 0x1D) {
        c = 'a' + (keycode - 0x04);
        if (shift != _capsLock) c -= 32;
    }
    else if (keycode >= 0x1E && keycode <= 0x27) {
        if (shift) {
            const char shifted[] = "!@#$%^&*()";
            c = shifted[keycode - 0x1E];
        } else {
            c = (keycode == 0x27) ? '0' : ('1' + keycode - 0x1E);
        }
    }
    else if (keycode == 0x2C) c = ' ';
    else if (keycode == 0x2D) c = shift ? '_' : '-';
    else if (keycode == 0x2E) c = shift ? '+' : '=';
    else if (keycode == 0x2F) c = shift ? '{' : '[';
    else if (keycode == 0x30) c = shift ? '}' : ']';
    else if (keycode == 0x31) c = shift ? '|' : '\\';
    else if (keycode == 0x33) c = shift ? ':' : ';';
    else if (keycode == 0x34) c = shift ? '"' : '\'';
    else if (keycode == 0x35) c = shift ? '~' : '`';
    else if (keycode == 0x36) c = shift ? '<' : ',';
    else if (keycode == 0x37) c = shift ? '>' : '.';
    else if (keycode == 0x38) c = shift ? '?' : '/';

    if (c != 0) {
        _lastChar = c;
        return BleKey::KEY_CHAR;
    }
    return BleKey::NONE;
}

// ── NimBLE Callbacks ────────────────────────────────────────

class HIDClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*) override {
        _connected = true;
        Serial.printf("[BLE] Connected: %s\n", _deviceName);
    }
    void onDisconnect(NimBLEClient*, int reason) override {
        _connected = false;
        repeatClear();
        Serial.printf("[BLE] Disconnected (reason=%d)\n", reason);
    }
};

static HIDClientCallbacks _clientCb;

static uint8_t _prevKeys[6] = {0};

static bool wasKeyHeld(uint8_t keycode) {
    for (int i = 0; i < 6; i++) {
        if (_prevKeys[i] == keycode) return true;
    }
    return false;
}

static uint8_t primaryHeldKey(uint8_t* data, size_t length) {
    for (size_t i = 2; i < length && i < 8; i++) {
        if (data[i] != 0) return data[i];
    }
    return 0;
}

void onHIDReport(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
    if (length == 0) return;

    // Consumer control (2-3 bytes)
    if (length == 2 || length == 3) {
        uint16_t usage = data[0] | (data[1] << 8);
        BleKey key = parseConsumerControl(usage);
        if (key != BleKey::NONE) { pushKey(key); return; }
    }

    // Keyboard report (3-10 bytes: modifier, reserved, keys[6])
    if (length >= 3 && length <= 10) {
        uint8_t modifiers = data[0];

        for (size_t i = 2; i < length && i < 8; i++) {
            if (data[i] != 0 && !wasKeyHeld(data[i])) {
                BleKey key = parseKeyboard(data[i], modifiers);
                if (key != BleKey::NONE) {
                    pushKey(key, _lastChar);
                    _repeat.keycode = data[i];
                    _repeat.modifiers = modifiers;
                    _repeat.bleKey = key;
                    _repeat.ch = (key == BleKey::KEY_CHAR) ? _lastChar : 0;
                    _repeat.pressedAt = millis();
                    _repeat.lastRepeatAt = 0;
                    _repeat.repeating = false;
                }
            }
        }

        uint8_t held = primaryHeldKey(data, length);
        if (_repeat.keycode != 0 && held != _repeat.keycode) {
            repeatClear();
        }

        memset(_prevKeys, 0, sizeof(_prevKeys));
        for (size_t i = 2; i < length && i < 8; i++) {
            _prevKeys[i - 2] = data[i];
        }
    }
}

bool subscribeToHID(NimBLEClient* client) {
    NimBLERemoteService* svc = client->getService(hidServiceUUID);
    if (!svc) {
        Serial.println("[BLE] No HID service found");
        return false;
    }
    int subCount = 0;
    // NimBLE 2.x: getCharacteristics returns const reference
    const auto& chars = svc->getCharacteristics(true);
    for (auto* chr : chars) {
        if (chr->getUUID() == hidReportUUID && chr->canNotify()) {
            chr->subscribe(true, onHIDReport);
            subCount++;
        }
    }
    Serial.printf("[BLE] Subscribed to %d HID report(s)\n", subCount);
    return subCount > 0;
}

bool doConnect(NimBLEAddress addr) {
    if (!_client) {
        _client = NimBLEDevice::createClient();
        _client->setClientCallbacks(&_clientCb);
    }
    if (!_client->connect(addr)) {
        Serial.println("[BLE] Connection failed");
        return false;
    }
    if (!subscribeToHID(_client)) {
        Serial.println("[BLE] HID subscribe failed — might work after pairing");
    }
    return true;
}

}  // namespace

// ── Public API ──────────────────────────────────────────────

namespace ble {

void init() {
    if (_initialized) return;
    Serial.printf("[BLE] Init, free heap before: %lu\n", (unsigned long)ESP.getFreeHeap());
    NimBLEDevice::init("SUMI");
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setPower(3);
    _initialized = true;
    Serial.printf("[BLE] Init done, free heap after: %lu\n", (unsigned long)ESP.getFreeHeap());
}

void startScan(int seconds) {
    if (!_initialized) init();
    if (_scanning) return;

    _scanCount = 0;
    _scanDone = false;
    _scanning = true;

    Serial.printf("[BLE] Scanning %ds...\n", seconds);

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(45);
    scan->setWindow(40);

    // NimBLE 2.x: getResults takes timeout in ms
    NimBLEScanResults results = scan->getResults(seconds * 1000, false);

    // First pass: collect named devices and HID devices (these matter)
    for (int i = 0; i < (int)results.getCount() && _scanCount < MAX_SCAN_RESULTS; i++) {
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        const char* n = dev->getName().c_str();
        bool hasName = (n[0] != '\0');
        bool hasHID = dev->isAdvertisingService(hidServiceUUID);

        if (!hasName && !hasHID) continue;  // Skip anonymous non-HID on first pass

        BleDevice& d = _scanDevices[_scanCount];
        memset(&d, 0, sizeof(d));
        strncpy(d.name, n, sizeof(d.name) - 1);
        strncpy(d.addr, dev->getAddress().toString().c_str(), sizeof(d.addr) - 1);
        d.rssi = dev->getRSSI();
        d.hasHID = hasHID;
        _scanAddrs[_scanCount] = dev->getAddress();
        _scanCount++;

        Serial.printf("[BLE]  %d: %s (%s) RSSI=%d HID=%d\n",
                      _scanCount, d.name, d.addr, d.rssi, d.hasHID);
    }

    // Second pass: fill remaining slots with unnamed devices
    for (int i = 0; i < (int)results.getCount() && _scanCount < MAX_SCAN_RESULTS; i++) {
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        const char* n = dev->getName().c_str();
        if (n[0] != '\0' || dev->isAdvertisingService(hidServiceUUID)) continue;  // Already added

        BleDevice& d = _scanDevices[_scanCount];
        memset(&d, 0, sizeof(d));
        snprintf(d.name, sizeof(d.name), "Device %s",
                 dev->getAddress().toString().c_str() + 12);
        strncpy(d.addr, dev->getAddress().toString().c_str(), sizeof(d.addr) - 1);
        d.rssi = dev->getRSSI();
        d.hasHID = false;
        _scanAddrs[_scanCount] = dev->getAddress();
        _scanCount++;
    }

    _scanning = false;
    _scanDone = true;
    Serial.printf("[BLE] Scan done, found %d devices\n", _scanCount);
}

bool isScanning()      { return _scanning; }
bool scanComplete()    { return _scanDone; }
int  scanResultCount() { return _scanCount; }

const BleDevice* scanResult(int index) {
    return (index >= 0 && index < _scanCount) ? &_scanDevices[index] : nullptr;
}

bool connectTo(int index) {
    if (index < 0 || index >= _scanCount) return false;
    if (_connected) disconnect();
    strncpy(_deviceName, _scanDevices[index].name, sizeof(_deviceName) - 1);
    return doConnect(_scanAddrs[index]);
}

bool reconnect(const char* addr) {
    if (!addr || addr[0] == '\0') return false;
    if (!_initialized) init();
    if (_connected) disconnect();
    Serial.printf("[BLE] Reconnecting to %s\n", addr);

    // Try public address first (type 0), then random (type 1)
    for (uint8_t addrType = 0; addrType <= 1; addrType++) {
        Serial.printf("[BLE] Trying address type %d...\n", addrType);
        NimBLEAddress bleAddr(std::string(addr), addrType);
        strncpy(_deviceName, "Saved Device", sizeof(_deviceName) - 1);
        if (doConnect(bleAddr)) return true;
    }
    return false;
}

BleKey poll() {
    repeatTick();
    BleKeyEvent evt;
    if (!popKey(evt)) return BleKey::NONE;
    if (evt.ch != 0) _lastChar = evt.ch;
    return evt.key;
}

char lastChar()              { return _lastChar; }
bool isConnected()           { return _connected; }
const char* connectedDevice(){ return _connected ? _deviceName : ""; }
bool isReady()               { return _initialized; }

void disconnect() {
    if (_client && _connected) _client->disconnect();
    _connected = false;
    memset(_deviceName, 0, sizeof(_deviceName));
    memset(_prevKeys, 0, sizeof(_prevKeys));
    _capsLock = false;
    repeatClear();
}

void deinit() {
    if (!_initialized) return;
    disconnect();
    _scanCount = 0;
    NimBLEDevice::deinit(true);
    _initialized = false;
    Serial.printf("[BLE] Deinit, free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
}

}  // namespace ble

#endif  // FEATURE_BLUETOOTH
