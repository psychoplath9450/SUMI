#include <Arduino.h>
#include <EInkDisplay.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <InputManager.h>
#include <SDCardManager.h>
// SdFat and FS.h both define FILE_READ/FILE_WRITE - undef before LittleFS re-includes FS.h
#undef FILE_READ
#undef FILE_WRITE
#include <LittleFS.h>
#include <SPI.h>
#include <builtinFonts/reader_2b.h>
#include <builtinFonts/reader_bold_2b.h>
#include <builtinFonts/reader_italic_2b.h>
// XSmall font (12pt)
#include <builtinFonts/reader_xsmall_bold_2b.h>
#include <builtinFonts/reader_xsmall_italic_2b.h>
#include <builtinFonts/reader_xsmall_regular_2b.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
// Medium font (16pt)
#include <builtinFonts/reader_medium_2b.h>
#include <builtinFonts/reader_medium_bold_2b.h>
#include <builtinFonts/reader_medium_italic_2b.h>
// Large font (18pt)
#include <builtinFonts/reader_large_2b.h>
#include <builtinFonts/reader_large_bold_2b.h>
#include <builtinFonts/reader_large_italic_2b.h>
#include <builtinFonts/small14.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>

#include "Battery.h"
#include "FontManager.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"
#include "config.h"
#include "content/ContentTypes.h"
#include "ui/Elements.h"

// New refactored core system
#include "core/Core.h"
#include "core/MemoryArena.h"
#include "core/StateMachine.h"
#include "images/SumiLogo.h"
#include "states/ErrorState.h"
#include "states/FileListState.h"
#include "states/HomeState.h"
#include "states/ReaderState.h"
#include "states/SettingsState.h"
#include "states/SleepState.h"
#include "states/StartupState.h"
#include "ui/views/BootSleepViews.h"

// Plugin system
#if FEATURE_PLUGINS
#include "states/PluginHostState.h"
#include "states/PluginListState.h"
#include "plugins/PluginRenderer.h"

// Games
#if FEATURE_GAMES
#include "plugins/Minesweeper.h"
#include "plugins/Checkers.h"
#include "plugins/Solitaire.h"
#include "plugins/Sudoku.h"
#include "plugins/ChessGame.h"
#include "plugins/SumiBoy.h"
#endif

// Productivity
#include "plugins/TodoList.h"
#include "plugins/Notes.h"
#include "plugins/ToolSuite.h"
#include "plugins/Images.h"
#include "plugins/Maps.h"

#if FEATURE_FLASHCARDS
#include "plugins/Flashcards.h"
#include "ble/BleFileTransfer.h"
#endif
#endif  // FEATURE_PLUGINS

#if FEATURE_BLUETOOTH
#include "ble/BleHid.h"
#endif

#define SPI_FQ 40000000
// Display SPI pins (custom pins for XteinkX4, not hardware SPI defaults)
#define EPD_SCLK 8   // SPI Clock
#define EPD_MOSI 10  // SPI MOSI (Master Out Slave In)
#define EPD_CS 21    // Chip Select
#define EPD_DC 4     // Data/Command
#define EPD_RST 5    // Reset
#define EPD_BUSY 6   // Busy

#define UART0_RXD 20  // Used for USB connection detection

#define SD_SPI_MISO 7

#define SERIAL_INIT_DELAY_MS 10
#define SERIAL_READY_TIMEOUT_MS 3000

// =============================================================================
// Boot loop detection - persists across soft resets (ESP.restart / panic)
// but resets on full power cycle, which is the desired behavior.
// =============================================================================
RTC_DATA_ATTR static uint8_t rtcBootCount = 0;
RTC_DATA_ATTR static uint32_t rtcBootTimestamp = 0;  // millis() at last boot
static constexpr uint8_t BOOT_LOOP_THRESHOLD = 4;    // 4 rapid reboots = boot loop
static constexpr uint32_t BOOT_LOOP_WINDOW_MS = 15000;  // reboots within 15s count as rapid
static bool bootLoopRecovered = false;  // Set if recovery was triggered this boot

/**
 * Check for boot loops and recover if detected.
 * Must be called very early in startup, before any complex init.
 * Uses RTC memory (survives soft resets) to track rapid reboot count.
 * If the device reboots BOOT_LOOP_THRESHOLD times within BOOT_LOOP_WINDOW_MS,
 * we assume a boot loop and reset state to attempt clean recovery.
 */
static void bootLoopGuard() {
  // On power-on reset, RTC memory is garbage - detect and reset
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT) {
    rtcBootCount = 0;
    rtcBootTimestamp = 0;
  }

  uint32_t now = millis();

  // If we're within the rapid-boot window, increment counter
  if (rtcBootCount > 0 && (now - rtcBootTimestamp) < BOOT_LOOP_WINDOW_MS) {
    rtcBootCount++;
  } else {
    // Outside window - start fresh
    rtcBootCount = 1;
  }
  rtcBootTimestamp = now;

  Serial.printf("[BOOT] Boot count: %d (reason: %d)\n", rtcBootCount, reason);

  if (rtcBootCount >= BOOT_LOOP_THRESHOLD) {
    Serial.println("[BOOT] *** BOOT LOOP DETECTED - forcing recovery ***");

    rtcBootCount = 0;
    rtcBootTimestamp = 0;
    bootLoopRecovered = true;

    Serial.println("[BOOT] Recovery complete - continuing normal boot");
  }
}

/**
 * Call after successful boot (few seconds in) to reset the rapid-boot counter.
 * This prevents normal startup sequences from triggering the guard.
 */
static void bootLoopGuardClear() {
  rtcBootCount = 0;
  rtcBootTimestamp = 0;
}

EInkDisplay einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
InputManager inputManager;
MappedInputManager mappedInputManager(inputManager);
GfxRenderer renderer(einkDisplay);

// Extern references for driver wrappers
EInkDisplay& display = einkDisplay;
MappedInputManager& mappedInput = mappedInputManager;

// Core system
namespace sumi {
Core core;
}

// State instances (pre-allocated, no heap per transition)
static sumi::StartupState startupState;
static sumi::HomeState homeState(renderer);
static sumi::FileListState fileListState(renderer);
static sumi::ReaderState readerState(renderer);
static sumi::SettingsState settingsState(renderer);
static sumi::SleepState sleepState(renderer);
static sumi::ErrorState errorState(renderer);
static sumi::StateMachine stateMachine;

// Plugin system instances
#if FEATURE_PLUGINS
static sumi::PluginRenderer pluginRenderer(renderer);
static sumi::PluginListState pluginListState(renderer);
static sumi::PluginHostState pluginHostState(renderer);
#endif

RTC_DATA_ATTR uint16_t rtcPowerButtonDurationMs = 400;

// Fonts - XSmall (12pt)
EpdFont readerXSmallFont(&reader_xsmall_regular_2b);
EpdFont readerXSmallBoldFont(&reader_xsmall_bold_2b);
EpdFont readerXSmallItalicFont(&reader_xsmall_italic_2b);
EpdFontFamily readerXSmallFontFamily(&readerXSmallFont, &readerXSmallBoldFont, &readerXSmallItalicFont,
                                     &readerXSmallBoldFont);

// Fonts - Small (14pt, default)
EpdFont readerFont(&reader_2b);
EpdFont readerBoldFont(&reader_bold_2b);
EpdFont readerItalicFont(&reader_italic_2b);
EpdFontFamily readerFontFamily(&readerFont, &readerBoldFont, &readerItalicFont, &readerBoldFont);

// Fonts - Medium (16pt)
EpdFont readerMediumFont(&reader_medium_2b);
EpdFont readerMediumBoldFont(&reader_medium_bold_2b);
EpdFont readerMediumItalicFont(&reader_medium_italic_2b);
EpdFontFamily readerMediumFontFamily(&readerMediumFont, &readerMediumBoldFont, &readerMediumItalicFont,
                                     &readerMediumBoldFont);

// Fonts - Large (18pt)
EpdFont readerLargeFont(&reader_large_2b);
EpdFont readerLargeBoldFont(&reader_large_bold_2b);
EpdFont readerLargeItalicFont(&reader_large_italic_2b);
EpdFontFamily readerLargeFontFamily(&readerLargeFont, &readerLargeBoldFont, &readerLargeItalicFont,
                                    &readerLargeBoldFont);

EpdFont smallFont(&small14);
EpdFontFamily smallFontFamily(&smallFont);

EpdFont ui12Font(&ui_12);
EpdFont uiBold12Font(&ui_bold_12);
EpdFontFamily uiFontFamily(&ui12Font, &uiBold12Font);

bool isUsbConnected() { return digitalRead(UART0_RXD) == HIGH; }

struct WakeupInfo {
  esp_reset_reason_t resetReason;
  bool isPowerButton;
};

WakeupInfo getWakeupInfo() {
  const bool usbConnected = isUsbConnected();
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  // Without USB: power button triggers a full power-on reset (not GPIO wakeup)
  // With USB: power button wakes from deep sleep via GPIO
  const bool isPowerButton =
      (!usbConnected && wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON) ||
      (usbConnected && wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP);

  return {resetReason, isPowerButton};
}

// Verify long press on wake-up from deep sleep
void verifyWakeupLongPress(esp_reset_reason_t resetReason) {
  if (resetReason == ESP_RST_SW) {
    Serial.printf("[%lu] [   ] Skipping wakeup verification (software restart)\n", millis());
    return;
  }

  // Fast path for short press mode - skip verification entirely.
  // Uses settings directly (not RTC variable) so it works even after a full power cycle
  // where RTC memory is lost. Needed because inputManager.isPressed() may take up to
  // ~500ms to return the correct state after wake-up.
  if (sumi::core.settings.shortPwrBtn == sumi::Settings::PowerSleep) {
    Serial.printf("[%lu] [   ] Skipping wakeup verification (short press mode)\n", millis());
    return;
  }

  // Give the user up to 1000ms to start holding the power button, and must hold for the configured duration
  const auto start = millis();
  bool abort = false;
  const uint16_t requiredPressDuration = sumi::core.settings.getPowerButtonDuration();

  inputManager.update();
  // Verify the user has actually pressed
  while (!inputManager.isPressed(InputManager::BTN_POWER) && millis() - start < 1000) {
    delay(10);  // only wait 10ms each iteration to not delay too much in case of short configured duration.
    inputManager.update();
  }

  if (inputManager.isPressed(InputManager::BTN_POWER)) {
    do {
      delay(10);
      inputManager.update();
    } while (inputManager.isPressed(InputManager::BTN_POWER) && inputManager.getHeldTime() < requiredPressDuration);
    abort = inputManager.getHeldTime() < requiredPressDuration;
  } else {
    abort = true;
  }

  if (abort) {
    // Button released too early. Returning to sleep.
    // IMPORTANT: Re-arm the wakeup trigger before sleeping again
    esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    // Hold all GPIO pins at their current state during deep sleep to keep the X4's LDO enabled.
    // Without this, floating pins can cause increased power draw during sleep.
    gpio_deep_sleep_hold_en();
    esp_deep_sleep_start();
  }
}

void waitForPowerRelease() {
  inputManager.update();
  while (inputManager.isPressed(InputManager::BTN_POWER)) {
    delay(50);
    inputManager.update();
  }
}

void setupDisplayAndFonts() {
  einkDisplay.begin();
  renderer.begin();
  Serial.printf("[%lu] [   ] Display initialized\n", millis());
  renderer.insertFont(READER_FONT_ID_XSMALL, readerXSmallFontFamily);
  renderer.insertFont(READER_FONT_ID, readerFontFamily);
  renderer.insertFont(READER_FONT_ID_MEDIUM, readerMediumFontFamily);
  renderer.insertFont(READER_FONT_ID_LARGE, readerLargeFontFamily);
  renderer.insertFont(UI_FONT_ID, uiFontFamily);
  renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
  Serial.printf("[%lu] [   ] Fonts setup\n", millis());
}

void applyThemeFonts() {
  Theme& theme = THEME_MANAGER.mutableCurrent();

  // Reset UI font to builtin first in case custom font loading fails
  theme.uiFontId = UI_FONT_ID;

  // Apply custom UI font if specified (small, always safe to load)
  if (theme.uiFontFamily[0] != '\0') {
    int customUiFontId = FONT_MANAGER.getFontId(theme.uiFontFamily, UI_FONT_ID);
    if (customUiFontId != UI_FONT_ID) {
      theme.uiFontId = customUiFontId;
      Serial.printf("[%lu] [FONT] UI font: %s (ID: %d)\n", millis(), theme.uiFontFamily, customUiFontId);
    }
  }

  // Only load the reader font that matches current font size setting
  // This saves ~500KB+ of RAM by not loading all three sizes
  const char* fontFamilyName = nullptr;
  int* targetFontId = nullptr;
  int builtinFontId = 0;

  switch (sumi::core.settings.fontSize) {
    case sumi::Settings::FontXSmall:
      fontFamilyName = theme.readerFontFamilyXSmall;
      targetFontId = &theme.readerFontIdXSmall;
      builtinFontId = READER_FONT_ID_XSMALL;
      break;
    case sumi::Settings::FontMedium:
      fontFamilyName = theme.readerFontFamilyMedium;
      targetFontId = &theme.readerFontIdMedium;
      builtinFontId = READER_FONT_ID_MEDIUM;
      break;
    case sumi::Settings::FontLarge:
      fontFamilyName = theme.readerFontFamilyLarge;
      targetFontId = &theme.readerFontIdLarge;
      builtinFontId = READER_FONT_ID_LARGE;
      break;
    default:  // FontSmall
      fontFamilyName = theme.readerFontFamilySmall;
      targetFontId = &theme.readerFontId;
      builtinFontId = READER_FONT_ID;
      break;
  }

  // Reset to builtin first in case custom font loading fails
  *targetFontId = builtinFontId;

  if (fontFamilyName && fontFamilyName[0] != '\0') {
    int customFontId = FONT_MANAGER.getFontId(fontFamilyName, builtinFontId);
    if (customFontId != builtinFontId) {
      *targetFontId = customFontId;
      Serial.printf("[%lu] [FONT] Reader font: %s (ID: %d)\n", millis(), fontFamilyName, customFontId);
    }
  }
}

void showErrorScreen(const char* message) {
  renderer.clearScreen(false);
  renderer.drawCenteredText(UI_FONT_ID, 100, message, true, BOLD);
  renderer.displayBuffer();
}

// Early initialization
// Returns false if critical initialization failed
bool earlyInit() {
  // Disable task watchdog — setup takes >5s (ADC reads + SD init + display refresh)
  esp_task_wdt_deinit();

  // Only start serial if USB connected
  pinMode(UART0_RXD, INPUT);
  gpio_deep_sleep_hold_dis();  // Release GPIO hold from deep sleep to allow fresh readings
  if (isUsbConnected()) {
    Serial.begin(115200);
    delay(SERIAL_INIT_DELAY_MS);  // Allow USB CDC to initialize
    unsigned long start = millis();
    while (!Serial && (millis() - start) < SERIAL_READY_TIMEOUT_MS) {
      delay(SERIAL_INIT_DELAY_MS);
    }
  }

  // Boot loop detection - must be early, before anything that could crash
  bootLoopGuard();

  inputManager.begin();

  // Initialize SPI and SD card before wakeup verification so settings are available
  SPI.begin(EPD_SCLK, SD_SPI_MISO, EPD_MOSI, EPD_CS);
  if (!SdMan.begin()) {
    Serial.printf("[%lu] [   ] SD card initialization failed\n", millis());
    setupDisplayAndFonts();
    showErrorScreen("SD card error");
    return false;
  }

  // Migrate data directory from .papyrix to .sumi (one-time)
  if (SdMan.exists("/.papyrix") && !SdMan.exists("/.sumi")) {
    Serial.println("[MIGRATE] Renaming /.papyrix -> /.sumi");
    SdMan.rename("/.papyrix", "/.sumi");
  }

  // Detect first boot (no .sumi folder yet) — used for welcome overlay on home screen
  sumi::core.settings.isFirstBoot = !SdMan.exists(SUMI_DIR);

  // Crash report to SD (CrossPoint #1145): log reset reason for post-mortem debugging.
  // Users without USB serial can check /.sumi/crash.log for crash info.
  {
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_WDT || bootLoopRecovered) {
      SdMan.mkdir(SUMI_DIR);
      FsFile crashFile;
      if (SdMan.openFileForWrite("CRS", SUMI_DIR "/crash.log", crashFile)) {
        crashFile.seekEnd();  // append
        char buf[128];
        snprintf(buf, sizeof(buf), "[%s] Reset reason: %d%s, boot count: %d, uptime: %lu ms\n",
                 SUMI_VERSION, reason,
                 bootLoopRecovered ? " (BOOT LOOP RECOVERED)" : "",
                 rtcBootCount, millis());
        crashFile.write(buf, strlen(buf));
        crashFile.close();
        Serial.printf("[BOOT] Crash logged to %s/crash.log\n", SUMI_DIR);
      }
    }
  }

  // Load settings before wakeup verification - without this, a full power cycle
  // (no USB) resets RTC memory and the short power button setting is ignored
  sumi::core.settings.loadFromFile();
  rtcPowerButtonDurationMs = sumi::core.settings.getPowerButtonDuration();

  // If boot loop was detected, also clear any pending transitions in settings
  // to prevent cascading boot issues (e.g., READER mode pointing to a bad file)
  if (bootLoopRecovered) {
    sumi::core.settings.pendingTransition = 0;
    sumi::core.settings.transitionReturnTo = 0;
    sumi::core.settings.saveToFile();
    Serial.println("[BOOT] Cleared pending transitions after boot loop recovery");
  }

  const auto wakeup = getWakeupInfo();
  if (wakeup.isPowerButton) {
    verifyWakeupLongPress(wakeup.resetReason);
  }

  Serial.printf("[%lu] [   ] Starting SUMI version " SUMI_VERSION "\n", millis());

  // Initialize battery ADC pin with proper attenuation for 0-3.3V range
  analogSetPinAttenuation(BAT_GPIO0, ADC_11db);

  // Initialize internal flash filesystem for font storage
  if (!LittleFS.begin(false)) {
    Serial.printf("[%lu] [FS] LittleFS mount failed, attempting format\n", millis());
    if (!LittleFS.format() || !LittleFS.begin(false)) {
      Serial.printf("[%lu] [FS] LittleFS recovery failed\n", millis());
      showErrorScreen("Internal storage error");
      return false;
    }
    Serial.printf("[%lu] [FS] LittleFS formatted and mounted\n", millis());
  } else {
    Serial.printf("[%lu] [FS] LittleFS mounted\n", millis());
  }

  // Initialize memory arena for image processing and decompression buffers.
  // This must happen before any image/ZIP operations to prevent fragmentation.
  if (!sumi::MemoryArena::init()) {
    showErrorScreen("Memory init failed");
    return false;
  }

  return true;
}

// Unified system initialization — all states registered, no dual-boot split.
// Determines initial state from settings (StartupLastDocument → Reader, else Home).
// State transitions happen in-process via StateTransition::to() — no ESP.restart().
void initSystem() {
  Serial.printf("[%lu] [BOOT] Initializing system\n", millis());
  Serial.printf("[%lu] [BOOT] Free heap: %lu, Max block: %lu\n", millis(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());

  // Determine initial state before font loading (XTC content can skip custom fonts)
  sumi::StateId initialState = sumi::StateId::Home;
  bool startInReader = false;

  // Check "Last Document" startup behavior for cold-boot reader start
  if (sumi::core.settings.startupBehavior == sumi::Settings::StartupLastDocument &&
      sumi::core.settings.lastBookPath[0] != '\0' &&
      SdMan.exists(sumi::core.settings.lastBookPath)) {
    Serial.printf("[%lu] [BOOT] 'Last Document' startup: %s\n", millis(), sumi::core.settings.lastBookPath);
    startInReader = true;
    initialState = sumi::StateId::Reader;
  }

  // Detect content type for font optimization (XTC doesn't need custom fonts)
  bool needsCustomFonts = true;
  if (startInReader) {
    sumi::ContentType contentType = sumi::detectContentType(sumi::core.settings.lastBookPath);
    needsCustomFonts = (contentType != sumi::ContentType::Xtc);
  }

  // Initialize theme and font managers
  FONT_MANAGER.init(renderer);
  THEME_MANAGER.loadTheme(sumi::core.settings.themeName);
  THEME_MANAGER.createDefaultThemeFiles();
  Serial.printf("[%lu] [   ] Theme loaded: %s\n", millis(), THEME_MANAGER.currentThemeName());

  setupDisplayAndFonts();
  if (needsCustomFonts) {
    applyThemeFonts();
  } else {
    Serial.printf("[%lu] [BOOT] Skipping custom fonts for XTC content\n", millis());
  }

  // Show boot splash when starting at Home
  if (!startInReader) {
    ui::BootView bootView;
    bootView.setLogo(SumiLogo, 128, 128);
    bootView.setVersion(SUMI_VERSION);
    bootView.setStatus("BOOTING");
    ui::render(renderer, THEME, bootView);
  }

  // Register ALL states (unified — transitions happen in-process)
  stateMachine.registerState(&startupState);
  stateMachine.registerState(&homeState);
  stateMachine.registerState(&fileListState);
  stateMachine.registerState(&readerState);
  stateMachine.registerState(&settingsState);
  stateMachine.registerState(&sleepState);
  stateMachine.registerState(&errorState);

#if FEATURE_PLUGINS
  // Register plugin states
  pluginListState.setHostState(&pluginHostState);
  fileListState.setHostState(&pluginHostState);
  stateMachine.registerState(&pluginListState);
  stateMachine.registerState(&pluginHostState);

  // Register available plugins
#if FEATURE_GAMES
  sumi::PluginListState::registerPlugin("Chess", "Games", []() -> sumi::PluginInterface* {
    return new sumi::ChessGame(pluginRenderer);
  }, CHESS_SAVE_PATH);
  sumi::PluginListState::registerPlugin("Sudoku", "Games", []() -> sumi::PluginInterface* {
    return new sumi::SudokuGame(pluginRenderer);
  }, SUDOKU_SAVE_PATH);
  sumi::PluginListState::registerPlugin("Minesweeper", "Games", []() -> sumi::PluginInterface* {
    return new sumi::MinesweeperGame(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("Checkers", "Games", []() -> sumi::PluginInterface* {
    return new sumi::CheckersGame(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("Solitaire", "Games", []() -> sumi::PluginInterface* {
    return new sumi::SolitaireGame(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("SumiBoy", "Games", []() -> sumi::PluginInterface* {
    return new sumi::SumiBoyRomPicker(pluginRenderer);
  });
#endif  // FEATURE_GAMES

  sumi::PluginListState::registerPlugin("Notes", "Productivity", []() -> sumi::PluginInterface* {
    return new sumi::NotesApp(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("Todo List", "Productivity", []() -> sumi::PluginInterface* {
    return new sumi::TodoApp(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("Tools", "Productivity", []() -> sumi::PluginInterface* {
    return new sumi::ToolSuiteApp(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("Images", "Productivity", []() -> sumi::PluginInterface* {
    return new sumi::ImagesApp(pluginRenderer);
  });
  sumi::PluginListState::registerPlugin("Maps", "Productivity", []() -> sumi::PluginInterface* {
    return new sumi::MapsApp(pluginRenderer);
  });

#if FEATURE_FLASHCARDS
  sumi::PluginListState::registerPlugin("Flashcards", "Learning", []() -> sumi::PluginInterface* {
    return new sumi::FlashcardsApp(pluginRenderer);
  });
#endif

  // Scan SD card for user-created Lua plugins
  sumi::PluginListState::scanLuaPlugins(pluginRenderer);

  Serial.printf("[BOOT] Registered %d plugins (%d built-in + %d Lua)\n",
                sumi::PluginListState::pluginCount,
                sumi::PluginListState::pluginCount - sumi::PluginListState::luaPluginCount_,
                sumi::PluginListState::luaPluginCount_);
#endif  // FEATURE_PLUGINS

  // Initialize core
  auto result = sumi::core.init();
  if (!result.ok()) {
    Serial.printf("[%lu] [CORE] Init failed: %s\n", millis(), sumi::errorToString(result.err));
    showErrorScreen("Core init failed");
    return;
  }

  Serial.printf("[%lu] [CORE] State machine starting (initial: %s)\n", millis(),
                startInReader ? "Reader" : "Home");
  mappedInputManager.setSettings(&sumi::core.settings);
  ui::setFrontButtonLayout(sumi::core.settings.frontButtonLayout);

  // Enable periodic half-refresh in non-reader states to clear e-ink ghosting.
  // Reader manages its own counter and disables this on enter.
  if (!startInReader) {
    renderer.setPeriodicRefreshInterval(sumi::core.settings.getPagesPerRefreshValue());
  }

  // Set up reader path if starting in reader
  if (startInReader) {
    strncpy(sumi::core.buf.path, sumi::core.settings.lastBookPath, sizeof(sumi::core.buf.path) - 1);
    sumi::core.buf.path[sizeof(sumi::core.buf.path) - 1] = '\0';
    Serial.printf("[%lu] [BOOT] Opening book: %s\n", millis(), sumi::core.buf.path);
  }

  stateMachine.init(sumi::core, initialState);

  // Force initial render
  Serial.printf("[%lu] [CORE] Forcing initial render\n", millis());
  stateMachine.update(sumi::core);

  Serial.printf("[%lu] [BOOT] After init - Free heap: %lu, Max block: %lu\n", millis(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
}

void setup() {
  // Early initialization
  if (!earlyInit()) {
    return;  // Critical failure
  }

  initSystem();

  // Ensure we're not still holding the power button before leaving setup
  waitForPowerRelease();
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;
  static bool bootGuardCleared = false;

  // After 10 seconds of stable operation, clear the boot loop counter
  if (!bootGuardCleared && millis() > 10000) {
    bootLoopGuardClear();
    bootGuardCleared = true;
  }

  inputManager.update();

  // Apply sunlight fading fix setting to renderer (like CrossPoint's fadingFix)
  renderer.setFadingFix(sumi::core.settings.sunlightFadingFix != 0);

  // Screenshot: simultaneous Up+Down press saves framebuffer as BMP to SD (CrossPoint #759)
  if (inputManager.isPressed(InputManager::BTN_UP) && inputManager.isPressed(InputManager::BTN_DOWN)) {
    static unsigned long lastScreenshotMs = 0;
    if (millis() - lastScreenshotMs > 3000) {  // debounce 3s
      lastScreenshotMs = millis();
      SdMan.mkdir("/screenshots");
      char path[48];
      snprintf(path, sizeof(path), "/screenshots/%lu.bmp", millis());

      // Write 1-bit BMP: 480x800 portrait (rotated from 800x480 physical buffer)
      const uint8_t* fb = einkDisplay.getFrameBuffer();
      const int W = 480, H = 800;  // output dimensions (portrait)
      const int rowBytes = (W + 31) / 32 * 4;  // BMP row stride (4-byte aligned)
      const int imageSize = rowBytes * H;
      const int fileSize = 62 + imageSize;  // 14 (file hdr) + 40 (info hdr) + 8 (palette) + image

      FsFile f;
      if (SdMan.openFileForWrite("SCR", path, f)) {
        // BMP file header (14 bytes)
        uint8_t hdr[62] = {};
        hdr[0] = 'B'; hdr[1] = 'M';
        memcpy(hdr + 2, &fileSize, 4);
        uint32_t offset = 62; memcpy(hdr + 10, &offset, 4);
        // DIB header (40 bytes)
        uint32_t dibSize = 40; memcpy(hdr + 14, &dibSize, 4);
        int32_t w = W; memcpy(hdr + 18, &w, 4);
        int32_t h = H; memcpy(hdr + 22, &h, 4);  // positive = bottom-up
        uint16_t planes = 1; memcpy(hdr + 26, &planes, 2);
        uint16_t bpp = 1; memcpy(hdr + 28, &bpp, 2);
        memcpy(hdr + 34, &imageSize, 4);
        // Palette: black and white (8 bytes)
        hdr[54] = 0; hdr[55] = 0; hdr[56] = 0; hdr[57] = 0;       // black
        hdr[58] = 0xFF; hdr[59] = 0xFF; hdr[60] = 0xFF; hdr[61] = 0;  // white
        f.write(hdr, 62);

        // Write rows bottom-up, rotating 90deg CCW from 800x480 physical to 480x800 portrait
        uint8_t row[64];  // 480/8 = 60 bytes, padded to 64
        for (int outY = H - 1; outY >= 0; outY--) {
          memset(row, 0, sizeof(row));
          for (int outX = 0; outX < W; outX++) {
            // Map portrait (outX, outY) → physical (inX, inY)
            int inX = outY;               // portrait Y maps to physical X
            int inY = W - 1 - outX;       // portrait X maps to inverted physical Y
            int srcByte = inY * 100 + (inX / 8);  // 800/8 = 100 bytes per row
            int srcBit = 7 - (inX % 8);
            int pixel = (fb[srcByte] >> srcBit) & 1;
            if (pixel) row[outX / 8] |= (0x80 >> (outX % 8));  // BMP MSB-first
          }
          f.write(row, rowBytes);
        }
        f.close();
        Serial.printf("[%lu] [SCR] Screenshot saved: %s\n", millis(), path);
      }
    }
  }

  if (Serial && millis() - lastMemPrint >= 10000) {
    Serial.printf("[%lu] [MEM] Free: %d bytes, Total: %d bytes, Min Free: %d bytes\n", millis(), ESP.getFreeHeap(),
                  ESP.getHeapSize(), ESP.getMinFreeHeap());
    lastMemPrint = millis();
  }

  // Poll input and push events to queue
  sumi::core.input.poll();

#if FEATURE_BLUETOOTH
  // Check BLE inactivity timeout — disconnect and reclaim memory if idle too long
  if (ble::isReady() && ble::checkInactivityTimeout()) {
    Serial.println("[BLE] Timed out, reclaiming memory");
    ble_transfer::deinit();
    ble::deinit();
    if (!sumi::MemoryArena::isInitialized()) {
      sumi::MemoryArena::init();
    }
  }
#endif

  // Auto-sleep after inactivity
  // Skip if BLE file transfer is active — sleeping during transfer crashes with
  // a FreeRTOS assert (xQueueGenericSend) as the settings state exits while
  // the BLE stack is holding mutexes in its write callbacks.
  const auto autoSleepTimeout = sumi::core.settings.getAutoSleepTimeoutMs();
  if (autoSleepTimeout > 0 && sumi::core.input.idleTimeMs() >= autoSleepTimeout) {
    if (ble_transfer::isTransferring() || ble_transfer::isConnected()) {
      // Don't sleep — BLE is active. Reset the idle timer by doing nothing
      // (the next input event will reset it naturally).
    } else {
      Serial.printf("[%lu] [SLP] Auto-sleep after %lu ms idle\n", millis(), autoSleepTimeout);
      stateMachine.init(sumi::core, sumi::StateId::Sleep);
      return;
    }
  }

  // Power button sleep check: track held time that excludes long rendering gaps
  // where button state changes could have been missed by inputManager
  {
    static unsigned long powerHeldSinceMs = 0;
    static unsigned long prevPowerCheckMs = 0;
    const unsigned long loopGap = loopStartTime - prevPowerCheckMs;
    prevPowerCheckMs = loopStartTime;

    if (inputManager.isPressed(InputManager::BTN_POWER)) {
      if (powerHeldSinceMs == 0 || loopGap > 100) {
        powerHeldSinceMs = loopStartTime;
      }
      if (loopStartTime - powerHeldSinceMs > sumi::core.settings.getPowerButtonDuration()) {
        stateMachine.init(sumi::core, sumi::StateId::Sleep);
        return;
      }
    } else {
      powerHeldSinceMs = 0;
    }
  }

  // Update state machine (handles transitions and rendering)
  const unsigned long activityStartTime = millis();
  stateMachine.update(sumi::core);
  const unsigned long activityDuration = millis() - activityStartTime;

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      Serial.printf("[%lu] [LOOP] New max loop duration: %lu ms (activity: %lu ms)\n", millis(), maxLoopDuration,
                    activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // Increase delay after idle to save power (~4x less CPU load)
  // Idea: https://github.com/crosspoint-reader/crosspoint-reader/commit/0991782 by @ngxson (https://github.com/ngxson)
  static constexpr unsigned long kIdlePowerSavingMs = 3000;
  if (sumi::core.input.idleTimeMs() >= kIdlePowerSavingMs) {
    delay(50);
  } else {
    delay(10);
  }
}
