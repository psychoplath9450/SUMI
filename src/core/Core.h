#pragma once

#include "../content/ContentHandle.h"
#include "../drivers/Display.h"
#include "../drivers/Input.h"
#include "../drivers/Storage.h"
#include "EventQueue.h"
#include "SumiSettings.h"
#include "Result.h"
#include "Types.h"

namespace sumi {

struct Core {
  // === Drivers (thin wrappers, no heap) ===
  drivers::Display display;
  drivers::Storage storage;
  drivers::Input input;

  // === Settings ===
  Settings settings;

  // === Content (tagged union - one book at a time) ===
  ContentHandle content;

  // === Events (fixed ring buffer) ===
  EventQueue events;

  // === Shared buffers (pre-allocated, reused) ===
  struct Buffers {
    char path[BufferSize::Path];
    char text[BufferSize::Text];
    uint8_t decompress[BufferSize::Decompress];
  } buf;

  // === Lifecycle ===
  Result<void> init();
  void shutdown();

  // === Debug ===
  uint32_t freeHeap() const;
  void logMemory(const char* label) const;
};

// Global core instance (defined in main.cpp)
extern Core core;

}  // namespace sumi
