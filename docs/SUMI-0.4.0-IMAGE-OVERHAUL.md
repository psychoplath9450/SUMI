# Sumi v0.4.0: Image System Overhaul

## Implementation Status

This was the planning document for v0.4.0. Here's what was implemented:

| Phase | Feature | Status |
|-------|---------|--------|
| 1 | Memory Arena (120KB) | ✅ Implemented |
| 2 | JPEGDEC Integration | ⏸️ Deferred |
| 3 | Flash Thumbnail Cache | ✅ Implemented |
| 4 | Arena Migration | ✅ Partial |

### Implemented

- **MemoryArena** — 120KB pre-allocated at boot (`src/core/MemoryArena.h/cpp`)
- **ThumbnailCache** — LittleFS storage for instant home screen (`src/content/ThumbnailCache.h/cpp`)
- **JpegToBmpConverter** — Uses arena for decode buffers
- **PngToBmpConverter** — Uses arena for decode buffers
- **HomeState** — Uses arena scratchBuffer, integrates flash cache

### Deferred

- **JPEGDEC** — Requires more testing, picojpeg works fine with arena
- **ZipFile arena** — Complex, low risk with current implementation
- **Ditherer arena** — Still uses new/delete, acceptable until JPEGDEC

### See Also

- `docs/MEMORY_ARCHITECTURE.md` — Current memory system documentation
- `CHANGELOG.md` — Release notes for v0.4.0

---

## Executive Summary

This document outlines a comprehensive overhaul of Sumi's image handling system. The goals are:
- **Eliminate heap corruption bugs** (the class of bugs we just debugged)
- **Maximize RAM utilization** (Sumi has ~300KB+, currently using patterns designed for WiFi devices with ~160KB)
- **Instant home screen loading** (flash-cached thumbnails)
- **Simplified, battle-tested code** (JPEGDEC replaces picojpeg + 4 ditherer classes)

---

## Sumi Hardware Specs (Reference)

| Component | Value |
|-----------|-------|
| Display | 800×480 (rotated for portrait viewing) |
| Framebuffer | 48KB (800×480÷8) |
| ESP32-C3 SRAM | ~400KB total |
| BLE stack overhead | ~25KB |
| FreeRTOS + stacks | ~30KB |
| **Available for app** | **~300KB** |
| MAX_IMAGE_WIDTH | 2048 |
| MAX_IMAGE_HEIGHT | 3072 |
| TARGET_MAX (covers) | 480×800 |
| THUMB_WIDTH/HEIGHT | 320×440 (SD card) |
| COVER_CACHE | 120×180 (in-memory) |
| TINFL_LZ_DICT_SIZE | 32KB (fixed) |

---

## Phase 1: Memory Arena System (120KB)

### Overview
Pre-allocate all major buffers ONCE at boot. Never free them. Pass them to all image operations.

### New File: `src/core/MemoryArena.h`

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace sumi {

class MemoryArena {
public:
  // ==================== BUFFER SIZES ====================
  // Sumi has ~300KB+ available (BLE-only, no WiFi stack)
  // 120KB arena leaves ~180KB for everything else
  
  // Primary image processing buffer (JPEG/PNG decode, cover art)
  // Sized for: 2048×16 MCU rows × 1 byte grayscale (worst case inline image)
  static constexpr size_t IMAGE_BUFFER_SIZE = 32 * 1024;  // 32KB
  
  // Secondary image buffer (for scaling operations that need src+dst)
  // Sized for: 480×16 MCU rows (scaled cover art)
  static constexpr size_t IMAGE_BUFFER2_SIZE = 16 * 1024; // 16KB
  
  // Row processing buffer (bitmap rows, dithering output)
  // Sized for: 2048 pixels × 3 bytes (24-bit max width) + padding
  static constexpr size_t ROW_BUFFER_SIZE = 8 * 1024;     // 8KB
  
  // Dither error diffusion buffers (3 rows for Atkinson)
  // Sized for: 2048 pixels × 3 rows × 2 bytes (int16_t errors)
  static constexpr size_t DITHER_BUFFER_SIZE = 12 * 1024; // 12KB
  
  // ZIP/EPUB decompression buffer (tinfl dictionary - FIXED SIZE)
  static constexpr size_t ZIP_BUFFER_SIZE = 32 * 1024;    // 32KB (TINFL_LZ_DICT_SIZE)
  
  // General scratch buffer (thumbnails, Group5 compression, temp ops)
  // Sized for: 320×440÷8 thumb = 17.6KB, with headroom
  static constexpr size_t SCRATCH_BUFFER_SIZE = 20 * 1024; // 20KB
  
  // ==================== TOTAL: 120KB ====================
  // Leaves ~180KB for: fonts, page cache, UI state, plugins
  
  // ==================== BUFFER POINTERS ====================
  static uint8_t* imageBuffer;      // 32KB - Primary image processing
  static uint8_t* imageBuffer2;     // 16KB - Secondary (scaling)
  static uint8_t* rowBuffer;        // 8KB  - Row processing
  static uint8_t* ditherBuffer;     // 12KB - Dither error rows (3 rows × 2048 × int16)
  static uint8_t* zipBuffer;        // 32KB - ZIP decompression (TINFL dict)
  static uint8_t* scratchBuffer;    // 20KB - General scratch (thumbnails, Group5, etc.)
  
  // Dither buffer subdivisions (for convenience)
  // ditherRow0 = ditherBuffer
  // ditherRow1 = ditherBuffer + 4096
  // ditherRow2 = ditherBuffer + 8192
  static int16_t* ditherRow0() { return reinterpret_cast<int16_t*>(ditherBuffer); }
  static int16_t* ditherRow1() { return reinterpret_cast<int16_t*>(ditherBuffer + 4096); }
  static int16_t* ditherRow2() { return reinterpret_cast<int16_t*>(ditherBuffer + 8192); }
  
  // ==================== LIFECYCLE ====================
  
  // Call ONCE at boot, before any other initialization
  static bool init();
  
  // For debugging: print allocation status
  static void printStatus();
  
  // Check if arena is initialized
  static bool isInitialized() { return initialized_; }
  
  // Get total arena size (120KB)
  static constexpr size_t totalSize() {
    return IMAGE_BUFFER_SIZE + IMAGE_BUFFER2_SIZE + ROW_BUFFER_SIZE +
           DITHER_BUFFER_SIZE + ZIP_BUFFER_SIZE + SCRATCH_BUFFER_SIZE;
  }
  
private:
  static bool initialized_;
  
  // Prevent instantiation
  MemoryArena() = delete;
};

} // namespace sumi
```

### New File: `src/core/MemoryArena.cpp`

```cpp
#include "MemoryArena.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace sumi {

// Static member definitions
uint8_t* MemoryArena::imageBuffer = nullptr;
uint8_t* MemoryArena::imageBuffer2 = nullptr;
uint8_t* MemoryArena::rowBuffer = nullptr;
uint8_t* MemoryArena::ditherBuffer = nullptr;
uint8_t* MemoryArena::zipBuffer = nullptr;
uint8_t* MemoryArena::scratchBuffer = nullptr;
bool MemoryArena::initialized_ = false;

bool MemoryArena::init() {
  if (initialized_) {
    Serial.println("[ARENA] Already initialized");
    return true;
  }
  
  Serial.printf("[ARENA] Initializing 120KB memory arena\n");
  Serial.printf("[ARENA] Free heap before: %lu, largest block: %lu\n", 
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  
  // Allocate all buffers with MALLOC_CAP_8BIT (internal RAM, byte-accessible)
  imageBuffer = static_cast<uint8_t*>(heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_8BIT));
  imageBuffer2 = static_cast<uint8_t*>(heap_caps_malloc(IMAGE_BUFFER2_SIZE, MALLOC_CAP_8BIT));
  rowBuffer = static_cast<uint8_t*>(heap_caps_malloc(ROW_BUFFER_SIZE, MALLOC_CAP_8BIT));
  ditherBuffer = static_cast<uint8_t*>(heap_caps_malloc(DITHER_BUFFER_SIZE, MALLOC_CAP_8BIT));
  zipBuffer = static_cast<uint8_t*>(heap_caps_malloc(ZIP_BUFFER_SIZE, MALLOC_CAP_8BIT));
  scratchBuffer = static_cast<uint8_t*>(heap_caps_malloc(SCRATCH_BUFFER_SIZE, MALLOC_CAP_8BIT));
  
  // Verify all allocations succeeded
  if (!imageBuffer || !imageBuffer2 || !rowBuffer || !ditherBuffer ||
      !zipBuffer || !scratchBuffer) {
    Serial.println("[ARENA] FATAL: Failed to allocate memory arena!");
    Serial.printf("[ARENA]   imageBuffer:   %p\n", imageBuffer);
    Serial.printf("[ARENA]   imageBuffer2:  %p\n", imageBuffer2);
    Serial.printf("[ARENA]   rowBuffer:     %p\n", rowBuffer);
    Serial.printf("[ARENA]   ditherBuffer:  %p\n", ditherBuffer);
    Serial.printf("[ARENA]   zipBuffer:     %p\n", zipBuffer);
    Serial.printf("[ARENA]   scratchBuffer: %p\n", scratchBuffer);
    // Free any partial allocations
    heap_caps_free(imageBuffer);
    heap_caps_free(imageBuffer2);
    heap_caps_free(rowBuffer);
    heap_caps_free(ditherBuffer);
    heap_caps_free(zipBuffer);
    heap_caps_free(scratchBuffer);
    return false;
  }
  
  // Zero-initialize all buffers (prevents undefined behavior)
  memset(imageBuffer, 0, IMAGE_BUFFER_SIZE);
  memset(imageBuffer2, 0, IMAGE_BUFFER2_SIZE);
  memset(rowBuffer, 0, ROW_BUFFER_SIZE);
  memset(ditherBuffer, 0, DITHER_BUFFER_SIZE);
  memset(zipBuffer, 0, ZIP_BUFFER_SIZE);
  memset(scratchBuffer, 0, SCRATCH_BUFFER_SIZE);
  
  initialized_ = true;
  
  Serial.printf("[ARENA] Free heap after: %lu, largest block: %lu\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  Serial.println("[ARENA] Memory arena initialized (120KB)");
  
  return true;
}

void MemoryArena::printStatus() {
  Serial.println("[ARENA] === Memory Arena Status ===");
  Serial.printf("[ARENA] imageBuffer:   %p (32KB)\n", imageBuffer);
  Serial.printf("[ARENA] imageBuffer2:  %p (16KB)\n", imageBuffer2);
  Serial.printf("[ARENA] rowBuffer:     %p (8KB)\n", rowBuffer);
  Serial.printf("[ARENA] ditherBuffer:  %p (12KB)\n", ditherBuffer);
  Serial.printf("[ARENA] zipBuffer:     %p (32KB)\n", zipBuffer);
  Serial.printf("[ARENA] scratchBuffer: %p (20KB)\n", scratchBuffer);
  Serial.println("[ARENA] Total: 120KB");
  Serial.printf("[ARENA] Free heap: %lu, largest block: %lu\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

} // namespace sumi
```

### Integration Point: `main.cpp`

```cpp
// In setup(), BEFORE any other initialization:
#include "core/MemoryArena.h"

void setup() {
  Serial.begin(115200);
  
  // FIRST THING: Initialize memory arena
  if (!sumi::MemoryArena::init()) {
    Serial.println("FATAL: Memory arena init failed!");
    while(1) { delay(1000); }  // Halt
  }
  
  // ... rest of initialization
}
```

---

## Phase 2: JPEGDEC Integration

### Overview
Replace picojpeg with JPEGDEC library. JPEGDEC has built-in dithering for e-ink displays.

### Step 2.1: Add JPEGDEC Library

```bash
# In lib/ directory, clone or add:
# https://github.com/bitbank2/JPEGDEC
```

Create `lib/JPEGDEC/library.json`:
```json
{
  "name": "JPEGDEC",
  "version": "1.6.0",
  "description": "Optimized JPEG decoder with e-ink dithering support"
}
```

### Step 2.2: New JpegToBmpConverter (Rewrite)

The key insight: JPEGDEC provides a callback for each decoded MCU block. We process directly in the callback, writing to the arena buffer, then to the output file. No intermediate full-image buffer needed.

```cpp
// lib/JpegToBmpConverter/JpegToBmpConverter.cpp (NEW VERSION)

#include "JpegToBmpConverter.h"
#include "JPEGDEC.h"
#include "core/MemoryArena.h"

// JPEGDEC instance (reused, never freed)
static JPEGDEC jpeg;

// Context passed to callback
struct DecodeContext {
  FsFile* outFile;
  int outWidth;
  int outHeight;
  int bytesPerRow;
  int currentY;
  uint8_t bpp;  // 1 or 2
  bool dither;
};

// JPEGDEC draw callback - called for each MCU block
int jpegDrawCallback(JPEGDRAW* pDraw) {
  auto* ctx = static_cast<DecodeContext*>(pDraw->pUser);
  
  // pDraw contains:
  // - pPixels: decoded pixel data (RGB565 or grayscale depending on options)
  // - x, y: position in output image
  // - iWidth, iHeight: size of this block
  
  // JPEGDEC with JPEG_DITHER already outputs 1/2/4/8-bit dithered!
  // We just need to pack and write to BMP
  
  // ... (implementation details)
  
  return 1; // Continue decoding
}

bool jpegFileToBmpStream(const char* jpegPath, const char* bmpPath,
                         int targetWidth, int targetHeight,
                         bool oneBit, bool dither) {
  // Use arena buffers instead of malloc
  uint8_t* workBuffer = sumi::MemoryArena::imageBuffer;
  
  FsFile jpegFile;
  if (!jpegFile.open(jpegPath, O_RDONLY)) {
    return false;
  }
  
  // JPEGDEC can decode directly from file callback
  jpeg.open(jpegPath, jpegOpenCallback, jpegCloseCallback, 
            jpegReadCallback, jpegSeekCallback, jpegDrawCallback);
  
  // Set decode options
  int options = 0;
  if (dither) {
    options |= JPEG_DITHER;  // Built-in Floyd-Steinberg!
  }
  if (oneBit) {
    options |= JPEG_1BIT;    // Output 1-bit directly
  } else {
    options |= JPEG_2BIT;    // Output 2-bit (4 gray levels)
  }
  
  // Calculate scale factor (1, 1/2, 1/4, 1/8)
  int scale = JPEG_SCALE_ONE;
  // ... determine optimal scale
  
  DecodeContext ctx = {
    .outFile = &bmpFile,
    .outWidth = outWidth,
    .outHeight = outHeight,
    .bytesPerRow = bytesPerRow,
    .currentY = 0,
    .bpp = oneBit ? 1 : 2,
    .dither = dither
  };
  
  jpeg.setUserPointer(&ctx);
  jpeg.decode(0, 0, scale | options);
  jpeg.close();
  
  return true;
}
```

### Step 2.3: Delete Old Ditherer Classes

After JPEGDEC integration, DELETE these (JPEGDEC handles dithering internally):

- `lib/GfxRenderer/BitmapHelpers.h`: Remove `AtkinsonDitherer`, `FloydSteinbergDitherer`, `Atkinson1BitDitherer`
- `lib/GfxRenderer/BitmapHelpers.cpp`: Remove `RawAtkinson1BitDitherer`
- Clean up `Bitmap.cpp` to remove ditherer instantiation

**NOTE:** Keep Atkinson for PNG (JPEGDEC only handles JPEG). Consider adding PNGdec later.

---

## Phase 3: Flash Thumbnail Cache

### Overview
Store pre-dithered 1-bit thumbnails in LittleFS. Generate in Reader mode, load instantly in UI mode.

**Sumi already has LittleFS mounted with 3.4MB partition - no changes needed!**

### New File: `src/content/ThumbnailCache.h`

```cpp
#pragma once
#include <cstdint>
#include <SdFat.h>

namespace sumi {

class ThumbnailCache {
public:
  // Thumbnail dimensions - use COVER_CACHE size for consistency with HomeState
  // These are the compressed thumbnails for instant loading
  static constexpr int WIDTH = 120;   // COVER_CACHE_WIDTH from HomeState
  static constexpr int HEIGHT = 180;  // COVER_CACHE_HEIGHT from HomeState
  static constexpr int BYTES_PER_ROW = (WIDTH + 7) / 8;  // 15 bytes
  static constexpr size_t SIZE = BYTES_PER_ROW * HEIGHT; // 2,700 bytes
  
  // Maximum cached thumbnails (matches MAX_RECENT_BOOKS)
  static constexpr int MAX_CACHED = 3;
  
  // Cache path in LittleFS (already mounted in Sumi)
  static constexpr const char* CACHE_DIR = "/thumbs";
  
  // Generate thumbnail from cover BMP and store in flash
  // Called in READER mode after cover is processed
  static bool generate(const char* coverBmpPath, uint32_t bookHash);
  
  // Load thumbnail from flash into provided buffer
  // Called in UI mode for home screen - INSTANT, no decode needed
  // Returns true if found and loaded
  static bool load(uint32_t bookHash, uint8_t* buffer);
  
  // Check if thumbnail exists for book
  static bool exists(uint32_t bookHash);
  
  // Delete thumbnail (when book removed from recent)
  static bool remove(uint32_t bookHash);
  
  // Clear all cached thumbnails
  static void clearAll();
  
  // Prune cache to MAX_CACHED entries (keeps only books in RecentBooks)
  static void prune(const uint32_t* keepHashes, int count);
  
private:
  static void getPath(uint32_t bookHash, char* pathOut, size_t pathSize);
};

} // namespace sumi
```

### New File: `src/content/ThumbnailCache.cpp`

```cpp
#include "ThumbnailCache.h"
#include <LittleFS.h>
#include <Arduino.h>
#include "core/MemoryArena.h"

namespace sumi {

void ThumbnailCache::getPath(uint32_t bookHash, char* pathOut, size_t pathSize) {
  snprintf(pathOut, pathSize, "%s/%08X.thm", CACHE_DIR, bookHash);
}

bool ThumbnailCache::generate(const char* coverBmpPath, uint32_t bookHash) {
  char thumbPath[48];
  getPath(bookHash, thumbPath, sizeof(thumbPath));
  
  // Ensure cache directory exists
  if (!LittleFS.exists(CACHE_DIR)) {
    LittleFS.mkdir(CACHE_DIR);
  }
  
  // Use existing bmpTo1BitBmpScaled but output to LittleFS
  // Scale cover.bmp (2-bit, ~480×700) to thumbnail (1-bit, 120×180)
  
  // Use arena scratch buffer for intermediate data
  uint8_t* buffer = MemoryArena::scratchBuffer;
  
  // ... scaling and dithering logic using arena buffers
  
  // Write to LittleFS
  File thumbFile = LittleFS.open(thumbPath, "w");
  if (!thumbFile) {
    Serial.printf("[THUMB] Failed to create %s\n", thumbPath);
    return false;
  }
  
  thumbFile.write(buffer, SIZE);
  thumbFile.close();
  
  Serial.printf("[THUMB] Generated %s (%zu bytes)\n", thumbPath, SIZE);
  return true;
}

bool ThumbnailCache::load(uint32_t bookHash, uint8_t* buffer) {
  char thumbPath[48];
  getPath(bookHash, thumbPath, sizeof(thumbPath));
  
  if (!LittleFS.exists(thumbPath)) {
    return false;
  }
  
  File thumbFile = LittleFS.open(thumbPath, "r");
  if (!thumbFile) {
    return false;
  }
  
  size_t bytesRead = thumbFile.read(buffer, SIZE);
  thumbFile.close();
  
  return bytesRead == SIZE;
}

bool ThumbnailCache::exists(uint32_t bookHash) {
  char thumbPath[48];
  getPath(bookHash, thumbPath, sizeof(thumbPath));
  return LittleFS.exists(thumbPath);
}

bool ThumbnailCache::remove(uint32_t bookHash) {
  char thumbPath[48];
  getPath(bookHash, thumbPath, sizeof(thumbPath));
  return LittleFS.remove(thumbPath);
}

void ThumbnailCache::clearAll() {
  File dir = LittleFS.open(CACHE_DIR);
  if (!dir || !dir.isDirectory()) return;
  
  File entry;
  while ((entry = dir.openNextFile())) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, entry.name());
    entry.close();
    LittleFS.remove(path);
  }
  dir.close();
}

void ThumbnailCache::prune() {
  // Keep only MAX_CACHED thumbnails (oldest removed)
  // Implementation: read RecentBooks, keep only those hashes
}

} // namespace sumi
```

### Integration: HomeState

```cpp
// In HomeState::loadRecentBooks() or render()
#include "content/ThumbnailCache.h"

void HomeState::renderCoverThumbnail(int bookIndex) {
  const auto& book = recentBooks_[bookIndex];
  uint32_t hash = hashPath(book.path);
  
  // Try flash cache first (instant!)
  if (ThumbnailCache::load(hash, thumbnailBuffer_)) {
    // Direct blit to framebuffer - no decode needed
    renderer_.drawBitmap1Bit(thumbnailBuffer_, x, y, 
                              ThumbnailCache::WIDTH, ThumbnailCache::HEIGHT);
    return;
  }
  
  // Fallback: decode from SD card (slow, first time only)
  // ... existing cover loading logic
}
```

### Integration: ReaderState

```cpp
// In ReaderState, after cover is processed:
void ReaderState::onCoverProcessed() {
  uint32_t hash = hashPath(contentPath_);
  
  // Generate thumbnail for flash cache (runs once per book)
  if (!ThumbnailCache::exists(hash)) {
    ThumbnailCache::generate(coverBmpPath_.c_str(), hash);
  }
}
```

---

## Phase 4: Modify Existing Systems to Use Arena

### 4.1: JpegToBmpConverter

Replace all `malloc`/`free` with arena buffers:

```cpp
// OLD:
auto* mcuRowBuffer = static_cast<uint8_t*>(calloc(1, mcuRowPixels + SAFETY_PADDING));
// ... use it
free(mcuRowBuffer);

// NEW:
uint8_t* mcuRowBuffer = MemoryArena::imageBuffer;
// ... use it (no free!)
```

### 4.2: PngToBmpConverter

Same pattern - use `MemoryArena::imageBuffer` and `MemoryArena::rowBuffer`.

### 4.3: ZipFile

Replace per-operation malloc with arena:

```cpp
// OLD:
const auto inflator = static_cast<tinfl_decompressor*>(malloc(sizeof(tinfl_decompressor)));
auto outputBuffer = static_cast<uint8_t*>(malloc(TINFL_LZ_DICT_SIZE));
// ... free both later

// NEW:
// inflator can live on stack (small)
tinfl_decompressor inflator;
// outputBuffer uses arena
uint8_t* outputBuffer = MemoryArena::zipBuffer;
```

### 4.4: HomeState Thumbnail Compression

```cpp
// OLD:
uint8_t* thumbBuffer = static_cast<uint8_t*>(malloc(thumbUncompressedSize));
compressedThumb_ = static_cast<uint8_t*>(malloc(MAX_COVER_CACHE_SIZE));

// NEW:
uint8_t* thumbBuffer = MemoryArena::scratchBuffer;
// compressedThumb_ stays as member (runtime cache, not arena)
```

### 4.5: Flashcards Plugin

```cpp
// OLD:
cards = (Card*)malloc(needed);

// NEW:
// Cards array should be static member, not malloc'd
// Or use MemoryArena::scratchBuffer if temporary
```

---

## Phase 5: LittleFS Partition (ALREADY EXISTS)

**No changes needed!** Sumi already has LittleFS configured:

```csv
# Current partitions.csv
spiffs,   data, spiffs,  0xc90000,0x360000,  # 3.4MB - already mounted as LittleFS
```

LittleFS is already initialized in `main.cpp`:
```cpp
if (!LittleFS.begin(false)) {
  // ... format and retry
}
```

The ThumbnailCache will use `/thumbs/` directory within the existing LittleFS partition.

---

## Implementation Order

### Sprint 1: Foundation (Memory Arena)
1. Create `MemoryArena.h` and `MemoryArena.cpp`
2. Integrate into `main.cpp` (both UI and READER modes)
3. Test boot with arena allocation
4. Verify heap has ~200KB+ remaining

### Sprint 2: Arena Migration
1. Modify `JpegToBmpConverter` to use arena (keep picojpeg for now)
2. Modify `PngToBmpConverter` to use arena
3. Modify `ZipFile` to use arena
4. Modify `HomeState` thumbnail ops to use arena
5. Test thoroughly - no more heap corruption

### Sprint 3: JPEGDEC Integration
1. Add JPEGDEC library
2. Create new `JpegToBmpConverter` implementation
3. Test JPEG decoding with built-in dithering
4. Remove old picojpeg dependency
5. Clean up (but keep) ditherer classes for PNG

### Sprint 4: Flash Thumbnail Cache
1. Add LittleFS partition
2. Create `ThumbnailCache` class
3. Integrate with `ReaderState` (generation)
4. Integrate with `HomeState` (loading)
5. Test instant home screen

### Sprint 5: Cleanup
1. Remove unused ditherer classes (if PNG moved to PNGdec)
2. Optimize buffer sizes based on actual usage
3. Add heap monitoring/logging
4. Final testing across all features

---

## Expected Outcomes

| Metric | Before | After |
|--------|--------|-------|
| Heap corruption bugs | Possible (fragmentation) | Impossible (no malloc/free cycling) |
| Home screen load time | ~500ms (decode from SD) | ~10ms (load from LittleFS) |
| JPEG decode code | picojpeg + 4 ditherer classes (~1000 lines) | JPEGDEC (~200 lines) |
| RAM utilization | ~40% of 300KB (conservative) | ~60% intentional (120KB arena) |
| Largest contiguous block | Varies wildly (fragmentation) | Stable ~180KB (arena is static) |
| Memory debugging | "where did 32KB go?" | `MemoryArena::printStatus()` |

### Memory Budget After Arena (Sumi-Specific)

| Component | Size |
|-----------|------|
| ESP32-C3 SRAM (no WiFi!) | ~400KB |
| Display framebuffer | 48KB |
| **Memory Arena** | **120KB** |
| BLE stack | ~25KB |
| FreeRTOS + task stacks | ~30KB |
| GfxRenderer (bitmap rows, BW chunks) | ~12KB |
| Fonts (loaded on demand) | ~10KB |
| **Free for app logic** | **~155KB** |

This is way more than Papyrix/Crosspoint which have ~100KB free after WiFi takes its cut.

---

## Files to Create

- `src/core/MemoryArena.h` - Arena buffer definitions
- `src/core/MemoryArena.cpp` - Arena initialization
- `src/content/ThumbnailCache.h` - Flash thumbnail cache
- `src/content/ThumbnailCache.cpp` - Thumbnail cache implementation

## Files to Modify

- `src/main.cpp` - Arena init at boot (both UI and READER modes)
- `lib/JpegToBmpConverter/JpegToBmpConverter.cpp` - Use arena, then JPEGDEC
- `lib/PngToBmpConverter/PngToBmpConverter.cpp` - Use arena buffers
- `lib/ZipFile/ZipFile.cpp` - Use arena for inflate dictionary
- `lib/GfxRenderer/BitmapHelpers.cpp` - Use arena for dithering (if keeping for PNG)
- `src/states/HomeState.cpp` - Load from ThumbnailCache
- `src/states/ReaderState.cpp` - Generate thumbnail to cache
- `lib/Epub/Epub.cpp` - Use arena parseBuffer for TOC parsing

## Files to Delete (after JPEGDEC integration)

- `lib/picojpeg/picojpeg.c` 
- `lib/picojpeg/picojpeg.h`
- Parts of `lib/GfxRenderer/BitmapHelpers.h`:
  - `AtkinsonDitherer` class (JPEGDEC handles JPEG dithering)
  - `FloydSteinbergDitherer` class
  - `Atkinson1BitDitherer` class
- `lib/GfxRenderer/BitmapHelpers.cpp`:
  - `RawAtkinson1BitDitherer` class

**NOTE:** Keep ditherer classes until PNGdec is integrated (Phase 6, future). PNG still needs them.

---

## Risk Mitigation

1. **JPEGDEC doesn't support some JPEG type**: Keep picojpeg as fallback initially. JPEGDEC doesn't support progressive JPEG (neither does picojpeg), but Sumi already rejects those.

2. **Arena too small for some image**: Images are prescaled to 480×800 max before hitting the arena. The 32KB imageBuffer handles 2048×16 MCU rows - way more than needed for scaled images.

3. **LittleFS corruption**: Thumbnails are regenerable, not critical data. Worst case: home screen falls back to SD card decode (current behavior).

4. **Regression in page turn speed**: Arena doesn't affect rendering pipeline. Same algorithms, same speed. If anything, fewer malloc calls = microseconds faster.

5. **Plugin memory conflicts**: Flashcards currently malloc's card array. Will migrate to use scratchBuffer when deck is loaded. Same pattern for other plugins.

---

## Sumi-Specific Notes

### Current Flow (with problems)

```
READER MODE:
  1. Open EPUB → extract cover.jpg
  2. JpegToBmpConverter: malloc mcuRowBuffer, rowBuffer, ditherer error rows
  3. Write cover.bmp (2-bit) to SD
  4. bmpTo1BitBmpScaled: malloc srcRows, outRow, ditherer
  5. Write thumb.bmp (1-bit) to SD
  6. Free everything
  7. User reads book... more image malloc/free cycles
  8. Press Back → reboot to UI mode

UI MODE:
  1. Boot fresh heap (no fragmentation... yet)
  2. HomeState loads recent books
  3. Loads thumb.bmp from SD (slow, ~500ms)
  4. Group5 compress to in-memory cache
  5. CRASH if previous session left heap corrupted? No - fresh boot
  6. CRASH comes from: loading cover for large images → overflow in dither buffers
```

### New Flow (with arena)

```
READER MODE:
  1. Open EPUB → extract cover.jpg
  2. JpegToBmpConverter uses MemoryArena::imageBuffer (no malloc)
  3. Write cover.bmp to SD
  4. ThumbnailCache::generate() scales + writes to LittleFS (no malloc)
  5. User reads book... all image ops use arena
  6. Press Back → reboot to UI mode

UI MODE:
  1. Boot → MemoryArena::init() claims 120KB
  2. HomeState loads recent books
  3. ThumbnailCache::load() reads from LittleFS (~10ms, instant!)
  4. Direct blit to framebuffer (no decode, no Group5)
  5. No malloc/free cycles = no fragmentation = no corruption
```

---

## Questions Answered

1. **LittleFS already enabled?** YES - 3.4MB partition, already mounted
2. **Any features needing dynamic malloc?** 
   - Flashcards: Can use scratchBuffer when loading deck
   - Notes: Uses file streaming, minimal RAM
   - Plugins: Created on entry, destroyed on exit (fine)
3. **Start now?** Ready when you are
