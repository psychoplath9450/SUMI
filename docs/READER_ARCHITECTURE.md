# SUMI E-Reader Architecture

*Last updated: January 25, 2026*

Technical documentation for SUMI's e-reader implementation.

## Overview

SUMI's e-reader is designed for the ESP32-C3's memory constraints (384KB RAM) while supporting arbitrarily large EPUB files.

### Pre-Processing Mode (Default)

As of v1.4.2, SUMI uses **browser-based pre-processing** by default. EPUBs are processed in the portal (on your phone/computer) before they can be read. This approach was adopted because on-device ZIP decompression and XML parsing was consuming too much memory and causing reliability issues on the ESP32-C3.

**How it works:**
1. Upload EPUB to portal
2. Portal extracts text chapters using JSZip (in browser)
3. Rich text files saved to SD: `/.sumi/books/{hash}/ch_000.txt`, etc.
4. Device reads text and applies formatting - no ZIP or XML parsing needed

**Rich Text Markers (v1.4.3+):**
The portal preprocessor outputs formatting markers that the device interprets:
- `**bold text**` - Bold formatting (`<b>`, `<strong>`)
- `*italic text*` - Italic formatting (`<i>`, `<em>`)
- `# Header text` - Headers (`<h1>` - `<h6>`) - rendered centered and bold
- `• item` - List bullets (`<li>`)
- `[Image]` or `[Image: alt text]` - Image placeholders
- `[Table]` - Table placeholders
- Soft hyphens (U+00AD) are preserved for better line breaking

**Benefits:**
- Much faster chapter loading (~200ms vs 2-5 seconds)
- More reliable - no memory allocation failures
- Simpler device code - just read text files
- Better text quality - browser has full HTML parser
- Rich text formatting preserved (bold, italic, headers)

### Legacy Mode (Optional)

On-device EPUB parsing can be re-enabled in Reader Settings by turning off "Require Pre-processing". This also outputs rich text markers and uses the same `addRichText()` rendering path, but is less reliable on complex EPUBs.

## Pre-Processed Cache Structure

```
/.sumi/books/{hash}/
├── meta.json          # Book metadata, chapter list
├── cover_full.jpg     # Full-size cover (300px wide)
├── cover_thumb.jpg    # Thumbnail cover (80px wide)
├── ch_000.txt         # Chapter 0 rich text (with markers)
├── ch_001.txt         # Chapter 1 rich text
└── ...
```

The hash is computed from the EPUB filename using a simple string hash function (must match between portal and device).

## Text Layout System (v1.4.3+)

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

## Legacy Rendering Pipeline (when pre-processing disabled)

```
User Opens Book
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 1. EPUB EXTRACTION (EpubParser.cpp)                         │
│    - Opens .epub as ZIP via miniz                           │
│    - Parses container.xml → finds content.opf               │
│    - Extracts spine order (chapter list)                    │
│    - Does NOT extract full content to RAM                   │
│    - Memory: ~2KB for metadata                              │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. CHAPTER STREAMING (epub.streamChapterToFile)             │
│    - Streams chapter HTML from ZIP to temp file on SD       │
│    - Memory used: Only ~4KB buffer at a time                │
│    - Temp file: /.sumi/temp_chX.html                        │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. HTML PARSING (ExpatHtmlParser.cpp)                       │
│    - Streaming XML parser (expat library)                   │
│    - Callback on each paragraph: onParagraph(text, isHeader)│
│    - Handles: <p>, <div>, <br>, <h1-h6>, <hr>              │
│    - HTML entities decoded inline                           │
│    - Memory: ~8KB parser state                              │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. TEXT LAYOUT (TextLayout.cpp)                             │
│    - Receives paragraphs one at a time                      │
│    - Measures words using display.getTextBounds()           │
│    - Performs line breaking (greedy algorithm)              │
│    - Calculates justified positions (if enabled)            │
│    - Emits CachedPage when page fills                       │
│    - Memory: ~14KB per page being built                     │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. PAGE CACHING (PageCache.cpp)                             │
│    - Each CachedPage saved to SD immediately                │
│    - Format: Binary (word positions, line Y coords)         │
│    - Path: /.sumi/books/{HASH}/pages/0_0.bin                │
│    - Cache key: fontSize + margins + lineSpacing + screen   │
│    - Invalidated when settings change                       │
└─────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│ 6. PAGE RENDERING (Library.h::drawReadingPage)              │
│    - Load CachedPage from SD (~14KB)                        │
│    - Iterate lines, draw each word at pre-computed X,Y      │
│    - GxEPD2 paged rendering (firstPage/nextPage loop)       │
│    - Handles images if present in cache                     │
└─────────────────────────────────────────────────────────────┘
```

## Memory Management

Reader subsystems (TextLayout, PageCache, EpubParser) are allocated when the Library app opens and freed when it closes. They don't exist on the home screen or in other apps.

```
HEAP USAGE BY CONTEXT:

Home Screen (~5KB active)
├── Core systems only
├── Widgets read from cache files (no plugins needed)
└── ~150KB+ free for other operations

Library Open (~20KB active)
├── LibraryApp: ~5KB
├── TextLayout: ~4KB (allocated by LibraryApp)
├── PageCache: ~2KB (allocated by LibraryApp)
├── EpubParser: ~2KB (allocated by LibraryApp)
├── CachedPage: ~5KB (temporary, freed after render)
└── ~130KB free

Other Plugin Open (~varies)
├── Only that plugin allocated
├── Reader subsystems NOT loaded
└── Maximum RAM available for the plugin
```

**Key Memory Optimizations:**
1. **Streaming parsing** - Never loads full chapter to RAM
2. **Immediate cache flush** - Pages saved to SD as built, then vector cleared
3. **Heap allocation for large structs** - CachedPage uses `new` not stack
4. **SD card as extended storage** - 32GB microSD acts as swap space

## Page Cache Structure

```
/.sumi/
├── books/
│   └── A7F3C2B1/              # Hash of book path
│       ├── meta.bin           # CacheKey + chapter count
│       ├── progress.bin       # Last read position
│       ├── bookmarks.bin      # User bookmarks
│       └── pages/
│           ├── 0_0.bin        # Chapter 0, Page 0
│           ├── 0_1.bin        # Chapter 0, Page 1
│           └── ...
├── reading_stats.bin          # Global reading statistics
├── lastbook.bin               # Last opened book info
└── temp_ch0.html              # Temp file (deleted after use)
```

### CachedPage Binary Format

```cpp
struct CachedWord {           // 24 bytes each
    uint16_t xPos;            // X position (0-800)
    uint8_t style;            // 0=normal, 1=bold, 2=italic
    uint8_t length;           // Text length
    char text[20];            // Word text (truncated if longer)
};

struct CachedLine {           // ~244 bytes each (10 words max)
    uint16_t yPos;            // Y baseline position
    uint8_t wordCount;        // Words on this line (0-10)
    uint8_t flags;            // isLastInPara, isImage
    CachedWord words[10];     // Or ImageInfo if isImage
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
- pagesPerFullRefresh: 30
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
- **Justification**: Left-aligned or Justified text
- **Extra Paragraph Spacing**: Optional half-line-height spacing between paragraphs
- All settings configurable via reader menu or web portal
- Settings changes invalidate cache (pages re-flow)

### Status Bar
- Shows chapter name on left (truncated if too long)
- Shows page number (X / Y) on right
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

### Scene Break Spacing
- Detects `<hr>` tags in EPUB content
- Adds configurable vertical spacing (0-60px)
- Default: 30px

### EPUB Images (Beta)
- Images extracted from EPUB to cache
- Stored as special "image lines" in page cache
- Rendered using TJpgDec with dithering
- Centered within page margins

## Performance

| Operation | Time |
|-----------|------|
| Page turn (cached) | ~200ms |
| Page turn (uncached) | ~800ms |
| Chapter load | ~1500ms |
| Full refresh | ~1200ms |
| Book open (cached) | ~500ms |
| Book open (first time) | ~3000ms |

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
│  │            │            │            │            │        │
│  └────────────┴────────────┴────────────┴────────────┘        │
│                            │                                   │
│                            ▼                                   │
│  ┌─────────────────────────────────────────────────────┐      │
│  │                   Core Services                      │      │
│  │  ┌──────────────┬──────────────┬──────────────┐     │      │
│  │  │ PageCache    │ TextLayout   │ EpubParser   │     │      │
│  │  │ (SD caching) │ (pagination) │ (ZIP + XML)  │     │      │
│  │  └──────────────┴──────────────┴──────────────┘     │      │
│  │  ┌──────────────┬──────────────┬──────────────┐     │      │
│  │  │ WiFiManager  │ WebServer    │ PowerManager │     │      │
│  │  │ (credentials)│ (portal API) │ (deep sleep) │     │      │
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

### Blank Pages
If pages appear blank:
1. Check `draw()` is calling `drawReadingPage()` for READING state
2. Verify `cacheValid` is true after chapter load
3. Check cache file exists: `/.sumi/books/{hash}/pages/X_Y.bin`

### Slow Page Turns
1. Cache may be invalidated - check settings haven't changed
2. SD card may be slow - use Class 10 or better
3. Large chapter being re-indexed

### Memory Crashes
1. Reduce concurrent operations
2. Check heap before large allocations: `ESP.getFreeHeap()`
3. Use streaming parser instead of loading full content
