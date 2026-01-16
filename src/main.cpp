/**
 * @file main.cpp
 * @brief Sumi Firmware - Open source e-reader for ESP32-C3
 * @version 2.1.16
 *
 * Main entry point for Sumi firmware. Handles initialization, display setup,
 * button input processing, and the main event loop.
 *
 * Boot sequence:
 * 1. Initialize hardware (buttons, battery, SD card, display)
 * 2. Load settings from SD card
 * 3. Check if first boot (setup mode) or normal operation
 * 4. Enter appropriate mode (WiFi portal or home screen)
 *
 * @see docs/ARCHITECTURE.md for system design details
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "config.h"

// =============================================================================
// Core Systems
// =============================================================================
#include "core/WiFiManager.h"
#include "core/SettingsManager.h"
#include "core/BatteryMonitor.h"
#include "core/HomeItems.h"
#include "core/RefreshManager.h"
#include "core/PowerManager.h"
#include "core/ButtonInput.h"
#include "core/SetupWizard.h"
#include "core/HomeScreen.h"
#include "core/SettingsScreen.h"
#include "core/AppLauncher.h"
#include "core/SettingsState.h"

// Global RefreshManager instance
RefreshManager refreshManager;

#if FEATURE_WEBSERVER
    #include "core/WebServer.h"
#endif

#if FEATURE_BLUETOOTH
    #include "core/BluetoothManager.h"
#endif

// =============================================================================
// PLUGINS - Include headers (implementations compile separately in src/plugins/)
// =============================================================================

// Library is CORE - always included when READER is enabled
#if FEATURE_READER
#include "plugins/Library.h"
#endif

// Flashcards - separate feature
#if FEATURE_FLASHCARDS
#include "plugins/Flashcards.h"
#endif

// Weather - separate feature (uses WiFi for API calls)
#if FEATURE_WEATHER
#include "plugins/Weather.h"
#endif

// Games and productivity plugins - only when FEATURE_GAMES is enabled
#if FEATURE_GAMES
#include "plugins/Notes.h"
#include "plugins/ChessGame.h"
#include "plugins/Checkers.h"
#include "plugins/Sudoku.h"
#include "plugins/Minesweeper.h"
#include "plugins/Solitaire.h"
#include "plugins/TodoList.h"
#include "plugins/Images.h"
#include "plugins/ToolSuite.h"
#endif

// Include plugin runner templates (after plugin headers)
#include "core/PluginRunner.h"

// =============================================================================
// Display
// =============================================================================
GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display(
    GxEPD2_426_GDEQ0426T82(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// =============================================================================
// Global State
// =============================================================================
bool sd_card_present = false;
bool setup_mode = false;
bool portal_active = false;

enum Screen { SCREEN_HOME, SCREEN_SETTINGS };
Screen currentScreen = SCREEN_HOME;

// =============================================================================
// Function Declarations
// =============================================================================
void initDisplay();
void initSDCard();
void handleButtons();

// For button state tracking
static Button lastButton = BTN_NONE;

// =============================================================================
// Bluetooth Keyboard Support
// =============================================================================
#if FEATURE_BLUETOOTH
// Callback for BLE keyboard input - converts HID keycodes to Button presses
void onBluetoothKey(const KeyEvent& event) {
    if (!event.pressed) return;  // Only handle key down
    
    Button btn = BTN_NONE;
    
    // Convert HID keycodes to Button enum
    switch (event.keyCode) {
        case 0x52: btn = BTN_UP; break;      // Up arrow
        case 0x51: btn = BTN_DOWN; break;    // Down arrow
        case 0x50: btn = BTN_LEFT; break;    // Left arrow
        case 0x4F: btn = BTN_RIGHT; break;   // Right arrow
        case 0x28: btn = BTN_CONFIRM; break; // Enter
        case 0x29: btn = BTN_BACK; break;    // Escape
        case 0x2C: btn = BTN_CONFIRM; break; // Space
        // WASD support
        case 0x1A: btn = BTN_UP; break;      // W
        case 0x16: btn = BTN_DOWN; break;    // S
        case 0x04: btn = BTN_LEFT; break;    // A
        case 0x07: btn = BTN_RIGHT; break;   // D
    }
    
    if (btn != BTN_NONE) {
        Serial.printf("[BT] Key: 0x%02X -> %s\n", event.keyCode, getButtonName(btn));
        injectButton(btn);
    }
}
#endif

// =============================================================================
// Wake-up Verification
// Prevents accidental wake from brief button contact during deep sleep
// =============================================================================
void verifyWakeupLongPress() {
    pinMode(BTN_GPIO3, INPUT_PULLUP);
    
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_GPIO) {
        return;  // Not waking from sleep, proceed normally
    }
    
    Serial.println("[POWER] Verifying wake-up long press...");
    
    // Wait for button release or timeout
    unsigned long start = millis();
    while (digitalRead(BTN_GPIO3) == LOW) {
        if (millis() - start >= POWER_BUTTON_WAKEUP_MS) {
            Serial.println("[POWER] Wake-up confirmed!");
            return;
        }
        delay(10);
    }
    
    // Button released too early - go back to sleep
    Serial.println("[POWER] Wake-up cancelled - returning to sleep");
    powerManager.enterDeepSleep();
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println();
    Serial.println("[SUMI] ========================================");
    Serial.println("[SUMI]   SUMI E-READER STARTING");
    Serial.printf("[SUMI]   Version: %s\n", SUMI_VERSION);
    Serial.println("[SUMI] ========================================");
    
    // Initialize button pins FIRST
    pinMode(BTN_GPIO1, INPUT);
    pinMode(BTN_GPIO2, INPUT);
    pinMode(BTN_GPIO3, INPUT_PULLUP);
    
    // Verify intentional wake-up (long press required)
    verifyWakeupLongPress();
    
    // Initialize battery monitoring
    batteryMonitor.begin();
    
    // Initialize SD card before display (required for proper SPI bus sharing)
    initSDCard();
    
    // Initialize display (after SD)
    initDisplay();
    
    // Load settings from SD card
    if (sd_card_present) {
        settingsManager.begin();
        Serial.println("[SUMI] Settings loaded from SD");
    }
    
    // Set orientation from settings
    bool isHorizontal = (settingsManager.display.orientation == 0);
    display.setRotation(isHorizontal ? 0 : 3);
    setButtonOrientation(isHorizontal);
    
    // Check if setup is needed
    setup_mode = !settingsManager.isSetupComplete();
    
    // Initialize WiFi manager (loads saved credentials from NVS)
    wifiManager.begin();
    
    if (setup_mode) {
        Serial.println("[SUMI] First boot - entering setup mode");
        
        // Play animation (ends with setup screen displayed)
        playDeployAnimation();
        
        // Start WiFi AP for portal
        wifiManager.startAP();
        portal_active = true;
        
        #if FEATURE_WEBSERVER
        webServer.begin();
        #endif
    } else {
        Serial.println("[SUMI] Normal boot - loading home screen");
        
        // Build home screen items
        buildHomeScreenItems();
        updateGridLayout();
        
        // Show home screen
        showHomeScreen();
        
        // Sync time in background if we have WiFi credentials
        // This runs after home screen is shown so user sees UI quickly
        if (wifiManager.hasCredentials()) {
            Serial.println("[SUMI] Syncing time from network...");
            if (wifiManager.syncTime()) {
                Serial.println("[SUMI] Time sync successful");
            } else {
                Serial.println("[SUMI] Time sync failed (will use last known time)");
            }
        }
    }
    
    // Initialize Bluetooth
    #if FEATURE_BLUETOOTH
    bluetoothManager.begin();
    bluetoothManager.setKeyCallback(onBluetoothKey);
    #endif
    
    // Start activity timer
    powerManager.resetActivityTimer();
    
    printMemoryReport();
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    static unsigned long lastMemReport = 0;
    
    // Memory report every 30 seconds (debug only)
    if (millis() - lastMemReport > 30000) {
        lastMemReport = millis();
        Serial.printf("[MEM] Free: %d  Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
    
    // Handle button input
    if (!setup_mode) {
        handleButtons();
    }
    
    // Auto-sleep check (only when not in setup mode)
    if (!setup_mode && !portal_active) {
        uint8_t sleepMins = settingsManager.display.sleepMinutes;
        if (sleepMins > 0) {
            unsigned long idleMs = powerManager.getIdleTime();
            unsigned long sleepMs = sleepMins * 60000UL;
            if (idleMs >= sleepMs) {
                Serial.printf("[POWER] Idle for %lu seconds - entering sleep\n", idleMs / 1000);
                powerManager.enterDeepSleep();
            }
        }
    }
    
    // Only process WiFi when portal is active
    if (portal_active) {
        wifiManager.update();
    }
    
    #if FEATURE_BLUETOOTH
    // Update Bluetooth manager (scan completion, etc.)
    bluetoothManager.update();
    #endif
    
    #if FEATURE_WEBSERVER
    // Check for portal events
    if (g_wifiJustConnected) {
        g_wifiJustConnected = false;
        Serial.println("[SUMI] WiFi connected via portal");
        // Time sync is handled by WebServer with proper timezone - don't override here
    }
    
    if (g_settingsDeployed) {
        g_settingsDeployed = false;
        Serial.println("[SUMI] Settings deployed - shutting down portal");
        
        // Show deployed screen
        showDeployedScreen();
        waitForButtonPress();
        
        // Clean up portal resources to free memory
        cleanupPortalResources();
        
        // Disconnect WiFi - we don't want it running in the background
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[SUMI] Disconnecting WiFi after deploy");
            WiFi.disconnect();
        }
        WiFi.mode(WIFI_OFF);
        
        // Reload settings and switch to normal mode
        settingsManager.load();
        setup_mode = false;
        
        // Build home screen
        bool isHorizontal = (settingsManager.display.orientation == 0);
        display.setRotation(isHorizontal ? 0 : 3);
        setButtonOrientation(isHorizontal);
        
        buildHomeScreenItems();
        updateGridLayout();
        showHomeScreen();
        
        printMemoryReport();
        Serial.println("[SUMI] Portal fully shut down, WiFi off");
    }
    #endif
    
    // WiFi stays OFF by default - only connects briefly for weather/time sync
    // No auto-reconnect needed
    
    delay(20);  // Small delay to prevent tight loop
}

// =============================================================================
// Display Init
// =============================================================================
void initDisplay() {
    display.init(115200, true, 50, false);
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);
    Serial.println("[DISPLAY] Initialized");
}

void initSDCard() {
    // SPI setup: use EPD_CS as SS pin for SPI.begin (EPD_CS=21, not SD_CS=12)
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, EPD_CS);
    
    // Now init SD with its actual CS pin
    sd_card_present = SD.begin(SD_CS, SPI, 4000000);  // 4MHz for reliability
    
    Serial.printf("[SD] %s\n", sd_card_present ? "Card detected" : "No card");
}

// =============================================================================
// Button Handling
// =============================================================================
void handleButtons() {
    static unsigned long lastPress = 0;
    
    Button btn = readButton();
    
    if (btn != BTN_NONE && lastButton == BTN_NONE) {
        Serial.printf("[MAIN] Button detected: %d\n", btn);
        if (millis() - lastPress > 100) {
            lastPress = millis();
            powerManager.resetActivityTimer();
            
            // Power button triggers deep sleep
            if (btn == BTN_POWER) {
                Serial.println("[BTN] POWER - entering deep sleep");
                powerManager.enterDeepSleep();
                return;
            }
            
            if (setup_mode) {
                if (btn == BTN_BACK) showSetupScreen();
            } else if (currentScreen == SCREEN_SETTINGS) {
                Serial.printf("[MAIN] Settings screen, btn=%d\n", btn);
                switch (btn) {
                    case BTN_UP:
                        settingsUp();
                        showSettingsScreen();
                        break;
                    case BTN_DOWN:
                        settingsDown();
                        showSettingsScreen();
                        break;
                    case BTN_CONFIRM:
                        {
                            bool wasLandscape = (settingsManager.display.orientation == 0);
                            settingsSelect();
                            
                            // Check if orientation changed
                            bool nowLandscape = (settingsManager.display.orientation == 0);
                            if (wasLandscape != nowLandscape) {
                                setButtonOrientation(nowLandscape);
                                display.setRotation(nowLandscape ? 0 : 3);
                                updateGridLayout();
                            }
                            
                            // Exit settings check
                            if (settingsShouldExit()) {
                                currentScreen = SCREEN_HOME;
                                showHomeScreen();
                            } else {
                                showSettingsScreen();
                            }
                        }
                        break;
                    case BTN_BACK:
                        Serial.println("[MAIN] Settings back pressed");
                        settingsBack();
                        if (settingsShouldExit()) {
                            Serial.println("[MAIN] Exiting to home screen");
                            currentScreen = SCREEN_HOME;
                            showHomeScreen();
                        } else {
                            Serial.println("[MAIN] Staying in settings");
                            showSettingsScreen();
                        }
                        break;
                    default:
                        break;
                }
            } else {
                // Home screen
                int itemsOnPage = getItemsOnCurrentPage();
                int totalPages = getTotalPages();
                
                switch (btn) {
                    case BTN_UP:
                        if (homeSelection >= homeCols) {
                            int oldSel = homeSelection;
                            homeSelection -= homeCols;
                            refreshChangedCells(oldSel, homeSelection);
                        }
                        break;
                    case BTN_DOWN:
                        if (homeSelection + homeCols < itemsOnPage) {
                            int oldSel = homeSelection;
                            homeSelection += homeCols;
                            refreshChangedCells(oldSel, homeSelection);
                        }
                        break;
                    case BTN_LEFT:
                        if (homeSelection > 0) {
                            int oldSel = homeSelection;
                            homeSelection--;
                            refreshChangedCells(oldSel, homeSelection);
                        } else if (homePageIndex > 0) {
                            homePageIndex--;
                            homeSelection = getItemsOnCurrentPage() - 1;
                            showHomeScreenPartial(true);
                        }
                        break;
                    case BTN_RIGHT:
                        if (homeSelection < itemsOnPage - 1) {
                            int oldSel = homeSelection;
                            homeSelection++;
                            refreshChangedCells(oldSel, homeSelection);
                        } else if (homePageIndex < totalPages - 1) {
                            homePageIndex++;
                            homeSelection = 0;
                            showHomeScreenPartial(true);
                        }
                        break;
                    case BTN_CONFIRM:
                        openApp(homeSelection);
                        break;
                    case BTN_BACK:
                        if (homePageIndex > 0) {
                            homePageIndex = 0;
                            homeSelection = 0;
                            showHomeScreenPartial(true);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    lastButton = btn;
}
