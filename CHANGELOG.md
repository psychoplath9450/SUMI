# SUMI Changelog

All notable changes to this project will be documented in this file.

## [1.5.0] - 2026-01-28

### Breaking Changes
- **All EPUBs must be preprocessed via portal** - On-device EPUB parsing removed
- Old books will show "Process this book in the portal first" message
- **Home WiFi required for book processing** - Hotspot mode is games-only

### Added
- **Web Flasher** - Flash SUMI directly from your browser at `docs/flasher/`
  - Backup current firmware before flashing
  - Flash latest SUMI (auto-downloads from GitHub)
  - Flash custom .bin files (restore backups, other firmware)
  - Erase flash option for clean installs
  - Classic Mac OS-style UI matching the portal
- **Ready-to-use SD card contents** (`sample_sd/` folder)
  - 4 classic novels from Project Gutenberg
  - 20+ language flashcard decks
  - 10 e-ink optimized wallpapers
  - Chess pieces and weather icons
  - ASL alphabet images
- **EPUB3 support** - Both EPUB2 and EPUB3 formats now work seamlessly
- **Page preloading** - Next page pre-read into RAM for instant page turns
- **TOC extraction** - Chapter titles extracted from NCX (EPUB2) or nav.xhtml (EPUB3)
- **Smart typography** - Straight quotes → curly quotes, -- → em-dash, ... → ellipsis
- **Soft hyphenation** - Words get automatic break points for better line wrapping
- **Portal cache management** - New buttons to clear cache and reprocess all books
- **Reader settings live preview** - Font size, margins, spacing changes apply immediately
- **Chess piece rendering** - Newspaper diagram style (proper black/white piece contrast)
- **GitHub Pages landing page** - Project info and quick links at `docs/index.html`

### Changed
- **Setup wizard** - Now emphasizes home WiFi requirement for books
- **Portal banners** - Red warning when in hotspot mode (books unavailable)
- **Library navigation** - Improved button debouncing for faster response
- **Widget refresh** - Better partial refresh handling for weather widget borders
- **Installation options** - Web Flasher now primary recommendation

### Fixed
- **Book loading failures** - Fixed critical bug where PageCache was deleting portal's preprocessed chapter files
- **Hash mismatch for long filenames** - Books with 64+ character filenames now load correctly
- **TOC href matching** - Chapter select now properly maps TOC entries to chapters
- **Justification gaps** - Capped word spacing at 2x normal to prevent excessive gaps
- **Reader settings cursor** - Menu cursor now aligns with displayed options
- **Portal textAlign toggle** - Now accepts both boolean and integer values

### Removed (Dead Code Cleanup)
- `EpubParser.cpp/h` (1,148 lines) - On-device EPUB parsing
- `ExpatHtmlParser.cpp/h` (531 lines) - XML parsing
- `StreamingHtmlProcessor.h` (23KB) - Unused streaming processor
- `ZipReader.cpp/h` + miniz library (~65KB) - ZIP decompression
- `lib/expat/` - XML parsing library (~500KB source)
- `lib/miniz/` - ZIP library (~100KB source)

### Technical Improvements
- **~45KB more RAM** for reading (no ZIP buffer, no parsing libraries)
- **~110KB smaller firmware** (removed parsing code)
- **Instant page turns** - Next page pre-cached in RAM
- **Cleaner codebase** - 1,700+ lines of parsing code removed
- **Portal preprocessing v4** - TOC, smart quotes, soft hyphens, language detection

## [1.4.3] - 2026-01-26

### Added
- **Improved text layout** - better rendering for e-ink displays
  - Line height compression multiplier (Tight: 0.95x, Normal: 1.0x, Wide: 1.1x)
  - Viewable margins that account for e-ink panel edges
  - Extra Large font size option (4 sizes total)
- **Rich text support** in preprocessed books
  - **Bold** text preserved from EPUB
  - *Italic* text preserved from EPUB
  - Headers centered and bold
  - List bullets preserved
- **Paragraph indent mode** - em-dash indent when extra spacing disabled
- **Flashcard settings in portal** - configure from web interface
- **Memory fragmentation protection** - friendly reboot message instead of crash
- **Processing stuck detection** - Portal shows retry option after 30 seconds

### Changed
- Portal preprocessor outputs rich text markers
- Margins display as pixel values in settings
- Settings structure updated to v3

### Fixed
- Book widget cover not displaying
- Words concatenated in reader
- Portal tag stripping
- Home screen grid layout
- Library crash after portal use

## [1.4.2] - 2026-01-25

### Added
- **Browser-based EPUB processing** - Pre-processed in browser using JSZip
- **"Require Pre-processing" setting** (default: ON)
- **Rich book metadata** - word count, estimated pages, reading time
- **"My SUMI" summary page** - comprehensive config overview
- **Connection status banner** - 3-state network indicator
- **Unified "Open Portal" menu** - merged WiFi and Portal options

### Changed
- Portal requires internet for book processing
- Library scan skips EPUB parsing when preprocessing required
- Improved directory creation

### Fixed
- File descriptor leak in download endpoint
- mkdir endpoint returning 400
- Upload paths going to SD root
- Cover art scaling issues

## [1.4.1] - 2026-01-20

### Added
- Soft hyphen and zero-width character filtering
- KOReader sync support (KOSync protocol)
- Configurable full refresh frequency

### Fixed
- UTF-8 BOM handling in EPUB chapters
- Progress display during chapter loading

## [1.4.0] - 2026-01-15

### Added
- Two-tier book metadata caching (RAM + SD card)
- Streaming EPUB parser using Expat XML library
- Activity lifecycle for proper resource management

### Changed
- Library split into multiple source files
- Book list stored as binary index on SD card
- Chapter content streamed in 1KB chunks

### Fixed
- Memory fragmentation during long reading sessions
- WiFi memory cleanup when returning to reader

## [1.3.0] - 2026-01-10

### Added
- Cover image extraction and caching
- Reading statistics tracking
- Bookmark support with timestamps

### Changed
- Improved EPUB 2/3 TOC parsing
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
