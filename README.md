# SUMI

**Ink, Simplified.**

Custom firmware for the Xteink X4

---

<p align="center">
  <img src="docs/images/home_screen.jpg" width="280" alt="Home Screen">
  <img src="docs/images/reading_view.jpg" width="280" alt="Reading">
  <img src="docs/images/weather_app.jpg" width="280" alt="Weather">
</p>

---

## 🚨 MAJOR DISCLAIMER - PLEASE READ

**This is an experimental proof-of-concept, NOT a finished product.**

SUMI is essentially a framework and tech demo. It is:

- ❌ **NOT production ready**
- ❌ **NOT polished or optimized**
- ❌ **NOT a replacement for your Kindle/Kobo**
- ❌ **NOT reliable enough for daily use**

**What to actually expect:**
- Everything is **slow**. This is a hobby ESP32 project, not commercial firmware.
- The UI is **glitchy**. Screen artifacts, visual bugs, and refresh issues are common.
- The e-reader **barely works**. It will probably fail on most real-world EPUB files.
- Features may be **half-implemented** or completely non-functional.
- Things will **crash**. Memory is limited and the code is not bulletproof.
- You **will** encounter bugs. Lots of them.

If you're looking for a reliable e-reader experience, use the stock firmware or buy a Kindle. This project exists for tinkerers who want to experiment with ESP32 + e-ink development, not for people who want to actually read books comfortably.

**You have been warned.**

---

## ⚠️ SD Card Folder Structure (REQUIRED)

**The firmware will only recognize files in folders with these EXACT names.** If your folders are named anything else, SUMI won't find your files.

<p align="center">
  <img src="docs/images/sd_card_structure.png" width="600" alt="SD Card Structure">
</p>

Create these folders on your SD card **before using SUMI**:

```
SD Card Root/
├── books/          ← Put your .epub files here
├── flashcards/     ← Flashcard deck files go here
├── images/         ← BMP images only
├── maps/           ← Map tiles (if using maps)
└── notes/          ← Text notes
```

### Important Notes:
- **Folder names must be lowercase** and spelled exactly as shown
- **Don't rename these folders** - the firmware looks for these specific names
- You can create these folders manually, or the firmware will create them when you first use each app
- The firmware also creates hidden folders (`.cache`, `.config`, `.sumi`) automatically - don't delete these

### Quick Setup:
1. Insert your SD card into your computer
2. Create a folder called `books` (not "Books" or "BOOKS" or "My Books")
3. Create a folder called `images` (not "Pictures" or "Photos")
4. Create any other folders you need from the list above
5. Add your files to the appropriate folders
6. Insert SD card


---



## What Is This?

SUMI is an experimental custom firmware / framework for the Xteink X4. It's a playground for learning embedded development with e-ink displays.

**In theory**, it has:

- A customizable home screen with widgets and apps
- A web-based portal for configuration
- E-Reader functionality
- Weather display
- Games (Chess, Sudoku, Minesweeper, Solitaire, Checkers)
- Flashcards with FSRS algorithm
- Bluetooth page turner support

**In practice**, most of these features are rough implementations that work sometimes, under ideal conditions, with simple test data. Real-world usage will likely be frustrating.

---

## Honest Status Report

Let me be brutally honest about where things stand:

### 🟡 Somewhat Functional (but don't get excited)

- **The Portal** - The web configuration interface is actually the most polished part. It works reasonably well. This is probably the only thing I'm not embarrassed about.
- **Home Screen** - Shows apps, time, battery. It's basic but it works.
- **Weather App** - Connects to OpenWeatherMap. Usually works if you have internet.
- **Games** - Chess, Sudoku, etc. Simple but functional. These work okay since they're self-contained.
- **Sleep/Wake** - Deep sleep works. Battery life is decent.
- **First-Time Setup** - The setup wizard basically works.

### 🟠 Barely Functional (manage expectations)

- **E-Reader / Library** - This is where reality gets harsh:
  - Will probably **fail on most EPUBs** you throw at it
  - Only works reliably with **very simple** EPUB files
  - Complex formatting, CSS, or embedded content = crashes or garbage rendering
  - Memory constraints mean large books may not load at all
  - Page turning is **slow**
  - Text rendering is **glitchy**
  - Don't expect bookmarks or progress tracking to work reliably
  - **Seriously, this is not a usable e-reader.** It's a demo that sometimes displays text from simple files.
- **Flashcards** - Basic functionality exists but it's rough around the edges.
- **Notes App** - Extremely basic. Barely more than a text file viewer.
- **File Uploads** - Works but slow. Large files may time out or fail.
- **Time Sync** - Usually works. Sometimes doesn't.

### 🔴 Non-Functional / Placeholder

- **PDF Support** - Listed in the UI but essentially doesn't work. Forget about it.
- **Images App** - BMP only. Limited functionality.
- **Maps** - Complete placeholder. Does not work.
- **EPUB Images** - Very broken. Simple inline images might display. Probably won't.
- **KOReader Sync** - Infrastructure exists, actual functionality is untested/broken.

---

## Performance Reality Check

**Everything is slow.** This is an ESP32 with limited RAM running on a slow e-ink display. Expect:

- **2-5 second** delays when opening apps
- **Noticeable lag** when navigating menus
- **Screen ghosting** and refresh artifacts
- **Multi-second** page turns in the e-reader
- **Frequent full-screen refreshes** to clear ghosting (which are even slower)
- **Random freezes** requiring a reboot

If you're used to commercial e-readers, this will feel painfully slow. That's just the nature of the hardware and my unoptimized code.

---

## Hardware Requirements

- **MicroSD Card** - Required for storing books, images, settings. 8GB+ recommended.
- **USB-C cable** - For flashing firmware
- **A computer** - For flashing and using the portal
- **Patience** - Lots of it

---

## Installation

### What You'll Need

1. [PlatformIO](https://platformio.org/) - Install the VS Code extension or CLI
2. [Python 3](https://python.org/) - For the portal build script
3. This repository

### Flashing Steps

1. **Clone the repo:**
   ```bash
   git clone https://github.com/psychoplath9450/SUMI.git
   cd SUMI
   ```

2. **Build the portal HTML** (packages the web interface into the firmware):
   ```bash
   cd portal
   python build.py
   cd ..
   ```

3. **Connect your Xteink X4 via USB-C**

4. **Build and upload:**
   ```bash
   pio run -t upload
   ```

5. **If things go wrong, erase and try again:**
   ```bash
   pio run -t erase
   pio run -t upload
   ```

### First Boot

On first boot (or after erasing), you'll see the setup wizard:

1. The device creates a WiFi hotspot called something like `Sumi-Setup-XXXX`
2. Connect to it with your phone or computer (no password)
3. Open `http://sumi.local` or `http://192.168.4.1` in your browser
4. You'll see the portal - connect to your home WiFi from there
5. Configure your apps, upload books, etc.

---

## Using SUMI

### Home Screen

<p align="center">
  <img src="docs/images/home_screen.jpg" width="350" alt="Home Screen">
</p>

Navigate with the buttons:
- **UP/DOWN/LEFT/RIGHT** - Move selection
- **OK/SELECT** - Open selected app
- **BACK** - Return to home from any app

The home screen shows time (top left) and battery percentage (top right). Widgets show your current book cover and weather. The apps shown are whatever you've enabled in the portal.

### The Portal

The portal is where you do most configuration. To access it after initial setup:

1. Open **Settings** on the device
2. Select **Open Portal**
3. Connect to the `Sumi-Setup-XXXX` WiFi and visit `sumi.local`

<p align="center">
  <img src="docs/images/portal_customize.png" width="350" alt="Portal Customize">
</p>

The portal lets you:
- Choose which apps appear on your home screen with live preview
- Upload books, flashcard decks, and images
- Configure weather location and display settings
- Adjust reader settings (font size, margins, line spacing)
- Backup and restore your settings

### E-Reader (Use With Low Expectations)

<p align="center">
  <img src="docs/images/library_browser.jpg" width="280" alt="Library Browser">
  <img src="docs/images/reading_view.jpg" width="280" alt="Reading View">
  <img src="docs/images/reader_settings.jpg" width="280" alt="Reader Settings">
</p>

**Reality check:** This e-reader is a proof-of-concept. It might display text from simple EPUB files. That's about it.

What sometimes works:
- Loading very simple EPUBs with minimal formatting
- Basic text display with adjustable font size
- Page navigation (slowly)

What probably won't work:
- Most EPUBs you download from the internet
- Books with complex CSS, tables, or formatting
- Large books (memory limitations)
- Reliable progress saving
- Smooth reading experience

### Weather

<p align="center">
  <img src="docs/images/weather_app.jpg" width="350" alt="Weather App">
</p>

Shows current conditions and 7-day forecast. Uses OpenWeatherMap (free tier). Location is auto-detected from your IP, or you can set a ZIP code manually.

Controls:
- **LEFT/RIGHT** - Cycle through days
- **UP** - Change ZIP code
- **DOWN** - Toggle °F/°C
- **OK** - Refresh

### Games

<p align="center">
  <img src="docs/images/chess.jpg" width="280" alt="Chess">
  <img src="docs/images/sudoku.jpg" width="280" alt="Sudoku">
</p>

- **Chess** - Play against a simple AI with full rules (castling, en passant, promotion)
- **Sudoku** - Multiple difficulties with pencil marks
- **Minesweeper** - Classic minesweeper
- **Solitaire** - Klondike solitaire
- **Checkers** - Basic checkers

(The games are actually the most reliable part of this firmware since they're self-contained and simple.)

### Flashcards

<p align="center">
  <img src="docs/images/flashcards.jpg" width="350" alt="Flashcards">
</p>

Spaced repetition flashcards using the FSRS algorithm. Create decks through the portal with question/answer pairs, then study on the device. 

### Settings

<p align="center">
  <img src="docs/images/settings.jpg" width="350" alt="Settings">
</p>

Access WiFi settings, open the portal, adjust display settings, and reboot the device.

---

## Features Status

| Feature | Status | Reality |
|---------|--------|---------|
| Home Screen | 🟡 Basic | Works but slow |
| Portal | 🟢 Functional | Actually decent |
| E-Reader | 🔴 Barely Works | Fails on most EPUBs, extremely limited |
| Weather | 🟡 Basic | Usually works |
| Chess | 🟡 Basic | Works okay |
| Sudoku | 🟡 Basic | Works okay |
| Flashcards | 🟡 Basic | Rough but functional |
| Checkers | 🟡 Basic | Works okay |
| Notes | 🟠 Minimal | Very basic |
| Images | 🔴 Limited | BMP only, barely functional |
| Maps | 🔴 Placeholder | Does not work |

---

## Known Issues (There Are Many)

1. **The e-reader will fail on most EPUBs** - This is the biggest issue. The parser is extremely limited.

2. **Everything is slow** - Inherent limitation of the hardware and unoptimized code.

3. **Screen ghosting** - E-ink partial refresh leaves artifacts everywhere.

4. **Memory crashes** - Large files, complex EPUBs, or too many operations = crash.

5. **Complex EPUBs break everything** - CSS, tables, images, nested formatting = crash or garbage output.

6. **Portal shows wrong supported formats** - UI says things are supported that aren't.

7. **Time sync is unreliable** - Sometimes works, sometimes doesn't.

8. **Portal needs internet** - Uses CDN-hosted fonts/icons. Doesn't work fully offline.

9. **Random freezes** - Just restart the device.

10. **Probably dozens more I haven't documented.**

---

## Project Structure

```
sumi/
├── src/
│   ├── core/           # Core system (power, wifi, settings, etc.)
│   ├── plugins/        # Apps (weather, chess, library, etc.)
│   └── main.cpp
├── include/
│   ├── core/
│   ├── plugins/
│   └── config.h        # Feature flags and pins
├── portal/
│   ├── js/app.js       # Portal JavaScript
│   ├── css/styles.css  # Portal styling  
│   ├── templates/      # HTML templates
│   └── build.py        # Builds portal into C header
├── lib/                # Dependencies (miniz, expat)
└── docs/               # Documentation and images
```

---

## Building from Source

```bash
# Install PlatformIO CLI or VS Code extension first

# Clone
git clone https://github.com/psychoplath9450/SUMI.git
cd SUMI

# Build portal (required before first build)
cd portal && python build.py && cd ..

# Build firmware
pio run

# Upload to device
pio run -t upload

# Monitor serial output (useful for debugging)
pio device monitor

# Full erase (if things are really broken)
pio run -t erase
```

---

## Contributing

This project needs a lot of work. If you're into embedded development and want to help make a janky e-ink project less janky, PRs are welcome.

The code is rough. Really rough. This started as a learning project and it shows.

**Areas that desperately need help:**
- **EPUB rendering** - The whole parser/renderer needs to be rewritten or replaced
- **Memory management** - Leaks everywhere
- **Image format support** - Proper decoding for common formats
- **General stability** - Crash recovery, error handling
- **Performance optimization** - Everything could be faster

---

## License

MIT. Do whatever you want with it.

---

## Acknowledgments

- [GxEPD2](https://github.com/ZinggJM/GxEPD2) for the display library
- [OpenWeatherMap](https://openweathermap.org/) for the free weather API
- Everyone who's worked on ESP32 Arduino libraries

---

## Final Thoughts

This is a hobby project. A learning exercise. A proof-of-concept that proves mostly that e-ink development is hard and EPUBs are complicated.

If you flash this expecting a polished experience, you will be disappointed. If you flash this expecting a buggy framework to tinker with and maybe learn something from, you might have some fun.

**It is what it is.**

Happy hacking (and debugging).
