#include "BleFileTransfer.h"
#include "../config.h"

#if FEATURE_BLUETOOTH

#include <NimBLEDevice.h>
#include <SdFat.h>
#include <SDCardManager.h>

// Use types from ble_transfer namespace
using ble_transfer::TransferEvent;
using ble_transfer::TransferCallback;
using ble_transfer::TransferResult;

// BLE UUIDs for file transfer service
#define FILE_TRANSFER_SERVICE_UUID   "19B10000-E8F2-537E-4F6C-D104768A1214"
#define METADATA_CHAR_UUID           "19B10001-E8F2-537E-4F6C-D104768A1214"
#define DATA_CHAR_UUID               "19B10002-E8F2-537E-4F6C-D104768A1214"
#define STATUS_CHAR_UUID             "19B10003-E8F2-537E-4F6C-D104768A1214"

// Flow control: ACK every N bytes
constexpr uint32_t ACK_INTERVAL_BYTES = 4096;

namespace {

// State
bool _initialized = false;
bool _advertising = false;
bool _connected = false;
bool _transferring = false;
TransferCallback _callback = nullptr;

// Transfer state
char _filename[128] = {0};
char _folder[32] = {0};
char _fullPath[192] = {0};
uint32_t _expectedSize = 0;
uint32_t _receivedBytes = 0;
uint32_t _lastAckBytes = 0;  // Bytes at last ACK
FsFile _file;
uint16_t _mtu = 20;  // Will be negotiated

// Stats for debugging
uint32_t _transferStartTime = 0;
uint32_t _chunksReceived = 0;
uint32_t _lastProgressLog = 0;

// Queue state
uint8_t _queueIndex = 0;    // Current file (1-based), 0 = no queue
uint8_t _queueTotal = 0;    // Total files in queue, 0 = no queue
uint8_t _queueReceived = 0; // Files successfully received

// Result state (persists after transfer for UI display)
bool _hasResult = false;
TransferResult _result = {};

// BLE objects
NimBLEServer* _server = nullptr;
NimBLEService* _service = nullptr;
NimBLECharacteristic* _metadataChar = nullptr;
NimBLECharacteristic* _dataChar = nullptr;
NimBLECharacteristic* _statusChar = nullptr;
NimBLEAdvertising* _advertising_ptr = nullptr;

// Forward declarations
void sendStatus(const char* json);
void notifyCallback(TransferEvent event, const char* data);
void resetTransfer();
void storeResult(bool success, float speedKBs, const char* errorMsg);

// ── Simple JSON Parser ────────────────────────────────────────

bool parseJsonString(const char* json, const char* key, char* out, size_t outLen) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* start = strstr(json, pattern);
    if (!start) return false;
    start += strlen(pattern);
    const char* end = strchr(start, '"');
    if (!end) return false;
    size_t len = end - start;
    if (len >= outLen) len = outLen - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

bool parseJsonInt(const char* json, const char* key, uint32_t* out) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return false;
    start += strlen(pattern);
    while (*start == ' ') start++;
    *out = strtoul(start, nullptr, 10);
    return true;
}

bool parseJsonBool(const char* json, const char* key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return false;
    start += strlen(pattern);
    while (*start == ' ') start++;
    return strncmp(start, "true", 4) == 0;
}

// ── Result Storage ──────────────────────────────────────────────

void storeResult(bool success, float speedKBs, const char* errorMsg) {
    _result.success = success;
    strncpy(_result.filename, _filename, sizeof(_result.filename) - 1);
    _result.filename[sizeof(_result.filename) - 1] = '\0';
    _result.fileSize = _receivedBytes;
    _result.speedKBs = speedKBs;
    _result.queueIndex = _queueIndex;
    _result.queueTotal = _queueTotal;
    if (errorMsg) {
        strncpy(_result.errorMsg, errorMsg, sizeof(_result.errorMsg) - 1);
        _result.errorMsg[sizeof(_result.errorMsg) - 1] = '\0';
    } else {
        _result.errorMsg[0] = '\0';
    }
    _hasResult = true;
}

// ── Server Callbacks ────────────────────────────────────────────────

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        _connected = true;
        _mtu = pServer->getPeerMTU(connInfo.getConnHandle()) - 3;
        if (_mtu < 20) _mtu = 20;
        if (_mtu > 509) _mtu = 509;
        Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
        Serial.printf("[BLE-FT] CLIENT CONNECTED\n");
        Serial.printf("[BLE-FT] MTU: %d bytes\n", _mtu);
        Serial.printf("[BLE-FT] Free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
        Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
        
        // Reset queue state on new connection
        _queueIndex = 0;
        _queueTotal = 0;
        _queueReceived = 0;
        
        notifyCallback(TransferEvent::CONNECTED, nullptr);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
        Serial.printf("[BLE-FT] CLIENT DISCONNECTED (reason=%d)\n", reason);
        if (_transferring) {
            Serial.printf("[BLE-FT] WARNING: Transfer interrupted!\n");
            Serial.printf("[BLE-FT] Received %lu / %lu bytes (%d%%)\n", 
                          _receivedBytes, _expectedSize, 
                          _expectedSize > 0 ? (int)((_receivedBytes * 100) / _expectedSize) : 0);
            if (_file.isOpen()) {
                _file.close();
                SdMan.remove(_fullPath);
                Serial.printf("[BLE-FT] Removed partial file: %s\n", _fullPath);
            }
            storeResult(false, 0, "Connection lost");
            resetTransfer();
            notifyCallback(TransferEvent::TRANSFER_ERROR, "Connection lost");
        }
        
        // If we were in a queue, fire queue complete with what we got
        if (_queueTotal > 0 && _queueReceived > 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", _queueReceived);
            notifyCallback(TransferEvent::QUEUE_COMPLETE, buf);
        }
        
        Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
        
        _connected = false;
        notifyCallback(TransferEvent::DISCONNECTED, nullptr);
        if (_advertising_ptr && _advertising) {
            _advertising_ptr->start();
        }
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo& connInfo) override {
        _mtu = mtu - 3;
        if (_mtu < 20) _mtu = 20;
        if (_mtu > 509) _mtu = 509;
        Serial.printf("[BLE-FT] MTU negotiated: %d (payload=%d)\n", mtu, _mtu);
    }
};

static ServerCallbacks _serverCallbacks;

// ── Metadata Characteristic Callbacks ──────────────────────────────

class MetadataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string value = pChar->getValue();
        if (value.empty()) return;

        Serial.printf("[BLE-FT] ──────────────────────────────────────\n");
        Serial.printf("[BLE-FT] METADATA: %s\n", value.c_str());
        Serial.printf("[BLE-FT] State: transferring=%d, queueIdx=%d, queueTotal=%d, queueRx=%d\n",
                      _transferring, _queueIndex, _queueTotal, _queueReceived);

        // Check for queue complete signal
        if (parseJsonBool(value.c_str(), "queueComplete")) {
            Serial.printf("[BLE-FT] Queue complete signal received\n");
            Serial.printf("[BLE-FT] Received %d / %d files\n", _queueReceived, _queueTotal);
            
            char status[96];
            snprintf(status, sizeof(status), "{\"state\":\"queueDone\",\"received\":%d,\"total\":%d}", 
                     _queueReceived, _queueTotal);
            sendStatus(status);
            
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", _queueReceived);
            notifyCallback(TransferEvent::QUEUE_COMPLETE, buf);
            return;
        }

        if (!parseJsonString(value.c_str(), "name", _filename, sizeof(_filename))) {
            Serial.printf("[BLE-FT] ERROR: Missing 'name'\n");
            sendStatus("{\"state\":\"error\",\"msg\":\"Missing filename\"}");
            return;
        }
        if (!parseJsonInt(value.c_str(), "size", &_expectedSize)) {
            Serial.printf("[BLE-FT] ERROR: Missing 'size'\n");
            sendStatus("{\"state\":\"error\",\"msg\":\"Missing size\"}");
            return;
        }
        if (!parseJsonString(value.c_str(), "folder", _folder, sizeof(_folder))) {
            strcpy(_folder, "books");
        }

        // Parse queue info
        uint32_t qi = 0, qt = 0;
        if (parseJsonInt(value.c_str(), "queue", &qi)) {
            _queueIndex = (uint8_t)qi;
        }
        if (parseJsonInt(value.c_str(), "queueTotal", &qt)) {
            if (_queueTotal == 0 && qt > 0) {
                // First file in a new queue
                _queueTotal = (uint8_t)qt;
                _queueReceived = 0;
                char buf[8];
                snprintf(buf, sizeof(buf), "%d", _queueTotal);
                notifyCallback(TransferEvent::QUEUE_STARTED, buf);
            }
            _queueTotal = (uint8_t)qt;
        }

        Serial.printf("[BLE-FT] File: %s (%lu bytes) -> /%s/\n", _filename, _expectedSize, _folder);
        if (_queueTotal > 0) {
            Serial.printf("[BLE-FT] Queue: file %d of %d\n", _queueIndex, _queueTotal);
        }

        // Validate folder
        const char* validFolders[] = {"books", "comics", "images", "sleep", "flashcards", "notes", "maps", "custom", "config/fonts"};
        bool folderValid = false;
        for (const char* vf : validFolders) {
            if (strcmp(_folder, vf) == 0) { folderValid = true; break; }
        }
        if (!folderValid) {
            Serial.printf("[BLE-FT] ERROR: Invalid folder\n");
            sendStatus("{\"state\":\"error\",\"msg\":\"Invalid folder\"}");
            return;
        }

        // Validate filename
        if (strchr(_filename, '/') || strchr(_filename, '\\') || strstr(_filename, "..")) {
            Serial.printf("[BLE-FT] ERROR: Invalid filename\n");
            sendStatus("{\"state\":\"error\",\"msg\":\"Invalid filename\"}");
            return;
        }

        // Build path and ensure directory
        snprintf(_fullPath, sizeof(_fullPath), "/%s/%s", _folder, _filename);
        char dirPath[64];
        snprintf(dirPath, sizeof(dirPath), "/%s", _folder);
        
        char* slash = dirPath + 1;
        while ((slash = strchr(slash, '/')) != nullptr) {
            *slash = '\0';
            if (!SdMan.exists(dirPath)) SdMan.mkdir(dirPath);
            *slash = '/';
            slash++;
        }
        if (!SdMan.exists(dirPath)) SdMan.mkdir(dirPath);

        // Open file
        if (_file.isOpen()) _file.close();
        _file = SdMan.open(_fullPath, O_WRONLY | O_CREAT | O_TRUNC);
        if (!_file) {
            Serial.printf("[BLE-FT] ERROR: Failed to create file\n");
            sendStatus("{\"state\":\"error\",\"msg\":\"Failed to create file\"}");
            return;
        }

        _receivedBytes = 0;
        _lastAckBytes = 0;
        _chunksReceived = 0;
        _transferring = true;
        _hasResult = false;  // Clear previous result when new transfer starts
        _transferStartTime = millis();
        _lastProgressLog = 0;

        Serial.printf("[BLE-FT] File opened: %s\n", _fullPath);
        Serial.printf("[BLE-FT] Ready to receive %lu bytes\n", _expectedSize);
        Serial.printf("[BLE-FT] Free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
        Serial.printf("[BLE-FT] ──────────────────────────────────────\n");

        char status[128];
        snprintf(status, sizeof(status), "{\"state\":\"ready\",\"mtu\":%d,\"ack\":%lu}", _mtu, ACK_INTERVAL_BYTES);
        sendStatus(status);
        notifyCallback(TransferEvent::TRANSFER_START, _filename);
    }
};

static MetadataCallbacks _metadataCallbacks;

// ── Data Characteristic Callbacks ──────────────────────────────────

class DataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        if (!_transferring || !_file.isOpen()) {
            Serial.printf("[BLE-FT] WARN: Data received but not transferring (transferring=%d, fileOpen=%d, len=%d)\n",
                          _transferring, _file.isOpen(), (int)pChar->getValue().length());
            return;
        }

        std::string value = pChar->getValue();
        _chunksReceived++;

        // Empty write = end of transfer
        if (value.empty()) {
            _file.flush();
            _file.close();
            
            uint32_t elapsed = millis() - _transferStartTime;
            float kbps = elapsed > 0 ? (_receivedBytes / 1024.0f) / (elapsed / 1000.0f) : 0;
            
            Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
            Serial.printf("[BLE-FT] TRANSFER END SIGNAL (empty write)\n");
            Serial.printf("[BLE-FT] File: %s\n", _filename);
            Serial.printf("[BLE-FT] Expected: %lu | Received: %lu | Match: %s\n", 
                          _expectedSize, _receivedBytes, 
                          (_receivedBytes == _expectedSize) ? "YES" : "NO");
            Serial.printf("[BLE-FT] Chunks: %lu | Time: %lums | Speed: %.1f KB/s\n", _chunksReceived, elapsed, kbps);
            
            if (_receivedBytes == _expectedSize) {
                Serial.printf("[BLE-FT] ✓ SUCCESS: %s (%lu bytes, %.1f KB/s)\n", _filename, _receivedBytes, kbps);
                Serial.printf("[BLE-FT] Free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
                Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
                
                // Store result for UI
                storeResult(true, kbps, nullptr);
                _queueReceived++;
                
                // Send done status with file info
                char status[192];
                snprintf(status, sizeof(status), 
                         "{\"state\":\"done\",\"name\":\"%s\",\"size\":%lu,\"speed\":%.1f}", 
                         _filename, _receivedBytes, kbps);
                sendStatus(status);
                
                notifyCallback(TransferEvent::TRANSFER_COMPLETE, _filename);
                
                // Fire queue file done if in a queue
                if (_queueTotal > 0) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d/%d", _queueIndex, _queueTotal);
                    notifyCallback(TransferEvent::QUEUE_FILE_DONE, buf);
                }
            } else {
                Serial.printf("[BLE-FT] ✗ SIZE MISMATCH: expected %lu, got %lu (delta %ld)\n", 
                              _expectedSize, _receivedBytes, (long)(_expectedSize - _receivedBytes));
                Serial.printf("[BLE-FT] Chunks received: %lu\n", _chunksReceived);
                Serial.printf("[BLE-FT] Free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
                Serial.printf("[BLE-FT] ══════════════════════════════════════\n");
                SdMan.remove(_fullPath);
                
                char errMsg[64];
                snprintf(errMsg, sizeof(errMsg), "Size mismatch: %lu/%lu", _receivedBytes, _expectedSize);
                storeResult(false, kbps, errMsg);
                
                char status[128];
                snprintf(status, sizeof(status), "{\"state\":\"error\",\"msg\":\"Size mismatch: %lu/%lu\"}", _receivedBytes, _expectedSize);
                sendStatus(status);
                notifyCallback(TransferEvent::TRANSFER_ERROR, "Size mismatch");
            }
            
            // Reset transfer state but keep queue state and result
            _transferring = false;
            _receivedBytes = 0;
            _lastAckBytes = 0;
            _expectedSize = 0;
            _chunksReceived = 0;
            // Don't clear filename/path - result screen needs them
            // Don't clear queue state - more files may follow
            
            // Signal readiness for next file after a brief delay
            // NOTE: Do NOT send idle from here. The done status sent above
            // must be read by the client first, and sending idle from inside
            // onWrite causes both to queue up — idle overwrites done before
            // the client's write-with-response promise resolves.
            // The client handles inter-file timing with its own delays.
            
            return;
        }

        // Re-check file is still open (disconnect callback may have closed it)
        if (!_file.isOpen()) {
            Serial.println("[BLE-FT] WARN: File closed during write");
            return;
        }

        // Write chunk
        size_t written = _file.write((const uint8_t*)value.data(), value.length());
        if (written != value.length()) {
            Serial.printf("[BLE-FT] ERROR: SD write failed (wrote %d of %d bytes)\n", (int)written, (int)value.length());
            Serial.printf("[BLE-FT] Total received so far: %lu / %lu\n", _receivedBytes, _expectedSize);
            Serial.printf("[BLE-FT] Free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
            _file.close();
            SdMan.remove(_fullPath);
            storeResult(false, 0, "SD write failed");
            sendStatus("{\"state\":\"error\",\"msg\":\"SD write failed\"}");
            notifyCallback(TransferEvent::TRANSFER_ERROR, "SD write failed");
            resetTransfer();
            return;
        }

        _receivedBytes += written;

        // Log progress every 20KB
        if (_receivedBytes - _lastProgressLog >= 20480) {
            _lastProgressLog = _receivedBytes;
            int pct = _expectedSize > 0 ? (int)((_receivedBytes * 100) / _expectedSize) : 0;
            Serial.printf("[BLE-FT] %lu/%lu (%d%%) chunks=%lu heap=%lu\n", 
                          _receivedBytes, _expectedSize, pct, _chunksReceived,
                          (unsigned long)ESP.getFreeHeap());
        }

        // ACK every ACK_INTERVAL_BYTES for flow control and progress reporting
        if (_receivedBytes - _lastAckBytes >= ACK_INTERVAL_BYTES) {
            _lastAckBytes = _receivedBytes;
            _file.flush();
            
            int pct = _expectedSize > 0 ? (int)((_receivedBytes * 100) / _expectedSize) : 0;
            char ack[96];
            snprintf(ack, sizeof(ack), "{\"state\":\"ack\",\"bytes\":%lu,\"pct\":%d}", _receivedBytes, pct);
            sendStatus(ack);
            
            // Fire progress event for UI updates
            char progressData[32];
            snprintf(progressData, sizeof(progressData), "%lu/%lu", _receivedBytes, _expectedSize);
            notifyCallback(TransferEvent::TRANSFER_PROGRESS, progressData);
        }
    }
};

static DataCallbacks _dataCallbacks;

// ── Helper Functions ────────────────────────────────────────────────

void sendStatus(const char* json) {
    if (_statusChar && _connected) {
        _statusChar->setValue((uint8_t*)json, strlen(json));
        bool sent = _statusChar->notify();
        Serial.printf("[BLE-FT] >> Status: %s (notify=%s)\n", json, sent ? "OK" : "FAIL");
    } else {
        Serial.printf("[BLE-FT] >> Status DROPPED: char=%s connected=%s json=%s\n",
                      _statusChar ? "OK" : "NULL", _connected ? "Y" : "N", json);
    }
}

void notifyCallback(TransferEvent event, const char* data) {
    if (_callback) _callback(event, data);
}

void resetTransfer() {
    if (_file.isOpen()) _file.close();
    _transferring = false;
    _receivedBytes = 0;
    _lastAckBytes = 0;
    _expectedSize = 0;
    _chunksReceived = 0;
    memset(_filename, 0, sizeof(_filename));
    memset(_folder, 0, sizeof(_folder));
    memset(_fullPath, 0, sizeof(_fullPath));
}

}  // anonymous namespace

// ══════════════════════════════════════════════════════════════════
//  PUBLIC API
// ══════════════════════════════════════════════════════════════════

namespace ble_transfer {

void init() {
    if (_initialized) return;

    Serial.println("[BLE-FT] ══════════════════════════════════════");
    Serial.println("[BLE-FT] INIT FILE TRANSFER SERVICE");
    Serial.printf("[BLE-FT] Heap: %lu\n", (unsigned long)ESP.getFreeHeap());

    NimBLEDevice::init("SUMI");
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setPower(3);

    _server = NimBLEDevice::createServer();
    _server->setCallbacks(&_serverCallbacks);

    _service = _server->createService(FILE_TRANSFER_SERVICE_UUID);

    // Metadata: write with response
    _metadataChar = _service->createCharacteristic(
        METADATA_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    _metadataChar->setCallbacks(&_metadataCallbacks);

    // Data: support BOTH write modes for compatibility
    // Write-with-response (WRITE) for reliability — website default
    // Write-without-response (WRITE_NR) for speed — future optimization
    _dataChar = _service->createCharacteristic(
        DATA_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _dataChar->setCallbacks(&_dataCallbacks);

    // Status: notify
    _statusChar = _service->createCharacteristic(
        STATUS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    _statusChar->setValue("{\"state\":\"idle\"}");

    _service->start();

    _advertising_ptr = NimBLEDevice::getAdvertising();
    _advertising_ptr->setName("SUMI");
    _advertising_ptr->addServiceUUID(FILE_TRANSFER_SERVICE_UUID);
    _advertising_ptr->setAppearance(0x0480);  // Generic Media Player — shows device type in OS
    _advertising_ptr->setMinInterval(0x20);
    _advertising_ptr->setMaxInterval(0x40);

    _initialized = true;
    Serial.printf("[BLE-FT] Heap after: %lu\n", (unsigned long)ESP.getFreeHeap());
    Serial.println("[BLE-FT] READY");
    Serial.println("[BLE-FT] ══════════════════════════════════════");
}

void deinit() {
    if (!_initialized) return;
    
    stopAdvertising();
    resetTransfer();
    _queueIndex = 0;
    _queueTotal = 0;
    _queueReceived = 0;
    _hasResult = false;
    _connected = false;
    
    // Fully shut down NimBLE stack — frees all BLE resources.
    // Without this, re-init returns stale server/service/characteristic
    // objects whose notification descriptors are dead. The client can
    // write to characteristics but never receives status notifications.
    NimBLEDevice::deinit(true);
    _server = nullptr;
    _service = nullptr;
    _metadataChar = nullptr;
    _dataChar = nullptr;
    _statusChar = nullptr;
    _advertising_ptr = nullptr;
    
    _initialized = false;
    _advertising = false;
    Serial.println("[BLE-FT] Deinit (NimBLE stack released)");
}

bool isReady() { return _initialized; }

void startAdvertising() {
    if (!_initialized || _advertising) return;
    if (_advertising_ptr) {
        _advertising_ptr->start();
        _advertising = true;
        Serial.println("[BLE-FT] Advertising as 'SUMI'");
    }
}

void stopAdvertising() {
    if (!_initialized || !_advertising) return;
    if (_advertising_ptr) {
        _advertising_ptr->stop();
        _advertising = false;
        Serial.println("[BLE-FT] Advertising stopped");
    }
}

bool isAdvertising() { return _advertising; }
bool isConnected() { return _connected; }
bool isTransferring() { return _transferring; }

int transferProgress() {
    if (!_transferring || _expectedSize == 0) return 0;
    return (int)(((uint64_t)_receivedBytes * 100) / _expectedSize);
}

uint32_t bytesReceived() { return _receivedBytes; }
uint32_t expectedSize() { return _expectedSize; }
const char* currentFilename() { return _filename; }

uint8_t queueIndex() { return _queueIndex; }
uint8_t queueTotal() { return _queueTotal; }
uint8_t queueReceived() { return _queueReceived; }

bool hasResult() { return _hasResult; }
const TransferResult& lastResult() { return _result; }
void clearResult() { _hasResult = false; }

void cancelTransfer() {
    if (!_transferring) return;
    Serial.println("[BLE-FT] Cancelled");
    if (_file.isOpen()) {
        _file.close();
        SdMan.remove(_fullPath);
    }
    storeResult(false, 0, "Cancelled");
    sendStatus("{\"state\":\"cancelled\"}");
    notifyCallback(TransferEvent::TRANSFER_ERROR, "Cancelled");
    resetTransfer();
}

void setCallback(TransferCallback cb) { _callback = cb; }
void process() {}

}  // namespace ble_transfer

#else  // !FEATURE_BLUETOOTH

namespace ble_transfer {

static TransferResult _dummyResult = {};

void init() {}
void deinit() {}
bool isReady() { return false; }
void startAdvertising() {}
void stopAdvertising() {}
bool isAdvertising() { return false; }
bool isConnected() { return false; }
bool isTransferring() { return false; }
int transferProgress() { return 0; }
uint32_t bytesReceived() { return 0; }
uint32_t expectedSize() { return 0; }
const char* currentFilename() { return ""; }
uint8_t queueIndex() { return 0; }
uint8_t queueTotal() { return 0; }
uint8_t queueReceived() { return 0; }
bool hasResult() { return false; }
const TransferResult& lastResult() { return _dummyResult; }
void clearResult() {}
void cancelTransfer() {}
void setCallback(TransferCallback) {}
void process() {}
}

#endif  // FEATURE_BLUETOOTH
