#include "Core.h"

#include <Arduino.h>

namespace sumi {

Result<void> Core::init() {
  logMemory("Core::init start");

  // Storage first - needed for settings/themes
  TRY(storage.init());
  logMemory("Storage initialized");

  // Note: Settings are loaded earlier in setup() via loadFromFile()
  // before Core::init() is called (needed for theme/font setup)

  // Display
  TRY(display.init());
  logMemory("Display initialized");

  // Input - connects to event queue
  TRY(input.init(events));
  logMemory("Input initialized");

  logMemory("Core::init complete");
  return Ok();
}

void Core::shutdown() {
  logMemory("Core::shutdown");

  // Shutdown in reverse order
  input.shutdown();
  display.shutdown();
  storage.shutdown();
}

uint32_t Core::freeHeap() const { return ESP.getFreeHeap(); }

void Core::logMemory(const char* label) const {
  Serial.printf("[MEM] %s: free=%lu, largest=%lu\n", label, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

}  // namespace sumi
