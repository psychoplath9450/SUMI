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
#if FEATURE_WEBSERVER
#include "core/WebServer.h"
#endif

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
    .lastUpdate = 0
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
    int maxItems = settingsGetItemCount();
    if (state.selection > 0) {
        state.selection--;
    }
}

void settingsDown() {
    state.prevSelection = state.selection;
    int maxItems = settingsGetItemCount();
    if (state.selection < maxItems - 1) {
        state.selection++;
    }
}

void settingsSelect() {
    switch (state.screen) {
        case SETTINGS_SCREEN_MAIN:
            switch (state.selection) {
                case MAIN_CONNECT_WIFI:
                    // Open WiFi screen immediately
                    state.screen = SETTINGS_SCREEN_WIFI;
                    state.selection = 0;
                    state.needsFullRefresh = true;
                    
                    // Start connecting if credentials available
                    if (wifiManager.hasCredentials() && !wifiManager.isConnected()) {
                        Serial.println("[SETTINGS] Connecting to saved WiFi...");
                        state.wifiConnecting = true;
                        wifiManager.connectSaved();
                    }
                    break;
                    
                case MAIN_OPEN_PORTAL:
                    // Open Portal screen and start AP immediately
                    state.screen = SETTINGS_SCREEN_PORTAL;
                    state.selection = 0;
                    state.needsFullRefresh = true;
                    
                    if (!wifiManager.isAPMode()) {
                        Serial.println("[SETTINGS] Starting Portal...");
                        wifiManager.startAP();
                        #if FEATURE_WEBSERVER
                        extern SumiWebServer webServer;
                        webServer.begin();
                        extern bool portal_active;
                        portal_active = true;
                        #endif
                        state.portalJustStarted = true;
                    }
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
            // On Portal screen, SELECT toggles portal
            if (wifiManager.isAPMode()) {
                Serial.println("[SETTINGS] Stopping Portal...");
                #if FEATURE_WEBSERVER
                extern SumiWebServer webServer;
                webServer.stop();
                extern bool portal_active;
                portal_active = false;
                #endif
                wifiManager.stopAP();
                // Also disconnect STA and turn WiFi off completely
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.disconnect();
                }
                WiFi.mode(WIFI_OFF);
                Serial.println("[SETTINGS] Portal and WiFi fully stopped");
            } else {
                Serial.println("[SETTINGS] Starting Portal...");
                wifiManager.startAP();
                #if FEATURE_WEBSERVER
                extern SumiWebServer webServer;
                webServer.begin();
                extern bool portal_active;
                portal_active = true;
                #endif
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
        case MAIN_CONNECT_WIFI:
            if (wifiManager.isConnected()) {
                return "WiFi: " + wifiManager.getSSID();
            } else if (wifiManager.hasCredentials()) {
                return "Connect to WiFi";
            } else {
                return "WiFi (no saved network)";
            }
            
        case MAIN_OPEN_PORTAL:
            if (wifiManager.isAPMode()) {
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
    extern bool portal_active;
    portal_active = false;
    #endif
    
    wifiManager.stopAP();
    
    // Signal to exit settings
    state.shouldExit = true;
    state.needsFullRefresh = true;
}
