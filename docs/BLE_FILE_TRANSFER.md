# SUMI BLE File Transfer — Protocol v2

Wireless file transfer from sumi.page to your SUMI device via Bluetooth Low Energy.

## Overview

Transfer files directly to your SUMI device without removing the SD card. Uses Chrome's Web Bluetooth API with write-with-response for guaranteed delivery and device-confirmed progress.

**Speed**: ~20-80 KB/s (writeWithoutResponse + ACK flow control)  
**Reliability**: Device ACKs every 4KB, auto-fallback to reliable mode if needed

## Requirements

### On your computer:
- Chrome or Edge browser (Web Bluetooth support)
- Bluetooth enabled
- sumi.page open in the browser

### On your SUMI device:
- Firmware v0.5.0 or later
- Go to Settings → Wireless Transfer → Enable

## How to Use

1. Process/convert your files on any sumi.page tool
2. On your SUMI device: Settings → Wireless Transfer → Enable
3. On the website: Click "Send to SUMI" and select your device
4. Watch progress on both the website and the device screen
5. Multiple files are sent as a queue automatically

## Protocol v2 — Technical Details

### BLE Service

- **Service UUID**: `19B10000-E8F2-537E-4F6C-D104768A1214`
- **Metadata Char**: `19B10001-...` (Write) — JSON file metadata + queue control
- **Data Char**: `19B10002-...` (Write + Write-NR) — Binary file chunks
- **Status Char**: `19B10003-...` (Read + Notify) — JSON status/ACK from device

### Single File Transfer Flow

```
Website                              Device
  │                                    │
  ├── Metadata (JSON) ───────────────► │  Parse filename, size, folder
  │   {"name":"book.epub",             │  Validate folder, create file
  │    "size":51200,                   │  Open for writing
  │    "folder":"books"}               │
  │                                    │
  │ ◄── Ready Status ─────────────────┤  {"state":"ready","mtu":509,"ack":4096}
  │                                    │
  ├── Data (writeWithoutResponse) ────►│  Write to SD (fast, no round-trip)
  ├── Data (WNR) ─────────────────────►│  Write to SD
  ├── Data (WNR) ─────────────────────►│  Write to SD
  ├── ... ~8 chunks ...                │  (4096 bytes received:)
  │ ◄── ACK ──────────────────────────┤  {"state":"ack","bytes":4096,"pct":8}
  │   (website resumes sending)        │
  ├── Data (WNR) ─────────────────────►│  Write to SD
  ├── ... more chunks + ACKs ...       │
  ├── Empty chunk (writeWithResponse) ─►│  Verify size match, close file
  │                                    │
  │ ◄── Done Status ──────────────────┤  {"state":"done","name":"book.epub",
  │                                    │   "size":51200,"speed":45.2}
```

### Multi-File Queue Flow

```
Website                              Device
  │                                    │
  ├── Metadata + Queue Info ──────────►│  {"name":"a.epub","size":1000,
  │                                    │   "folder":"books","queue":1,
  │                                    │   "queueTotal":3}
  │ ◄── Ready ────────────────────────┤
  ├── ... chunks + ACKs ...            │  "Receiving file 1 of 3"
  ├── Empty chunk ────────────────────►│
  │ ◄── Done ─────────────────────────┤
  │   (300ms delay)                    │
  ├── Metadata (file 2) ─────────────►│  {"queue":2,"queueTotal":3}
  │   ... repeat ...                   │
  ├── Queue Complete ─────────────────►│  {"queueComplete":true}
  │ ◄── Queue Done ───────────────────┤  {"state":"queueDone","received":3,"total":3}
```

### Status Messages (Device → Website)

| State | Fields | When |
|-------|--------|------|
| `ready` | `mtu`, `ack` | After metadata accepted, file opened |
| `ack` | `bytes`, `pct` | Every 4096 bytes received |
| `done` | `name`, `size`, `speed` | File complete, size verified |
| `error` | `msg` | Any error (bad filename, SD write fail, size mismatch) |
| `cancelled` | — | Transfer cancelled by device |
| `queueDone` | `received`, `total` | All files in queue processed |

### Key Design Decisions

1. **Hybrid write mode**: Fast `writeWithoutResponse` for data chunks, with ACK-gated flow control every 4KB. Auto-falls back to `writeWithResponse` if ACKs stop arriving. Gives ~20-80 KB/s vs ~1 KB/s with pure write-with-response.
2. **Device-confirmed progress**: Website tracks ACKs, not local send offset. Prevents "100% but nothing saved".
3. **No queue metadata for single files**: Clean individual result screen instead of "1 of 1".
4. **Connection check between queue files**: Abort immediately on disconnect rather than retrying.
5. **Stale status cleared per-file**: `_lastDeviceStatus = null` before each file prevents cross-file status leaks.

### Valid Folders

`books`, `comics`, `images`, `sleep`, `flashcards`, `notes`, `maps`, `custom`, `config/fonts`

## Troubleshooting

- **No devices found**: Enable Wireless Transfer on SUMI first, check computer Bluetooth
- **Connection failed**: Move closer (~10m range), ensure no other BLE connection
- **Transfer timeout**: Large files take 40-90s, don't leave the tab or press Back on device
- **Size mismatch**: SD card may be full or have filesystem errors

## Source Files

### Firmware:
- `src/ble/BleFileTransfer.h` — Public API: init, callbacks, queue, result
- `src/ble/BleFileTransfer.cpp` — BLE service, chunked write, ACK flow, queue protocol
- `src/states/SettingsState.h/cpp` — Transfer UI, result screen, queue summary

### Website:
- `ble-transfer.js` — `SumiBleTransfer` class + `createBleTransferUI()` widget
- All tool pages — Integration via shared `getFilesForBle()` + `createBleTransferUI()`
