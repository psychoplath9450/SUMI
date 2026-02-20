#include "MemoryArena.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace sumi {

uint8_t* MemoryArena::arenaBase_ = nullptr;
uint8_t* MemoryArena::primaryBuffer = nullptr;
uint8_t* MemoryArena::imageBuffer = nullptr;
uint8_t* MemoryArena::zipBuffer = nullptr;
uint8_t* MemoryArena::rowBuffer = nullptr;
uint8_t* MemoryArena::ditherBuffer = nullptr;
uint8_t* MemoryArena::imageBuffer2 = nullptr;
uint8_t* MemoryArena::scratchBuffer = nullptr;
bool MemoryArena::initialized_ = false;
size_t MemoryArena::scratchOffset_ = 0;

bool MemoryArena::init() {
  if (initialized_) {
    return true;
  }

  Serial.printf("[%lu] [MEM] Allocating memory arena (%zuKB)\n", millis(), totalSize() / 1024);
  Serial.printf("[%lu] [MEM] Heap before: free=%lu, largest=%lu\n", millis(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());

  arenaBase_ = static_cast<uint8_t*>(heap_caps_malloc(totalSize(), MALLOC_CAP_8BIT));
  if (!arenaBase_) {
    Serial.printf("[%lu] [MEM] FATAL: Failed to allocate %zuKB arena\n", millis(), totalSize() / 1024);
    return false;
  }

  // PRIMARY BUFFER (32KB) - shared ZIP/JPEG
  primaryBuffer = arenaBase_;
  imageBuffer = primaryBuffer;
  zipBuffer = primaryBuffer;

  // WORK BUFFER (48KB)
  uint8_t* workBuffer = arenaBase_ + PRIMARY_BUFFER_SIZE;
  size_t offset = 0;

  rowBuffer = workBuffer + offset;
  offset += ROW_BUFFER_SIZE;

  ditherBuffer = workBuffer + offset;
  offset += DITHER_BUFFER_SIZE;

  imageBuffer2 = workBuffer + offset;
  offset += IMAGE_BUFFER2_SIZE;

  scratchBuffer = workBuffer + offset;

  memset(arenaBase_, 0, totalSize());
  scratchOffset_ = 0;
  initialized_ = true;

  Serial.printf("[%lu] [MEM] Heap after: free=%lu, largest=%lu\n", millis(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
  Serial.printf("[%lu] [MEM] Arena ready (%zuKB scratch available)\n", millis(), totalSize() / 1024);

  return true;
}

void MemoryArena::release() {
  if (!initialized_ || !arenaBase_) {
    return;
  }

  Serial.printf("[%lu] [MEM] Releasing arena (%zuKB)\n", millis(), totalSize() / 1024);
  Serial.printf("[%lu] [MEM] Heap before release: free=%lu, largest=%lu\n", millis(),
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  heap_caps_free(arenaBase_);

  // Clear all pointers
  arenaBase_ = nullptr;
  primaryBuffer = nullptr;
  imageBuffer = nullptr;
  zipBuffer = nullptr;
  rowBuffer = nullptr;
  ditherBuffer = nullptr;
  imageBuffer2 = nullptr;
  scratchBuffer = nullptr;
  scratchOffset_ = 0;
  initialized_ = false;

  Serial.printf("[%lu] [MEM] Heap after release: free=%lu, largest=%lu\n", millis(),
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

void* MemoryArena::scratchAlloc(size_t size) {
  if (!initialized_ || !arenaBase_ || size == 0) {
    return nullptr;
  }

  // Align to 4-byte boundary (required for int/size_t arrays on ESP32)
  const size_t alignedOffset = (scratchOffset_ + 3) & ~static_cast<size_t>(3);
  const size_t total = totalSize();

  if (alignedOffset + size > total) {
    return nullptr;
  }

  void* ptr = arenaBase_ + alignedOffset;
  scratchOffset_ = alignedOffset + size;
  return ptr;
}

void MemoryArena::scratchReset() {
  scratchOffset_ = 0;
}

size_t MemoryArena::scratchRemaining() {
  if (!initialized_) return 0;
  const size_t alignedOffset = (scratchOffset_ + 3) & ~static_cast<size_t>(3);
  const size_t total = totalSize();
  return (alignedOffset < total) ? (total - alignedOffset) : 0;
}

void MemoryArena::printStatus() {
  if (initialized_) {
    Serial.println("[MEM] === Arena Status (80KB allocated) ===");
    Serial.printf("[MEM] PRIMARY (32KB): %p\n", primaryBuffer);
    Serial.printf("[MEM] WORK (48KB): row=%p dither=%p buf2=%p scratch=%p\n",
                  rowBuffer, ditherBuffer, imageBuffer2, scratchBuffer);
    Serial.printf("[MEM] Bump: %zu/%zu bytes used\n", scratchOffset_, totalSize());
  } else {
    Serial.println("[MEM] === Arena Status (RELEASED) ===");
  }
  Serial.printf("[MEM] Heap free: %lu, largest: %lu\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

// --- ArenaScratch RAII guard ---

ArenaScratch::ArenaScratch() : savedOffset_(MemoryArena::scratchOffset_) {}

ArenaScratch::~ArenaScratch() {
  // Restore watermark to where it was when this guard was created.
  // Correctly handles nested ArenaScratch guards.
  MemoryArena::scratchSetOffset(savedOffset_);
}

bool ArenaScratch::isValid() const { return MemoryArena::isInitialized(); }

}  // namespace sumi
