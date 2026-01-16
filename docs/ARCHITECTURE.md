# Architecture

## Overview

Sumi runs on an ESP32-C3 with 400KB RAM and 16MB flash. The main constraints are memory (no PSRAM) and the slow e-ink display.

```
┌─────────────────────────────────────────────────┐
│                    ESP32-C3                      │
├─────────────────────────────────────────────────┤
│                                                  │
│  Display ◄────► Core Systems ◄────► SD Card     │
│  (e-ink)        │                   (books,     │
│                 │                    cards,     │
│                 ▼                    etc)       │
│             Plugins                             │
│   (Library, Flashcards, Games, Weather...)      │
│                                                  │
└─────────────────────────────────────────────────┘
```

## Boot Sequence

1. Check if this is a valid wake from deep sleep (power button held)
2. Init display (SPI, GxEPD2)
3. Mount SD card
4. Load settings from NVS and SD
5. If first boot → start WiFi AP and show setup portal
6. Otherwise → show home screen

## Memory Management

The C3 is tight on RAM. We do a few things to cope:

**Display buffering:** GxEPD2 uses paged drawing - only 100 lines at a time instead of the full 480. This saves a ton of RAM.

**Feature flags:** `SUMI_LOW_MEMORY=1` reduces buffer sizes everywhere. Disabling games saves another ~100KB.

**Streaming parsers:** EPUBs and HTML are parsed in chunks using Expat, not loaded all at once.

**Portal cleanup:** After setup, the web server resources get freed to reclaim 30-40KB.

## Settings

Settings live in two places:

- **NVS (flash):** Critical stuff like WiFi credentials and "setup complete" flag
- **SD card:** Everything else, in `/.config/settings.json`

The SettingsManager loads both at boot and auto-saves to SD 5 seconds after any change.

## E-Ink Refresh

E-ink displays are slow and prone to ghosting. We use a three-tier refresh system:

- **Partial:** Fast (~200ms), used for most updates, builds up ghosting
- **Half:** Medium (~500ms), clears most ghosting, every ~10 partials
- **Full:** Slow (~1.5s), clears everything, every ~30 partials

## Plugins

Each plugin is basically its own app. It handles:

- `init(w, h)` - setup when launched
- `handleButton(btn)` - input, return false to exit
- `draw()` - render to display

Plugins get registered in HomeItems.h and wired up in AppLauncher.cpp.

## SD Card Layout

The SD card stores user content only. The web portal is embedded in the firmware.

```
/books/         EPUB, PDF, and TXT files
/flashcards/    study decks (JSON)
/images/        pictures (BMP, PNG, JPG)
/maps/          map images or OSM tile folders
/notes/         text notes (.txt, .md)
/.config/       settings.json (auto-created)
/.sumi/         internal data (cover cache, book progress)
```

## Feature Flags

Compile-time switches in platformio.ini:

```
FEATURE_READER=1      # book reading
FEATURE_FLASHCARDS=1  # study cards  
FEATURE_WEATHER=1     # weather widget
FEATURE_GAMES=1       # chess, sudoku, etc
FEATURE_BLUETOOTH=0   # BLE keyboard (off)
SUMI_LOW_MEMORY=1     # memory optimizations
```
