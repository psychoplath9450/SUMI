# Web Portal

The web portal lets you configure SUMI from a browser. It's embedded in the firmware and served directly by the ESP32.

## Accessing the Portal

**During first-time setup:**
1. Device creates a WiFi network called `Sumi-Setup-XXXX`
2. Connect to it
3. Go to `192.168.4.1`

**After setup:**
- Connect to the same WiFi as your device
- Go to `sumi.local` (or the device's IP address)

## Portal Pages

### My SUMI (Summary)
A comprehensive "save game" style overview of your device configuration:
- **Device Status** - Battery, storage, RAM, uptime
- **Connection & Location** - WiFi network, IP address, weather location, timezone
- **SD Card Contents** - File counts for books, images, maps, flashcards, notes
- **Home Screen** - Current theme, icon style, orientation, enabled apps
- **Settings** - Display, reader, sync, keyboard settings

### WiFi
Scan for and connect to WiFi networks. The portal shows a connection status banner:
- 🟡 **Hotspot Mode** - SUMI not connected to WiFi yet
- 🟢 **Transitional** - SUMI connected, but your browser is still on hotspot
- 🟢 **Home Network** - Both SUMI and browser on same network

### Files
Browse and manage SD card contents:
- **Books** - Upload EPUBs, process existing books, view library
- **Images** - Upload photos for viewer
- **Maps** - Upload offline map tiles
- **Flashcards** - Upload card decks (CSV format)

### Book Processing

**Important:** As of v1.4.2, books must be pre-processed through the portal before they can be read. On-device EPUB parsing was causing memory issues and reliability problems on the ESP32-C3, so browser-based pre-processing is now the default.

EPUBs are pre-processed in the browser before the ESP32 reads them:
1. Upload EPUB to `/books/` folder
2. Click "Process Now" (requires internet for JSZip library)
3. Portal extracts metadata, chapters, and cover art
4. Processed files saved to `/.sumi/books/{hash}/`
5. ESP32 reads rich text files instead of parsing complex EPUBs

If you try to open a book that hasn't been processed, you'll see "Process this book in the portal first".

**Rich Text Preservation (v1.4.3+):**
The portal preserves formatting from the original EPUB:
- **Bold text** (`<b>`, `<strong>`) → `**text**`
- *Italic text* (`<i>`, `<em>`) → `*text*`
- Headers (`<h1>`-`<h6>`) → `# Header` (rendered centered and bold)
- List items (`<li>`) → `• item`
- Images → `[Image]` or `[Image: alt text]` placeholder
- Tables → `[Table]` placeholder
- Soft hyphens preserved for better line breaking

**Reader Settings → Require Pre-processing** (default: ON)
- When ON: Only pre-processed books can be opened (recommended)
- When OFF: Falls back to on-device EPUB parsing (also outputs rich text markers, but unreliable on large books)

**Cache structure:**
```
/.sumi/books/a1b2c3d4/
  meta.json         # Title, author, chapter count, word count, etc.
  cover_thumb.jpg   # 80×120 for home widget
  cover_full.jpg    # 300px wide for library browser
  ch_000.txt        # Chapter 1 with rich text markers
  ch_001.txt        # Chapter 2
  ...
```

**Re-processing books:**
If you update the firmware and need to re-process books (e.g., to get new formatting features), delete the book's cache folder and process again.

### Customize
Configure home screen, display settings, reader preferences, and more.

## What You Can Configure

- **WiFi** - scan and connect to networks
- **Home screen** - pick which apps show up and in what order
- **Display** - orientation, sleep timeout, refresh settings, boot to last book
- **Reader** - font size (4 options), margins (5 options), line spacing (3 compression levels), justification, extra paragraph spacing
- **Reading Statistics** - view pages read, time spent, books finished
- **KOReader Sync** - sync reading progress with other devices
- **Bluetooth** - pair keyboards and page turners
- **Backup/Restore** - export and import all settings

## How It Works

The portal is a single HTML file with all the CSS and JavaScript inline. It's compiled into the firmware at build time (see `portal/build.py`), stored in `src/core/portal_html.h`.

When you request the page, the ESP32 streams it in chunks to avoid running out of RAM (the HTML is ~27KB and free heap can be as low as 80KB).

## Development

The source files are in `portal/`:

```
portal/
├── templates/
│   ├── index.html    # wrapper
│   └── body.html     # main content
├── css/
│   └── styles.css
├── js/
│   └── app.js
└── build.py          # combines everything into portal_html.h
```

To rebuild after making changes:

```bash
cd portal
python build.py
```

Then recompile the firmware.

## API Endpoints

The portal talks to these endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/settings` | GET | load all settings |
| `/api/settings` | POST | save settings |
| `/api/wifi/scan` | GET | scan for networks |
| `/api/wifi/connect` | POST | connect to a network |
| `/api/status` | GET | device info, battery, etc |

See [API_REFERENCE.md](API_REFERENCE.md) for details.

## Styling

The portal uses CSS variables for theming:

```css
:root {
    --primary: #6366f1;
    --bg: #09090b;
    --surface: #18181b;
    --text: #fafafa;
    --radius: 12px;
}
```

It's dark-themed by default and responsive for mobile.

## Troubleshooting

**Portal won't load / device crashes:**

This usually means the ESP32 ran out of memory. The portal HTML is served via chunked response to avoid this, but if you're seeing crashes, check the serial output for heap warnings.

**Changes don't stick:**

Make sure you click Save. Settings auto-save after 5 seconds of no changes, but you can also manually save.

**Can't access sumi.local:**

mDNS can be flaky. Try the IP address instead - check your router's DHCP list or look at the serial output during boot.
