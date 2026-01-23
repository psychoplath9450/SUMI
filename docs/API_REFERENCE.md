# REST API

The ESP32 runs a web server on port 80. All POST requests expect JSON.

**Base URL:** `http://192.168.4.1` (during setup) or `http://sumi.local` (after setup)

## Device Status

### GET /api/status

```json
{
  "version": "2.1.26",
  "variant": "lite",
  "setupComplete": true,
  "battery": {
    "percent": 85,
    "voltage": 4.1
  },
  "wifi": {
    "connected": true,
    "ssid": "HomeNetwork",
    "ip": "192.168.1.100",
    "rssi": -45
  },
  "memory": {
    "freeHeap": 142384,
    "minFreeHeap": 98432
  }
}
```

## Settings

### GET /api/settings

Returns all settings.

### POST /api/settings

Saves settings. Send the full settings object (or just the parts you want to change).

```json
{
  "display": {
    "orientation": 0,
    "sleepMinutes": 5
  },
  "reader": {
    "fontSize": 18,
    "margins": 20
  },
  "homeItems": [0, 1, 3, 5, 9, 12]
}
```

Response:
```json
{ "status": "deployed" }
```

## WiFi

### GET /api/wifi/scan

```json
{
  "networks": [
    { "ssid": "HomeNetwork", "rssi": -45, "secure": true },
    { "ssid": "GuestWiFi", "rssi": -67, "secure": false }
  ]
}
```

### POST /api/wifi/connect

```json
{ "ssid": "HomeNetwork", "password": "secret123" }
```

Response:
```json
{ "status": "connected", "ip": "192.168.1.100" }
```

## Files

### GET /api/files?path=/books

```json
{
  "path": "/books",
  "files": [
    { "name": "book.epub", "size": 524288, "dir": false },
    { "name": "classics", "size": 0, "dir": true }
  ]
}
```

### POST /api/files/upload

Multipart form upload. Fields:
- `file`: the file
- `path`: target directory (optional)

### POST /api/files/delete

```json
{ "path": "/books/old.epub" }
```

## System

### POST /api/reboot

Reboots the device.

### POST /api/factory-reset

Wipes all settings and reboots.

### POST /api/refresh

Forces a display refresh.

## Reading Statistics

### GET /api/stats

Returns reading statistics.

```json
{
  "totalPagesRead": 1523,
  "totalMinutesRead": 845,
  "totalHoursRead": 14,
  "booksFinished": 3
}
```

### DELETE /api/stats

Resets all reading statistics to zero.

## Backup & Restore

### GET /api/backup

Downloads a JSON backup file containing all settings.

Response headers include `Content-Disposition: attachment; filename="sumi-backup.json"`

### POST /api/restore

Restores settings from a backup file.

```json
{
  "backupVersion": 1,
  "display": { ... },
  "reader": { ... },
  "weather": { ... }
}
```

## KOReader Sync

### GET /api/sync/settings

```json
{
  "url": "https://sync.koreader.rocks",
  "username": "myuser",
  "enabled": true
}
```

### POST /api/sync/settings

```json
{
  "url": "https://sync.koreader.rocks",
  "username": "myuser",
  "password": "secret",
  "enabled": true
}
```

### GET /api/sync/test

Tests connection to sync server.

```json
{
  "success": true,
  "message": "Connected to sync server"
}
```

## Reader Settings

### GET /api/reader/settings

```json
{
  "fontSize": 18,
  "lineHeight": 150,
  "margins": 20,
  "paraSpacing": 10,
  "sceneBreakSpacing": 30,
  "textAlign": 1,
  "hyphenation": false,
  "showProgress": true,
  "showChapter": true,
  "showPages": true
}
```

### POST /api/reader/settings

Updates reader settings. All fields optional.

## Errors

All endpoints return errors like:

```json
{
  "status": "error",
  "message": "what went wrong"
}
```

## Portal

### GET /

Serves the embedded portal HTML. This is the setup/config interface.

The firmware also responds to captive portal detection requests from Windows, Apple, Android, and Firefox so the portal pops up automatically when you connect to the setup WiFi.
