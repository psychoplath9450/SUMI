#pragma once

/**
 * @file PluginInterface.h
 * @brief Base interface for SUMI plugins
 *
 * All ported SUMI plugins implement this interface.
 * PluginHostState wraps them as a SUMI State.
 */

#include <cstdint>

#include "../config.h"

#if FEATURE_PLUGINS

namespace sumi {

// Button enum matching SUMI's button codes for plugin compatibility
enum class PluginButton : uint8_t {
  None = 0,
  Up,
  Down,
  Left,
  Right,
  Center,   // Confirm/OK/Select
  Back,
  Power,
};

// Plugin run mode — determines how PluginHostState drives the plugin
enum class PluginRunMode : uint8_t {
  Simple,       // draw() + handleInput() — most plugins
  SelfRefresh,  // plugin manages its own partial refresh regions
  WithUpdate,   // has periodic update() (timers, stopwatch)
  Animation,    // continuous frame loop (Cube3D)
};

class PluginInterface {
 public:
  virtual ~PluginInterface() = default;

  // Lifecycle
  virtual void init(int screenW, int screenH) = 0;
  virtual void cleanup() {}  // Optional: free resources before destruction

  // Drawing
  virtual void draw() = 0;
  virtual void drawPartial() { draw(); }       // Default: full redraw
  virtual void drawFullScreen() { draw(); }    // For animation plugins

  // Input — return true if consumed, false to let host handle (Back → exit)
  virtual bool handleInput(PluginButton btn) = 0;

  // Character input from BLE keyboard (override for text-input plugins)
  virtual bool handleChar(char c) { (void)c; return false; }

  // Updates — return true if display needs refresh
  virtual bool update() { return false; }

  // State queries
  virtual bool isRunning() const { return true; }  // For animation plugins
  virtual bool needsRedraw() const { return needsFullRedraw; }
  virtual PluginRunMode runMode() const { return PluginRunMode::Simple; }

  // Name for debug logging
  virtual const char* name() const = 0;

  // Orientation — override to return true for landscape mode (800x480 on this display)
  virtual bool wantsLandscape() const { return false; }

  // Self-refresh — override to return true if plugin calls displayBuffer() itself
  // (used by Benchmark to time refresh modes directly)
  virtual bool handlesOwnRefresh() const { return false; }

  bool needsFullRedraw = true;
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
