# SUMI E-Reader Architecture

*Last updated: January 28, 2026*

Technical documentation for SUMI's e-reader implementation.

## Overview

SUMI's e-reader is designed for the ESP32-C3's memory constraints (380KB RAM) while supporting arbitrarily large EPUB files.

### Browser-Based Processing

SUMI uses **browser-based pre-processing** for all books. EPUBs are processed in the portal (on your phone/computer) before they can be read. This design choice was made because:

1. On-device ZIP decompression and XML parsing consumed too much RAM
2. Complex EPUBs frequently crashed the embedded parser
3. Browser processing enables features like smart typography and hyphenation
4. Books load instantly with no parsing delay

**How it works:**
1. Upload EPUB to portal
2. Portal extracts text chapters using JSZip (in browser)
3. Rich text files saved to SD: `/.sumi/books/{hash}/ch_000.txt`, etc.
4. Device reads pre-extracted text - no ZIP or XML parsing needed

**Rich Text Markers:**
The portal preprocessor outputs formatting markers that the device interprets:
- `**bold text**` - Bold formatting (`<b>`, `<strong>`)
- `*italic text*` - Italic formatting (`<i>`, `<em>`)
- `# Header text` - Headers (`<h1>` - `<h6>`) - rendered centered and bold
- `• item` - List bullets (`<li>`)
- `[Image]` or `[Image: alt text]` - Image placeholders
- `[Table]` - Table placeholders
- Soft hyphens (U+00AD) preserved for better line breaking

**Benefits:**
- Much faster chapter loading (~200ms vs 2-5 seconds)
- More reliable - no memory allocation failures
- Simpler device code - just read text files
- Better text quality - browser has full HTML parser
- Rich text formatting preserved (bold, italic, headers)
- Smart typography (curly quotes, em-dashes, ellipses)
- Soft hyphenation for better line wrapping

## Pre-Processed Cache Structure

```
/.sumi/books/{hash}/
├── meta.json          # Book metadata, chapter list, word count
├── toc.json           # Table of contents with chapter titles
├── cover_full.jpg     # Full-size cover (300px wide)
├── cover_thumb.jpg    # Thumbnail cover (120x180)
├── ch_000.txt         # Chapter 0 rich text (with markers)
├── ch_001.txt         # Chapter 1 rich text
└── ...
```

The hash is computed from the EPUB filename using a simple string hash function (`hash * 31 + char`). This must match between portal and device.

## Text Layout System

The `TextLayout` class handles pagination with proper typography:

### Margin System
```
┌──────────────────────────────────┐
│ ← Viewable Top (9px)             │  Panel edge compensation
├──────────────────────────────────┤
│ ← Screen Margin (user setting)   │  0-20px configurable
├──────────────────────────────────┤
│                                  │
│    Content Area                  │  Where text renders
│                                  │
├──────────────────────────────────┤
│ ← Screen Margin (user setting)   │
├──────────────────────────────────┤
│ ← Status Bar Area (22px)         │  Chapter / page display
├──────────────────────────────────┤
│ ← Viewable Bottom (3px)          │  Panel edge compensation
└──────────────────────────────────┘
```

**Viewable Margins** (hardcoded, compensate for e-ink panel edges):
- Top: 9px, Right: 3px, Bottom: 3px, Left: 3px

**Screen Margin** (user configurable): 0px, 5px, 10px, 15px, or 20px

### Line Height Compression
Line spacing uses a multiplier on the font's natural height:
- **Tight**: 0.95x (more text per page)
- **Normal**: 1.0x (default)
- **Wide**: 1.1x (easier reading)

### Font Sizes
- **Small**: 22px base height
- **Medium**: 26px base height  
- **Large**: 30px base height
- **Extra Large**: 34px base height

### Rich Text Rendering
The `addRichText()` method parses markers and applies styling:
```cpp
// Parsing state machine:
// - "**" toggles bold
// - "*" toggles italic (when not preceded by *)
// - "# " at line start = header (centered, bold)
// - "• " at line start = bullet point
// - "[Image]" = italic placeholder
```

Font variants are set via `setFont()`, `setBoldFont()`, `setItalicFont()`, `setBoldItalicFont()`.

## Rendering Pipeline

```
User Opens Book
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 1. LOAD BOOK METADATA                                       │
│    - Read meta.json from /.sumi/books/{hash}/               │
│    - Get chapter count, title, author                       │
│    - Load TOC if available                                  │
│    - Memory: ~2KB for metadata                              │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. LOAD CHAPTER TEXT                                        │
│    - Read ch_XXX.txt from cache directory                   │
│    - Plain text with rich text markers                      │
│    - Streamed in chunks to avoid memory spikes              │
│    - Memory: ~4KB buffer at a time                          │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. TEXT LAYOUT (TextLayout.cpp)                             │
│    - Parse rich text markers (**bold**, *italic*, etc.)     │
│    - Measure words using display.getTextBounds()            │
│    - Perform line breaking (greedy algorithm)               │
│    - Calculate justified positions (if enabled)             │
│    - Cap justification gaps at 2x normal spacing            │
│    - Emit CachedPage when page fills                        │
│    - Memory: ~14KB per page being built                     │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. PAGE CACHING (PageCache.cpp)                             │
│    - Each CachedPage saved to SD immediately                │
│    - Format: Binary (word positions, line Y coords)         │
│    - Path: /.sumi/books/{HASH}/pages/0_0.bin                │
│    - Cache key: fontSize + margins + lineSpacing + screen   │
│    - Invalidated when settings change                       │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. PAGE RENDERING (Library.h::drawReadingPage)              │
│    - Load CachedPage from SD (~5KB)                         │
│    - Iterate lines, draw each word at pre-computed X,Y      │
│    - GxEPD2 paged rendering (firstPage/nextPage loop)       │
│    - Apply bold/italic fonts based on word style flags      │
└─────────────────────────────────────────────────────────────┘
```

## Memory Management

Reader subsystems (TextLayout, PageCache) are allocated when the Library app opens and freed when it closes. They don't exist on the home screen or in other apps.

```
HEAP USAGE BY CONTEXT:

Home Screen (~5KB active)
├── Core systems only
├── Widgets read from cache files
└── ~150KB+ free for other operations

Library Open (~15KB active)
├── LibraryApp: ~5KB
├── TextLayout: ~4KB (allocated by LibraryApp)
├── PageCache: ~2KB (allocated by LibraryApp)
├── CachedPage: ~5KB (temporary, freed after render)
└── ~135KB free

Reading with WiFi Off (~30KB more free)
├── WiFi suspended during reading
├── Frees ~30-45KB of heap
└── ~165KB+ available
```

**Key Memory Optimizations:**
1. **Portal preprocessing** - No ZIP/XML libraries needed on device
2. **Streaming text loading** - Chapters read in chunks
3. **Immediate cache flush** - Pages saved to SD as built, then freed
4. **WiFi suspension** - WiFi turned off during reading to free memory
5. **SD card as extended storage** - 32GB microSD acts as swap space

## Page Cache Structure

```
/.sumi/books/{hash}/
├── meta.bin           # CacheKey + chapter count
├── progress.bin       # Last read position
├── bookmarks.bin      # User bookmarks
└── pages/
    ├── 0_0.bin        # Chapter 0, Page 0
    ├── 0_1.bin        # Chapter 0, Page 1
    └── ...
```

**Note:** The PageCache only manages its own files (meta.bin, progress.bin, pages/*). Portal's preprocessed files (ch_XXX.txt, meta.json, covers) are never modified by PageCache.

### CachedPage Binary Format

```cpp
struct CachedWord {           // 24 bytes each
    uint16_t xPos;            // X position (0-480)
    uint8_t style;            // 0=normal, 1=bold, 2=italic, 3=bold+italic
    uint8_t length;           // Text length
    char text[20];            // Word text (truncated if longer)
};

struct CachedLine {           // ~244 bytes each (10 words max)
    uint16_t yPos;            // Y baseline position
    uint8_t wordCount;        // Words on this line (0-10)
    uint8_t flags;            // isLastInPara, isImage, isHeader
    CachedWord words[10];
};

struct CachedPage {           // ~4.9KB each (20 lines max)
    uint8_t lineCount;
    CachedLine lines[20];
};
```

### Cache Invalidation

Cache is invalidated when any of these change:
- Font size
- Margins  
- Line spacing
- Justify setting
- Screen dimensions (orientation change)

## Display Refresh Strategy

```cpp
enum RefreshMode {
    REFRESH_PARTIAL,   // Fast, some ghosting (page turns)
    REFRESH_HALF,      // Medium quality (every 10 pages)
    REFRESH_FULL       // Full black-white-content (every 30 pages)
};

// Configuration in ReaderSettings:
- pagesPerHalfRefresh: 10
- pagesPerFullRefresh: 30 (configurable via portal)
```

**Refresh Flow:**
```
Page Turn
   │
   ├─► Counter check
   │      │
   │      ├─► partialCount < 10 → Partial refresh (200ms)
   │      ├─► partialCount < 30 → Half refresh (400ms)  
   │      └─► partialCount >= 30 → Full refresh (1200ms)
   │
   └─► Render with GxEPD2 paged mode
          - display.setFullWindow()
          - display.firstPage() / nextPage() loop
```

## Features

### Font Settings
- **Font Size**: Small (22px), Medium (26px), Large (30px), Extra Large (34px)
- **Line Spacing**: Tight (0.95x), Normal (1.0x), Wide (1.1x)
- **Margins**: 0px, 5px, 10px, 15px, 20px (plus viewable edge compensation)
- **Justification**: Left-aligned or Justified text (with 2x gap cap)
- **Paragraph Indent**: Automatic first-line indent for paragraphs
- All settings configurable via reader menu or web portal
- Settings changes invalidate cache (pages re-flow)
- **Live preview**: Changes apply immediately when adjusted in reader menu

### Status Bar
- Shows chapter info on left (e.g., "Ch 3 - 1/12")
- Shows reading progress percentage on right
- Horizontal separator line above

### Bookmarks
- Up to 20 bookmarks per book
- Stored in `/.sumi/books/{hash}/bookmarks.bin`
- Accessible from Settings menu while reading
- Jump to any bookmarked chapter/page

### Reading Statistics
- Tracks: pages read, time spent, books finished
- Session stats and lifetime stats
- Stored in `/.sumi/reading_stats.bin`
- Viewable on device and in web portal
- Can be reset from portal

### Quick-Open
- Library plugin auto-resumes last book
- Press BACK to go to browser instead
- Last book info stored in `/.sumi/lastbook.bin`

### Chapter Select
- Shows extracted TOC with chapter titles
- Navigate with UP/DOWN, select with OK
- Chapter titles loaded from toc.json

## Performance

| Operation | Time |
|-----------|------|
| Page turn (cached) | ~200ms |
| Page turn (uncached) | ~800ms |
| Chapter load (preprocessed) | ~500ms |
| Full refresh | ~1200ms |
| Book open (preprocessed) | ~500ms |

## Button Mapping

| Button | Reading Mode | Menu Mode |
|--------|-------------|-----------|
| LEFT | Previous page | Navigate |
| RIGHT | Next page | Navigate |
| UP | Chapter select | Navigate |
| DOWN/OK | Settings menu | Select |
| BACK | Close book | Back |

## Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         SUMI FIRMWARE                           │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  main.cpp    │  │ HomeScreen   │  │ SettingsScreen│         │
│  │  (loop)      │  │ (grid UI)    │  │ (menu UI)    │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                 │                 │                   │
│         ▼                 ▼                 ▼                   │
│  ┌─────────────────────────────────────────────────────┐       │
│  │                   AppLauncher                        │       │
│  │  (routes to plugins based on home item selection)    │       │
│  └─────────────────────────┬───────────────────────────┘       │
│                            │                                    │
│  ┌────────────┬────────────┼────────────┬────────────┐        │
│  ▼            ▼            ▼            ▼            ▼        │
│ Library    Weather    Flashcards    Games      Settings       │
│  │                                                              │
│  └─► Reads pre-processed text from /.sumi/books/{hash}/        │
│                            │                                   │
│                            ▼                                   │
│  ┌─────────────────────────────────────────────────────┐      │
│  │                   Core Services                      │      │
│  │  ┌──────────────┬──────────────┬──────────────┐     │      │
│  │  │ PageCache    │ TextLayout   │ WebServer    │     │      │
│  │  │ (SD caching) │ (pagination) │ (portal+API) │     │      │
│  │  └──────────────┴──────────────┴──────────────┘     │      │
│  │  ┌──────────────┬──────────────┬──────────────┐     │      │
│  │  │ WiFiManager  │ PowerManager │ Settings     │     │      │
│  │  │ (credentials)│ (deep sleep) │ (NVS + SD)   │     │      │
│  │  └──────────────┴──────────────┴──────────────┘     │      │
│  └─────────────────────────────────────────────────────┘      │
│                            │                                   │
│                            ▼                                   │
│  ┌─────────────────────────────────────────────────────┐      │
│  │                   Hardware Layer                     │      │
│  │  GxEPD2 (display) │ SD.h (storage) │ GPIO (buttons) │      │
│  └─────────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────────┘
```

## Troubleshooting

### Book Won't Open / "Process in portal" Error
1. Book hasn't been preprocessed - use portal to process it
2. Hash mismatch - clear cache and reprocess (portal has "Clear Cache" button)
3. Filename too long - rename the EPUB to something shorter

### Blank Pages
1. Check cache file exists: `/.sumi/books/{hash}/ch_XXX.txt`
2. Verify `cacheValid` is true after chapter load
3. Check layout page files: `/.sumi/books/{hash}/pages/X_Y.bin`

### Slow Page Turns
1. Layout cache may be invalidated - check settings haven't changed
2. SD card may be slow - use Class 10 or better
3. Large chapter being re-laid-out

### Cover Not Showing
1. Check `cover_full.jpg` or `cover_thumb.jpg` exists in book cache
2. Reprocess book through portal
3. Some EPUBs don't include cover images
