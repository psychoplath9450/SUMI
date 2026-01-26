# REST API

The ESP32 runs a web server on port 80. All POST requests expect JSON.

**Base URL:** `http://192.168.4.1` (during setup) or `http://sumi.local` (after setup)

## Device Status

### GET /api/status

```json
{
  "version": "1.4.2",
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
    "fontSize": 1,
    "screenMargin": 5,
    "lineSpacing": 1
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

### GET /api/download?path=/books/book.epub

Downloads a file from the SD card. Returns the raw file content with appropriate Content-Type header.

Used by the portal to fetch EPUBs for browser-side processing.

### POST /api/files/upload

Multipart form upload. Fields:
- `file`: the file
- `path`: target directory (optional, defaults to root)

The handler auto-creates parent directories if they don't exist.

### POST /api/files/mkdir

Creates a directory on the SD card.

```json
{ "path": "/.sumi/books/abc123" }
```

Response:
```json
{ "status": "created" }
```

Or if already exists:
```json
{ "status": "exists" }
```

### POST /api/files/delete

```json
{ "path": "/books/old.epub" }
```

## Books

### GET /api/books/status

Returns information about books on the device, split into processed and unprocessed.

```json
{
  "processed": [
    {
      "name": "Alice's Adventures in Wonderland.epub",
      "hash": "a1b2c3d4",
      "title": "Alice's Adventures in Wonderland",
      "author": "Lewis Carroll",
      "totalChapters": 12,
      "totalWords": 26432,
      "estimatedPages": 80,
      "estimatedReadingMins": 106,
      "processedAt": 1706198400,
      "currentChapter": 3,
      "currentPage": 5,
      "lastRead": 1706284800
    }
  ],
  "unprocessed": [
    {
      "name": "New Book.epub",
      "size": 524288
    }
  ]
}
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
  "fontSize": 1,
  "lineSpacing": 1,
  "screenMargin": 5,
  "textAlign": 0,
  "extraParagraphSpacing": false,
  "refreshFrequency": 15,
  "requirePreprocessed": true
}
```

Field values:
- `fontSize`: 0=SMALL (22px), 1=MEDIUM (26px), 2=LARGE (30px), 3=EXTRA_LARGE (34px)
- `lineSpacing`: 0=TIGHT (0.95x), 1=NORMAL (1.0x), 2=WIDE (1.1x)
- `screenMargin`: 0, 5, 10, 15, or 20 pixels
- `textAlign`: 0=JUSTIFIED, 1=LEFT, 2=CENTER, 3=RIGHT

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
