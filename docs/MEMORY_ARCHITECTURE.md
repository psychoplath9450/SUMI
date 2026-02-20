# SUMI Memory Architecture

## Overview

SUMI uses a pre-allocated memory arena to eliminate heap fragmentation during image processing. This document describes the memory layout and how different components use the shared buffers.

## Memory Budget

| Component | Size | Notes |
|-----------|------|-------|
| ESP32-C3 SRAM | ~400KB | Total available |
| Display framebuffer | 48KB | 800×480 ÷ 8 |
| Memory Arena | 80KB | Pre-allocated at boot |
| BLE stack | ~25KB | Much smaller than WiFi |
| FreeRTOS + stacks | ~30KB | Task stacks |
| **Available for app** | **~215KB** | Fonts, UI, plugins |

## Memory Arena

The arena is allocated once in `earlyInit()` and never freed. All image processing operations share these buffers.

### Buffer Layout

```
┌──────────────────────────────────────────────────────────────┐
│ MemoryArena (80KB total — one contiguous allocation)         │
├──────────────────────────────────────────────────────────────┤
│ PRIMARY BUFFER (32KB)                                        │
│   primaryBuffer │ 32KB │ Aliased as imageBuffer + zipBuffer │
│                 │      │ (time-shared, never concurrent)    │
├──────────────────────────────────────────────────────────────┤
│ WORK BUFFER (48KB)                                           │
│   rowBuffer     │  4KB │ Bitmap row I/O                     │
│   ditherBuffer  │ 32KB │ Error diffusion for JPEG dithering │
│   imageBuffer2  │  4KB │ Scaling accumulators               │
│   scratchBuffer │  8KB │ Thumbnails, Group5, temp ops       │
└──────────────────────────────────────────────────────────────┘
```

The arena also serves as a **bump allocator** for temporary scratch allocations
via `scratchAlloc()`. Text layout uses this for DP line-breaking arrays and
hyphenation vectors, avoiding heap fragmentation. The `ArenaScratch` RAII guard
saves the watermark on construction and restores it on destruction, supporting
nested usage.

### Source Files

- `src/core/MemoryArena.h` — Buffer definitions, bump allocator API, ArenaScratch
- `src/core/MemoryArena.cpp` — Allocation, initialization, bump allocator impl

### Usage Pattern

```cpp
// Check arena is ready
if (!sumi::MemoryArena::isInitialized()) {
  return false;
}

// Use pre-allocated buffer (no malloc needed)
uint8_t* buffer = sumi::MemoryArena::imageBuffer;

// For dithering, use the typed accessors
int16_t* errorRow0 = sumi::MemoryArena::ditherRow0();
int16_t* errorRow1 = sumi::MemoryArena::ditherRow1();
int16_t* errorRow2 = sumi::MemoryArena::ditherRow2();

// Bump allocator for temporary arrays (e.g., text layout DP)
{
  sumi::ArenaScratch guard;
  int* dp = guard.alloc<int>(n);      // from arena, not heap
  size_t* ans = guard.alloc<size_t>(n);
  // ... use dp, ans ...
}  // watermark auto-restored here
```

## Buffer Consumers

### JpegToBmpConverter

| Buffer | Use |
|--------|-----|
| `primaryBuffer` (as imageBuffer) | MCU row decode (32KB handles 2048×16) |
| `imageBuffer2` | Scaling accumulators (rowAccum, rowCount) |
| `rowBuffer` | Output BMP row |
| `ditherBuffer` | Error diffusion rows for Atkinson/Floyd-Steinberg |

### PngToBmpConverter

| Buffer | Use |
|--------|-----|
| `primaryBuffer` (as imageBuffer) | Source row buffer |
| `imageBuffer2` | Scaling accumulators |
| `rowBuffer` | Output BMP row |

### Text Layout (ParsedText)

| Buffer | Use |
|--------|-----|
| Arena bump allocator | DP cost array and answer array for line breaking |

The minimum-raggedness DP line-breaking algorithm allocates two arrays
proportional to the number of words. These come from the arena bump allocator
via `ArenaScratch`, falling back to `std::vector` on heap if the arena is
unavailable.

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
| `zipBuffer` | TINFL dictionary (32KB, fixed size) |

Note: `zipBuffer` is an alias for `primaryBuffer` — they share the same 32KB.
EPUB decompression and image decoding are time-shared (ZIP finishes before images start).

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
       └─ MemoryArena::init()  ← Arena allocated here
           └─ heap_caps_malloc(80KB) — single contiguous block
```

## Debugging

Print arena status to serial:

```cpp
sumi::MemoryArena::printStatus();
```

Output:
```
[MEM] === Arena Status (80KB allocated) ===
[MEM] PRIMARY (32KB): 0x3FC80000
[MEM] WORK (48KB): row=0x3FC88000 dither=0x3FC89000 buf2=0x3FC91000 scratch=0x3FC92000
[MEM] Bump: 0/81920 bytes used
[MEM] Heap free: 215432, largest: 180224
```

## Design Rationale

### Why pre-allocate?

ESP32 malloc/free cycles cause fragmentation. After enough image operations, the heap becomes swiss cheese — plenty of total free bytes but no single block large enough for the next allocation. Pre-allocating fixed buffers eliminates this failure mode entirely.

### Why 80KB?

- 32KB `primaryBuffer`: Handles 2048×16 MCU rows (worst case inline image) AND TINFL_LZ_DICT_SIZE (32KB). Time-shared — ZIP decompression finishes before image decode starts.
- 4KB `rowBuffer`: 2048×2 bytes (max BMP row width at 32bpp)
- 32KB `ditherBuffer`: JPEGDEC dithering needs width×16 pixels of error state
- 4KB `imageBuffer2`: Scaling accumulators for prescaled decode
- 8KB `scratchBuffer`: Thumbnails, Group5 compression, temp ops

Total 80KB leaves ~215KB free for fonts, page cache, UI state, plugins. The bump
allocator overlay lets text layout reuse the arena for DP arrays without any
additional heap allocation.

### Why not use PSRAM?

The Xteink X4 has an ESP32-C3 without PSRAM. All memory is internal SRAM.
