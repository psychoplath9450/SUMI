# SUMI Changelog

All notable changes to this project will be documented in this file.

## [1.4.3] - 2026-01-26

### Added
- **Improved text layout** - better rendering for e-ink displays
  - Line height compression multiplier (Tight: 0.95x, Normal: 1.0x, Wide: 1.1x)
  - Viewable margins that account for e-ink panel edges (Top: 9px, Right/Bottom/Left: 3px)
  - Configurable screen margin added on top of viewable margins
  - Extra Large font size option (4 sizes total)
  - Status bar area properly reserved at bottom (22px)
- **Rich text support** in preprocessed books
  - **Bold** text preserved from EPUB (`<b>`, `<strong>`)
  - *Italic* text preserved from EPUB (`<i>`, `<em>`)
  - Headers centered and bold (`<h1>` - `<h6>`)
  - List bullets preserved (`<li>` → • )
  - Image placeholders shown (`[Image]` or `[Image: alt text]`)
  - Table placeholders shown (`[Table]`)
  - Soft hyphens preserved for better line breaking
- **New TextAlign enum** - Justified, Left, Center, Right alignment options
- **Paragraph indent mode** - when extra spacing disabled, uses em-dash indent instead
- **Flashcard settings in portal** - configure flashcards from the web portal
  - Font size: Small, Medium, Large, Extra Large (XL uses 2x scale)
  - Center Text: vertically and horizontally center question/answer in their areas
  - Shuffle Cards: randomize card order when loading deck
  - Show Progress Bar: display session progress at top
  - Show Stats: display correct/incorrect count
  - Live preview in portal shows settings changes
- **Memory fragmentation protection** - Library now checks for sufficient contiguous memory before launching, shows friendly "Memory fragmented - Please reboot" message instead of crashing
- **Processing stuck detection** - Portal shows "Stuck - click to retry" after 30 seconds without progress
- **Setup screen redesign** - single clean screen with card-style steps
  - Dynamic status bar shows hotspot vs home network connection
  - Larger text and better spacing for readability
- **Smart portal default tab** - Files tab default when on home network, WiFi tab on hotspot
- **Connection detection fix** - verifies actual internet access before showing "Connected" banner

### Changed
- Portal preprocessor now outputs rich text markers instead of plain text
- Margins now display as pixel values in settings (0px, 5px, 10px, 15px, 20px)
- Font sizes: Small (22px), Medium (26px), Large (30px), Extra Large (34px)
- Line spacing now affects actual line height via compression multiplier
- Settings structure updated to v3 (auto-migrates from v2)
- Books processed with older portal versions will still work (no markers = plain text)

### Technical
- `ReaderSettings.h` rewritten with proper viewable margin constants
- `TextLayout` now uses `_lineHeightMultiplier` for proper line compression
- New `addRichText()` method parses **bold**, *italic*, # headers, • bullets
- `applyFontSettings()` uses new settings accessors
- Cache key includes new margin format for proper invalidation

### Fixed
- **Book widget cover not displaying** - Cover JPEG was only being decoded on first page iteration of paged drawing, leaving it invisible
- **Words concatenated in reader** - Rich text markers weren't followed by spaces, causing "wordwordword" instead of "word word word"
- **Portal tag stripping** - Tags nested inside bold/italic/headers were stripped without preserving word boundaries
- **On-device EPUB processing** - Updated to output rich text markers and use proper font measurement
- **Font measurement warning** - Added debug logging when display not available for text measurement
- **Text breaking too early** - Space width calculation reduced by 50% to pack more words per line before justification
- **Home screen grid layout** - Portrait mode now always uses 2 columns (was incorrectly using 3 for 5-6 items)
- **Portal processing state lost** - Processing UI now persists when switching tabs during book processing
- **Widget defaults not syncing** - Portal now properly syncs widget visibility flags when switching preset sets
- **Library crash after portal use** - Added memory check before launching Library to catch heap fragmentation

### Improved
- ExpatHtmlParser now outputs rich text markers (**bold**, *italic*, # headers) for on-device processing
- On-device EPUB and TXT file processing now uses `addRichText()` for consistent formatting
- applyFontSettings now sets bold font for rich text rendering

## [1.4.2] - 2026-01-25

### Added
- **Browser-based EPUB processing** - EPUBs are now pre-processed in the browser
  - Extracts metadata, chapters, and cover art using JSZip
  - Converts HTML chapters to rich text with formatting markers
  - Uploads processed files to `/.sumi/books/{hash}/` cache
  - ESP32 reads rich text files instead of parsing complex EPUBs
  - Dramatically faster book loading (~200ms vs 2-5s)
- **"Require Pre-processing" setting** (default: ON)
  - On-device EPUB parsing was causing memory issues and reliability problems
  - When enabled, only pre-processed books can be opened
  - Friendly error message: "Process this book in the portal first"
  - Can be disabled in Reader Settings if you want legacy behavior
- **Rich book metadata** - word count, estimated pages, reading time
  - `meta.json` now includes `totalWords`, `estimatedPages`, `estimatedReadingMins`
  - Per-chapter word and character counts
  - Displayed in portal library grid
- **"My SUMI" summary page** - replaced dashboard with comprehensive config overview
  - Six color-coded cards: Device Status, Connection, SD Contents, Home Screen, Settings
  - Shows file counts, book processing status, all current settings
  - Auto-refreshes on page navigation
- **Connection status banner** - 3-state indicator for network status
  - Hotspot mode (yellow): prompts to connect SUMI to WiFi
  - Transitional (green): SUMI connected, suggests switching browser
  - Home network (green): fully connected with checkmark
- **Unified "Open Portal" menu** - merged WiFi and Portal into single option
  - Choose connection mode: Create Hotspot or Use Home WiFi
  - Shows `sumi.local` alongside IP address
- **File download API** (`/api/download`) - proper chunked file streaming from SD card

### Changed
- Portal now requires internet access for book processing (JSZip loaded from CDN)
- Library scan skips EPUB parsing when Require Pre-processing is enabled (faster scanning)
- Upload handler auto-creates parent directories
- 50ms delay between chapter uploads to prevent overwhelming ESP32
- Improved `ensureDirectory()` creates `/.sumi` and `/.sumi/books` before book cache
- After portal deploy, returns to home screen instead of staying in settings

### Fixed
- **File descriptor leak** in download endpoint - was causing "no free file descriptors" errors
- **mkdir endpoint** returning 400 - now creates parent directories first
- **Hostname detection** - `sumi.local` now correctly identified as home network (mDNS works from both)
- **Upload paths** - files no longer end up in SD card root
- **Route conflict** - `/api/files/download` was caught by `/api/files`, renamed to `/api/download`
- **Cover art scaling** - fixed white squares/gaps in dithered cover images

### Technical
- Download endpoint uses static file handle to avoid memory leaks
- Upload handler logs full path construction for debugging
- Book cache structure: `/.sumi/books/{hash}/meta.json`, `cover_thumb.jpg`, `cover_full.jpg`, `ch_000.txt`...
- Cover images pre-resized: 80×120 (widget), 300px wide (library)
- Bayer dithering for smoother grayscale rendering on covers

## [1.4.1] - 2026-01-20

### Added
- Soft hyphen and zero-width character filtering in HTML processor
- KOReader sync support (KOSync protocol)
- Configurable full refresh frequency in reader settings

### Fixed
- UTF-8 BOM handling in EPUB chapters
- Progress display during chapter loading

## [1.4.0] - 2026-01-15

### Added
- Two-tier book metadata caching (RAM + SD card)
- Streaming EPUB parser using Expat XML library
- Activity lifecycle for proper resource management

### Changed
- Library split into multiple source files for faster compilation
- Book list now stored as binary index on SD card
- Chapter content streamed in 1KB chunks instead of loading entirely

### Fixed
- Memory fragmentation during long reading sessions
- WiFi memory cleanup when returning to reader

## [1.3.0] - 2026-01-10

### Added
- Cover image extraction and caching
- Reading statistics tracking
- Bookmark support with timestamps

### Changed
- Improved EPUB 2 (NCX) and EPUB 3 (NAV) TOC parsing
- Better error handling for malformed EPUBs

### Fixed
- Display buffer overflow on large chapters
- Progress saving on chapter change

## [1.2.0] - 2026-01-06

### Added
- TXT file support in reader
- Chapter selection menu
- Last book resume on startup

### Changed
- Switched to paged display buffer mode (saves ~38KB RAM)
- Improved text layout justification

## [1.1.0] - 2026-01-03

### Added
- Basic EPUB reading support
- Library browser with flip and list views
- Reader settings (font size, margins, line spacing)

### Fixed
- SD card initialization on some devices
- Button debouncing issues

## [1.0.0] - 2026-01-01

### Added
- Initial release
- Home screen with app grid
- Settings portal via WiFi
- Weather widget
- Flashcards app
- Games: Chess, Checkers, Sudoku, Minesweeper, Solitaire
- Notes app
- Image viewer
- Power management with deep sleep
