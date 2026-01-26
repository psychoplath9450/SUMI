#ifndef SUMI_SETTINGS_STATE_H
#define SUMI_SETTINGS_STATE_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Settings Screen Types
// =============================================================================
enum SettingsScreen {
    SETTINGS_SCREEN_MAIN = 0,      // Main menu
    SETTINGS_SCREEN_WIFI,          // WiFi connection screen
    SETTINGS_SCREEN_PORTAL,        // Portal/Hotspot screen with QR code
    SETTINGS_SCREEN_DISPLAY,       // Display options
    SETTINGS_SCREEN_ABOUT          // About/system info
};

// Main menu items
enum SettingsMainItem {
    MAIN_OPEN_PORTAL = 0,     // "Open Portal" - shows connection options + starts portal
    MAIN_DISPLAY,             // "Display" - opens display submenu
    MAIN_ABOUT,               // "About" - system info
    MAIN_REBOOT,              // "Reboot Device"
    MAIN_ITEM_COUNT
};

// Display submenu items
enum SettingsDisplayItem {
    DISP_ORIENTATION = 0,     // Portrait/Landscape toggle
    DISP_DARK_MODE,           // Invert colors toggle
    DISP_SHOW_CLOCK,          // Show clock on home toggle
    DISP_SHOW_BATTERY,        // Show battery on home toggle
    DISP_SLEEP_TIMEOUT,       // Auto-sleep time (5/10/15/30/60 min)
    DISP_SLEEP_SCREEN,        // Sleep screen style (Default/Images/Covers)
    DISP_BACK,                // Back to main menu
    DISP_ITEM_COUNT
};

// Portal mode options
enum PortalMode {
    PORTAL_MODE_HOTSPOT = 0,   // Create SUMI hotspot
    PORTAL_MODE_HOME_WIFI,     // Use existing WiFi
    PORTAL_MODE_COUNT
};

// =============================================================================
// Settings State
// =============================================================================
struct SettingsState {
    SettingsScreen screen;
    int selection;              // Current selection index
    int prevSelection;          // Previous selection (for partial refresh)
    bool needsFullRefresh;      // Force full refresh (e.g., screen change)
    bool portalJustStarted;     // Flag for portal startup
    bool wifiConnecting;        // WiFi connection in progress
    bool shouldExit;            // Exit back to home
    unsigned long lastUpdate;   // For connection status updates
    int portalModeSelection;    // 0 = hotspot, 1 = home wifi
};

// =============================================================================
// Public API
// =============================================================================

// Initialize settings
void settingsInit();

// Get current state
SettingsState& settingsGetState();

// Navigation
void settingsUp();
void settingsDown();
void settingsSelect();
void settingsBack();

// Exit settings state check
bool settingsShouldExit();

// Get menu item labels with current values
String settingsGetMainLabel(int index);
String settingsGetDisplayLabel(int index);

// Get item count for current screen
int settingsGetItemCount();

// Portal/WiFi specific
bool settingsIsPortalActive();
bool settingsIsWiFiConnected();
String settingsGetPortalIP();
String settingsGetWiFiSSID();

// Called when settings deployed from portal
void settingsOnDeployed();

#endif // SUMI_SETTINGS_STATE_H
