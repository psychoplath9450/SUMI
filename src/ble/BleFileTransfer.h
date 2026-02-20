#pragma once
#include <Arduino.h>
#include <functional>

/**
 * BLE File Transfer Service
 *
 * Allows files to be transferred from a Web Bluetooth client (Chrome/Edge)
 * directly to the SD card without removing it.
 *
 * Protocol:
 *   1. Client connects and negotiates MTU (up to 512 bytes)
 *   2. Client writes metadata (JSON): {"name":"file.epub","size":12345,"folder":"books","queue":1,"queueTotal":5}
 *   3. Server sends status notification: {"state":"ready","mtu":512}
 *   4. Client writes data chunks sequentially (writeValueWithResponse)
 *   5. Server sends progress notifications: {"state":"ack","bytes":4096,"pct":25}
 *   6. Client writes empty chunk to signal completion
 *   7. Server verifies and sends final status: {"state":"done","name":"file.epub","size":12345}
 *   8. For queued transfers, client sends next file metadata
 *   9. After all files, client sends: {"queueComplete":true}
 *   10. Server sends: {"state":"queueDone","received":5,"total":5}
 *
 * Characteristics:
 *   - METADATA_CHAR: Write - client sends file info
 *   - DATA_CHAR:     Write - client sends file chunks (with response for reliability)
 *   - STATUS_CHAR:   Notify - server sends progress/status updates
 *
 * Throughput: ~10-25 KB/s depending on connection parameters
 */

namespace ble_transfer {

// Transfer result info (persists after transfer completes for UI display)
struct TransferResult {
    bool success;
    char filename[128];
    uint32_t fileSize;
    float speedKBs;
    char errorMsg[64];
    uint8_t queueIndex;    // Which file in queue (1-based), 0 if single
    uint8_t queueTotal;    // Total in queue, 0 if single
};

// Callback for transfer events
enum class TransferEvent {
    CONNECTED,
    DISCONNECTED,
    TRANSFER_START,       // data = filename
    TRANSFER_PROGRESS,    // data = "bytes/total" (e.g. "4096/12345")
    TRANSFER_COMPLETE,    // data = filename
    TRANSFER_ERROR,       // data = error message
    QUEUE_STARTED,        // data = total count as string (e.g. "5")
    QUEUE_FILE_DONE,      // data = "index/total" (e.g. "2/5")
    QUEUE_COMPLETE,       // data = total received as string
};

using TransferCallback = std::function<void(TransferEvent event, const char* data)>;

// Initialize the BLE file transfer service
void init();
void deinit();
bool isReady();

// Advertising control
void startAdvertising();
void stopAdvertising();
bool isAdvertising();

// Connection state
bool isConnected();

// Transfer state
bool isTransferring();
int transferProgress();
uint32_t bytesReceived();
uint32_t expectedSize();
const char* currentFilename();

// Queue info
uint8_t queueIndex();     // Current file index (1-based), 0 if no queue
uint8_t queueTotal();     // Total files in queue, 0 if no queue
uint8_t queueReceived();  // Files successfully received so far

// Transfer result (persists after completion for UI display)
bool hasResult();
const TransferResult& lastResult();
void clearResult();

// Control
void cancelTransfer();
void setCallback(TransferCallback cb);
void process();

}  // namespace ble_transfer
