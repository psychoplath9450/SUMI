# SUMI Source Architecture

## Directory Structure

```
src/
├── main.cpp                 # Entry point, boot mode detection, initialization
├── config.h                 # Build configuration, feature flags
│
├── core/                    # Core systems
│   ├── MemoryArena.h/cpp    # Pre-allocated buffer pool (80KB) + bump allocator
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
│   └── BleHid.h/cpp         # Keyboard support
│
├── plugins/                 # Built-in apps
│   ├── PluginInterface.h    # Plugin base class
│   ├── PluginRenderer.h     # Drawing API
│   ├── PluginHelpers.h      # Shared utilities
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
│   ├── Cube3D.h             # 3D demo
│   └── SumiBoy.h            # GB emulator launcher
│
├── rendering/               # Page rendering
│   └── XtcPageRenderer.h/cpp # XTC format renderer
│
├── ui/                      # UI framework
│   ├── Elements.h/cpp       # UI primitives
│   ├── Views.h              # View index
│   └── views/               # State-specific views
│
├── assets/                  # Embedded resources
│   └── sumi_home_bg.h       # Default home art
│
└── images/                  # Logos and icons
    └── SumiLogo.h           # Boot logo
```

## Key Libraries (lib/)

| Library | Purpose |
|---------|---------|
| Epub | EPUB parsing, OPF, TOC, CSS, HTML-to-pages, Liang hyphenation, DP line breaking |
| GfxRenderer | 1-bit graphics, fonts, dithering |
| EInkDisplay | SSD1677 driver, refresh modes |
| JpegToBmpConverter | JPEG decode with prescaling |
| PngToBmpConverter | PNG decode with streaming |
| ZipFile | EPUB decompression |
| Xtc | XTC format reader/writer |
| Group5 | Fax compression for thumbnails |
| SDCardManager | SD card abstraction |
| InputManager | Button debouncing |
| BatteryMonitor | ADC battery reading |
| miniz | zlib decompression |
| pngle | Streaming PNG decoder |
| picojpeg | Minimal JPEG decoder |

## Boot Sequence

```
setup()
├── earlyInit()
│   ├── Serial, GPIO, SPI init
│   ├── Boot loop guard check
│   ├── SD card mount
│   ├── Settings load
│   ├── LittleFS mount
│   └── MemoryArena::init()  ← 80KB allocated here
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
│ imageBuffer2    │◀── MemoryArena (4KB)
│ rowBuffer       │◀── MemoryArena (4KB)
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ Ditherer uses   │
│ ditherBuffer    │◀── MemoryArena (32KB)
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
