# Changelog

All notable changes to the Sumi firmware.

## [2.1.28] - 2026-01-15

### Added
- **Sleep Screen Options**: New "Sleep Screen" setting in Display menu with three options:
  - Default: Shows centered "SUMI" logo with tagline
  - Images: Shuffles random images from /images folder (existing behavior)
  - Covers: Shuffles random book covers from your library

### Changed
- Default sleep screen is now clean "SUMI" logo instead of clock/date
- Clock/date sleep screen removed (simplified options)

## [2.1.27] - 2026-01-12

### Fixed
- **TEXT RENDERING FIX**: measureText() now uses display.getTextBounds() properly instead of broken character-count estimation
- Word widths are now measured accurately, fixing line wrapping and text justification
- Space width calculated correctly using actual font metrics
- Portal screen now shows correct WiFi AP name (Sumi-Setup-XXXX) instead of wrong "SUMI-Reader"
- Timezone auto-detection: time sync now automatically fetches timezone from IP if not already set
- Mountain time and other US timezones now detected correctly on first WiFi connection

## [2.1.26] - 2026-01-10

### Added
- JPEG cover art display in library browser (uses TJpg_Decoder with ordered dithering)
- Orientation toggle in reader settings (portrait/landscape, applies immediately)
- "Go to Chapter" option in reader settings when reading multi-chapter books
- Weather app: 7-day forecast with LEFT/RIGHT navigation between days
- Weather app: Manual ZIP code entry (press UP) for checking other locations
- Weather app: Better weather icons (sun, clouds, rain, snow, storm, fog)
- Weather app: DOWN toggles Celsius/Fahrenheit

### Changed
- Reader settings menu now accessible from library browser (press DOWN)
- Settings menu reorganized: orientation at top, cleaner layout
- Font size, margins, line spacing, justify all properly trigger book reformat
- Navigation while reading: LEFT/RIGHT for pages, UP for chapters, DOWN/OK for settings
- All games/apps now use raw button handling (no orientation remapping)
- Minesweeper, Checkers, Solitaire, TodoList, Images, Notes all optimized

### Fixed
- Web portal crash on ESP32-C3 (switched to chunked response for 27KB HTML)
- Timezone not applying correctly after WiFi connect

---

## [2.1.25] - 2026-01-07

### Fixed
- AsyncWebServer memory crash when serving portal HTML
- Changed to chunked streaming response to avoid large RAM allocation

---

## [2.1.24] - 2026-01-06

### Fixed
- Timezone display bug - time showed UTC instead of local time
- Removed duplicate configTime() call in main.cpp that was overwriting correct timezone

---

## [2.1.23] - 2026-01-05

### Added
- Sudoku game with partial refresh support and dirty cell tracking
- Updated default home screen apps: library, flashcards, chess, sudoku, weather, settings
- Settings app now locked (can't be removed from home screen)

### Changed
- E-reader text rendering fixed for GxEPD2's 2-pass page buffer system
- Sudoku controls: UP/DOWN to navigate, LEFT/RIGHT to change numbers

### Fixed
- Text disappearing on second half of e-ink display
- Partial refresh now works correctly with band-aware rendering

---

## [2.1.16] - 2026-01-01

### Added
- Plugin registration macro system for easier plugin development
- Button calibration wizard (hold POWER 10s on boot)
- RAII resource management utilities
- Portal cleanup after deployment (fixes RAM leak)
- Captive portal detection for all major platforms
- Auto-sleep and power button sleep trigger

### Changed
- Consolidated 35+ tools into single ToolSuite plugin
- Memory-optimized display buffer for ESP32-C3
- Streaming HTML/XML parsers

### Removed
- Unused games (Snake, Tetris, etc.) to save flash space

---

## [2.1.0] - 2025-12-20

### Added
- ToolSuite (calculator, clock, timer, stopwatch, countdown, counter, dice, converter, random)
- Feature flags for conditional compilation
- Low-memory mode for ESP32-C3
- Cover image caching
- Reading progress persistence

### Changed
- Plugin count reduced from 45 to 16
- Improved memory management
- Optimized e-ink refresh

---

## [2.0.0] - 2025-12-10

### Added
- Web-based setup portal
- WiFi AP mode for initial config
- EPUB reader with chapters
- Flashcards (TXT, CSV, TSV, JSON)
- Weather widget
- Games: Chess, Checkers, Sudoku, Minesweeper, Solitaire
- Notes and Images apps

### Changed
- Complete architecture redesign
- New plugin system

---

## [1.0.0] - 2025-12-01

Initial release with basic e-reader functionality.
