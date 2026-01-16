# Web Portal

The web portal lets you configure Sumi from a browser. It's embedded in the firmware and served directly by the ESP32.

## Accessing the Portal

**During first-time setup:**
1. Device creates a WiFi network called `Sumi-Setup-XXXX`
2. Connect to it
3. Go to `192.168.4.1`

**After setup:**
- Connect to the same WiFi as your device
- Go to `sumi.local` (or the device's IP address)

## What You Can Configure

- **WiFi** - scan and connect to networks
- **Home screen** - pick which apps show up and in what order
- **Display** - orientation, sleep timeout, refresh settings
- **Reader** - font size, margins, line spacing (syncs with on-device settings)

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
