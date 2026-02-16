#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * RAII mutex lock for FreeRTOS semaphores.
 * Automatically releases mutex when scope exits.
 *
 * Based on: https://www.esp32.com/viewtopic.php?t=32432
 * "Found a case where the function crashed and exited without
 *  returning the semaphore first, this was causing the issue."
 *
 * CRITICAL: Keep scopes SHORT. Never hold mutex across:
 * - Long operations (>100ms)
 * - File/Network I/O
 * - Blocking waits
 */
class ScopedMutex {
 public:
  explicit ScopedMutex(SemaphoreHandle_t mutex, TickType_t timeout = portMAX_DELAY) : mutex_(mutex), acquired_(false) {
    if (mutex_) {
      acquired_ = (xSemaphoreTake(mutex_, timeout) == pdTRUE);
    }
  }

  ~ScopedMutex() { release(); }

  // Non-copyable (prevent double-release)
  ScopedMutex(const ScopedMutex&) = delete;
  ScopedMutex& operator=(const ScopedMutex&) = delete;

  // Move-only (no move-assignment - would require releasing current mutex first)
  ScopedMutex(ScopedMutex&& other) noexcept : mutex_(other.mutex_), acquired_(other.acquired_) {
    other.mutex_ = nullptr;
    other.acquired_ = false;
  }
  ScopedMutex& operator=(ScopedMutex&&) = delete;

  /** Check if mutex was successfully acquired. */
  bool acquired() const { return acquired_; }
  explicit operator bool() const { return acquired_; }

  /** Early release (before scope exit). */
  void release() {
    if (acquired_ && mutex_) {
      xSemaphoreGive(mutex_);
      acquired_ = false;
    }
  }

 private:
  SemaphoreHandle_t mutex_;
  bool acquired_;
};

// Helper macro - creates unique variable name
#define SCOPED_LOCK_IMPL2(mutex, line) ScopedMutex _scopedLock##line(mutex)
#define SCOPED_LOCK_IMPL(mutex, line) SCOPED_LOCK_IMPL2(mutex, line)
#define SCOPED_LOCK(mutex) SCOPED_LOCK_IMPL(mutex, __LINE__)
