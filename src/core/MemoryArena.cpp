#include "MemoryArena.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace sumi {

uint8_t* MemoryArena::primaryBase_ = nullptr;
uint8_t* MemoryArena::workBase_ = nullptr;
uint8_t* MemoryArena::taskStackBase_ = nullptr;
uint8_t* MemoryArena::primaryBuffer = nullptr;
uint8_t* MemoryArena::zipBuffer = nullptr;
uint8_t* MemoryArena::scratchBuffer = nullptr;
uint8_t* MemoryArena::ditherRegion = nullptr;
uint8_t* MemoryArena::imageRowRegion = nullptr;
uint8_t* MemoryArena::taskStackRegion = nullptr;
uint8_t* MemoryArena::fallbackBuffer = nullptr;
bool MemoryArena::initialized_ = false;
size_t MemoryArena::scratchOffset_ = 0;

bool MemoryArena::init() {
  if (initialized_) {
    return true;
  }

  Serial.printf("[%lu] [MEM] Allocating arena (58KB essential + 24KB task stack)\n", millis());
  Serial.printf("[%lu] [MEM] Heap before: free=%lu, largest=%lu\n", millis(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());

  // Allocate primary buffer (32KB) — ZIP LZ77 dictionary
  primaryBase_ = static_cast<uint8_t*>(heap_caps_malloc(PRIMARY_BUFFER_SIZE, MALLOC_CAP_8BIT));
  if (!primaryBase_) {
    Serial.printf("[%lu] [MEM] FATAL: Failed to allocate %zuKB primary buffer\n", millis(), PRIMARY_BUFFER_SIZE / 1024);
    return false;
  }

  // Allocate work buffer (26KB) — scratch, dither, image rows
  workBase_ = static_cast<uint8_t*>(heap_caps_malloc(WORK_BUFFER_SIZE, MALLOC_CAP_8BIT));
  if (!workBase_) {
    Serial.printf("[%lu] [MEM] FATAL: Failed to allocate %zuKB work buffer\n", millis(), WORK_BUFFER_SIZE / 1024);
    heap_caps_free(primaryBase_);
    primaryBase_ = nullptr;
    return false;
  }

  // Allocate task stack buffer (24KB) — optional, falls back to heap xTaskCreate
  taskStackBase_ = static_cast<uint8_t*>(heap_caps_malloc(TASK_STACK_SIZE, MALLOC_CAP_8BIT));
  if (!taskStackBase_) {
    Serial.printf("[%lu] [MEM] Task stack (24KB) unavailable — will use heap fallback\n", millis());
    // Not fatal: BackgroundTask::start() with heap allocation still works
  }

  // PRIMARY BUFFER (32KB)
  primaryBuffer = primaryBase_;
  zipBuffer = primaryBuffer;

  // WORK BUFFER regions (26KB)
  size_t offset = 0;

  scratchBuffer = workBase_ + offset;
  offset += SCRATCH_BUFFER_SIZE;

  ditherRegion = workBase_ + offset;
  offset += DITHER_REGION_SIZE;

  imageRowRegion = workBase_ + offset;
  offset += IMAGE_ROW_REGION_SIZE;

  // Remaining 6KB is spare (not mapped to a pointer)

  // TASK STACK (24KB) — separate allocation (may be null)
  taskStackRegion = taskStackBase_;

  memset(primaryBase_, 0, PRIMARY_BUFFER_SIZE);
  memset(workBase_, 0, WORK_BUFFER_SIZE);
  if (taskStackBase_) {
    memset(taskStackBase_, 0, TASK_STACK_SIZE);
  }
  scratchOffset_ = 0;
  initialized_ = true;

  Serial.printf("[%lu] [MEM] Heap after: free=%lu, largest=%lu\n", millis(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
  Serial.printf("[%lu] [MEM] Arena ready (32+26%s)\n", millis(),
                taskStackBase_ ? "+24KB" : "KB, task stack on heap");

  return true;
}

void MemoryArena::releasePrimary() {
  if (!primaryBase_) return;
  heap_caps_free(primaryBase_);
  primaryBase_ = nullptr;
  primaryBuffer = nullptr;
  zipBuffer = nullptr;
  Serial.printf("[%lu] [MEM] Released primary 32KB, heap free=%lu\n", millis(), ESP.getFreeHeap());
}

bool MemoryArena::reclaimPrimary() {
  if (primaryBase_) return true;
  primaryBase_ = static_cast<uint8_t*>(heap_caps_malloc(PRIMARY_BUFFER_SIZE, MALLOC_CAP_8BIT));
  if (!primaryBase_) {
    Serial.printf("[%lu] [MEM] Failed to reclaim primary 32KB\n", millis());
    return false;
  }
  primaryBuffer = primaryBase_;
  zipBuffer = primaryBuffer;
  memset(primaryBase_, 0, PRIMARY_BUFFER_SIZE);
  Serial.printf("[%lu] [MEM] Reclaimed primary 32KB, heap free=%lu\n", millis(), ESP.getFreeHeap());
  return true;
}

void MemoryArena::release() {
  if (!initialized_) {
    return;
  }

  Serial.printf("[%lu] [MEM] Releasing arena (%zuKB)\n", millis(), totalSize() / 1024);
  Serial.printf("[%lu] [MEM] Heap before release: free=%lu, largest=%lu\n", millis(),
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  if (primaryBase_) {
    heap_caps_free(primaryBase_);
    primaryBase_ = nullptr;
  }
  if (workBase_) {
    heap_caps_free(workBase_);
    workBase_ = nullptr;
  }
  if (taskStackBase_) {
    heap_caps_free(taskStackBase_);
    taskStackBase_ = nullptr;
  }

  primaryBuffer = nullptr;
  zipBuffer = nullptr;
  scratchBuffer = nullptr;
  ditherRegion = nullptr;
  imageRowRegion = nullptr;
  taskStackRegion = nullptr;
  scratchOffset_ = 0;
  initialized_ = false;

  Serial.printf("[%lu] [MEM] Heap after release: free=%lu, largest=%lu\n", millis(),
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

void* MemoryArena::scratchAlloc(size_t size) {
  if (!initialized_ || !scratchBuffer || size == 0) {
    return nullptr;
  }

  // Align to 4-byte boundary (required for int/size_t arrays on ESP32)
  const size_t alignedOffset = (scratchOffset_ + 3) & ~static_cast<size_t>(3);

  if (alignedOffset + size > SCRATCH_BUFFER_SIZE) {
    return nullptr;
  }

  void* ptr = scratchBuffer + alignedOffset;
  scratchOffset_ = alignedOffset + size;
  return ptr;
}

void MemoryArena::scratchReset() {
  scratchOffset_ = 0;
}

size_t MemoryArena::scratchRemaining() {
  if (!initialized_ || !scratchBuffer) return 0;
  const size_t alignedOffset = (scratchOffset_ + 3) & ~static_cast<size_t>(3);
  return (alignedOffset < SCRATCH_BUFFER_SIZE) ? (SCRATCH_BUFFER_SIZE - alignedOffset) : 0;
}

void MemoryArena::printStatus() {
  if (initialized_) {
    Serial.println("[MEM] === Arena Status (32+26+24KB) ===");
    Serial.printf("[MEM] PRIMARY (32KB): %p\n", primaryBuffer);
    Serial.printf("[MEM] WORK (26KB): scratch=%p dither=%p imgrow=%p\n",
                  scratchBuffer, ditherRegion, imageRowRegion);
    Serial.printf("[MEM] TASK STACK (24KB): %p\n", taskStackRegion);
    Serial.printf("[MEM] Scratch: %zu/%zu bytes used\n", scratchOffset_, SCRATCH_BUFFER_SIZE);
  } else {
    Serial.println("[MEM] === Arena Status (RELEASED) ===");
  }
  Serial.printf("[MEM] Heap free: %lu, largest: %lu\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

// --- ArenaScratch RAII guard ---

ArenaScratch::ArenaScratch() : savedOffset_(MemoryArena::scratchOffset_) {}

ArenaScratch::~ArenaScratch() {
  MemoryArena::scratchSetOffset(savedOffset_);
}

bool ArenaScratch::isValid() const { return MemoryArena::isInitialized() && MemoryArena::scratchBuffer != nullptr; }

}  // namespace sumi
