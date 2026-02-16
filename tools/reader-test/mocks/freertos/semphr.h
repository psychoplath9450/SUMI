#pragma once

#include "FreeRTOS.h"

#include <mutex>

// Simplified semaphore mock - no registry needed for reader-test
struct MockSemaphore {
  std::mutex mtx;
};

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new MockSemaphore(); }

inline void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) { delete static_cast<MockSemaphore*>(xSemaphore); }

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t) {
  auto* sem = static_cast<MockSemaphore*>(xSemaphore);
  if (!sem) return pdFALSE;
  sem->mtx.lock();
  return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
  auto* sem = static_cast<MockSemaphore*>(xSemaphore);
  if (!sem) return pdFALSE;
  sem->mtx.unlock();
  return pdTRUE;
}
