/**
 * @file main.cpp
 * @brief Sumi Firmware - Open source e-reader for ESP32-C3
 * @version 1.3.0
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
#include <climits>

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

enum AppScreen { APP_SCREEN_HOME, APP_SCREEN_SETTINGS };
AppScreen currentAppScreen = APP_SCREEN_HOME;

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
    
    // Memory baseline at startup
    Serial.printf("[MEM:BOOT] Heap Total: %d, Free: %d, Min Ever: %d\n",
                  ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
    
    // Report stack size at startup
    UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("[SUMI] Loop task stack high water: %d words (%d bytes free)\n", 
                  (int)stackHighWater, (int)(stackHighWater * 4));
    
    // Initialize button pins FIRST
    pinMode(BTN_GPIO1, INPUT);
    pinMode(BTN_GPIO2, INPUT);
    pinMode(BTN_GPIO3, INPUT_PULLUP);
    
    // Verify intentional wake-up (long press required)
    verifyWakeupLongPress();
    
    // Initialize battery monitoring
    batteryMonitor.begin();
    Serial.printf("[MEM:BATT] Free: %d, Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());;
    
    // Initialize SD card before display (required for proper SPI bus sharing)
    initSDCard();
    Serial.printf("[MEM:SD] Free: %d, Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());;
    
    // Load settings from SD card
    if (sd_card_present) {
        settingsManager.begin();
        Serial.println("[SUMI] Settings loaded from SD");
        Serial.printf("[MEM:SETTINGS] Free: %d, Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
    
    // Check if setup is needed BEFORE allocating large buffers
    setup_mode = !settingsManager.isSetupComplete();
    
    // Initialize display (after SD)
    initDisplay();
    Serial.printf("[MEM:DISP] Free: %d, Min: %d (after display buffer)\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    
    // Set orientation from settings
    bool isHorizontal = (settingsManager.display.orientation == 0);
    display.setRotation(isHorizontal ? 0 : 3);
    setButtonOrientation(isHorizontal);
    
    // Initialize WiFi manager (loads saved credentials from NVS)
    wifiManager.begin();
    
    if (setup_mode) {
        Serial.println("[SUMI] First boot - entering setup mode");
        
        // Start WiFi AP immediately so it's ready when user sees setup screen
        wifiManager.startAP();
        portal_active = true;
        
        #if FEATURE_WEBSERVER
        webServer.begin();
        #endif
        
        // Play animation (ends with setup screen displayed)
        playDeployAnimation();
    } else {
        Serial.println("[SUMI] Normal boot - loading home screen");
        
        // Build home screen items
        buildHomeScreenItems();
        updateGridLayout();
        
        // PRE-RENDER: Cache cover and calculate layout (makes showHomeScreen instant)
        prepareHomeScreen();
        
        // Check if we should boot directly to last book
        bool bootedToBook = false;
        if (settingsManager.display.bootToLastBook) {
            // Check if we have a last book saved
            if (SD.exists("/.sumi/lastbook.bin")) {
                Serial.println("[SUMI] Boot to last book enabled - launching reader");
                #if FEATURE_READER
                LibraryApp* libraryApp = new LibraryApp();
                if (libraryApp) {
                    libraryApp->init(display.width(), display.height());
                    libraryApp->resumeLastBook();  // This will load and display the last book
                    
                    // Enter reader loop
                    bool reading = true;
                    while (reading) {
                        Button btn = readButton();
                        if (btn != BTN_NONE) {
                            powerManager.resetActivityTimer();
                            if (btn == BTN_POWER) {
                                delete libraryApp;
                                powerManager.enterDeepSleep();
                            }
                            reading = libraryApp->handleInput(btn);
                            if (reading) {
                                libraryApp->draw();
                            }
                        }
                        
                        // Check auto-sleep
                        uint8_t sleepMins = settingsManager.display.sleepMinutes;
                        if (sleepMins > 0 && powerManager.getIdleTime() >= sleepMins * 60000UL) {
                            delete libraryApp;
                            powerManager.enterDeepSleep();
                        }
                        delay(20);
                    }
                    delete libraryApp;
                    bootedToBook = true;
                }
                #endif
            }
        }
        
        // Show home screen (if we didn't boot to book, or after exiting reader)
        if (!bootedToBook) {
            showHomeScreen();
        } else {
            // Returned from reader - show home screen
            showHomeScreen();
        }
        
        // Start background time sync if needed (non-blocking)
        // User sees home screen immediately while WiFi connects in background
        if (powerManager.needsTimeSync()) {
            powerManager.startBackgroundTimeSync();
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
    static int minFreeEver = INT_MAX;
    
    // Track lowest memory we've ever seen
    int currentFree = ESP.getFreeHeap();
    if (currentFree < minFreeEver) {
        minFreeEver = currentFree;
        Serial.printf("[MEM:NEW_LOW] Free dropped to: %d bytes!\n", currentFree);
    }
    
    // Memory report every 10 seconds for debugging
    #if SUMI_MEM_DEBUG
    if (millis() - lastMemReport > 10000) {
        lastMemReport = millis();
        UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        Serial.printf("[MEM:LOOP] Free: %d | Min: %d | Stack Free: %d words | Uptime: %lu sec\n", 
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), 
                      (int)stackHighWater, millis() / 1000);
    }
    #else
    // Less frequent in production
    if (millis() - lastMemReport > 30000) {
        lastMemReport = millis();
        Serial.printf("[MEM] Free: %d  Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
    #endif
    
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
        
        // Track setup step changes and refresh screen when step changes
        static int lastSetupStep = 0;
        if (setup_mode) {
            int currentStep = getSetupStep();
            
            if (currentStep != lastSetupStep) {
                Serial.printf("[SUMI] Setup step changed: %d -> %d\n", lastSetupStep, currentStep);
                lastSetupStep = currentStep;
                
                // Refresh screen with new step highlighted
                stopRippleAnimation();
                showSetupScreen();
                startRippleAnimation();
            }
            
            // Update ripple animation (non-blocking, 2Hz)
            if (isRippleAnimationActive()) {
                updateRippleAnimation();
            }
        }
    }
    
    // Check background time sync progress (non-blocking)
    powerManager.checkBackgroundTimeSync();
    
    #if FEATURE_BLUETOOTH
    // Update Bluetooth manager (scan completion, etc.)
    bluetoothManager.update();
    #endif
    
    #if FEATURE_WEBSERVER
    // Check for portal events
    if (g_wifiJustConnected) {
        g_wifiJustConnected = false;
        Serial.println("[SUMI] WiFi connected via portal");
        // Refresh screen to update status bar (SUMI connected but step may not change)
        if (setup_mode) {
            stopRippleAnimation();
            showSetupScreen();
            startRippleAnimation();
        }
        // Time sync is handled by WebServer with proper timezone - don't override here
    }
    
    if (g_settingsDeployed) {
        g_settingsDeployed = false;
        Serial.println("[SUMI] Settings deployed - transitioning to home screen");
        
        // Stop ripple animation
        stopRippleAnimation();
        
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
        
        // Set up display orientation
        bool isHorizontal = (settingsManager.display.orientation == 0);
        display.setRotation(isHorizontal ? 0 : 3);
        setButtonOrientation(isHorizontal);
        
        // Build home screen items (loads widget data)
        buildHomeScreenItems();
        updateGridLayout();
        
        // Prepare home screen (caches cover, pre-calculates layout)
        prepareHomeScreen();
        
        Serial.println("[SUMI] Background loading complete");
        printMemoryReport();
        
        // Go straight to home screen
        currentAppScreen = APP_SCREEN_HOME;
        settingsInit();  // Reset settings state so next visit starts fresh
        showHomeScreen();
        
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
            } else if (currentAppScreen == APP_SCREEN_SETTINGS) {
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
                                currentAppScreen = APP_SCREEN_HOME;
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
                            currentAppScreen = APP_SCREEN_HOME;
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
                int numWidgets = getWidgetCount();
                
                // Check if we're in widget selection mode
                if (widgetSelection >= 0) {
                    bool isLandscape = (settingsManager.display.orientation == 0);
                    Serial.printf("[HOME] Widget nav: selection=%d, numWidgets=%d, btn=%d, landscape=%d\n", 
                                  widgetSelection, numWidgets, btn, isLandscape);
                    
                    // Widget navigation - direction depends on orientation AND widget layout
                    // 
                    // PORTRAIT LAYOUT (widgets 0=Book, 1=Weather, 2=Orient):
                    //   [Book Cover]  [Weather ]
                    //                 [Orient  ]
                    // - From Book: RIGHT→Orient, DOWN→grid(left col)
                    // - From Orient: LEFT→Book, UP→Weather, DOWN→grid(right col)  
                    // - From Weather: DOWN→Orient
                    //
                    // LANDSCAPE LAYOUT (widgets stacked vertically on left):
                    //   [Book   ] | [Grid]
                    //   [Weather] | [Grid]
                    //   [Orient ] | [Grid]
                    // - UP/DOWN navigates between widgets
                    // - DOWN from last widget exits to grid
                    
                    switch (btn) {
                        case BTN_UP:
                            if (isLandscape) {
                                // Landscape: UP moves to previous widget
                                if (widgetSelection > 0) {
                                    int oldWidget = widgetSelection;
                                    widgetSelection--;
                                    Serial.printf("[HOME] Widget UP: %d -> %d\n", oldWidget, widgetSelection);
                                    refreshWidgetSelection(oldWidget, widgetSelection);
                                }
                            } else {
                                // Portrait: UP from Orient(2) → Weather(1)
                                // Weather and Book can't go UP
                                if (widgetSelection == numWidgets - 1 && numWidgets >= 2) {
                                    // From Orient → Weather (middle widget)
                                    int oldWidget = widgetSelection;
                                    widgetSelection = 1;  // Weather is index 1
                                    Serial.printf("[HOME] Portrait Orient UP → Weather: %d -> %d\n", oldWidget, widgetSelection);
                                    refreshWidgetSelection(oldWidget, widgetSelection);
                                }
                            }
                            break;
                        case BTN_DOWN:
                            if (isLandscape) {
                                // Landscape: DOWN exits to grid
                                Serial.println("[HOME] Widget DOWN: exit to grid");
                                widgetSelection = -1;
                                showHomeScreenPartialFast();
                            } else {
                                // Portrait: DOWN exits to grid
                                // From Book → exit to grid left column
                                // From Orient → exit to grid right column  
                                // From Weather → go to Orient
                                if (widgetSelection == 1 && numWidgets >= 3) {
                                    // Weather → Orient
                                    int oldWidget = widgetSelection;
                                    widgetSelection = numWidgets - 1;
                                    Serial.printf("[HOME] Portrait Weather DOWN → Orient: %d -> %d\n", oldWidget, widgetSelection);
                                    refreshWidgetSelection(oldWidget, widgetSelection);
                                } else {
                                    // Book or Orient → exit to grid
                                    int exitCol = (widgetSelection == 0) ? 0 : 1;
                                    Serial.printf("[HOME] Portrait widget %d DOWN: exit to grid col %d\n", widgetSelection, exitCol);
                                    widgetSelection = -1;
                                    homeSelection = exitCol;  // Set to appropriate column
                                    showHomeScreenPartialFast();
                                }
                            }
                            break;
                        case BTN_LEFT:
                            if (isLandscape && widgetSelection > 0) {
                                // Landscape: LEFT moves between side-by-side widgets
                                int oldWidget = widgetSelection;
                                widgetSelection--;
                                Serial.printf("[HOME] Widget LEFT: %d -> %d\n", oldWidget, widgetSelection);
                                refreshWidgetSelection(oldWidget, widgetSelection);
                            } else if (!isLandscape) {
                                // Portrait: LEFT from Orient/Weather → Book
                                if (widgetSelection > 0 && numWidgets >= 2) {
                                    int oldWidget = widgetSelection;
                                    widgetSelection = 0;  // Go to Book
                                    Serial.printf("[HOME] Portrait LEFT → Book: %d -> %d\n", oldWidget, widgetSelection);
                                    refreshWidgetSelection(oldWidget, widgetSelection);
                                }
                            }
                            break;
                        case BTN_RIGHT:
                            if (isLandscape && widgetSelection < numWidgets - 1) {
                                // Landscape: RIGHT moves between side-by-side widgets  
                                int oldWidget = widgetSelection;
                                widgetSelection++;
                                Serial.printf("[HOME] Widget RIGHT: %d -> %d\n", oldWidget, widgetSelection);
                                refreshWidgetSelection(oldWidget, widgetSelection);
                            } else if (!isLandscape) {
                                // Portrait: RIGHT from Book → Orient (skip Weather)
                                if (widgetSelection == 0 && numWidgets >= 2) {
                                    int oldWidget = widgetSelection;
                                    widgetSelection = numWidgets - 1;  // Go to Orient (last)
                                    Serial.printf("[HOME] Portrait Book RIGHT → Orient: %d -> %d\n", oldWidget, widgetSelection);
                                    refreshWidgetSelection(oldWidget, widgetSelection);
                                }
                            }
                            break;
                        case BTN_CONFIRM:
                            activateWidget(widgetSelection);
                            break;
                        case BTN_BACK:
                            widgetSelection = -1;
                            showHomeScreenPartialFast();
                            break;
                        default:
                            break;
                    }
                } else {
                    // Grid navigation
                    switch (btn) {
                        case BTN_UP:
                            if (homeSelection >= homeCols) {
                                int oldSel = homeSelection;
                                homeSelection -= homeCols;
                                refreshChangedCells(oldSel, homeSelection);
                            } else if (numWidgets > 0 && homePageIndex == 0) {
                                // At top row of grid, go to widgets
                                bool isLandscape = (settingsManager.display.orientation == 0);
                                
                                if (isLandscape) {
                                    // Landscape: widgets stacked, enter at bottom (orientation)
                                    widgetSelection = numWidgets - 1;
                                } else {
                                    // Portrait: widgets side by side
                                    // Left column (col 0) → Book widget (index 0)
                                    // Right column (col 1) → Orientation widget (last index)
                                    int col = homeSelection % homeCols;
                                    if (col == 0) {
                                        // Coming from left column - go to Book (if shown) or first widget
                                        widgetSelection = 0;
                                    } else {
                                        // Coming from right column - go to Orientation (last widget)
                                        widgetSelection = numWidgets - 1;
                                    }
                                    Serial.printf("[HOME] Portrait UP from col %d -> widget %d\n", col, widgetSelection);
                                }
                                showHomeScreenPartialFast();
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
                                widgetSelection = -1;  // Reset when leaving page 0
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
                                widgetSelection = -1;  // Reset when leaving page 0
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
    }
    
    lastButton = btn;
}
