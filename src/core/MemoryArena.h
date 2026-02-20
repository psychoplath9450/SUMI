#pragma once

#include <cstddef>
#include <cstdint>

namespace sumi {

/**
 * Pre-allocated memory arena - 80KB for image/cache and text layout operations.
 *
 * The arena can be released when not needed (e.g., BLE transfer mode)
 * to free heap for other operations, then reclaimed when needed again.
 *
 * Bump allocator: The entire arena can be used as a temporary scratch pool
 * via scratchAlloc(). This is used by text layout (DP arrays, hyphenation
 * vectors) to avoid heap fragmentation. Call scratchReset() when done.
 * Use ArenaScratch for RAII-based automatic reset.
 */
class MemoryArena {
 public:
  // === PRIMARY BUFFER (32KB) ===
  static constexpr size_t PRIMARY_BUFFER_SIZE = 32 * 1024;

  // === WORK BUFFER (48KB) ===
  static constexpr size_t WORK_BUFFER_SIZE = 48 * 1024;

  // Work buffer regions (must fit in WORK_BUFFER_SIZE = 48KB)
  static constexpr size_t ROW_BUFFER_SIZE = 4 * 1024;       // 4KB
  static constexpr size_t DITHER_BUFFER_SIZE = 32 * 1024;   // 32KB for JPEGDEC dithering (width * 16, max ~2000px)
  static constexpr size_t IMAGE_BUFFER2_SIZE = 4 * 1024;    // 4KB
  static constexpr size_t SCRATCH_BUFFER_SIZE = 8 * 1024;   // 8KB
  // Total: 4 + 32 + 4 + 8 = 48KB

  // Legacy size constants
  static constexpr size_t IMAGE_BUFFER_SIZE = PRIMARY_BUFFER_SIZE;
  static constexpr size_t ZIP_BUFFER_SIZE = 32 * 1024;

  // Buffer pointers (valid after init(), nullptr after release())
  static uint8_t* primaryBuffer;
  static uint8_t* imageBuffer;
  static uint8_t* zipBuffer;
  static uint8_t* rowBuffer;
  static uint8_t* ditherBuffer;
  static uint8_t* imageBuffer2;
  static uint8_t* scratchBuffer;

  // Dither row accessors
  static int16_t* ditherRow0() { return reinterpret_cast<int16_t*>(ditherBuffer); }
  static int16_t* ditherRow1() { return reinterpret_cast<int16_t*>(ditherBuffer + 4000); }
  static int16_t* ditherRow2() { return reinterpret_cast<int16_t*>(ditherBuffer + 8000); }

  // Initialize arena (allocates memory)
  static bool init();

  // Release arena (frees memory for other uses like BLE)
  static void release();

  // Check if arena is currently allocated
  static bool isInitialized() { return initialized_; }

  // --- Bump allocator for temporary scratch allocations ---
  // Returns aligned pointer to `size` bytes from the arena, or nullptr if exhausted.
  // Memory is valid until scratchReset() is called.
  static void* scratchAlloc(size_t size);

  // Reset bump allocator watermark — all scratch allocations become invalid.
  static void scratchReset();

  // Bytes remaining in the scratch region.
  static size_t scratchRemaining();

  static void printStatus();

  static constexpr size_t totalSize() {
    return PRIMARY_BUFFER_SIZE + WORK_BUFFER_SIZE;
  }

 private:
  friend class ArenaScratch;
  static bool initialized_;
  static uint8_t* arenaBase_;  // Keep track of base for freeing
  static size_t scratchOffset_;  // Bump allocator watermark (byte offset from arenaBase_)
  static void scratchSetOffset(size_t offset) { scratchOffset_ = offset; }
  MemoryArena() = delete;
};

/**
 * RAII guard that resets the arena bump allocator on destruction.
 * Use this around text layout operations to ensure scratch memory is reclaimed.
 *
 * Usage:
 *   {
 *     sumi::ArenaScratch guard;
 *     int* dp = guard.alloc<int>(n);  // from arena, not heap
 *     // ... use dp ...
 *   }  // automatically calls scratchReset()
 */
class ArenaScratch {
  size_t savedOffset_;

 public:
  ArenaScratch();
  ~ArenaScratch();

  // Non-copyable, non-movable
  ArenaScratch(const ArenaScratch&) = delete;
  ArenaScratch& operator=(const ArenaScratch&) = delete;

  // Typed allocation helper. Returns nullptr if arena is exhausted or not initialized.
  template <typename T>
  T* alloc(size_t count) {
    void* p = MemoryArena::scratchAlloc(count * sizeof(T));
    return static_cast<T*>(p);
  }

  // Returns true if the arena is available for scratch allocations
  bool isValid() const;
};

}  // namespace sumi
