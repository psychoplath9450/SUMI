#include "SettingsState.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include "../core/MemoryArena.h"
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
#include "../ble/BleFileTransfer.h"
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
  
#if FEATURE_BLUETOOTH
  // Clear BLE callback to prevent dangling this pointer
  if (bleCallbackRegistered_) {
    ble_transfer::setCallback(nullptr);
    bleCallbackRegistered_ = false;
  }
#endif
  
  // Re-allocate memory arena if it was released for BLE
  if (!sumi::MemoryArena::isInitialized()) {
    Serial.println("[SETTINGS] Re-allocating memory arena");
    sumi::MemoryArena::init();
  }
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
              case SettingsScreen::HomeArt:
                homeArtView_.moveUp();
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
              case SettingsScreen::HomeArt:
                homeArtView_.moveDown();
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
              case SettingsScreen::HomeArt:
                homeArtView_.moveUp();
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
              case SettingsScreen::HomeArt:
                homeArtView_.moveDown();
                break;
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
      case SettingsScreen::HomeArt:
        viewNeedsRender = homeArtView_.needsRender;
        break;
      case SettingsScreen::BleTransfer:
        // Check if transfer status changed
        updateBleTransfer();
        viewNeedsRender = needsRender_;
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
#if FEATURE_BLUETOOTH
      case SettingsScreen::Bluetooth:
        viewNeedsRender = true;  // Always check for BLE updates
        break;
#endif
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
    case SettingsScreen::HomeArt:
      ui::render(renderer_, THEME, homeArtView_);
      homeArtView_.needsRender = false;
      break;
    case SettingsScreen::BleTransfer:
      renderBleTransfer();
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

  // idx 0=HomeArt, 1=Wireless Transfer, 2=Reader, 3=Device, then BT/Cleanup/SystemInfo
  switch (idx) {
    case 0:  // Home Art
      loadHomeArtSettings();
      homeArtView_.needsRender = true;
      currentScreen_ = SettingsScreen::HomeArt;
      break;
    case 1:  // Wireless Transfer
      enterBleTransfer();
      currentScreen_ = SettingsScreen::BleTransfer;
      break;
    case 2:  // Reader
      loadReaderSettings();
      readerView_.selected = 0;
      readerView_.needsRender = true;
      currentScreen_ = SettingsScreen::Reader;
      break;
    case 3:  // Device
      loadDeviceSettings();
      deviceView_.selected = 0;
      deviceView_.needsRender = true;
      currentScreen_ = SettingsScreen::Device;
      break;
#if FEATURE_BLUETOOTH
    case 4:  // Bluetooth
      enterBluetooth();
      currentScreen_ = SettingsScreen::Bluetooth;
      break;
    case 5:  // Cleanup
#else
    case 4:  // Cleanup
#endif
      cleanupView_.selected = 0;
      cleanupView_.needsRender = true;
      currentScreen_ = SettingsScreen::Cleanup;
      break;
#if FEATURE_BLUETOOTH
    case 6:  // System Info
#else
    case 5:  // System Info
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
    case SettingsScreen::HomeArt:
      saveHomeArtSettings();
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
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
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
    case SettingsScreen::BleTransfer:
      // Block back during active transfer
      if (ble_transfer::isTransferring()) {
        // Don't navigate away during transfer
        return;
      }
      // Clean up result state
      bleShowResult_ = false;
      bleQueueComplete_ = false;
      ble_transfer::clearResult();
      // Clear callback
      if (bleCallbackRegistered_) {
        ble_transfer::setCallback(nullptr);
        bleCallbackRegistered_ = false;
      }
      // If files were received, do a full refresh to clear e-ink ghosting
      if (bleTransferDirty_) {
        bleTransferDirty_ = false;
        renderer_.clearScreen(THEME.backgroundColor);
        renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
      }
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
#if FEATURE_BLUETOOTH
    case SettingsScreen::Bluetooth:
      currentScreen_ = SettingsScreen::Menu;
      menuView_.needsRender = true;
      break;
#endif
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

    case SettingsScreen::HomeArt:
      // Apply the selected theme
      saveHomeArtSettings();
      needsRender_ = true;
      break;

    case SettingsScreen::BleTransfer:
      // During active transfer, OK does nothing
      if (ble_transfer::isTransferring()) {
        break;
      }
      // Toggle BLE transfer service
      if (bleTransferEnabled_) {
        ble_transfer::stopAdvertising();
        ble_transfer::deinit();
        bleTransferEnabled_ = false;
        bleShowResult_ = false;
        bleQueueComplete_ = false;
        if (bleCallbackRegistered_) {
          ble_transfer::setCallback(nullptr);
          bleCallbackRegistered_ = false;
        }
        Serial.println("[BLE] File transfer disabled");
        // Re-allocate memory arena when BLE is disabled
        if (!sumi::MemoryArena::isInitialized()) {
          Serial.println("[BLE] Re-allocating memory arena");
          sumi::MemoryArena::init();
        }
      } else {
        // Release memory arena to free up heap for BLE stack
        if (sumi::MemoryArena::isInitialized()) {
          Serial.println("[BLE] Releasing memory arena for BLE stack");
          sumi::MemoryArena::release();
        }
        ble_transfer::init();
        ble_transfer::startAdvertising();
        bleTransferEnabled_ = true;
        bleShowResult_ = false;
        bleQueueComplete_ = false;
        Serial.println("[BLE] File transfer enabled");
        // Re-register callback
        enterBleTransfer();
      }
      needsRender_ = true;
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
          // Clear Book Cache - clear all book-related data
          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Clearing cache...");

          // Clear cache directory (covers, thumbnails, section caches, progress)
          core.storage.rmdir(SUMI_CACHE_DIR);
          
          // Clear recent books list
          core.storage.remove("/.sumi/recent.bin");
          
          // Clear library index
          core.storage.remove("/.sumi/library.bin");
          
          // Clear last book path from settings
          core.settings.lastBookPath[0] = '\0';
          core.settings.save(core.storage);

          ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Cache cleared!");
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

  // Index 8: Show Tables (toggle)
  readerView_.values[8] = settings.showTables;

  // Index 9: Status Bar (0=None, 1=Show)
  readerView_.values[9] = settings.statusBar;

  // Index 10: Reading Orientation (0=Portrait, 1=Landscape CW, 2=Inverted, 3=Landscape CCW)
  readerView_.values[10] = settings.orientation;
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

  // Index 8: Show Tables
  settings.showTables = readerView_.values[8];

  // Index 9: Status Bar
  settings.statusBar = readerView_.values[9];

  // Index 10: Reading Orientation
  settings.orientation = readerView_.values[10];
}

void SettingsState::loadHomeArtSettings() {
  auto& settings = core_->settings;
  
  // Reset view state
  homeArtView_.themeCount = 0;
  homeArtView_.selectedIndex = 0;
  homeArtView_.appliedIndex = 0;
  homeArtView_.scrollOffset = 0;
  homeArtView_.needsRender = true;
  
  // Always add "default" first (built-in PROGMEM theme)
  strncpy(homeArtView_.themeNames[0], "default", sizeof(homeArtView_.themeNames[0]) - 1);
  strncpy(homeArtView_.displayNames[0], "Default (Built-in)", sizeof(homeArtView_.displayNames[0]) - 1);
  homeArtView_.themeCount = 1;
  
  // Check if default is currently applied
  if (strcmp(settings.homeArtTheme, "default") == 0 || settings.homeArtTheme[0] == '\0') {
    homeArtView_.selectedIndex = 0;
    homeArtView_.appliedIndex = 0;
  }
  
  // Scan SD card for additional themes at /config/themes/*.bmp
  FsFile dir = SdMan.open("/config/themes");
  if (dir && dir.isDirectory()) {
    FsFile file;
    char filename[64];
    
    while (file.openNext(&dir, O_RDONLY) && homeArtView_.themeCount < ui::HomeArtSettingsView::MAX_THEMES) {
      if (!file.isDirectory()) {
        file.getName(filename, sizeof(filename));
        
        // Check if it's a .bmp file
        size_t len = strlen(filename);
        if (len > 4 && strcasecmp(filename + len - 4, ".bmp") == 0) {
          // Extract theme name (filename without extension)
          char themeName[32];
          size_t nameLen = std::min(len - 4, sizeof(themeName) - 1);
          strncpy(themeName, filename, nameLen);
          themeName[nameLen] = '\0';
          
          // Skip if it's "default" (already added)
          if (strcasecmp(themeName, "default") != 0) {
            int idx = homeArtView_.themeCount;
            strncpy(homeArtView_.themeNames[idx], themeName, sizeof(homeArtView_.themeNames[0]) - 1);
            homeArtView_.themeNames[idx][sizeof(homeArtView_.themeNames[0]) - 1] = '\0';
            
            // Display name = theme name (could be prettier but works)
            strncpy(homeArtView_.displayNames[idx], themeName, sizeof(homeArtView_.displayNames[0]) - 1);
            homeArtView_.displayNames[idx][sizeof(homeArtView_.displayNames[0]) - 1] = '\0';
            
            // Check if this is the currently applied theme
            if (strcmp(settings.homeArtTheme, themeName) == 0) {
              homeArtView_.selectedIndex = idx;
              homeArtView_.appliedIndex = idx;
            }
            
            homeArtView_.themeCount++;
          }
        }
      }
      file.close();
    }
    dir.close();
  }
  
  // Ensure selected item is visible
  if (homeArtView_.selectedIndex >= ui::HomeArtSettingsView::VISIBLE_ITEMS) {
    homeArtView_.scrollOffset = homeArtView_.selectedIndex - ui::HomeArtSettingsView::VISIBLE_ITEMS + 1;
  }
  
  Serial.printf("[SETTINGS] Found %d home art themes (1 built-in + %d on SD), applied: %s\n", 
                homeArtView_.themeCount, homeArtView_.themeCount - 1, settings.homeArtTheme);
}

void SettingsState::saveHomeArtSettings() {
  auto& settings = core_->settings;
  
  const char* selectedTheme = homeArtView_.getCurrentThemeName();
  if (strcmp(settings.homeArtTheme, selectedTheme) != 0) {
    strncpy(settings.homeArtTheme, selectedTheme, sizeof(settings.homeArtTheme) - 1);
    settings.homeArtTheme[sizeof(settings.homeArtTheme) - 1] = '\0';
    homeArtView_.appliedIndex = homeArtView_.selectedIndex;
    homeArtView_.needsRender = true;
    
    // Save to persistent storage
    settings.save(core_->storage);
    
    Serial.printf("[SETTINGS] Home art theme changed to: %s\n", settings.homeArtTheme);
  }
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

// ============================================================================
// BLE File Transfer Screen
// ============================================================================

void SettingsState::enterBleTransfer() {
  // Check current state
  bleTransferEnabled_ = ble_transfer::isReady();
  lastBleUpdate_ = millis();
  lastBleProgress_ = -1;  // Force initial render
  bleShowResult_ = false;
  bleQueueComplete_ = false;
  bleTransferDirty_ = false;
  needsRender_ = true;
  
  // Register callback for real-time transfer events
  if (!bleCallbackRegistered_ && bleTransferEnabled_) {
    ble_transfer::setCallback([this](ble_transfer::TransferEvent event, const char* data) {
      switch (event) {
        case ble_transfer::TransferEvent::TRANSFER_START:
          bleShowResult_ = false;
          ble_transfer::clearResult();
          needsRender_ = true;
          break;
        case ble_transfer::TransferEvent::TRANSFER_PROGRESS:
          // DON'T set needsRender_ here!
          // Each render triggers a full e-ink refresh (~500ms of CPU + SPI blocking)
          // which starves the BLE stack and causes connection drops on the ESP32-C3.
          // Let updateBleTransfer() handle throttled rendering via polling instead.
          break;
        case ble_transfer::TransferEvent::TRANSFER_COMPLETE:
          bleShowResult_ = true;
          bleTransferDirty_ = true;  // Files changed on SD
          needsRender_ = true;
          break;
        case ble_transfer::TransferEvent::TRANSFER_ERROR:
          bleShowResult_ = true;
          needsRender_ = true;
          break;
        case ble_transfer::TransferEvent::QUEUE_COMPLETE:
          bleQueueComplete_ = true;
          bleShowResult_ = true;
          needsRender_ = true;
          break;
        case ble_transfer::TransferEvent::CONNECTED:
        case ble_transfer::TransferEvent::DISCONNECTED:
          needsRender_ = true;
          break;
        default:
          break;
      }
    });
    bleCallbackRegistered_ = true;
  }
  
  Serial.printf("[BLE] Entering transfer screen, enabled: %d\n", bleTransferEnabled_);
}

void SettingsState::updateBleTransfer() {
  unsigned long now = millis();
  
  bool isTransferring = ble_transfer::isTransferring();
  // During transfer: only check every 3 seconds to minimize CPU contention with BLE stack.
  // The ESP32-C3 is single-core — e-ink refreshes take ~500ms of blocking SPI/wait time
  // that prevents the BLE stack from servicing the connection, causing timeouts.
  uint32_t checkInterval = isTransferring ? 3000 : 500;
  
  if (now - lastBleUpdate_ < checkInterval) return;
  lastBleUpdate_ = now;
  
  // During active transfer, poll progress as backup to callback-driven updates
  if (isTransferring) {
    int progress = ble_transfer::transferProgress();
    if (progress != lastBleProgress_) {
      int delta = progress - lastBleProgress_;
      lastBleProgress_ = progress;
      // Render every 10% change or at completion — keep display updates rare
      // to avoid starving the BLE connection on single-core ESP32-C3
      if (delta >= 10 || delta < 0 || progress >= 100) {
        needsRender_ = true;
      }
    }
  } else if (lastBleProgress_ >= 0) {
    // Transfer just ended — reset progress tracker
    lastBleProgress_ = -1;
    needsRender_ = true;
  }
  
  // Catch result if callback was missed (belt and suspenders)
  if (ble_transfer::hasResult() && !bleShowResult_ && !isTransferring) {
    bleShowResult_ = true;
    needsRender_ = true;
  }
}

void SettingsState::renderBleTransfer() {
  renderer_.clearScreen(THEME.backgroundColor);
  const Theme& t = THEME;
  
  ui::title(renderer_, t, t.screenMarginTop, "Wireless Transfer");
  
  const int W = renderer_.getScreenWidth();   // 480
  const int H = renderer_.getScreenHeight();  // 800
  const int cx = W / 2;
  const int smH = renderer_.getLineHeight(t.smallFontId) + 6;
  const int mdH = renderer_.getLineHeight(t.menuFontId) + 8;
  const int lgH = renderer_.getLineHeight(t.readerFontIdMedium) + 8;
  
  // Icon helper: draw a square "icon" with a label inside
  // filled=true draws inverted (white text on black box)
  auto drawIcon = [&](int y, int size, const char* label, bool filled) {
    int ix = cx - size / 2;
    if (filled) {
      renderer_.fillRect(ix, y, size, size, true);
      renderer_.drawCenteredText(t.readerFontIdMedium, y + size / 2 - lgH / 2 + 4, label, false);
    } else {
      renderer_.drawRect(ix, y, size, size, true);
      renderer_.drawRect(ix + 1, y + 1, size - 2, size - 2, true); // double border
      renderer_.drawCenteredText(t.readerFontIdMedium, y + size / 2 - lgH / 2 + 4, label, true);
    }
  };
  
  // Divider helper
  auto drawDivider = [&](int y) {
    int dw = 80;
    renderer_.drawLine(cx - dw / 2, y, cx + dw / 2, y, true);
  };
  
  // ══════════════════════════════════════════════════════════════
  // OFF STATE
  // ══════════════════════════════════════════════════════════════
  if (!bleTransferEnabled_) {
    int y = 160;
    drawIcon(y, 64, "BT", false);
    y += 64 + 24;
    
    renderer_.drawCenteredText(t.readerFontIdMedium, y, "Wireless is Off", t.primaryTextBlack, EpdFontFamily::BOLD);
    y += lgH + 28;
    
    drawDivider(y);
    y += 28;
    
    renderer_.drawCenteredText(t.menuFontId, y, "Send files from your browser", t.secondaryTextBlack);
    y += mdH;
    renderer_.drawCenteredText(t.menuFontId, y, "directly to this device", t.secondaryTextBlack);
    y += mdH;
    renderer_.drawCenteredText(t.menuFontId, y, "over Bluetooth.", t.secondaryTextBlack);
    y += mdH + 32;
    
    renderer_.drawCenteredText(t.menuFontId, y, "Press OK to enable", t.primaryTextBlack, EpdFontFamily::BOLD);
    
    ui::ButtonBar buttons{"Back", "Enable", "", ""};
    ui::buttonBar(renderer_, t, buttons);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
    return;
  }
  
  // ══════════════════════════════════════════════════════════════
  // QUEUE COMPLETE SUMMARY
  // ══════════════════════════════════════════════════════════════
  if (bleQueueComplete_ && !ble_transfer::isTransferring()) {
    uint8_t received = ble_transfer::queueReceived();
    uint8_t total = ble_transfer::queueTotal();
    
    int y = 160;
    
    if (received == total) {
      // All success
      drawIcon(y, 68, "OK", false);
      y += 68 + 24;
      renderer_.drawCenteredText(t.readerFontIdMedium, y, "All Files Received", t.primaryTextBlack, EpdFontFamily::BOLD);
    } else if (received > 0) {
      // Partial success
      drawIcon(y, 68, "OK", false);
      y += 68 + 24;
      renderer_.drawCenteredText(t.readerFontIdMedium, y, "Transfer Complete", t.primaryTextBlack, EpdFontFamily::BOLD);
    } else {
      // All failed
      drawIcon(y, 68, "X", true);
      y += 68 + 24;
      renderer_.drawCenteredText(t.readerFontIdMedium, y, "Transfer Failed", t.primaryTextBlack, EpdFontFamily::BOLD);
    }
    y += lgH + 8;
    
    char summary[48];
    snprintf(summary, sizeof(summary), "%d of %d", received, total);
    renderer_.drawCenteredText(t.menuFontId, y, summary, t.primaryTextBlack, EpdFontFamily::BOLD);
    y += mdH + 28;
    
    drawDivider(y);
    y += 28;
    
    if (received == total) {
      renderer_.drawCenteredText(t.menuFontId, y, "Your files are ready", t.secondaryTextBlack);
      y += mdH;
      renderer_.drawCenteredText(t.menuFontId, y, "in the library.", t.secondaryTextBlack);
      y += mdH + 20;
      renderer_.drawCenteredText(t.smallFontId, y, "Press Back to start reading", t.secondaryTextBlack);
    } else if (received > 0) {
      char failMsg[48];
      snprintf(failMsg, sizeof(failMsg), "%d file%s failed", total - received, (total - received > 1) ? "s" : "");
      renderer_.drawCenteredText(t.menuFontId, y, failMsg, t.secondaryTextBlack);
      y += mdH + 8;
      renderer_.drawCenteredText(t.menuFontId, y, "Saved files are in the library.", t.secondaryTextBlack);
      y += mdH;
      renderer_.drawCenteredText(t.menuFontId, y, "Try sending failed files again,", t.secondaryTextBlack);
      y += mdH;
      renderer_.drawCenteredText(t.menuFontId, y, "or copy them to the SD card.", t.secondaryTextBlack);
    } else {
      renderer_.drawCenteredText(t.menuFontId, y, "No files were saved.", t.secondaryTextBlack);
      y += mdH + 8;
      renderer_.drawCenteredText(t.menuFontId, y, "Large files transfer better", t.secondaryTextBlack);
      y += mdH;
      renderer_.drawCenteredText(t.menuFontId, y, "by copying to the SD card.", t.secondaryTextBlack);
    }
    
    ui::ButtonBar buttons{"Back", "Disable", "", ""};
    ui::buttonBar(renderer_, t, buttons);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
    return;
  }
  
  // ══════════════════════════════════════════════════════════════
  // SINGLE FILE RESULT
  // ══════════════════════════════════════════════════════════════
  if (bleShowResult_ && ble_transfer::hasResult() && !ble_transfer::isTransferring()) {
    const auto& result = ble_transfer::lastResult();
    int y = 160;
    
    if (result.success) {
      drawIcon(y, 68, "OK", false);
      y += 68 + 24;
      renderer_.drawCenteredText(t.readerFontIdMedium, y, "Transfer Complete", t.primaryTextBlack, EpdFontFamily::BOLD);
      y += lgH + 12;
      
      renderer_.drawCenteredText(t.menuFontId, y, result.filename, t.primaryTextBlack);
      y += mdH;
      
      char sizeInfo[64];
      if (result.fileSize < 1048576) {
        snprintf(sizeInfo, sizeof(sizeInfo), "%.1f KB at %.1f KB/s", result.fileSize / 1024.0f, result.speedKBs);
      } else {
        snprintf(sizeInfo, sizeof(sizeInfo), "%.1f MB at %.1f KB/s", result.fileSize / 1048576.0f, result.speedKBs);
      }
      renderer_.drawCenteredText(t.smallFontId, y, sizeInfo, t.secondaryTextBlack);
      y += smH;
      
      if (result.queueTotal > 0) {
        y += 8;
        char queueInfo[48];
        snprintf(queueInfo, sizeof(queueInfo), "File %d of %d", result.queueIndex, result.queueTotal);
        renderer_.drawCenteredText(t.smallFontId, y, queueInfo, t.secondaryTextBlack);
        
        if (result.queueIndex < result.queueTotal) {
          y += smH + 16;
          renderer_.drawCenteredText(t.menuFontId, y, "Waiting for next file...", t.secondaryTextBlack);
        }
      }
    } else {
      drawIcon(y, 68, "X", true);
      y += 68 + 24;
      renderer_.drawCenteredText(t.readerFontIdMedium, y, "Transfer Failed", t.primaryTextBlack, EpdFontFamily::BOLD);
      y += lgH + 12;
      
      if (result.filename[0]) {
        renderer_.drawCenteredText(t.smallFontId, y, result.filename, t.primaryTextBlack);
        y += smH;
      }
      if (result.errorMsg[0]) {
        renderer_.drawCenteredText(t.smallFontId, y, result.errorMsg, t.secondaryTextBlack);
        y += smH;
      }
      
      y += 20;
      drawDivider(y);
      y += 28;
      
      renderer_.drawCenteredText(t.menuFontId, y, "Large files transfer better", t.secondaryTextBlack);
      y += mdH;
      renderer_.drawCenteredText(t.menuFontId, y, "by copying directly to", t.secondaryTextBlack);
      y += mdH;
      renderer_.drawCenteredText(t.menuFontId, y, "the SD card.", t.secondaryTextBlack);
    }
    
    ui::ButtonBar buttons{"Back", "Disable", "", ""};
    ui::buttonBar(renderer_, t, buttons);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
    return;
  }
  
  // ══════════════════════════════════════════════════════════════
  // ACTIVE TRANSFER
  // ══════════════════════════════════════════════════════════════
  if (ble_transfer::isTransferring()) {
    int y = 130;
    
    // Queue position header
    uint8_t qi = ble_transfer::queueIndex();
    uint8_t qt = ble_transfer::queueTotal();
    if (qt > 0) {
      char queueHeader[48];
      snprintf(queueHeader, sizeof(queueHeader), "File %d of %d", qi, qt);
      renderer_.drawCenteredText(t.menuFontId, y, queueHeader, t.primaryTextBlack, EpdFontFamily::BOLD);
    } else {
      renderer_.drawCenteredText(t.menuFontId, y, "Receiving file...", t.primaryTextBlack, EpdFontFamily::BOLD);
    }
    y += mdH + 4;
    
    // Filename
    const char* filename = ble_transfer::currentFilename();
    if (filename && filename[0]) {
      renderer_.drawCenteredText(t.smallFontId, y, filename, t.primaryTextBlack);
      y += smH;
    }
    y += 28;
    
    // Wide progress bar
    int progress = ble_transfer::transferProgress();
    const int barW = W - 60;  // Nearly full width
    const int barH = 28;
    const int barX = cx - barW / 2;
    renderer_.drawRect(barX, y, barW, barH, t.primaryTextBlack);
    renderer_.drawRect(barX + 1, y + 1, barW - 2, barH - 2, t.primaryTextBlack);
    if (progress > 0) {
      int fillW = (barW - 6) * progress / 100;
      renderer_.fillRect(barX + 3, y + 3, fillW, barH - 6, t.primaryTextBlack);
    }
    y += barH + 20;
    
    // Giant percentage (hero element)
    char pctText[8];
    snprintf(pctText, sizeof(pctText), "%d%%", progress);
    renderer_.drawCenteredText(t.readerFontIdLarge, y, pctText, t.primaryTextBlack, EpdFontFamily::BOLD);
    y += renderer_.getLineHeight(t.readerFontIdLarge) + 16;
    
    // Bytes received
    uint32_t received = ble_transfer::bytesReceived();
    uint32_t expected = ble_transfer::expectedSize();
    char progText[64];
    if (expected < 1048576) {
      snprintf(progText, sizeof(progText), "%.0f / %.0f KB", 
               received / 1024.0f, expected / 1024.0f);
    } else {
      snprintf(progText, sizeof(progText), "%.1f / %.1f MB", 
               received / 1048576.0f, expected / 1048576.0f);
    }
    renderer_.drawCenteredText(t.menuFontId, y, progText, t.primaryTextBlack);
    y += mdH + 40;
    
    renderer_.drawCenteredText(t.smallFontId, y, "Do not leave this screen", t.secondaryTextBlack);
    
    ui::ButtonBar buttons{"", "", "", ""};
    ui::buttonBar(renderer_, t, buttons);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
    return;
  }
  
  // ══════════════════════════════════════════════════════════════
  // CONNECTED, WAITING
  // ══════════════════════════════════════════════════════════════
  if (ble_transfer::isConnected()) {
    int y = 200;
    drawIcon(y, 64, "BT", true);
    y += 64 + 24;
    
    renderer_.drawCenteredText(t.readerFontIdMedium, y, "Connected", t.primaryTextBlack, EpdFontFamily::BOLD);
    y += lgH + 28;
    
    drawDivider(y);
    y += 28;
    
    renderer_.drawCenteredText(t.menuFontId, y, "Waiting for files...", t.primaryTextBlack);
    y += mdH + 12;
    renderer_.drawCenteredText(t.smallFontId, y, "Start the transfer from", t.secondaryTextBlack);
    y += smH;
    renderer_.drawCenteredText(t.smallFontId, y, "your browser to begin.", t.secondaryTextBlack);
    
    ui::ButtonBar buttons{"Back", "Disable", "", ""};
    ui::buttonBar(renderer_, t, buttons);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
    return;
  }
  
  // ══════════════════════════════════════════════════════════════
  // READY, ADVERTISING
  // ══════════════════════════════════════════════════════════════
  {
    int y = 160;
    drawIcon(y, 64, "BT", true);
    y += 64 + 24;
    
    renderer_.drawCenteredText(t.readerFontIdMedium, y, "Ready", t.primaryTextBlack, EpdFontFamily::BOLD);
    y += lgH + 4;
    renderer_.drawCenteredText(t.menuFontId, y, "Visible as \"SUMI\"", t.secondaryTextBlack);
    y += mdH + 24;
    
    drawDivider(y);
    y += 28;
    
    renderer_.drawCenteredText(t.menuFontId, y, "Open sumi.page in Chrome,", t.secondaryTextBlack);
    y += mdH;
    renderer_.drawCenteredText(t.menuFontId, y, "convert your files, then tap", t.secondaryTextBlack);
    y += mdH;
    renderer_.drawCenteredText(t.menuFontId, y, "Send to SUMI.", t.primaryTextBlack, EpdFontFamily::BOLD);
    y += mdH + 32;
    
    renderer_.drawCenteredText(t.smallFontId, y, "Press OK to disable", t.secondaryTextBlack);
    
    ui::ButtonBar buttons{"Back", "Disable", "", ""};
    ui::buttonBar(renderer_, t, buttons);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
  }
}

#if FEATURE_BLUETOOTH
void SettingsState::enterBluetooth() {
  btSelected_ = 0;
  btScanned_ = false;
  btConnecting_ = false;

  // Release memory arena to free up heap for BLE stack (~40KB needed)
  if (sumi::MemoryArena::isInitialized()) {
    Serial.println("[BLE] Releasing memory arena for BLE stack");
    sumi::MemoryArena::release();
  }

  ble::init();

  // Initialize file transfer service (runs alongside HID)
  ble_transfer::init();
  ble_transfer::startAdvertising();
  Serial.println("[BLE] File transfer service ready - device can receive files from sumi.page");

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

  // File transfer status at bottom
  if (ble_transfer::isReady()) {
    const int footerY = renderer_.getScreenHeight() - 35;
    renderer_.drawCenteredText(font, footerY, "File transfer: Ready (sumi.page)", true);
  }

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
