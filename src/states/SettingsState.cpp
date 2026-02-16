#include "SettingsState.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
// SdFat and FS.h both define FILE_READ/FILE_WRITE - undef before LittleFS re-includes FS.h
#undef FILE_READ
#undef FILE_WRITE
#include <LittleFS.h>

#include <algorithm>

#include "../Battery.h"
#include "../config.h"
#include "../core/Core.h"
#include "../ui/Elements.h"
#if FEATURE_BLUETOOTH
#include "../ble/BleHid.h"
#endif
#include "MappedInputManager.h"
#include "ThemeManager.h"

namespace sumi {

SettingsState::SettingsState(GfxRenderer& renderer)
    : renderer_(renderer),
      core_(nullptr),
      currentScreen_(SettingsScreen::Menu),
      needsRender_(true),
      goHome_(false),
      themeWasChanged_(false),
      pendingAction_(0),
      menuView_{},
      readerView_{},
      deviceView_{},
      cleanupView_{},
      confirmView_{},
      infoView_{} {}

SettingsState::~SettingsState() = default;

void SettingsState::enter(Core& core) {
  Serial.println("[SETTINGS] Entering");
  core_ = &core;  // Store for helper methods
  currentScreen_ = SettingsScreen::Menu;

  // Reset all views to ensure clean state
  menuView_.selected = 0;
  menuView_.needsRender = true;
  readerView_.selected = 0;
  readerView_.needsRender = true;
  deviceView_.selected = 0;
  deviceView_.needsRender = true;
  cleanupView_.selected = 0;
  cleanupView_.needsRender = true;
  confirmView_.needsRender = true;
  infoView_.clear();
  infoView_.needsRender = true;

  needsRender_ = true;
  goHome_ = false;
  goApps_ = false;
  themeWasChanged_ = false;
  pendingAction_ = 0;
}

void SettingsState::exit(Core& core) {
  Serial.println("[SETTINGS] Exiting");
  // Save settings on exit
  core.settings.save(core.storage);
}

StateTransition SettingsState::update(Core& core) {
  Event e;
  while (core.events.pop(e)) {
    switch (e.type) {
      case EventType::ButtonPress:
        switch (e.button) {
          case Button::Up:
            switch (currentScreen_) {
              case SettingsScreen::Menu:
                menuView_.moveUp();
                break;
              case SettingsScreen::Reader:
                readerView_.moveUp();
                break;
              case SettingsScreen::Device:
                deviceView_.moveUp();
                break;
              case SettingsScreen::Cleanup:
                cleanupView_.moveUp();
                break;
              case SettingsScreen::ConfirmDialog:
                confirmView_.toggleSelection();
                break;
#if FEATURE_BLUETOOTH
              case SettingsScreen::Bluetooth:
                if (btScanned_) {
                  int count = ble::scanResultCount();
                  if (count > 0) btSelected_ = (btSelected_ == 0) ? count - 1 : btSelected_ - 1;
                }
                break;
#endif
              default:
                break;
            }
            needsRender_ = true;
            break;

          case Button::Down:
            switch (currentScreen_) {
              case SettingsScreen::Menu:
                menuView_.moveDown();
                break;
              case SettingsScreen::Reader:
                readerView_.moveDown();
                break;
              case SettingsScreen::Device:
                deviceView_.moveDown();
                break;
              case SettingsScreen::Cleanup:
                cleanupView_.moveDown();
                break;
              case SettingsScreen::ConfirmDialog:
                confirmView_.toggleSelection();
                break;
#if FEATURE_BLUETOOTH
              case SettingsScreen::Bluetooth:
                if (btScanned_) {
                  int count = ble::scanResultCount();
                  if (count > 0) btSelected_ = (btSelected_ + 1) % count;
                }
                break;
#endif
              default:
                break;
            }
            needsRender_ = true;
            break;

          case Button::Left:
            switch (currentScreen_) {
              case SettingsScreen::Menu:
                core.settings.save(core.storage);
                goHome_ = true;
                break;
              case SettingsScreen::Reader:
                if (readerView_.buttons.isActive(2)) handleLeftRight(-1);
                break;
              case SettingsScreen::Device:
                if (deviceView_.buttons.isActive(2)) handleLeftRight(-1);
                break;
              case SettingsScreen::ConfirmDialog:
                pendingAction_ = 0;
                currentScreen_ = SettingsScreen::Cleanup;
                cleanupView_.needsRender = true;
                needsRender_ = true;
                break;
              default:
                goBack(core);
                break;
            }
            break;

          case Button::Right:
            switch (currentScreen_) {
              case SettingsScreen::Reader:
                if (readerView_.buttons.isActive(3)) handleLeftRight(+1);
                break;
              case SettingsScreen::Device:
                if (deviceView_.buttons.isActive(3)) handleLeftRight(+1);
                break;
#if FEATURE_BLUETOOTH
              case SettingsScreen::Bluetooth:
                // Rescan
                enterBluetooth();
                break;
#endif
              default:
                break;
            }
            break;

          case Button::Center:
            handleConfirm(core);
            break;

          case Button::Back:
            if (currentScreen_ == SettingsScreen::Menu) {
              core.settings.save(core.storage);
              goHome_ = true;
            } else if (currentScreen_ == SettingsScreen::ConfirmDialog) {
              // Cancel confirmation dialog
              pendingAction_ = 0;
              currentScreen_ = SettingsScreen::Cleanup;
              cleanupView_.needsRender = true;
              needsRender_ = true;
            } else {
              goBack(core);
            }
            break;

          case Button::Power:
            break;
        }
        break;

      default:
        break;
    }
  }

#if FEATURE_PLUGINS
  if (goApps_) {
    goApps_ = false;
    return StateTransition::to(StateId::PluginList);
  }
#endif

  if (goHome_) {
    goHome_ = false;
    return StateTransition::to(StateId::Home);
  }

  return StateTransition::stay(StateId::Settings);
}

void SettingsState::render(Core& core) {
  if (!needsRender_) {
    bool viewNeedsRender = false;
    switch (currentScreen_) {
      case SettingsScreen::Menu:
        viewNeedsRender = menuView_.needsRender;
        break;
      case SettingsScreen::Reader:
        viewNeedsRender = readerView_.needsRender;
        break;
      case SettingsScreen::Device:
        viewNeedsRender = deviceView_.needsRender;
        break;
      case SettingsScreen::Cleanup:
        viewNeedsRender = cleanupView_.needsRender;
        break;
      case SettingsScreen::SystemInfo:
        viewNeedsRender = infoView_.needsRender;
        break;
      case SettingsScreen::ConfirmDialog:
        viewNeedsRender = confirmView_.needsRender;
        break;
    }
    if (!viewNeedsRender) {
      return;
    }
  }

  switch (currentScreen_) {
    case SettingsScreen::Menu:
      ui::render(renderer_, THEME, menuView_);
      menuView_.needsRender = false;
      break;
    case SettingsScreen::Reader:
      ui::render(renderer_, THEME, readerView_);
      readerView_.needsRender = false;
      break;
    case SettingsScreen::Device:
      ui::render(renderer_, THEME, deviceView_);
      deviceView_.needsRender = false;
      break;
    case SettingsScreen::Cleanup:
      ui::render(renderer_, THEME, cleanupView_);
      cleanupView_.needsRender = false;
      break;
    case SettingsScreen::SystemInfo:
      ui::render(renderer_, THEME, infoView_);
      infoView_.needsRender = false;
      break;
    case SettingsScreen::ConfirmDialog:
      ui::render(renderer_, THEME, confirmView_);
      confirmView_.needsRender = false;
      break;
#if FEATURE_BLUETOOTH
    case SettingsScreen::Bluetooth:
      renderBluetooth();
      break;
#endif
  }

  needsRender_ = false;
  core.display.markDirty();
}

void SettingsState::openSelected() {
  int idx = menuView_.selected;

#if FEATURE_PLUGINS
  // "Apps" is index 0 — transitions to PluginList state
  if (idx == 0) {
    goApps_ = true;
    return;
  }
  idx--;  // Shift remaining items down
#endif

  // idx 0=Reader, 1=Device, then BT/Cleanup/SystemInfo
  switch (idx) {
    case 0:  // Reader
      loadReaderSettings();
      readerView_.selected = 0;
      readerView_.needsRender = true;
      currentScreen_ = SettingsScreen::Reader;
      break;
    case 1:  // Device
      loadDeviceSettings();
      deviceView_.selected = 0;
      deviceView_.needsRender = true;
      currentScreen_ = SettingsScreen::Device;
      break;
#if FEATURE_BLUETOOTH
    case 2:  // Bluetooth
      enterBluetooth();
      currentScreen_ = SettingsScreen::Bluetooth;
      break;
    case 3:  // Cleanup
#else
    case 2:  // Cleanup
#endif
      cleanupView_.selected = 0;
      cleanupView_.needsRender = true;
      currentScreen_ = SettingsScreen::Cleanup;
      break;
#if FEATURE_BLUETOOTH
    case 4:  // System Info
#else
    case 3:  // System Info
#endif
      populateSystemInfo();
      infoView_.needsRender = true;
      currentScreen_ = SettingsScreen::SystemInfo;
      break;
  }
  needsRender_ = true;
}

void SettingsState::goBack(Core& core) {
  switch (currentScreen_) {
    case SettingsScreen::Reader:
      saveReaderSettings();
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
    case SettingsScreen::Device:
      saveDeviceSettings();
      // Apply button layouts now that we're leaving the screen
      core.settings.frontButtonLayout = std::min(deviceView_.values[6], uint8_t(Settings::FrontLRBC));
      core.settings.sideButtonLayout = std::min(deviceView_.values[7], uint8_t(Settings::NextPrev));
      ui::setFrontButtonLayout(core.settings.frontButtonLayout);
      core.input.resyncState();
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
    case SettingsScreen::Cleanup:
    case SettingsScreen::SystemInfo:
#if FEATURE_BLUETOOTH
    case SettingsScreen::Bluetooth:
#endif
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
    case SettingsScreen::ConfirmDialog:
      pendingAction_ = 0;
      currentScreen_ = SettingsScreen::Cleanup;
      cleanupView_.needsRender = true;
      break;
    default:
      break;
  }
  needsRender_ = true;
}

void SettingsState::handleConfirm(Core& core) {
  switch (currentScreen_) {
    case SettingsScreen::Menu:
      openSelected();
      break;

    case SettingsScreen::Reader:
      readerView_.cycleValue(1);
      saveReaderSettings();
      needsRender_ = true;
      break;

    case SettingsScreen::Device:
      deviceView_.cycleValue(1);
      saveDeviceSettings();
      needsRender_ = true;
      break;

    case SettingsScreen::Cleanup:
      clearCache(cleanupView_.selected, core);
      break;

    case SettingsScreen::SystemInfo:
      goBack(core);
      break;

#if FEATURE_BLUETOOTH
    case SettingsScreen::Bluetooth:
      if (btScanned_ && ble::scanResultCount() > 0) {
        btConnecting_ = true;
        needsRender_ = true;
        renderer_.clearScreen(0xFF);
        ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Connecting...");
        renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

        if (ble::connectTo(btSelected_)) {
          Serial.printf("[BLE] Connected to device %d\n", btSelected_);
          // Save address for auto-reconnect
          const BleDevice* dev = ble::scanResult(btSelected_);
          if (dev) {
            // Detect device type by name
            String nameLower = String(dev->name);
            nameLower.toLowerCase();
            bool isPageTurner = nameLower.indexOf("page") >= 0 ||
                                nameLower.indexOf("remote") >= 0 ||
                                nameLower.indexOf("clicker") >= 0 ||
                                nameLower.indexOf("shutter") >= 0 ||
                                nameLower.indexOf("free") >= 0;
            if (isPageTurner) {
              strncpy(core.settings.blePageTurner, dev->addr,
                      sizeof(core.settings.blePageTurner) - 1);
            } else {
              strncpy(core.settings.bleKeyboard, dev->addr,
                      sizeof(core.settings.bleKeyboard) - 1);
            }
            core.settings.save(core.storage);
          }
        }
        btConnecting_ = false;
        needsRender_ = true;
      } else {
        enterBluetooth();
      }
      break;
#endif

    case SettingsScreen::ConfirmDialog:
      if (confirmView_.isYesSelected()) {
        if (pendingAction_ == 10) {
          // Clear Book Cache
          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Clearing cache...");

          auto result = core.storage.rmdir(SUMI_CACHE_DIR);

          const char* msg = result.ok() ? "Cache cleared" : "No cache to clear";
          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, msg);
          vTaskDelay(1500 / portTICK_PERIOD_MS);

          pendingAction_ = 0;
          currentScreen_ = SettingsScreen::Cleanup;
          cleanupView_.needsRender = true;
          needsRender_ = true;

        } else if (pendingAction_ == 11) {
          // Clear Device Storage
          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Clearing device storage...");

          LittleFS.format();

          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Done. Restarting...");
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          ESP.restart();

        } else if (pendingAction_ == 12) {
          // Factory Reset
          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Resetting device...");

          LittleFS.format();
          core.storage.rmdir(SUMI_DIR);

          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Done. Restarting...");
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          ESP.restart();
        }
      } else {
        // No - cancel
        pendingAction_ = 0;
        currentScreen_ = SettingsScreen::Cleanup;
        cleanupView_.needsRender = true;
        needsRender_ = true;
      }
      break;
  }
}

void SettingsState::handleLeftRight(int delta) {
  if (currentScreen_ == SettingsScreen::Reader) {
    readerView_.cycleValue(delta);
    saveReaderSettings();
    needsRender_ = true;
  } else if (currentScreen_ == SettingsScreen::Device) {
    deviceView_.cycleValue(delta);
    saveDeviceSettings();
    needsRender_ = true;
  }
}

void SettingsState::loadReaderSettings() {
  auto& settings = core_->settings;

  // Index 0: Theme (ThemeSelect) - load available themes from SD card
  auto themes = THEME_MANAGER.listAvailableThemes();
  readerView_.themeCount = 0;
  readerView_.currentThemeIndex = 0;
  for (size_t i = 0; i < themes.size() && i < ui::ReaderSettingsView::MAX_THEMES; i++) {
    strncpy(readerView_.themeNames[i], themes[i].c_str(), sizeof(readerView_.themeNames[i]) - 1);
    readerView_.themeNames[i][sizeof(readerView_.themeNames[i]) - 1] = '\0';
    if (themes[i] == settings.themeName) {
      readerView_.currentThemeIndex = static_cast<int>(i);
    }
    readerView_.themeCount++;
  }
  readerView_.values[0] = 0;  // Not used for ThemeSelect

  // Index 1: Font Size (0=Small, 1=Normal, 2=Large)
  readerView_.values[1] = settings.fontSize;

  // Index 2: Text Layout (0=Compact, 1=Standard, 2=Large)
  readerView_.values[2] = settings.textLayout;

  // Index 3: Line Spacing (0=Compact, 1=Normal, 2=Relaxed, 3=Large)
  readerView_.values[3] = settings.lineSpacing;

  // Index 4: Text Anti-Aliasing (toggle)
  readerView_.values[4] = settings.textAntiAliasing;

  // Index 5: Paragraph Alignment (0=Justified, 1=Left, 2=Center, 3=Right)
  readerView_.values[5] = settings.paragraphAlignment;

  // Index 6: Hyphenation (toggle)
  readerView_.values[6] = settings.hyphenation;

  // Index 7: Show Images (toggle)
  readerView_.values[7] = settings.showImages;

  // Index 8: Status Bar (0=None, 1=Show)
  readerView_.values[8] = settings.statusBar;

  // Index 9: Reading Orientation (0=Portrait, 1=Landscape CW, 2=Inverted, 3=Landscape CCW)
  readerView_.values[9] = settings.orientation;
}

void SettingsState::saveReaderSettings() {
  auto& settings = core_->settings;

  // Index 0: Theme (ThemeSelect) - apply selected theme
  const char* selectedTheme = readerView_.getCurrentThemeName();
  if (strcmp(settings.themeName, selectedTheme) != 0) {
    strncpy(settings.themeName, selectedTheme, sizeof(settings.themeName) - 1);
    settings.themeName[sizeof(settings.themeName) - 1] = '\0';
    // Use cached theme for instant switching (no file I/O)
    if (!THEME_MANAGER.applyCachedTheme(settings.themeName)) {
      THEME_MANAGER.loadTheme(settings.themeName);
    }
    themeWasChanged_ = true;
  }

  // Index 1: Font Size
  settings.fontSize = readerView_.values[1];

  // Index 2: Text Layout
  settings.textLayout = readerView_.values[2];

  // Index 3: Line Spacing
  settings.lineSpacing = readerView_.values[3];

  // Index 4: Text Anti-Aliasing
  settings.textAntiAliasing = readerView_.values[4];

  // Index 5: Paragraph Alignment
  settings.paragraphAlignment = readerView_.values[5];

  // Index 6: Hyphenation
  settings.hyphenation = readerView_.values[6];

  // Index 7: Show Images
  settings.showImages = readerView_.values[7];

  // Index 8: Status Bar
  settings.statusBar = readerView_.values[8];

  // Index 9: Reading Orientation
  settings.orientation = readerView_.values[9];
}

void SettingsState::loadDeviceSettings() {
  const auto& settings = core_->settings;

  // Index 0: Auto Sleep Timeout (5 min=0, 10 min=1, 15 min=2, 30 min=3, Never=4)
  deviceView_.values[0] = settings.autoSleepMinutes;

  // Index 1: Sleep Screen (Dark=0, Light=1, Custom=2, Cover=3)
  deviceView_.values[1] = settings.sleepScreen;

  // Index 2: Startup Behavior (Last Document=0, Home=1)
  deviceView_.values[2] = settings.startupBehavior;

  // Index 3: Short Power Button (Ignore=0, Sleep=1, Page Turn=2)
  deviceView_.values[3] = settings.shortPwrBtn;

  // Index 4: Pages Per Refresh (1=0, 5=1, 10=2, 15=3, 30=4)
  deviceView_.values[4] = settings.pagesPerRefresh;

  // Index 5: Sunlight Fading Fix (toggle)
  deviceView_.values[5] = settings.sunlightFadingFix;

  // Index 6: Front Buttons (B/C/L/R=0, L/R/B/C=1)
  deviceView_.values[6] = settings.frontButtonLayout;

  // Index 7: Side Buttons (Prev/Next=0, Next/Prev=1)
  deviceView_.values[7] = settings.sideButtonLayout;
}

void SettingsState::saveDeviceSettings() {
  auto& settings = core_->settings;

  // Index 0: Auto Sleep Timeout
  settings.autoSleepMinutes = deviceView_.values[0];

  // Index 1: Sleep Screen
  settings.sleepScreen = deviceView_.values[1];

  // Index 2: Startup Behavior
  settings.startupBehavior = deviceView_.values[2];

  // Index 3: Short Power Button
  settings.shortPwrBtn = deviceView_.values[3];

  // Index 4: Pages Per Refresh
  settings.pagesPerRefresh = deviceView_.values[4];

  // Index 5: Sunlight Fading Fix
  settings.sunlightFadingFix = deviceView_.values[5];

  // Index 6: Front Buttons - deferred to goBack() on screen exit.
  // Changing layout while navigating causes ghost button events because the
  // MappedInputManager remaps physical buttons mid-press.

  // Index 7: Side Buttons - deferred to goBack() on screen exit.
  // Same as front buttons: changing layout mid-navigation causes ghost events.
}

void SettingsState::populateSystemInfo() {
  infoView_.clear();

  // Firmware version
  infoView_.addField("Version", SUMI_VERSION);

  // Uptime
  const unsigned long uptimeSeconds = millis() / 1000;
  const unsigned long hours = uptimeSeconds / 3600;
  const unsigned long minutes = (uptimeSeconds % 3600) / 60;
  const unsigned long seconds = uptimeSeconds % 60;
  char uptimeStr[24];
  snprintf(uptimeStr, sizeof(uptimeStr), "%luh %lum %lus", hours, minutes, seconds);
  infoView_.addField("Uptime", uptimeStr);

  // Battery
  const uint16_t millivolts = batteryMonitor.readMillivolts();
  char batteryStr[24];
  if (millivolts < 3000 || millivolts > 4500) {
    snprintf(batteryStr, sizeof(batteryStr), "-- (%umV)", millivolts);
  } else {
    const uint8_t percentage = BatteryMonitor::percentageFromMillivolts(millivolts);
    snprintf(batteryStr, sizeof(batteryStr), "%u%% (%umV)", percentage, millivolts);
  }
  infoView_.addField("Battery", batteryStr);

  // Chip model
  infoView_.addField("Chip", ESP.getChipModel());

  // CPU frequency
  char freqStr[16];
  snprintf(freqStr, sizeof(freqStr), "%d MHz", ESP.getCpuFreqMHz());
  infoView_.addField("CPU", freqStr);

  // Free heap memory
  char heapStr[24];
  snprintf(heapStr, sizeof(heapStr), "%lu KB", ESP.getFreeHeap() / 1024);
  infoView_.addField("Free Memory", heapStr);

  // Internal flash storage (LittleFS)
  const size_t totalBytes = LittleFS.totalBytes();
  const size_t usedBytes = LittleFS.usedBytes();
  char internalStr[32];
  snprintf(internalStr, sizeof(internalStr), "%lu / %lu KB", (unsigned long)(usedBytes / 1024),
           (unsigned long)(totalBytes / 1024));
  infoView_.addField("Internal Disk", internalStr);

  // SD Card status
  infoView_.addField("SD Card", SdMan.ready() ? "Ready" : "Not available");
}

void SettingsState::clearCache(int type, Core& core) {
  // Set up confirmation dialog messages based on action type
  if (type == 0) {
    // Clear Book Cache - show confirmation
    confirmView_.setup("Clear Caches?", "This will delete all book caches", "and reading progress.");
    pendingAction_ = 10;
    currentScreen_ = SettingsScreen::ConfirmDialog;
    needsRender_ = true;
    return;
  } else if (type == 1) {
    // Clear Device Storage
    confirmView_.setup("Clear Device?", "This will erase internal flash", "storage. Device will restart.");
    pendingAction_ = 11;
    currentScreen_ = SettingsScreen::ConfirmDialog;
    needsRender_ = true;
    return;
  } else if (type == 2) {
    // Factory Reset
    confirmView_.setup("Factory Reset?", "This will erase ALL data including", "settings and stored data!");
    pendingAction_ = 12;
    currentScreen_ = SettingsScreen::ConfirmDialog;
    needsRender_ = true;
    return;
  }
}

#if FEATURE_BLUETOOTH
void SettingsState::enterBluetooth() {
  btSelected_ = 0;
  btScanned_ = false;
  btConnecting_ = false;

  ble::init();

  // Try reconnecting to saved devices first
  const char* savedKb = core_->settings.bleKeyboard;
  const char* savedPt = core_->settings.blePageTurner;
  bool hasSaved = (savedKb[0] != '\0' || savedPt[0] != '\0');

  if (hasSaved && !ble::isConnected()) {
    renderer_.clearScreen(0xFF);
    ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Connecting to saved device...");
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

    if (savedPt[0] != '\0' && ble::reconnect(savedPt)) {
      Serial.println("[BLE] Reconnected to saved page turner");
      btScanned_ = true;
      needsRender_ = true;
      return;
    }
    if (savedKb[0] != '\0' && ble::reconnect(savedKb)) {
      Serial.println("[BLE] Reconnected to saved keyboard");
      btScanned_ = true;
      needsRender_ = true;
      return;
    }
    Serial.println("[BLE] Saved device not available, scanning...");
  }

  // Show scanning message
  renderer_.clearScreen(0xFF);
  ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Scanning for devices...");
  renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

  ble::startScan(10);
  btScanned_ = true;
  needsRender_ = true;

  Serial.printf("[BLE] Scan found %d devices\n", ble::scanResultCount());
}

void SettingsState::renderBluetooth() {
  renderer_.clearScreen(THEME.backgroundColor);
  const Theme& t = THEME;
  const int font = t.menuFontId;

  // Standard title
  ui::title(renderer_, t, t.screenMarginTop, "Bluetooth");

  if (!btScanned_) {
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2, "Press OK to scan", true);
  } else if (ble::isConnected() && ble::scanResultCount() == 0) {
    // Connected via saved device reconnect
    char status[64];
    snprintf(status, sizeof(status), "Connected: %s", ble::connectedDevice());
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 - 10, status, true, EpdFontFamily::BOLD);
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 + 25, "Press Rescan to find other devices", true);
  } else if (ble::scanResultCount() == 0) {
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 - 20,
                               "No devices found", true);
    renderer_.drawCenteredText(font, renderer_.getScreenHeight() / 2 + 20,
                               "Press OK or Rescan to try again", true);
  } else {
    const int startY = 60;
    int y = startY;

    // If connected, show status as first item
    if (ble::isConnected()) {
      char status[64];
      snprintf(status, sizeof(status), "Connected: %s", ble::connectedDevice());
      ui::menuItem(renderer_, t, y, status, false);
      y += t.menuItemHeight + t.itemSpacing;
    }

    // Device list using standard menu items
    for (int i = 0; i < ble::scanResultCount(); i++) {
      const BleDevice* dev = ble::scanResult(i);
      if (!dev) continue;

      // Build label: "Name  -XXdBm  HID"
      char label[64];
      if (dev->hasHID) {
        snprintf(label, sizeof(label), "%s  [HID]", dev->name);
      } else {
        snprintf(label, sizeof(label), "%s", dev->name);
      }

      bool sel = (i == btSelected_);
      ui::menuItem(renderer_, t, y, label, sel);
      y += t.menuItemHeight + t.itemSpacing;

      // Stop if we'd overflow into footer
      if (y + t.menuItemHeight > renderer_.getScreenHeight() - 40) break;
    }
  }

  renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
}
#endif

}  // namespace sumi
