#pragma once

#include <cstddef>
#include <cstdint>

namespace sumi {

/**
 * Pre-allocated memory arena - 76KB for image/cache and text layout operations.
 * Split into three independent allocations (32KB + 20KB + 24KB) to avoid requiring
 * large contiguous heap blocks, which fail when BLE fragments memory.
 *
 * Layout:
 *   Primary    (32KB): ZIP LZ77 decompression dictionary
 *   Work       (20KB): scratch (8KB) + dither (8KB) + image rows (4KB)
 *   Task Stack (24KB): PageCache background task stack (for xTaskCreateStatic)
 *
 * The arena can be released when not needed (e.g., BLE transfer, emulator)
 * to free heap for other operations, then reclaimed when needed again.
 *
 * Bump allocator: The 8KB scratch region can be used as a temporary scratch pool
 * via scratchAlloc(). Used by text layout (DP arrays). Use ArenaScratch for RAII.
 */
class MemoryArena {
 public:
  // === PRIMARY BUFFER (32KB) ===
  static constexpr size_t PRIMARY_BUFFER_SIZE = 32 * 1024;

  // === WORK BUFFER (20KB) ===
  // Reduced from 26KB — the old layout had 6KB of unmapped spare that was allocated
  // but never used. On a 380KB device, 6KB back to the heap matters.
  static constexpr size_t WORK_BUFFER_SIZE = 20 * 1024;

  // Work buffer regions (must fit in WORK_BUFFER_SIZE = 20KB)
  static constexpr size_t SCRATCH_BUFFER_SIZE = 8 * 1024;     // 8KB - text layout DP arrays (1024 words)
  static constexpr size_t DITHER_REGION_SIZE = 8 * 1024;      // 8KB - ditherer error rows (3x ~1.6KB)
  static constexpr size_t IMAGE_ROW_REGION_SIZE = 4 * 1024;   // 4KB - GfxRenderer bitmap row buffers
  // Total: 8 + 8 + 4 = 20KB (exact fit, no waste)

  // === TASK STACK (24KB) - separate allocation ===
  static constexpr size_t TASK_STACK_SIZE = 24 * 1024;        // 24KB - PageCache background task stack

  // Legacy size constants (for Epub.cpp zipBuffer reference)
  static constexpr size_t ZIP_BUFFER_SIZE = 32 * 1024;

  // Buffer pointers (valid after init(), nullptr after release())
  static uint8_t* primaryBuffer;   // 32KB - ZIP LZ77 dictionary
  static uint8_t* zipBuffer;       // Alias for primaryBuffer (first 32KB used by LZ77)
  static uint8_t* scratchBuffer;   // 8KB - bump allocator region
  static uint8_t* ditherRegion;    // 8KB - ditherer error rows
  static uint8_t* imageRowRegion;  // 4KB - bitmap row buffers
  static uint8_t* taskStackRegion; // 24KB - background task stack (for xTaskCreateStatic)
  static uint8_t* fallbackBuffer;  // Framebuffer fallback for ZIP when arena unavailable

  // Initialize arena (allocates memory)
  static bool init();

  // Release arena (frees memory for other uses like BLE transfer, emulator)
  static void release();

  // Temporarily release/reclaim just the 32KB primary buffer.
  // Used to free heap for parsing when BLE is connected (parser uses framebuffer as ZIP dict).
  static void releasePrimary();
  static bool reclaimPrimary();

  // Check if arena is currently allocated
  static bool isInitialized() { return initialized_; }

  // --- Bump allocator for temporary scratch allocations ---
  static void* scratchAlloc(size_t size);
  static void scratchReset();
  static size_t scratchRemaining();

  static void printStatus();

  static constexpr size_t totalSize() {
    return PRIMARY_BUFFER_SIZE + WORK_BUFFER_SIZE + TASK_STACK_SIZE;
  }

 private:
  friend class ArenaScratch;
  static bool initialized_;
  static uint8_t* primaryBase_;    // 32KB allocation base
  static uint8_t* workBase_;       // 26KB allocation base
  static uint8_t* taskStackBase_;  // 24KB allocation base
  static size_t scratchOffset_;    // Bump allocator watermark within scratch region
  static void scratchSetOffset(size_t offset) { scratchOffset_ = offset; }
  MemoryArena() = delete;
};

/**
 * RAII guard that resets the arena bump allocator on destruction.
 * Use this around text layout operations to ensure scratch memory is reclaimed.
 */
class ArenaScratch {
  size_t savedOffset_;

 public:
  ArenaScratch();
  ~ArenaScratch();

  ArenaScratch(const ArenaScratch&) = delete;
  ArenaScratch& operator=(const ArenaScratch&) = delete;

  template <typename T>
  T* alloc(size_t count) {
    void* p = MemoryArena::scratchAlloc(count * sizeof(T));
    return static_cast<T*>(p);
  }

  bool isValid() const;
};

}  // namespace sumi
