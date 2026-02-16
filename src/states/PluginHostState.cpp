#include "PluginHostState.h"

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <GfxRenderer.h>

#if FEATURE_BLUETOOTH
#include "../ble/BleHid.h"
#endif

#include "../core/Core.h"
#include "ThemeManager.h"

namespace sumi {

PluginHostState::PluginHostState(GfxRenderer& renderer) : renderer_(renderer), pluginRenderer_(renderer) {}

PluginHostState::~PluginHostState() {
  if (plugin_) {
    plugin_->cleanup();
    delete plugin_;
    plugin_ = nullptr;
  }
}

void PluginHostState::enter(Core& core) {
  Serial.printf("[PLUGIN_HOST] Entering, free heap: %lu\n", (unsigned long)ESP.getFreeHeap());

  goBack_ = false;
  needsRender_ = true;
  partialCount_ = 0;
  lastUpdateMs_ = millis();
  backPressCount_ = 0;

  if (plugin_) {
    plugin_->cleanup();
    delete plugin_;
    plugin_ = nullptr;
  }

  if (!factory_) {
    Serial.println("[PLUGIN_HOST] ERROR: No plugin factory set!");
    goBack_ = true;
    return;
  }

  plugin_ = factory_();
  if (!plugin_) {
    Serial.println("[PLUGIN_HOST] ERROR: Plugin factory returned null!");
    goBack_ = true;
    return;
  }

  Serial.printf("[PLUGIN_HOST] Created: %s, free heap: %lu\n", plugin_->name(), (unsigned long)ESP.getFreeHeap());

  // Switch to landscape if plugin requests it
  isLandscape_ = plugin_->wantsLandscape();
  if (isLandscape_) {
    renderer_.setOrientation(GfxRenderer::LandscapeClockwise);
    Serial.println("[PLUGIN_HOST] Switched to landscape mode");
  }

  pluginRenderer_.setRegularFontId(THEME.uiFontId);
  pluginRenderer_.setSmallFontId(THEME.smallFontId);
  plugin_->init(renderer_.getScreenWidth(), renderer_.getScreenHeight());
  plugin_->needsFullRedraw = true;

#if FEATURE_BLUETOOTH
  // Auto-scan for BLE keyboard when launching Notes (needs keyboard input)
  if (strcmp(plugin_->name(), "Notes") == 0 && !ble::isConnected()) {
    // Show scanning message
    renderer_.clearScreen(0xFF);
    const int font = THEME.uiFontId;
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 - 20,
                               "Scanning for keyboard...", true, EpdFontFamily::BOLD);
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 + 20,
                               "Make sure it's in pairing mode", true);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

    ble::init();
    ble::startScan(6);

    // Auto-connect to first HID device found
    bool connected = false;
    for (int i = 0; i < ble::scanResultCount(); i++) {
      const BleDevice* dev = ble::scanResult(i);
      if (dev && dev->hasHID) {
        renderer_.clearScreen(0xFF);
        char msg[64];
        snprintf(msg, sizeof(msg), "Connecting: %s", dev->name);
        renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2, msg, true);
        renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

        if (ble::connectTo(i)) {
          connected = true;
          Serial.printf("[PLUGIN_HOST] Auto-connected BLE: %s\n", dev->name);
        }
        break;
      }
    }

    if (!connected && ble::scanResultCount() == 0) {
      renderer_.clearScreen(0xFF);
      renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 - 10,
                                 "No keyboard found", true);
      renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 + 20,
                                 "Starting without keyboard", true);
      renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
      delay(1500);
    }

    // Force full redraw after scan screens
    plugin_->needsFullRedraw = true;
    needsRender_ = true;
  }
#endif
}

void PluginHostState::exit(Core& core) {
  if (plugin_) {
    plugin_->cleanup();
    delete plugin_;
    plugin_ = nullptr;
  }
  factory_ = nullptr;

  // Restore portrait orientation if we switched to landscape
  if (isLandscape_) {
    renderer_.setOrientation(GfxRenderer::Portrait);
    isLandscape_ = false;
    Serial.println("[PLUGIN_HOST] Restored portrait mode");
  }

  Serial.printf("[PLUGIN_HOST] Exited, free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
}

PluginButton PluginHostState::translateButton(Button btn) const {
  switch (btn) {
    case Button::Up:     return PluginButton::Up;
    case Button::Down:   return PluginButton::Down;
    case Button::Left:   return PluginButton::Left;
    case Button::Right:  return PluginButton::Right;
    case Button::Center: return PluginButton::Center;
    case Button::Back:   return PluginButton::Back;
    case Button::Power:  return PluginButton::Power;
    default:             return PluginButton::None;
  }
}

StateTransition PluginHostState::update(Core& core) {
  if (goBack_ || !plugin_) {
    goBack_ = false;
    return StateTransition::to(StateId::PluginList);
  }

  Event e;
  while (core.events.pop(e)) {
    // Long-press Back ALWAYS force-exits, no matter what the plugin does
    if (e.type == EventType::ButtonLongPress && e.button == Button::Back) {
      Serial.println("[PLUGIN_HOST] Long-press Back -> force exit");
      goBack_ = true;
      needsRender_ = true;
      continue;
    }

    // Long-press Power -> sleep
    if (e.type == EventType::ButtonLongPress && e.button == Button::Power) {
      return StateTransition::to(StateId::Sleep);
    }

    if (e.type != EventType::ButtonPress) continue;

    PluginButton pbtn = translateButton(e.button);
    if (pbtn == PluginButton::None) continue;

    // Power always goes to sleep
    if (pbtn == PluginButton::Power) {
      return StateTransition::to(StateId::Sleep);
    }

    // Track rapid Back presses - 3 quick Backs force-exits
    if (pbtn == PluginButton::Back) {
      unsigned long now = millis();
      if (now - lastBackMs_ < 800) {
        backPressCount_++;
      } else {
        backPressCount_ = 1;
      }
      lastBackMs_ = now;

      if (backPressCount_ >= 3) {
        Serial.println("[PLUGIN_HOST] 3x Back -> force exit");
        goBack_ = true;
        needsRender_ = true;
        continue;
      }
    } else {
      backPressCount_ = 0;
    }

    bool consumed = plugin_->handleInput(pbtn);

    if (!consumed && (pbtn == PluginButton::Back || pbtn == PluginButton::Center)) {
      // Plugin returned false for Back (wants to go back) or Center (e.g. Save & Exit)
      goBack_ = true;
    }

    needsRender_ = true;
  }

  // Poll BLE keyboard input
#if FEATURE_BLUETOOTH
  if (ble::isReady() && ble::isConnected() && plugin_) {
    BleKey bk;
    while ((bk = ble::poll()) != BleKey::NONE) {
      switch (bk) {
        case BleKey::KEY_CHAR:
          if (plugin_->handleChar(ble::lastChar())) {
            needsRender_ = true;
          }
          break;
        case BleKey::KEY_RETURN:
          if (plugin_->handleChar('\n')) needsRender_ = true;
          break;
        case BleKey::KEY_TAB:
          if (plugin_->handleChar('\t')) needsRender_ = true;
          break;
        case BleKey::KEY_BACKSPACE: {
          // Send backspace as char first; if not consumed, treat as Back button
          if (!plugin_->handleChar('\b')) {
            if (!plugin_->handleInput(PluginButton::Back)) goBack_ = true;
          }
          needsRender_ = true;
          break;
        }
        case BleKey::KEY_UP:
          plugin_->handleInput(PluginButton::Up); needsRender_ = true; break;
        case BleKey::KEY_DOWN:
          plugin_->handleInput(PluginButton::Down); needsRender_ = true; break;
        case BleKey::KEY_LEFT:
          plugin_->handleInput(PluginButton::Left); needsRender_ = true; break;
        case BleKey::KEY_RIGHT:
          plugin_->handleInput(PluginButton::Right); needsRender_ = true; break;
        case BleKey::KEY_ESCAPE:
          if (!plugin_->handleInput(PluginButton::Back)) goBack_ = true;
          needsRender_ = true; break;
        case BleKey::KEY_DELETE:
          // DEL char (127) — Notes uses this for forward-delete
          plugin_->handleChar(127);
          needsRender_ = true; break;
        case BleKey::KEY_HOME:
        case BleKey::KEY_END:
          // Not mapped to PluginButton — plugins can handle via future extension
          break;
        case BleKey::PAGE_NEXT:
          plugin_->handleInput(PluginButton::Down); needsRender_ = true; break;
        case BleKey::PAGE_PREV:
          plugin_->handleInput(PluginButton::Up); needsRender_ = true; break;
        case BleKey::ENTER:
          plugin_->handleInput(PluginButton::Center); needsRender_ = true; break;
        default: break;
      }
    }
  }
#endif

  // Periodic update for timer/animation plugins
  PluginRunMode mode = plugin_->runMode();
  if (mode == PluginRunMode::WithUpdate || mode == PluginRunMode::Animation) {
    unsigned long now = millis();
    if (now - lastUpdateMs_ >= 100) {  // 10Hz update tick
      lastUpdateMs_ = now;
      if (plugin_->update()) {
        needsRender_ = true;
      }
    }
  }

  if (mode == PluginRunMode::Animation) {
    if (!plugin_->isRunning()) goBack_ = true;
    // Only render if the plugin actually flagged something to draw
    // (via update() returning true, or needsFullRedraw set by input handler)
    if (plugin_->needsFullRedraw) {
      needsRender_ = true;
    }
  }

  if (goBack_) {
    goBack_ = false;
    return StateTransition::to(StateId::PluginList);
  }

  return StateTransition::stay(StateId::PluginHost);
}

void PluginHostState::render(Core& core) {
  if (!needsRender_ || !plugin_) return;

  renderer_.clearScreen(0xFF);

  if (plugin_->needsFullRedraw || partialCount_ == 0) {
    plugin_->draw();
    plugin_->needsFullRedraw = false;
  } else {
    plugin_->drawPartial();
  }

  // Skip display refresh if plugin handles its own (e.g. Benchmark timing modes)
  if (plugin_->handlesOwnRefresh()) {
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  // Periodic full refresh to clear e-ink ghosting
  partialCount_++;
  if (partialCount_ >= 30) {
    renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
    partialCount_ = 0;
  } else {
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
  }

  needsRender_ = false;
  core.display.markDirty();
}

}  // namespace sumi

#endif
