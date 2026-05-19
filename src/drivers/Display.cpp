#include "Display.h"

#include <EInkDisplay.h>
#include <config.h>

// Global display instance reference (defined in main.cpp)
extern EInkDisplay& display;

namespace sumi {
namespace drivers {

Result<void> Display::init() {
  if (initialized_) {
    return Ok();
  }

  display.begin();
  initialized_ = true;
  dirty_ = false;

  return Ok();
}

void Display::shutdown() {
  if (initialized_) {
    display.deepSleep();
    initialized_ = false;
  }
}

uint8_t* Display::getBuffer() { return display.getFrameBuffer(); }

const uint8_t* Display::getBuffer() const { return display.getFrameBuffer(); }

void Display::flush(RefreshMode mode) {
  if (!dirty_ || !initialized_) {
    return;
  }

  EInkDisplay::RefreshMode einkMode;
  switch (mode) {
    case RefreshMode::Full:
      einkMode = EInkDisplay::FULL_REFRESH;
      break;
    case RefreshMode::Half:
      einkMode = EInkDisplay::HALF_REFRESH;
      break;
    case RefreshMode::Fast:
    default:
      einkMode = EInkDisplay::FAST_REFRESH;
      break;
  }

  // X3 grayscale washout fix. GfxRenderer::displayBuffer covers most
  // refresh paths, but this driver-level flush() bypasses it (used by
  // states that paint directly to the framebuffer and request a refresh
  // without going through the renderer). requestResync() self-gates on
  // _x3Mode so it's a no-op on X4. CrossPoint 1.3.0 parity.
  if (einkMode == EInkDisplay::HALF_REFRESH) {
    display.requestResync(1);
  }

  display.refreshDisplay(einkMode);
  dirty_ = false;
}

void Display::clear(uint8_t color) {
  if (initialized_) {
    display.clearScreen(color);
    dirty_ = true;
  }
}

void Display::sleep() {
  if (initialized_) {
    display.deepSleep();
  }
}

void Display::wake() {
  if (initialized_) {
    display.begin();
  }
}

EInkDisplay& Display::raw() { return display; }

}  // namespace drivers
}  // namespace sumi
