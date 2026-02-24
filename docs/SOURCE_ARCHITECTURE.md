# SUMI Source Architecture

## Directory Structure

```
src/
├── main.cpp                 # Entry point, boot mode detection, initialization
├── config.h                 # Build configuration, feature flags
│
├── core/                    # Core systems
│   ├── MemoryArena.h/cpp    # Pre-allocated buffer pool (82KB: 32+26+24) + bump allocator
│   ├── Core.h/cpp           # Global state container
│   ├── StateMachine.h/cpp   # State transitions
│   ├── EventQueue.h         # Input event queue
│   ├── SumiSettings.h/cpp   # Persistent settings
│   ├── BootMode.h/cpp       # UI/Reader mode detection
│   ├── Result.h/cpp         # Error handling
│   └── Types.h              # Common types
│
├── states/                  # Application states
│   ├── State.h              # Base state interface
│   ├── HomeState.h/cpp      # Home screen with carousel
│   ├── ReaderState.h/cpp    # Book reading
│   ├── FileListState.h/cpp  # File browser
│   ├── SettingsState.h/cpp  # Settings menus
│   ├── PluginHostState.h/cpp # Plugin sandbox
│   ├── PluginListState.h/cpp # App launcher
│   ├── SleepState.h/cpp     # Deep sleep
│   ├── ErrorState.h/cpp     # Error display
│   └── StartupState.h/cpp   # Initial state
│
├── content/                 # Content providers
│   ├── ContentHandle.h/cpp  # Unified content interface
│   ├── EpubProvider.h/cpp   # EPUB reader
│   ├── ComicProvider.h/cpp  # Comic/manga reader
│   ├── TxtProvider.h/cpp    # Plain text
│   ├── MarkdownProvider.h/cpp # Markdown
│   ├── XtcProvider.h/cpp    # XTC pre-rendered format
│   ├── LibraryIndex.h/cpp   # Book metadata cache
│   ├── RecentBooks.h/cpp    # Reading history
│   ├── ThumbnailCache.h/cpp # LittleFS thumbnail cache
│   ├── ProgressManager.h/cpp # Page tracking
│   ├── ReaderNavigation.h/cpp # TOC navigation
│   └── ContentTypes.h/cpp   # Content type detection
│
├── drivers/                 # Hardware abstraction
│   ├── Display.h/cpp        # E-ink display control
│   ├── Input.h/cpp          # Button input
│   └── Storage.h/cpp        # SD card access
│
├── ble/                     # Bluetooth
│   ├── BleFileTransfer.h/cpp # Wireless file upload
│   └── BleHid.h/cpp         # Keyboard/page turner support
│
├── plugins/                 # Built-in apps
│   ├── PluginInterface.h    # Plugin base class
│   ├── PluginRenderer.h     # Drawing API
│   ├── PluginHelpers.h      # Shared utilities
│   ├── LuaPlugin.h/cpp      # Lua scripting runtime
│   ├── LuaBindings.h        # Lua-to-C bindings
│   ├── ChessGame.h          # Chess
│   ├── Sudoku.h             # Sudoku
│   ├── Solitaire.h          # Solitaire
│   ├── Minesweeper.h        # Minesweeper
│   ├── Checkers.h           # Checkers
│   ├── Flashcards.h         # Flashcard study
│   ├── Notes.h              # Text editor
│   ├── TodoList.h           # Task list
│   ├── Images.h             # Image viewer
│   ├── Maps.h               # Map viewer
│   ├── ToolSuite.h          # System tools
│   ├── SumiBoy.h            # GB emulator launcher
│   ├── SumiBoyEmulator.h/cpp # GB emulator core
│   ├── SumiBoyRomPicker.h   # ROM file picker
│   ├── gb_controls_img.h    # Control overlay image
│   └── icons/               # Plugin icon assets
│       ├── card_back_icon.h
│       ├── checkers_icons.h
│       ├── minesweeper_icons.h
│       └── suit_icons.h
│
├── rendering/               # Page rendering
│   └── XtcPageRenderer.h/cpp # XTC format renderer
│
├── ui/                      # UI framework
│   ├── Elements.h/cpp       # UI primitives
│   ├── Views.h              # View index
│   └── views/               # State-specific views
│       ├── HomeView.h/cpp       # Home screen layout
│       ├── ReaderViews.h/cpp    # Reader UI components
│       ├── SettingsViews.h/cpp  # Settings UI
│       ├── UtilityViews.h/cpp   # Utility UI components
│       └── BootSleepViews.h/cpp # Boot and sleep screens
│
├── assets/                  # Embedded resources
│   └── sumi_home_bg.h       # Default home art
│
├── IniParser.h/cpp          # INI config file parsing
├── Theme.h                  # Theme data structures
│
└── images/                  # Logos and icons
    └── SumiLogo.h           # Boot logo
```

## Key Libraries (lib/)

| Library | Purpose |
|---------|---------|
| Epub | EPUB parsing, OPF, TOC, CSS, HTML-to-pages, Liang hyphenation, DP line breaking |
| GfxRenderer | 1-bit graphics, fonts, dithering (OrderedDither), bitmap helpers |
| EInkDisplay | SSD1677 driver, refresh modes, grayscale via dual RAM banks |
| ImageConverter | Image conversion factory and base interface |
| JpegToBmpConverter | JPEG to 1-bit/2-bit BMP conversion with JPEGDEC dithering |
| PngToBmpConverter | PNG to 1-bit BMP conversion with ordered dithering |
| ComicReader | Comic/manga page layout and rendering |
| ZipFile | EPUB ZIP decompression, uses arena zipBuffer for LZ77 dictionary |
| PageCache | Page caching system, content parsers (EpubChapterParser, PlainTextParser) |
| AsyncTask | BackgroundTask (heap + static allocation), ScopedMutex |
| EpdFont | Font loading/rendering, streaming fonts, 17 built-in font files |
| ExternalFont | Custom user font loading from SD card |
| Xtc | XTC pre-rendered format reader/writer |
| Group5 | Fax compression for thumbnails |
| FsHelpers | Path normalization, file system utilities |
| Serialization | Binary serialization for settings, progress, metadata |
| Markdown | Markdown parser and renderer |
| Txt | Plain text file parser |
| Html5 | HTML5 tag normalization |
| Utf8 | UTF-8 string utilities |
| SDCardManager | SD card abstraction |
| InputManager | Button debouncing |
| BatteryMonitor | ADC battery reading |
| ArabicShaper | Arabic text shaping (contextual forms) |
| ThaiShaper | Thai text segmentation and clustering |
| ScriptDetector | Unicode script detection for text shaping |
| miniz | zlib decompression (used by ZipFile) |
| expat | Streaming XML parser (used by EPUB parsers) |
| lua54 | Lua 5.4 VM for user scripting |
| picojpeg | Legacy JPEG decoder (JPEGDEC is primary, installed via lib_deps) |

**External dependencies** (installed via platformio.ini `lib_deps`):
- `JPEGDEC` — Primary JPEG decoder with built-in dithering
- `NimBLE-Arduino` — BLE stack
- `ArduinoJson` — JSON parsing
- `pngle` — Streaming PNG decoder
- `SdFat` — SD card FAT32 filesystem

## Boot Sequence

```
setup()
├── earlyInit()
│   ├── Serial, GPIO, SPI init
│   ├── Boot loop guard check
│   ├── SD card mount
│   ├── Settings load
│   ├── LittleFS mount
│   └── MemoryArena::init()  ← 3-block allocation (32+26+24KB)
│
├── detectBootMode()
│   └── Check RTC for UI/READER flag
│
├── initUIMode() OR initReaderMode()
│   ├── Font/theme init
│   ├── State registration
│   └── Initial state transition
│
└── loop()
    ├── Input polling
    ├── BLE inactivity timeout check
    ├── Auto-sleep check
    └── StateMachine::update()
```

## State Transitions

```
         ┌──────────────────────────────────────────┐
         │                                          │
         ▼                                          │
    ┌─────────┐     ┌─────────┐     ┌─────────┐    │
    │  Home   │────▶│FileList │────▶│ Reader  │────┘
    └─────────┘     └─────────┘     └─────────┘
         │               │               │
         │               │               │
         ▼               ▼               ▼
    ┌─────────┐     ┌─────────┐     ┌─────────┐
    │Settings │     │ Plugins │     │  Sleep  │
    └─────────┘     └─────────┘     └─────────┘
```

## Memory Flow

### Image Processing

```
JPEG/PNG on SD
      │
      ▼
┌─────────────────┐
│ Decoder uses    │
│ primaryBuffer   │◀── MemoryArena (32KB, shared with ZIP)
│ imageRowRegion  │◀── MemoryArena (4KB)
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ Ditherer uses   │
│ ditherRegion    │◀── MemoryArena (8KB)
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ Output BMP      │
│ written to SD   │
└─────────────────┘
```

### Thumbnail Cache

```
First boot:                    Subsequent boots:

HomeState                      HomeState
    │                              │
    ▼                              ▼
Decode from SD (~500ms)        ThumbnailCache::load()
    │                              │
    ▼                              ▼
scratchBuffer ──────────────▶  scratchBuffer
    │                              │
    ▼                              │
ThumbnailCache::store()            │
    │                              │
    ▼                              ▼
LittleFS /thumbs/              Blit to framebuffer (~10ms)
```
