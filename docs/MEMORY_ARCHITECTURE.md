# SUMI Memory Architecture

## Overview

SUMI uses a pre-allocated memory arena to eliminate heap fragmentation during image processing and text layout. The arena is split into three independent allocations so each block can fit in fragmented heap alongside NimBLE's BLE stack.

## Memory Budget

| Component | Size | Notes |
|-----------|------|-------|
| ESP32-C3 SRAM | ~400KB | Total available |
| Static BSS | ~115KB | Global variables, FreeRTOS |
| Display framebuffer | 48KB | 800×480 ÷ 8 |
| Memory Arena | 52–76KB | 32+20KB essential, +24KB task stack (optional) |
| NimBLE (when connected) | ~48KB | BLE HID keyboard/page turner |
| **Available for app** | **~100–150KB** | Fonts, page cache, UI, plugins |

## Memory Arena

The arena is allocated at boot and released when BLE needs large contiguous heap (scanning, pairing). It consists of three independent `heap_caps_malloc` calls — not one contiguous block — so each piece can fit in fragmented memory.

### Buffer Layout

```
┌──────────────────────────────────────────────────────────────┐
│ ALLOCATION 1: PRIMARY BUFFER (32KB)                          │
│   primaryBuffer │ 32KB │ Full buffer base pointer            │
│   zipBuffer     │      │ Alias for primaryBuffer             │
│                 │      │ 32KB used as LZ77 dictionary        │
├──────────────────────────────────────────────────────────────┤
│ ALLOCATION 2: WORK BUFFER (20KB)                             │
│   scratchBuffer  │  8KB │ Bump allocator (DP arrays, temp)   │
│   ditherRegion   │  8KB │ Ditherer error rows (3× ~1.6KB)    │
│   imageRowRegion │  4KB │ Bitmap row I/O buffers             │
├──────────────────────────────────────────────────────────────┤
│ ALLOCATION 3: TASK STACK (24KB) — OPTIONAL                   │
│   taskStackRegion │ 24KB │ PageCache background task stack   │
│                   │      │ Used by xTaskCreateStatic          │
│                   │      │ Falls back to heap if unavailable  │
└──────────────────────────────────────────────────────────────┘
```

### Framebuffer Fallback (BLE Coexistence)

When BLE is connected and fragments the heap, the 48KB display framebuffer doubles as a ZIP dictionary via `MemoryArena::fallbackBuffer`. Set by `ReaderState::enter()` when BLE HID is active. The parser's `createOrExtendCache()` releases the 32KB primaryBuffer when `fallbackBuffer != nullptr`, giving the XML parser enough heap while the framebuffer provides the LZ77 dictionary.

| Buffer | Size | Use |
|--------|------|-----|
| `fallbackBuffer` | 48KB | Alias for display framebuffer, used as ZIP dict when BLE is active |

**Why three blocks?** After BLE connects, NimBLE holds ~48KB of heap. A single 76KB allocation would likely fail in fragmented heap. Three smaller blocks (32+20+24) each fit in whatever contiguous space remains. The 24KB task stack is optional — if it can't be allocated, `BackgroundTask` falls back to `xTaskCreate` with heap allocation.

**Essential vs optional:** The 32KB primary and 20KB work buffers (52KB total) MUST succeed for inline images to work. The `zipBuffer` alias on `primaryBuffer` provides the 32KB LZ77 dictionary for ZIP decompression. Without it, deflated inline images in EPUBs fail.

### Bump Allocator

The 8KB scratch region doubles as a bump allocator for temporary allocations via `scratchAlloc()`. Text layout uses this for DP line-breaking arrays and hyphenation vectors, avoiding heap fragmentation. The `ArenaScratch` RAII guard saves the watermark on construction and restores it on destruction, supporting nested usage.

### Source Files

- `src/core/MemoryArena.h` — Buffer definitions, sizes, bump allocator API, ArenaScratch
- `src/core/MemoryArena.cpp` — 3-block allocation, release, bump allocator impl

### Usage Pattern

```cpp
// Check arena is ready
if (!sumi::MemoryArena::isInitialized()) {
  return false;
}

// Use pre-allocated buffers (no malloc needed)
uint8_t* zipDict = sumi::MemoryArena::zipBuffer;       // 32KB LZ77 dictionary
uint8_t* imgRow  = sumi::MemoryArena::imageRowRegion;   // 4KB
uint8_t* dither  = sumi::MemoryArena::ditherRegion;     // 8KB

// Bump allocator for temporary arrays (e.g., text layout DP)
{
  sumi::ArenaScratch guard;
  int* dp = guard.alloc<int>(n);      // from arena, not heap
  size_t* ans = guard.alloc<size_t>(n);
  // ... use dp, ans ...
}  // watermark auto-restored here

// Static task allocation (avoids 24KB heap malloc for background task)
if (sumi::MemoryArena::taskStackRegion) {
  task.startStatic("Name", sumi::MemoryArena::taskStackRegion,
                   sumi::MemoryArena::TASK_STACK_SIZE, func, 0);
} else {
  task.start("Name", 24576, func, 0);  // heap fallback
}
```

## BLE Coexistence

The arena and NimBLE BLE stack compete for the same heap. NimBLE needs ~48KB contiguous for initialization, scanning, and pairing.

### Lifecycle

```
Boot:
  MemoryArena::init()           ← 3 allocations succeed (heap is clean)

Enter BLE settings:
  MemoryArena::release()        ← Free all 3 blocks
  ble::init()                   ← NimBLE takes ~48KB
  ble::startScan() / connectTo()

Exit settings (BLE connected):
  MemoryArena::init()           ← Re-allocate alongside NimBLE
    32KB primary  → succeeds    ← largest free ~88KB
    20KB work     → succeeds    ← ~50KB remains
    24KB task     → succeeds    ← ~32KB remains

Enter reader (BLE HID connected):
  ReaderState::enter()
    fallbackBuffer = framebuffer  ← 48KB framebuffer as ZIP dict backup
    createOrExtendCache()
      releases primaryBuffer      ← frees 32KB for parser heap
      ZIP uses fallbackBuffer     ← framebuffer provides LZ77 dictionary

BLE timeout / disconnect:
  ble::deinit()                 ← NimBLE frees ~48KB
  MemoryArena::init()           ← All 3 blocks succeed again
```

### Why Covers Work But Inline Images Don't (Without Arena)

Cover BMPs in EPUBs are stored **uncompressed** (ZIP method 0) — they only need a 1KB read buffer. Inline BMPs are stored **deflated** (ZIP method 8) — they need a 32KB LZ77 dictionary. When the arena is initialized, `zipBuffer` provides that dictionary. When it's not, ZIP falls back to `malloc(32KB)` which fails in fragmented heap.

## Buffer Consumers

### JPEGDEC (JpegToBmpConverter)

| Buffer | Use |
|--------|-----|
| `ditherRegion` | Ditherer error rows |
| heap `grayBuffer` | Full image grayscale buffer (heap-allocated, width×height) |

### PngToBmpConverter

| Buffer | Use |
|--------|-----|
| `scratchBuffer` | Source/output row buffers + scaling accumulators |
| `ditherRegion` | Ditherer error rows |

### BitmapHelpers (bmpTo1BitBmpScaled)

| Buffer | Use |
|--------|-----|
| `primaryBuffer` | Source rows + output row buffer |
| `ditherRegion` | Ditherer error rows |

### Text Layout (ParsedText)

| Buffer | Use |
|--------|-----|
| Arena bump allocator | DP cost array and answer array for line breaking |

The minimum-raggedness DP line-breaking algorithm allocates two arrays proportional to the number of words. These come from the arena bump allocator via `ArenaScratch`, falling back to `std::vector` on heap if the arena is unavailable.

### HomeState (Thumbnails)

| Buffer | Use |
|--------|-----|
| `scratchBuffer` | Thumbnail extraction and decompression |

Thumbnail flow:
1. Extract 120×180 region from framebuffer into `scratchBuffer`
2. Group5 compress into heap-allocated `compressedThumb_` (2-4KB)
3. Store raw thumbnail to LittleFS flash cache
4. On subsequent boots, load from flash into `scratchBuffer`

### ZipFile (EPUB Decompression)

| Buffer | Use |
|--------|-----|
| `zipBuffer` | TINFL dictionary (32KB, fixed size required by miniz) |

`zipBuffer` is an alias for `primaryBuffer`. EPUB decompression and image decoding are time-shared (ZIP finishes before images start). When the arena is unavailable, `Epub::readItemContentsToStream()` passes `nullptr` and ZipFile falls back to `malloc(32KB)`.

### BackgroundTask (PageCache)

| Buffer | Use |
|--------|-----|
| `taskStackRegion` | FreeRTOS task stack for `xTaskCreateStatic` |

When `taskStackRegion` is available, `BackgroundTask::startStatic()` uses it to avoid a 24KB heap allocation. When unavailable, falls back to `BackgroundTask::start()` which calls `xTaskCreate` with heap-allocated stack.

## Flash Thumbnail Cache

Thumbnails persist across reboots in LittleFS (`/thumbs/` directory).

### Source Files

- `src/content/ThumbnailCache.h` — Cache interface
- `src/content/ThumbnailCache.cpp` — LittleFS storage

### Storage Format

- Path: `/thumbs/<HASH>.thb`
- Hash: 32-bit FNV-1a of book path
- Size: 2,700 bytes (120×180 ÷ 8)
- Format: Raw 1-bit bitmap, MSB first

### Cache Flow

```
First open:
  HomeState::storeCoverThumbnail()
    → Extract from framebuffer
    → Group5 compress to RAM
    → ThumbnailCache::store() to flash

Subsequent boots:
  HomeState::tryLoadFlashThumbnail()
    → ThumbnailCache::load() from flash
    → Blit directly to framebuffer
    → Skip SD card decode entirely
```

## Initialization Order

```
setup()
  └─ earlyInit()
       ├─ Serial, GPIO, SPI
       ├─ SD card mount
       ├─ Settings load
       ├─ LittleFS mount
       └─ MemoryArena::init()  ← 3 independent heap_caps_malloc calls
           ├─ 32KB primary buffer
           ├─ 20KB work buffer
           └─ 24KB task stack (optional)
```

## Debugging

Print arena status to serial:

```cpp
sumi::MemoryArena::printStatus();
```

Output:
```
[MEM] === Arena Status (32+20+24KB) ===
[MEM] PRIMARY (32KB): 0x3FC80000
[MEM] WORK (20KB): scratch=0x3FC8C800 dither=0x3FC8E800 imgrow=0x3FC90800
[MEM] TASK STACK (24KB): 0x3FC91800
[MEM] Scratch: 0/8192 bytes used
[MEM] Heap free: 106384, largest: 88052
```

## Design Rationale

### Why pre-allocate?

ESP32 malloc/free cycles cause fragmentation. After enough image operations, the heap becomes swiss cheese — plenty of total free bytes but no single block large enough for the next allocation. Pre-allocating fixed buffers eliminates this failure mode entirely.

### Why 32+20+24KB?

- **32KB `primaryBuffer`**: Serves as the TINFL_LZ_DICT_SIZE (32KB) dictionary for ZIP decompression. Also used as scratch space for BMP scaling row buffers during thumbnailing (time-shared — ZIP decompression finishes before image operations start). Also used by Flashcards plugin for card storage (avoids heap fragmentation after EPUB reading).
- **8KB `scratchBuffer`**: Thumbnails, Group5 compression, bump allocator for DP arrays (supports up to 1024 words per paragraph).
- **8KB `ditherRegion`**: Ordered dithering error rows (3 rows × ~1.6KB each for 800px width).
- **4KB `imageRowRegion`**: BMP row I/O buffer (max 800×4 bytes).
- **24KB `taskStackRegion`** (optional): PageCache background task stack. Avoids 24KB heap `xTaskCreate` allocation. Falls back to heap when unavailable.

Total 52KB essential + 24KB optional = 76KB. With BLE connected (~48KB held by NimBLE), the 52KB essential portion easily fits in the remaining ~100KB heap.

### Why three blocks instead of one?

After NimBLE connects, it holds ~48KB in the middle of the heap. A single 76KB allocation would fail. Three smaller allocations (32+20+24) each fit in the available contiguous space. The 24KB task stack is optional — if heap is too fragmented for even that, the task falls back to heap allocation.

### Why not use PSRAM?

The Xteink X4 has an ESP32-C3 without PSRAM. All memory is internal SRAM.
