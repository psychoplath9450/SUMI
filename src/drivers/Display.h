#pragma once

#include <cstddef>
#include <cstdint>

#include "../core/Result.h"

class EInkDisplay;

namespace sumi {
namespace drivers {

class Display {
 public:
  enum class RefreshMode : uint8_t {
    Full,
    Half,
    Fast,
  };

  Result<void> init();
  void shutdown();

  // Buffer access
  uint8_t* getBuffer();
  const uint8_t* getBuffer() const;
  static constexpr size_t bufferSize();

  // Dimensions
  static constexpr uint16_t width();
  static constexpr uint16_t height();

  // Rendering control
  void markDirty() { dirty_ = true; }
  bool isDirty() const { return dirty_; }
  void flush(RefreshMode mode = RefreshMode::Fast);
  void clear(uint8_t color = 0xFF);

  // Power management
  void sleep();
  void wake();

  // Access underlying display (for legacy code during migration)
  EInkDisplay& raw();

 private:
  bool dirty_ = false;
  bool initialized_ = false;
};

// Inline constexpr methods
constexpr size_t Display::bufferSize() {
  return 48000;  // 800 * 480 / 8 = 48000 bytes
}

constexpr uint16_t Display::width() { return 800; }

constexpr uint16_t Display::height() { return 480; }

}  // namespace drivers
}  // namespace sumi
