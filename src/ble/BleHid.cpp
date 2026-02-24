#include "BleHid.h"
#include "../config.h"

#if FEATURE_BLUETOOTH

#include <NimBLEDevice.h>

// HID Service/Characteristic UUIDs
static NimBLEUUID hidServiceUUID("1812");
static NimBLEUUID hidReportUUID("2A4D");

// Nordic UART Service (NUS) UUIDs — used by many BLE page turners
static NimBLEUUID nusServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static NimBLEUUID nusRxCharUUID ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

namespace {

// Check if a device name matches known page turner / HID device patterns.
// Case-insensitive substring match against common names.
bool isKnownPageTurner(const char* name) {
    if (!name || name[0] == '\0') return false;
    // Make lowercase copy for matching
    char lower[32];
    int i = 0;
    for (; name[i] && i < 30; i++)
        lower[i] = (name[i] >= 'A' && name[i] <= 'Z') ? name[i] + 32 : name[i];
    lower[i] = '\0';

    // Known page turner device names (lowercase)
    // Free2/Free 2: HanLinYue page turner used by X4 community
    // AbootPaw: name used by stock firmware's expected page turner
    // Page/Turner/Remote: generic page turner keywords
    static const char* patterns[] = {
        "free",        // Free2, Free 2
        "abootpaw",    // Stock firmware expected name
        "page",        // Page Turner, PageTurner
        "turner",
        "remote",
        "jl_",         // JieLi chip prefix
        "hanlin",      // HanLinYue manufacturer
    };
    for (const char* pat : patterns) {
        if (strstr(lower, pat)) return true;
    }
    return false;
}

NimBLEClient* _client = nullptr;
bool _connected = false;
bool _scanning = false;
bool _scanDone = false;
bool _initialized = false;
char _deviceName[32] = {0};
char _lastChar = 0;

// Early-stop: when a known HID page turner is found during scan, stop
// scanning immediately so the caller can connect before the device sleeps.
bool _foundPageTurner = false;
int  _foundPTIndex = -1;  // index into _scanDevices of the found page turner

// Inactivity timeout
uint32_t _inactivityTimeoutMs = 0;
unsigned long _lastActivityMs = 0;

// Scan results
constexpr int MAX_SCAN_RESULTS = 8;
BleDevice _scanDevices[MAX_SCAN_RESULTS];
const NimBLEAdvertisedDevice* _scanAdvDevs[MAX_SCAN_RESULTS] = {nullptr};
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
    _lastActivityMs = millis();  // Reset inactivity timer on every input
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

    // Accept connection parameter update requests from peripherals.
    // The Free2 (and most HID devices) will request parameters it can handle.
    // Rejecting these causes the peripheral to disconnect.
    // Return true to accept, false to reject.
    bool onConnParamsUpdateRequest(NimBLEClient* client,
                                   const ble_gap_upd_params* params) override {
        Serial.printf("[BLE] Peripheral requests conn params: interval=%d-%d latency=%d timeout=%d\n",
                      params->itvl_min, params->itvl_max,
                      params->latency, params->supervision_timeout);
        // Accept whatever the peripheral wants — it knows its own limits.
        return true;
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

// ── Per-handle report ID tracking ───────────────────────────
// Populated during subscribeToHID() by reading Report Reference descriptors.
// Used to filter out sensor/vendor reports and parse button reports correctly.

struct HidReportInfo {
    uint16_t handle;
    uint8_t  reportId;
    uint8_t  reportType;  // 1=Input, 2=Output, 3=Feature
    bool     isSensor;    // true = skip (sensor/vendor data)
};
static HidReportInfo _reportInfo[8];
static int _reportInfoCount = 0;

static const HidReportInfo* getReportInfo(uint16_t handle) {
    for (int i = 0; i < _reportInfoCount; i++) {
        if (_reportInfo[i].handle == handle) return &_reportInfo[i];
    }
    return nullptr;
}

void onHIDReport(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t length, bool) {
    uint16_t handle = chr ? chr->getHandle() : 0;

    // Skip handles identified as sensor/vendor during subscribe
    const auto* info = getReportInfo(handle);
    if (info && info->isSensor) return;

    // Debug: log raw HID report data with handle
    Serial.printf("[BLE-HID] h=%d RX %d bytes:", handle, (int)length);
    for (size_t i = 0; i < length && i < 16; i++) Serial.printf(" %02X", data[i]);
    Serial.println();

    if (length == 0) return;

    // Free 2 page turner: 6-byte hybrid reports on reportId=1.
    // Byte 0 = button bitmask (bit 0 = forward, bit 1 = back), bytes 1-5 = accelerometer.
    // Detect by: reportId==1, length==6, bytes 2-5 always have 3+ non-zero (accel data).
    // Instead of discarding these as sensor noise, extract the button state from byte 0.
    if (info && info->reportId == 1 && length == 6) {
        int nonZeroKeys = 0;
        for (size_t i = 2; i < 6; i++) {
            if (data[i] != 0) nonZeroKeys++;
        }
        if (nonZeroKeys >= 3) {
            // This is a hybrid accel+button report. Decode byte 0 as button bitmask.
            static uint8_t prevButtons = 0;
            uint8_t buttons = data[0];
            if (buttons != prevButtons) {
                // Detect rising edges (new button presses)
                uint8_t pressed = buttons & ~prevButtons;
                if (pressed & 0x01) {
                    Serial.printf("[BLE-HID] Free2: Forward button pressed\n");
                    pushKey(BleKey::PAGE_NEXT);
                }
                if (pressed & 0x02) {
                    Serial.printf("[BLE-HID] Free2: Back button pressed\n");
                    pushKey(BleKey::PAGE_PREV);
                }
                prevButtons = buttons;
            }
            return;  // Don't parse accelerometer bytes as keyboard
        }
    }

    // Consumer control (2-3 bytes, or 3 bytes with report ID prefix)
    if (length == 2 || length == 3) {
        uint16_t usage = data[0] | (data[1] << 8);
        BleKey key = parseConsumerControl(usage);
        if (key != BleKey::NONE) {
            Serial.printf("[BLE-HID] Consumer: usage=0x%04X -> key=%d\n", usage, (int)key);
            pushKey(key); return;
        }
    }

    // Keyboard report (3-10 bytes: modifier, reserved, keys[6])
    if (length >= 3 && length <= 10) {
        uint8_t modifiers = data[0];

        for (size_t i = 2; i < length && i < 8; i++) {
            if (data[i] != 0 && !wasKeyHeld(data[i])) {
                BleKey key = parseKeyboard(data[i], modifiers);
                if (key != BleKey::NONE) {
                    Serial.printf("[BLE-HID] Keyboard: code=0x%02X mod=0x%02X -> key=%d\n",
                                  data[i], modifiers, (int)key);
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

// ── NUS (Nordic UART) Parsing ───────────────────────────────
// Page turners using NUS send simple byte commands over the RX characteristic.
// Common protocols: single byte or short string for next/prev page.

void onNUSData(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
    if (length == 0) return;

    Serial.printf("[BLE-NUS] RX %d bytes:", (int)length);
    for (size_t i = 0; i < length && i < 16; i++) Serial.printf(" %02X", data[i]);
    Serial.println();

    // Common page turner NUS protocols:
    // Many send single-byte: 0x01=next, 0x02=prev (or variations)
    // Some send HID-like keyboard reports over UART
    // Some send ASCII: 'N'=next, 'P'=prev or arrow keycodes
    if (length == 1) {
        switch (data[0]) {
            case 0x01: case 'N': case 'n': case 0x4F:  // next
                pushKey(BleKey::PAGE_NEXT); return;
            case 0x02: case 'P': case 'p':  // prev ('P'==0x50 is also LEFT keycode)
                pushKey(BleKey::PAGE_PREV); return;
            case 0x0D: case 0x28:  // enter
                pushKey(BleKey::KEY_RETURN); return;
        }
    }

    // 2-byte: could be consumer control or simple command
    if (length == 2) {
        uint16_t val = data[0] | (data[1] << 8);
        BleKey key = parseConsumerControl(val);
        if (key != BleKey::NONE) { pushKey(key); return; }
        // Also try first byte as command (second might be release/null)
        if (data[1] == 0x00) {
            switch (data[0]) {
                case 0x01: pushKey(BleKey::PAGE_NEXT); return;
                case 0x02: pushKey(BleKey::PAGE_PREV); return;
            }
        }
    }

    // Keyboard-style report sent over UART (some remotes do this)
    if (length >= 3 && length <= 10) {
        uint8_t modifiers = data[0];
        for (size_t i = 2; i < length && i < 8; i++) {
            if (data[i] != 0) {
                BleKey key = parseKeyboard(data[i], modifiers);
                if (key != BleKey::NONE) {
                    pushKey(key, _lastChar);
                    return;
                }
            }
        }
    }
}

bool subscribeToNUS(NimBLEClient* client) {
    NimBLERemoteService* svc = client->getService(nusServiceUUID);
    if (!svc) {
        Serial.println("[BLE] No NUS service found");
        return false;
    }
    NimBLERemoteCharacteristic* rxChar = svc->getCharacteristic(nusRxCharUUID);
    if (!rxChar || !rxChar->canNotify()) {
        Serial.println("[BLE] NUS RX characteristic not found or can't notify");
        return false;
    }
    rxChar->subscribe(true, onNUSData);
    Serial.println("[BLE] Subscribed to NUS RX notifications");
    return true;
}

bool subscribeToHID(NimBLEClient* client) {
    Serial.println("[BLE] Looking for HID service...");
    NimBLERemoteService* svc = client->getService(hidServiceUUID);
    if (!svc) {
        Serial.println("[BLE] No HID service found");
        return false;
    }
    Serial.println("[BLE] HID service found, enumerating characteristics...");
    _reportInfoCount = 0;
    int subCount = 0;
    const auto& chars = svc->getCharacteristics(true);
    Serial.printf("[BLE] Found %d characteristics in HID service\n", (int)chars.size());
    for (auto* chr : chars) {
        Serial.printf("[BLE]   char %s canNotify=%d canRead=%d\n",
                      chr->getUUID().toString().c_str(),
                      chr->canNotify(), chr->canRead());
        if (chr->getUUID() == hidReportUUID && chr->canNotify()) {
            if (!client->isConnected()) {
                Serial.println("[BLE]   Connection lost during subscribe!");
                break;
            }

            // Read Report Reference descriptor (0x2908) to identify report type
            uint8_t reportId = 0, reportType = 0;
            auto* refDesc = chr->getDescriptor(NimBLEUUID("2908"));
            if (refDesc) {
                auto val = refDesc->readValue();
                if (val.size() >= 2) {
                    reportId = val[0];
                    reportType = val[1];  // 1=Input, 2=Output, 3=Feature
                }
            }

            // Classify: report IDs >= 3 on HID devices with many reports
            // are typically vendor-specific (sensor/gyro data).
            // Input reports with ID 1-2 are usually keyboard/consumer control.
            bool isSensor = (reportId >= 3 && reportType == 1);

            if (_reportInfoCount < 8) {
                _reportInfo[_reportInfoCount++] = {
                    chr->getHandle(), reportId, reportType, isSensor
                };
            }

            Serial.printf("[BLE]   Report handle=%d reportId=%d type=%d%s\n",
                          chr->getHandle(), reportId, reportType,
                          isSensor ? " (SENSOR — will skip)" : "");

            if (chr->subscribe(true, onHIDReport)) {
                subCount++;
                Serial.printf("[BLE]   Subscribed to report #%d (handle=%d)\n",
                              subCount, chr->getHandle());
            } else {
                Serial.printf("[BLE]   Subscribe failed for handle=%d\n", chr->getHandle());
            }
        }
    }
    Serial.printf("[BLE] Subscribed to %d HID report(s)\n", subCount);
    return subCount > 0;
}

bool doConnect(const NimBLEAdvertisedDevice* advDev) {
    // Reuse existing client or create a new one (follows official NimBLE pattern)
    if (!_client) {
        _client = NimBLEDevice::createClient();
        _client->setClientCallbacks(&_clientCb, false);
    }

    // Connection params: use the Free2's preferred interval range (16-24,
    // i.e. 20-30ms) but with latency=0 and a generous 6s timeout.
    // After connection, the Free2 will request latency=10 via a param
    // update — we accept it in onConnParamsUpdateRequest.
    //
    // We don't start with latency=10 because during GATT discovery we
    // need responsive round-trips.  And we don't use 15ms because the
    // JieLi chip can't handle that pace during ATT enumeration.
    //
    // scanInterval/scanWindow = 96/96 (60ms/60ms = 100% duty cycle) so
    // the controller catches slow advertisers like the Free2 (~494ms interval).
    _client->setConnectionParams(16, 24, 0, 600, 96, 96);
    _client->setConnectTimeout(10000);

    Serial.printf("[BLE] Connecting to %s ...\n",
                  advDev->getAddress().toString().c_str());

    // Pass the advertised device pointer directly — this preserves the
    // correct address type, name, and all other scan data.
    if (!_client->connect(advDev)) {
        Serial.printf("[BLE] connect() failed, err=%d\n", _client->getLastError());
        return false;
    }

    Serial.printf("[BLE] Connected to: %s RSSI: %d\n",
                  _client->getPeerAddress().toString().c_str(),
                  _client->getRssi());
    _lastActivityMs = millis();

    // iPhone takes 5-10 seconds to fully connect to the Free2.
    // We need to be patient and let the connection fully stabilize.
    //
    // Sequence: connect → wait for param update → pair → wait → discover
    //
    // Step 1: Wait 2 seconds for the Free2 to send its connection
    // parameter update request and for that to be processed.
    Serial.println("[BLE] Phase 1: Waiting 2s for conn param negotiation...");
    for (int i = 0; i < 4; i++) {
        delay(500);
        if (!_client->isConnected()) {
            Serial.printf("[BLE] Lost connection in phase 1 (step %d)\n", i);
            return false;
        }
        Serial.printf("[BLE] Phase 1 step %d/4 OK\n", i + 1);
    }

    // Step 2: Initiate pairing/encryption
    Serial.println("[BLE] Phase 2: Securing connection...");
    if (!_client->secureConnection()) {
        Serial.printf("[BLE] secureConnection() failed, err=%d (continuing anyway)\n",
                      _client->getLastError());
    } else {
        Serial.println("[BLE] Secure connection established");
    }

    // Step 3: Wait another 2 seconds for the peripheral to settle
    // after pairing.  The JieLi chip is slow.
    Serial.println("[BLE] Phase 3: Waiting 2s post-pairing settle...");
    for (int i = 0; i < 4; i++) {
        delay(500);
        if (!_client->isConnected()) {
            Serial.printf("[BLE] Lost connection in phase 3 (step %d)\n", i);
            return false;
        }
        Serial.printf("[BLE] Phase 3 step %d/4 OK\n", i + 1);
    }

    Serial.println("[BLE] All phases complete, starting GATT discovery");

    // Try HID first, then NUS (Nordic UART) for page turners
    if (subscribeToHID(_client)) {
        Serial.println("[BLE] Using HID profile — ready for input!");
        // Don't switch to faster params — the Free2 already negotiated
        // the params it wants (20-30ms interval, latency=10).  Overriding
        // could cause another disconnection.
    } else if (subscribeToNUS(_client)) {
        Serial.println("[BLE] Using NUS (Nordic UART) profile");
    } else {
        Serial.println("[BLE] No HID or NUS service found — trying all notify chars");
        const auto& services = _client->getServices(true);
        int subCount = 0;
        for (auto* svc : services) {
            const auto& chars = svc->getCharacteristics(true);
            for (auto* chr : chars) {
                if (chr->canNotify()) {
                    chr->subscribe(true, onNUSData);
                    subCount++;
                    Serial.printf("[BLE]   Subscribed to %s / %s\n",
                                  svc->getUUID().toString().c_str(),
                                  chr->getUUID().toString().c_str());
                }
            }
        }
        if (subCount == 0) {
            Serial.println("[BLE] No notifiable characteristics found at all");
        }
    }
    return true;
}

}  // namespace

// ── Public API ──────────────────────────────────────────────

namespace ble {

void init() {
    if (_initialized) return;
    Serial.printf("[BLE] Init, free heap before: %lu\n", (unsigned long)ESP.getFreeHeap());
    NimBLEDevice::init("SumiReader");
    NimBLEDevice::setPower(9);  // Max power for best range

    // Enable bonding for HID devices (they require persistent pairing).
    // bonding=true, MITM=false, SC=true → "just works" pairing with bonding.
    // IO capability defaults to NoInputNoOutput which is correct for a headless device.
    NimBLEDevice::setSecurityAuth(true, false, true);

    _initialized = true;
    Serial.printf("[BLE] Init done, free heap after: %lu\n", (unsigned long)ESP.getFreeHeap());
}

// Callback-based scan collects devices as they're found
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        // Store name in local std::string to avoid dangling pointer
        // (getName() returns a temporary — .c_str() on a temporary is UB)
        std::string nameStr = dev->getName();
        const char* n = nameStr.c_str();
        bool hasHID = dev->isAdvertisingService(hidServiceUUID);
        bool hasNUS = dev->isAdvertisingService(nusServiceUUID);
        bool hasName = (n[0] != '\0');
        bool knownPT = isKnownPageTurner(n);

        if (knownPT || hasHID || hasNUS) {
            Serial.printf("[BLE]  >>> adv: %s(t%d) name=\"%s\" rssi=%d hid=%d nus=%d PT=%d <<<\n",
                          dev->getAddress().toString().c_str(), dev->getAddressType(), n,
                          dev->getRSSI(), hasHID, hasNUS, knownPT);
        } else {
            Serial.printf("[BLE]  adv: %s(t%d) name=\"%s\" rssi=%d\n",
                          dev->getAddress().toString().c_str(), dev->getAddressType(),
                          n, dev->getRSSI());
        }

        if (_scanCount >= MAX_SCAN_RESULTS) return;
        if (!hasName && !hasHID && !hasNUS) return;

        // Check for duplicates
        for (int i = 0; i < _scanCount; i++) {
            if (strcmp(_scanDevices[i].addr, dev->getAddress().toString().c_str()) == 0) {
                // Update name if we got it now (scan response)
                if (hasName && _scanDevices[i].name[0] == '\0') {
                    strncpy(_scanDevices[i].name, n, sizeof(_scanDevices[i].name) - 1);
                }
                if (hasHID || hasNUS || knownPT) _scanDevices[i].hasHID = true;
                return;
            }
        }

        BleDevice& d = _scanDevices[_scanCount];
        memset(&d, 0, sizeof(d));
        strncpy(d.name, n, sizeof(d.name) - 1);
        strncpy(d.addr, dev->getAddress().toString().c_str(), sizeof(d.addr) - 1);
        d.rssi = dev->getRSSI();
        d.hasHID = hasHID || hasNUS || knownPT;
        _scanAdvDevs[_scanCount] = dev;
        int thisIdx = _scanCount;
        _scanCount++;

        Serial.printf("[BLE]  +++ Added [%d]: \"%s\" (%s) HID=%d\n",
                      _scanCount, d.name, d.addr, d.hasHID);

        // Only stop scanning early for KNOWN page turner names (like "Free 2").
        // Don't stop for generic HID devices (like "Bluetooth Keyboard") —
        // those are regular keyboards, not page turners.
        if (knownPT && !_foundPageTurner) {
            _foundPageTurner = true;
            _foundPTIndex = thisIdx;
            Serial.printf("[BLE]  *** Page turner found! Stopping scan for immediate connect ***\n");
            NimBLEDevice::getScan()->stop();
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("[BLE] Scan complete (reason=%d), %d in results, %d collected\n",
                      reason, (int)results.getCount(), _scanCount);
        _scanning = false;
        _scanDone = true;
    }
};

static ScanCallbacks _scanCb;

void startScan(int seconds) {
    if (!_initialized) init();
    if (_scanning) return;

    _scanCount = 0;
    _scanDone = false;
    _scanning = true;
    _foundPageTurner = false;
    _foundPTIndex = -1;
    // Clear stale device pointers — they become invalid when a new scan starts.
    for (int i = 0; i < MAX_SCAN_RESULTS; i++) _scanAdvDevs[i] = nullptr;

    Serial.printf("[BLE] Scanning %ds...\n", seconds);

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&_scanCb, false);  // no duplicates, matches official example
    scan->clearResults();

    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(100);

    // Blocking scan — getResults waits until duration expires
    // (or until callback calls stop() when a page turner is found)
    NimBLEScanResults results = scan->getResults(seconds * 1000, false);

    // If the callback already found a page turner and populated _scanDevices,
    // keep those results as-is. The callback has already stored addresses.
    if (_foundPageTurner) {
        Serial.printf("[BLE] Scan stopped early — page turner at index %d\n", _foundPTIndex);
        // Callback already populated _scanDevices and _scanAddrs correctly.
        // Just finalize.
    } else {
        Serial.printf("[BLE] Raw scan: %d devices\n", (int)results.getCount());

        // Rebuild from final results (callback data may have duplicates
        // due to wantDuplicates=true, so results give a cleaner set).
        _scanCount = 0;

        // First pass: named + HID/NUS + known page turner devices
        for (int i = 0; i < (int)results.getCount() && _scanCount < MAX_SCAN_RESULTS; i++) {
            const NimBLEAdvertisedDevice* dev = results.getDevice(i);
            std::string nameStr = dev->getName();
            const char* n = nameStr.c_str();
            bool hasName = (n[0] != '\0');
            bool hasHID = dev->isAdvertisingService(hidServiceUUID);
            bool hasNUS = dev->isAdvertisingService(nusServiceUUID);
            bool knownPT = isKnownPageTurner(n);

            if (!hasName && !hasHID && !hasNUS) continue;

            BleDevice& d = _scanDevices[_scanCount];
            memset(&d, 0, sizeof(d));
            strncpy(d.name, n, sizeof(d.name) - 1);
            strncpy(d.addr, dev->getAddress().toString().c_str(), sizeof(d.addr) - 1);
            d.rssi = dev->getRSSI();
            d.hasHID = hasHID || hasNUS || knownPT;
            _scanAdvDevs[_scanCount] = dev;
            _scanCount++;

            Serial.printf("[BLE]  %d: %s (%s) RSSI=%d HID=%d PT=%d\n",
                          _scanCount, d.name, d.addr, d.rssi, d.hasHID, knownPT);
        }

        // Second pass: unnamed devices
        for (int i = 0; i < (int)results.getCount() && _scanCount < MAX_SCAN_RESULTS; i++) {
            const NimBLEAdvertisedDevice* dev = results.getDevice(i);
            std::string nameStr = dev->getName();
            if (!nameStr.empty() || dev->isAdvertisingService(hidServiceUUID)
                || dev->isAdvertisingService(nusServiceUUID)) continue;

            BleDevice& d = _scanDevices[_scanCount];
            memset(&d, 0, sizeof(d));
            snprintf(d.name, sizeof(d.name), "Device %s",
                     dev->getAddress().toString().c_str() + 12);
            strncpy(d.addr, dev->getAddress().toString().c_str(), sizeof(d.addr) - 1);
            d.rssi = dev->getRSSI();
            d.hasHID = false;
            _scanAdvDevs[_scanCount] = dev;
            _scanCount++;
        }
    }

    _scanning = false;
    _scanDone = true;
    Serial.printf("[BLE] Scan done, found %d devices\n", _scanCount);
}

// Check if a known page turner was found during the last scan.
// If so, return its index for immediate auto-connect.
int foundPageTurnerIndex() {
    return _foundPageTurner ? _foundPTIndex : -1;
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
    Serial.printf("[BLE] Connecting to [%d] \"%s\" (%s)\n",
                  index, _deviceName, _scanDevices[index].addr);

    // Use the cached scan device pointer if available (fast path,
    // avoids a re-scan).  If it's null (stale), fall back to
    // reconnect-by-address which does a fresh scan.
    if (_scanAdvDevs[index]) {
        return doConnect(_scanAdvDevs[index]);
    }
    Serial.println("[BLE] Scan pointer stale, falling back to reconnect");
    return reconnect(_scanDevices[index].addr);
}

bool reconnect(const char* addr) {
    if (!addr || addr[0] == '\0') return false;
    if (!_initialized) init();
    if (_connected) disconnect();
    Serial.printf("[BLE] Reconnecting to %s\n", addr);

    // For reconnect without a scan result, do a quick scan to find the device
    // so we can pass the proper advertised device pointer to connect().
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(100);
    NimBLEScanResults results = scan->getResults(5000, false);

    for (int i = 0; i < (int)results.getCount(); i++) {
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        if (strcmp(dev->getAddress().toString().c_str(), addr) == 0) {
            std::string nameStr = dev->getName();
            strncpy(_deviceName, nameStr.c_str(), sizeof(_deviceName) - 1);
            if (_deviceName[0] == '\0') strncpy(_deviceName, "Saved Device", sizeof(_deviceName) - 1);
            return doConnect(dev);
        }
    }

    Serial.println("[BLE] Device not found in scan for reconnect");
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
    _inactivityTimeoutMs = 0;
    NimBLEDevice::deinit(true);
    _initialized = false;
    Serial.printf("[BLE] Deinit, free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
}

void setInactivityTimeout(uint32_t timeoutMs) {
    _inactivityTimeoutMs = timeoutMs;
    if (timeoutMs > 0) _lastActivityMs = millis();
}

bool checkInactivityTimeout() {
    if (_inactivityTimeoutMs == 0 || !_connected) return false;
    if (millis() - _lastActivityMs >= _inactivityTimeoutMs) {
        Serial.printf("[BLE] Inactivity timeout (%lu ms), disconnecting\n",
                      (unsigned long)_inactivityTimeoutMs);
        disconnect();
        return true;
    }
    return false;
}

}  // namespace ble

#endif  // FEATURE_BLUETOOTH
