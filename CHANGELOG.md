# Changelog

All notable changes to the Sumi firmware.

## [2.4.0] - 2026-01-22

### Added
- **Demo Plugin Redesign**: Complete rewrite of 3D Demo as menu-based "Demo" plugin
  - Main menu with selectable demo modes
  - **3D Shapes**: Rotating wireframe shapes (Cube, Pyramid, Diamond, Heart)
    - OK: Cycle through shapes
    - LEFT/RIGHT: Change size (1-5)
    - UP/DOWN: Change rotation speed (1-5)
  - **Matrix Rain**: Falling characters effect
    - LEFT/RIGHT: Change density (1-5)
    - UP/DOWN: Change speed (1-5)
    - OK: Pause/unpause
  - Easy to extend with more demos in the future

- **Weather Widget Date Display**: Portrait mode now shows full date info
  - Location name
  - Weather icon + temperature
  - Humidity percentage
  - Full day of week (e.g., "Wednesday")
  - Date (e.g., "January 22")
  - Shows "Time not set" if device hasn't synced time

### Changed
- **Renamed "3D Demo" to "Demo"** in home screen and menus
- Landscape weather widget now shows location name below temperature
- **Sudoku Inline Number Entry**: Removed popup number picker
  - Navigate grid with D-pad (UP/DOWN/LEFT/RIGHT)
  - Press OK on empty cell to enter edit mode (cell turns black)
  - In edit mode: UP/DOWN cycles through valid numbers only
  - Press OK to confirm number, BACK to cancel
  - Only numbers valid for that position are shown (Sudoku rules)
  - Auto-saves after confirming each number

### Fixed
- **Sudoku Ghosting**: Added periodic smooth refresh every 4 cursor movements
  - Clears e-ink ghosting without black flash
  - Uses partial window for smooth refresh (not full window)
  - Counter resets on full redraws (number picker, etc.)

- **EPUB Book Opening**: Fixed "Failed to allocate dictionary buffer" error
  - Books failed to open after using portal mode
  - Root cause: ZIP buffers freed for portal weren't reallocated
  - Fix: openBook() now calls ZipReader_preallocateBuffer() instead of just resetFlags()

- **Chapter First Page Rendering**: Fixed right side being cut off on first page of chapter
  - When changing chapters, only part of the display rendered
  - Root cause: Render task was drawing without GxEPD2's required firstPage/nextPage loop
  - Fix: Render task now properly wraps drawing in display.firstPage()/nextPage() pattern

- **Flashcard Out of Memory**: Fixed "Out of memory" when loading flashcard decks
  - Reduced memory footprint: MAX_CARDS 100→50, MAX_TEXT 120→80
  - Frees ZIP reader buffers (~43KB) before loading decks
  - JSON file limit reduced to 16KB (still plenty for flashcards)
  - Memory usage reduced from ~56KB to ~24KB

- **Home Screen Grid Layout**: App buttons now properly fill available space
  - Grid dimensions adapt to actual number of apps (e.g., 6 apps = 3×2)
  - Cell height capped at 150px (portrait) / 200px (landscape) for usability
  - Works correctly in both portrait and landscape orientations

### Notes
- **Asterisks in books**: Some classic EPUBs (like Alice in Wonderland) use `**` or `***` as scene break markers in the actual text content. These are not image placeholders - they're part of the original book formatting.

## [2.3.0] - 2026-01-22

### Added
- **New Widget Layout**: Streamlined widget design optimized for each orientation
  - Portrait: Book cover left half, weather+orientation stacked on right half
  - Landscape: Book cover on TOP (large), weather+orientation side by side on BOTTOM
  - No borders, progress bars, or titles on book widget - just the cover
  - Weather simplified to icon + temperature only
  - Orientation is now a slim toggle switch
  - Widget area 50% taller for better visual impact

- **Sleep Screen Image Scaling**: Images now properly scale to fill screen
  - BMP images scale with proper aspect ratio preservation
  - JPEG support added for sleep folder images
  - Ordered dithering for better grayscale rendering
  - Supports 1-bit, 8-bit, and 24-bit BMP files

- **Orientation-Aware Widget Navigation**
  - Portrait mode: LEFT/RIGHT to navigate between widgets
  - Landscape mode: UP/DOWN to navigate between widgets
  - More intuitive based on widget layout direction

### Changed
- **8 App Maximum**: Home screen now limited to 8 apps (plus 3 widgets)
  - Portal shows limit note and prevents adding more than 8
  - Simplifies home screen, most users only need 4-6 apps
  - Settings always present, doesn't count toward limit

- **Reduced Margins**: Text now fills more of the screen
  - Narrow: 5px (was 10px)
  - Normal: 10px (was 20px)
  - Wide: 20px (was 35px)
- Cache version bumped to 4 (old caches auto-invalidate)
- Widget area height increased to 180px in portrait mode
- Faster book closing (mutex timeout instead of infinite wait)
- Library exit from root now returns to home (no view switching)

### Fixed
- **Landscape Grid Navigation**: Cell selection now uses correct dimensions
  - Fixed buttons appearing larger/overlapping when navigating
  - Selection refresh matches full redraw dimensions
  - Proper font sizing for narrow cells
- Book closing was slow due to infinite mutex wait
- Widget cursor disappeared when navigating to widgets
- Partial refresh area didn't include widget region
- Sleep images were distorted and too small
- Margins inconsistent between applyFontSettings and applyToLayout

## [2.2.0] - 2026-01-18

### Added
- **Bookmarks**: Save up to 20 bookmarks per book
  - Access from Settings menu while reading
  - Jump to any bookmarked position instantly
  - Bookmarks persist per-book in cache directory
  
- **Reading Statistics**: Track your reading progress
  - Lifetime stats: pages read, time reading, books finished
  - Session stats: current reading session tracking
  - View stats from reader Settings menu or web portal
  - Reset stats option available in portal
  
- **Boot to Last Book**: Skip home screen and jump directly into reading
  - New toggle in Display settings
  - Opens last-read book at exact position
  - Falls back to home screen if no book history
  
- **Scene Break Spacing**: Visual separation for `<hr>` tags in EPUBs
  - Configurable 0-60px spacing (default 30px)
  - Slider in Reader Settings on portal
  
- **Quick-Open Library**: Click Library plugin to resume reading
  - Automatically opens last book at saved position
  - Press BACK to go to book browser instead
  
- **Widget Selection**: Navigate to and activate home screen widgets
  - Press UP from top row to select widgets
  - UP/DOWN navigates between widgets
  - Press CONFIRM to activate (book opens reader, weather opens forecast)
  - Press DOWN from last widget or BACK to return to app grid
  - Widgets highlight when selected (inverted colors)
  
- **Book Cover Art in Widget**: Continue Reading widget shows actual book cover
  - Displays scaled EPUB cover image in widget
  - Dithered for e-ink display quality
  - Falls back to text icon if no cover available
  
- **Expanded Demo**: Multiple animated demos
  - Mode 1: Wireframe Cube (original)
  - Mode 2: Multiple Floating Cubes
  - Mode 3: Shaded Pyramid with depth sorting
  - Mode 4: Spinning Torus (donut shape)
  - Mode 5: 3D Starfield Tunnel
  - Mode 6: Bouncing Shaded Sphere
  - Mode 7: Dancing Robot figure
  - LEFT/RIGHT to switch modes, UP/DOWN to adjust speed
  
- **Background WiFi Time Sync**: Non-blocking time sync on wake
  - Home screen displays immediately after wake
  - Time syncs in background without freezing UI
  - WiFi disconnects automatically after sync complete

- **Chess Save/Resume**: Save your chess game and continue later
  - Prompts to resume saved game when opening Chess
  - Prompts to save when exiting mid-game
  - Saves board position, castling rights, en passant, move number
  - Automatic cleanup when game ends (checkmate/stalemate)

- **Orientation Widget**: Quick toggle between portrait and landscape
  - Shows toggle switch with current mode
  - Tap to instantly rotate display
  - Settings persist between sessions

- **Redesigned Widgets**: Simplified, cleaner widget design
  - Book Widget: Large cover art + thin progress bar (no text)
  - Weather Widget: Icon + temperature + location
  - Orientation Widget: Toggle switch design
  - Portrait mode: Widgets in horizontal row at top
  - Landscape mode: Widgets stacked vertically on left
  - All 3 widgets enabled by default in portal
  
- **Backup & Restore**: Export/import all settings
  - Download JSON backup from portal About page
  - Restore settings from backup file
  - Includes WiFi networks, display settings, reader prefs
  
- **KOReader Sync Support**: Sync progress with KOReader devices
  - Configure sync server URL in portal
  - Progress synced using document hash
  - Compatible with sync.koreader.rocks
  
- **Bluetooth Page Turner**: Use BT remotes for hands-free reading
  - Arrow keys for page turn
  - Space/Backspace as alternatives
  - Configurable key mapping
  
- **EPUB Image Support** (Beta): Display inline images
  - Images extracted and cached from EPUB
  - Rendered using TJpgDec with dithering
  - Centered within text flow

### Changed
- Reader `draw()` function now properly renders pages (fixes blank page bug)

### Fixed
- **Text Rendering**: Font size setting now properly applies to reader
  - Font changes from Small (9pt) to Medium/Large (12pt) based on settings
  - Line height calculated properly from font metrics
  - Fixed overlapping text caused by incorrect font/lineheight mismatch
  
- **Cover Browser Layout**: Fixed portrait mode cover image overlap
  - Cover images now properly clipped to frame bounds
  - Book info positioned below cover (doesn't overlap)
  - Simplified layout: title + file info above "Book X of Y"
  
- **Status Bar**: Now shows chapter name + page number
  - Chapter title on left (truncated if needed)
  - Page number (current/total) on right
  - Cleaner, more informative display

- **Partial Refresh**: Menu navigation now uses smooth partial refresh
  - Settings menus no longer flash black on every selection
  - Increased partial refresh count before forced full refresh (50 vs 20)

### Added
- **Flashcard Settings**: New settings menu for flashcards
  - Font Size: Small, Medium, Large
  - Shuffle on Load: Yes/No
  - Show Progress Bar: Yes/No  
  - Show Stats: Yes/No
  - Press LEFT on deck list to access settings
  - Settings persist between sessions

- **Sample Flashcard Decks**: 6 language learning decks included
  - Spanish Basics (CSV) - common phrases with pronunciation
  - Japanese Hiragana (TSV) - all hiragana with romaji
  - German Phrases (TXT) - travel phrases with pronunciation
  - Italian Travel (CSV) - essential travel vocabulary
  - Mandarin Basics (TXT) - common phrases with pinyin
  - Portuguese Phrases (TSV) - Brazilian Portuguese essentials

### Changed
- **Default Apps**: Simplified default app selection in portal
  - Now defaults to: Library, Chess, Demo, Settings
  - Previously included: Flashcards, Sudoku, Weather
  - Users can easily add more apps in portal Home Screen tab

- Settings menu reorganized with new bookmark and stats options
- Library plugin auto-resumes last book by default
- Improved page cache with image placeholder support

### Fixed
- **Blank pages when reading**: draw() was not calling drawReadingPage()
- Clicking Library now opens last book instead of always showing browser

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

## [2.1.0] - 2026-01-08

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

## [2.0.0] - 2026-01-05

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

## [1.0.0] - 2026-01-01

Initial release with basic e-reader functionality.
