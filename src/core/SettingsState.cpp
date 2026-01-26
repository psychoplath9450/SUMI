/**
 * @file Settings.cpp
 * @brief Optimized on-device settings with partial refresh
 * 
 * Features:
 * - Full-screen menu layout
 * - Partial refresh for smooth navigation (no black flash)
 * - Simplified WiFi/Portal flow
 * - Live status indicators
 */

#include "core/SettingsState.h"
#include "core/SettingsManager.h"
#include "core/WiFiManager.h"
#include "core/ZipReader.h"
#if FEATURE_WEBSERVER
#include "core/WebServer.h"
#endif

// External portal state
extern bool portal_active;

// =============================================================================
// State
// =============================================================================
static SettingsState state = {
    .screen = SETTINGS_SCREEN_MAIN,
    .selection = 0,
    .prevSelection = 0,
    .needsFullRefresh = true,
    .portalJustStarted = false,
    .wifiConnecting = false,
    .shouldExit = false,
    .lastUpdate = 0,
    .portalModeSelection = 0
};

// =============================================================================
// Initialization
// =============================================================================
void settingsInit() {
    state.screen = SETTINGS_SCREEN_MAIN;
    state.selection = 0;
    state.prevSelection = 0;
    state.needsFullRefresh = true;
    state.portalJustStarted = false;
    state.wifiConnecting = false;
    state.shouldExit = false;
    state.lastUpdate = millis();
}

SettingsState& settingsGetState() {
    return state;
}

// =============================================================================
// Navigation
// =============================================================================
void settingsUp() {
    state.prevSelection = state.selection;
    
    // Special handling for portal mode selection
    if (state.screen == SETTINGS_SCREEN_PORTAL && !portal_active) {
        if (state.portalModeSelection > 0) {
            state.portalModeSelection--;
            state.needsFullRefresh = true;
        }
        return;
    }
    
    int maxItems = settingsGetItemCount();
    if (state.selection > 0) {
        state.selection--;
    }
}

void settingsDown() {
    state.prevSelection = state.selection;
    
    // Special handling for portal mode selection
    if (state.screen == SETTINGS_SCREEN_PORTAL && !portal_active) {
        if (state.portalModeSelection < PORTAL_MODE_COUNT - 1) {
            state.portalModeSelection++;
            state.needsFullRefresh = true;
        }
        return;
    }
    
    int maxItems = settingsGetItemCount();
    if (state.selection < maxItems - 1) {
        state.selection++;
    }
}

void settingsSelect() {
    switch (state.screen) {
        case SETTINGS_SCREEN_MAIN:
            switch (state.selection) {
                case MAIN_OPEN_PORTAL:
                    // Open Portal screen to show connection options
                    state.screen = SETTINGS_SCREEN_PORTAL;
                    state.selection = 0;
                    state.portalModeSelection = 0;  // Default to hotspot
                    state.needsFullRefresh = true;
                    break;
                    
                case MAIN_DISPLAY:
                    state.screen = SETTINGS_SCREEN_DISPLAY;
                    state.selection = 0;
                    state.needsFullRefresh = true;
                    break;
                    
                case MAIN_ABOUT:
                    state.screen = SETTINGS_SCREEN_ABOUT;
                    state.selection = 0;
                    state.needsFullRefresh = true;
                    break;
                    
                case MAIN_REBOOT:
                    Serial.println("[SETTINGS] Rebooting...");
                    delay(500);
                    ESP.restart();
                    break;
            }
            break;
            
        case SETTINGS_SCREEN_WIFI:
            // On WiFi screen, SELECT toggles connection
            if (wifiManager.isConnected()) {
                Serial.println("[SETTINGS] Disconnecting WiFi...");
                wifiManager.disconnect();
            } else if (wifiManager.hasCredentials()) {
                Serial.println("[SETTINGS] Connecting to saved WiFi...");
                state.wifiConnecting = true;
                wifiManager.connectSaved();
            }
            state.needsFullRefresh = true;
            break;
            
        case SETTINGS_SCREEN_PORTAL:
            if (portal_active) {
                // Portal is running - SELECT stops it
                Serial.println("[SETTINGS] Stopping Portal...");
                #if FEATURE_WEBSERVER
                extern SumiWebServer webServer;
                webServer.stop();
                portal_active = false;
                #endif
                wifiManager.stopAP();
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.disconnect();
                }
                WiFi.mode(WIFI_OFF);
                
                // Reallocate ZIP buffers now that portal is closed
                Serial.println("[SETTINGS] Reallocating ZIP buffers...");
                ZipReader_preallocateBuffer();
                Serial.println("[SETTINGS] Portal and WiFi fully stopped");
            } else {
                // Portal not running - start based on selection
                Serial.println("[SETTINGS] Starting Portal...");
                
                // Free ZIP buffers to reclaim ~43KB for portal
                Serial.println("[SETTINGS] Freeing ZIP buffers for portal...");
                ZipReader_freeBuffers();
                
                if (state.portalModeSelection == PORTAL_MODE_HOTSPOT) {
                    // Start in hotspot mode
                    Serial.println("[SETTINGS] Using Hotspot mode");
                    wifiManager.startAP();
                } else {
                    // Start in home WiFi mode (STA+AP or just STA with portal)
                    Serial.println("[SETTINGS] Using Home WiFi mode");
                    if (wifiManager.hasCredentials()) {
                        // Connect to saved WiFi first
                        wifiManager.connectSaved();
                        // Wait for connection (with timeout)
                        int timeout = 50; // 5 seconds
                        while (!wifiManager.isConnected() && timeout > 0) {
                            delay(100);
                            timeout--;
                        }
                        if (wifiManager.isConnected()) {
                            Serial.printf("[SETTINGS] Connected to %s\n", wifiManager.getSSID().c_str());
                        } else {
                            Serial.println("[SETTINGS] Failed to connect, falling back to hotspot");
                            wifiManager.startAP();
                        }
                    } else {
                        // No credentials, fall back to hotspot
                        Serial.println("[SETTINGS] No saved WiFi, using hotspot");
                        wifiManager.startAP();
                    }
                }
                
                #if FEATURE_WEBSERVER
                extern SumiWebServer webServer;
                webServer.begin();
                portal_active = true;
                #endif
                state.portalJustStarted = true;
            }
            state.needsFullRefresh = true;
            break;
            
        case SETTINGS_SCREEN_DISPLAY:
            switch (state.selection) {
                case DISP_ORIENTATION:
                    // Toggle orientation
                    settingsManager.display.orientation = 
                        settingsManager.display.orientation == 1 ? 0 : 1;
                    settingsManager.display.rotation = 
                        settingsManager.display.orientation == 1 ? 0 : 1;
                    settingsManager.save();
                    Serial.printf("[SETTINGS] Orientation: %s (reboot required)\n",
                        settingsManager.display.orientation == 1 ? "Portrait" : "Landscape");
                    break;
                    
                case DISP_DARK_MODE:
                    settingsManager.display.invertColors = !settingsManager.display.invertColors;
                    settingsManager.save();
                    break;
                    
                case DISP_SHOW_CLOCK:
                    settingsManager.display.showClockHome = !settingsManager.display.showClockHome;
                    settingsManager.save();
                    break;
                    
                case DISP_SHOW_BATTERY:
                    settingsManager.display.showBatteryHome = !settingsManager.display.showBatteryHome;
                    settingsManager.save();
                    break;
                    
                case DISP_SLEEP_TIMEOUT:
                    {
                        // Cycle through sleep times
                        uint8_t times[] = {5, 10, 15, 30, 60};
                        int current = 0;
                        for (int i = 0; i < 5; i++) {
                            if (settingsManager.display.sleepMinutes == times[i]) {
                                current = i;
                                break;
                            }
                        }
                        current = (current + 1) % 5;
                        settingsManager.display.sleepMinutes = times[current];
                        settingsManager.save();
                    }
                    break;
                    
                case DISP_SLEEP_SCREEN:
                    // Cycle through: 0=Book Cover, 1=Shuffle Images, 2=Wake Me Up
                    settingsManager.display.sleepStyle = (settingsManager.display.sleepStyle + 1) % 3;
                    settingsManager.save();
                    break;
                    
                case DISP_BACK:
                    state.screen = SETTINGS_SCREEN_MAIN;
                    state.selection = MAIN_DISPLAY;
                    state.needsFullRefresh = true;
                    return;
            }
            // Partial refresh for toggle changes
            state.needsFullRefresh = true; // For now, full refresh on changes
            break;
            
        case SETTINGS_SCREEN_ABOUT:
            // Nothing to select on about screen
            break;
    }
}

void settingsBack() {
    Serial.printf("[SETTINGS] Back pressed, screen=%d\n", state.screen);
    
    switch (state.screen) {
        case SETTINGS_SCREEN_MAIN:
            // Exit settings entirely
            Serial.println("[SETTINGS] Exiting settings");
            state.shouldExit = true;
            break;
            
        case SETTINGS_SCREEN_WIFI:
        case SETTINGS_SCREEN_PORTAL:
        case SETTINGS_SCREEN_DISPLAY:
        case SETTINGS_SCREEN_ABOUT:
            // Return to main menu
            Serial.println("[SETTINGS] Returning to main menu");
            state.screen = SETTINGS_SCREEN_MAIN;
            state.selection = 0;
            state.needsFullRefresh = true;
            break;
            
        default:
            // Unknown screen - force exit
            Serial.printf("[SETTINGS] Unknown screen %d, forcing exit\n", state.screen);
            state.shouldExit = true;
            break;
    }
}

bool settingsShouldExit() {
    if (state.shouldExit) {
        Serial.println("[SETTINGS] shouldExit=true, exiting");
        state.shouldExit = false;
        return true;
    }
    return false;
}

// =============================================================================
// Labels with Current Values
// =============================================================================
String settingsGetMainLabel(int index) {
    switch (index) {
        case MAIN_OPEN_PORTAL:
            if (portal_active) {
                return "Portal Active";
            }
            return "Open Portal";
            
        case MAIN_DISPLAY:
            return "Display Settings";
            
        case MAIN_ABOUT:
            return "About";
            
        case MAIN_REBOOT:
            return "Reboot Device";
            
        default:
            return "";
    }
}

String settingsGetDisplayLabel(int index) {
    switch (index) {
        case DISP_ORIENTATION:
            return String("Orientation: ") + 
                (settingsManager.display.orientation == 1 ? "Portrait" : "Landscape");
            
        case DISP_DARK_MODE:
            return String("Dark Mode: ") + 
                (settingsManager.display.invertColors ? "ON" : "OFF");
            
        case DISP_SHOW_CLOCK:
            return String("Show Clock: ") + 
                (settingsManager.display.showClockHome ? "ON" : "OFF");
            
        case DISP_SHOW_BATTERY:
            return String("Show Battery: ") + 
                (settingsManager.display.showBatteryHome ? "ON" : "OFF");
            
        case DISP_SLEEP_TIMEOUT:
            return String("Auto-Sleep: ") + 
                String(settingsManager.display.sleepMinutes) + " min";
            
        case DISP_SLEEP_SCREEN:
            {
                const char* styles[] = {"Book Cover", "Shuffle", "Wake Me Up"};
                int style = settingsManager.display.sleepStyle;
                if (style > 2) style = 0;
                return String("Sleep Screen: ") + styles[style];
            }
            
        case DISP_BACK:
            return "< Back";
            
        default:
            return "";
    }
}

int settingsGetItemCount() {
    switch (state.screen) {
        case SETTINGS_SCREEN_MAIN:
            return MAIN_ITEM_COUNT;
        case SETTINGS_SCREEN_DISPLAY:
            return DISP_ITEM_COUNT;
        case SETTINGS_SCREEN_WIFI:
        case SETTINGS_SCREEN_PORTAL:
        case SETTINGS_SCREEN_ABOUT:
            return 1;  // Single item screens
        default:
            return 0;
    }
}

// =============================================================================
// Status Helpers
// =============================================================================
bool settingsIsPortalActive() {
    return wifiManager.isAPMode();
}

bool settingsIsWiFiConnected() {
    return wifiManager.isConnected();
}

String settingsGetPortalIP() {
    if (wifiManager.isAPMode()) {
        return wifiManager.getAPIP();
    }
    return "192.168.4.1";
}

String settingsGetWiFiSSID() {
    return wifiManager.getSSID();
}

// =============================================================================
// Portal Deploy Handler
// =============================================================================
void settingsOnDeployed() {
    // Called when settings are deployed from portal
    // Stop portal and return to home
    Serial.println("[SETTINGS] Deployed from portal, shutting down...");
    
    #if FEATURE_WEBSERVER
    extern SumiWebServer webServer;
    webServer.stop();
    portal_active = false;
    #endif
    
    wifiManager.stopAP();
    
    // Signal to exit settings
    state.shouldExit = true;
    state.needsFullRefresh = true;
}
