/**
 * @file SettingsScreen.cpp
 * @brief Settings menu UI implementation
 * @version 1.3.0
 */

#include "core/SettingsScreen.h"
#include "core/WiFiManager.h"
#include "core/BatteryMonitor.h"
#include "core/SettingsState.h"

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern bool sd_card_present;
extern bool portal_active;

// Global layout instance
SettingsLayout settingsLayout;

// =============================================================================
// Layout Helpers
// =============================================================================
int getSettingsItemY(int index) {
    return settingsLayout.startY + index * (settingsLayout.itemHeight + settingsLayout.itemSpacing);
}

// =============================================================================
// Drawing Functions
// =============================================================================
void drawSettingsItem(int index, bool selected, const String& label, bool partialOnly) {
    int w = display.width();
    int y = getSettingsItemY(index);
    int itemW = w - settingsLayout.margin * 2;
    
    if (selected) {
        display.fillRoundRect(settingsLayout.margin, y, itemW, settingsLayout.itemHeight, 10, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
    } else {
        display.fillRoundRect(settingsLayout.margin, y, itemW, settingsLayout.itemHeight, 10, GxEPD_WHITE);
        display.drawRoundRect(settingsLayout.margin, y, itemW, settingsLayout.itemHeight, 10, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
    }
    
    display.setFont(&FreeSansBold12pt7b);
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(label.c_str(), 0, 0, &tx, &ty, &tw, &th);
    int textY = y + (settingsLayout.itemHeight + th) / 2;
    display.setCursor(settingsLayout.margin + 20, textY);
    display.print(label);
    display.setTextColor(GxEPD_BLACK);
}

void drawSettingsHeader(const char* title) {
    int w = display.width();
    display.fillRect(0, 0, w, settingsLayout.headerHeight, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((w - tw) / 2, 48);
    display.print(title);
    display.setTextColor(GxEPD_BLACK);
}

void drawSettingsFooter() {
    int w = display.width();
    int h = display.height();
    
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Draw separator line
    display.drawLine(settingsLayout.margin, h - settingsLayout.footerHeight, 
                     w - settingsLayout.margin, h - settingsLayout.footerHeight, GxEPD_BLACK);
    
    display.setCursor(settingsLayout.margin, h - 18);
    display.print("UP/DOWN: Navigate   OK: Select   BACK: Return");
}

// =============================================================================
// Partial Refresh for Selection
// =============================================================================
void settingsRefreshSelection(int oldSel, int newSel, int itemCount) {
    SettingsState& state = settingsGetState();
    int w = display.width();
    
    // Calculate area to refresh (both old and new selection)
    int minIdx = min(oldSel, newSel);
    int maxIdx = max(oldSel, newSel);
    
    int y1 = getSettingsItemY(minIdx);
    int y2 = getSettingsItemY(maxIdx) + settingsLayout.itemHeight + settingsLayout.itemSpacing;
    
    display.setPartialWindow(0, y1 - 5, w, y2 - y1 + 10);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Redraw affected items
        for (int i = minIdx; i <= maxIdx && i < itemCount; i++) {
            bool selected = (i == newSel);
            String label;
            
            if (state.screen == SETTINGS_SCREEN_MAIN) {
                label = settingsGetMainLabel(i);
            } else if (state.screen == SETTINGS_SCREEN_DISPLAY) {
                label = settingsGetDisplayLabel(i);
            }
            
            drawSettingsItem(i, selected, label);
        }
    } while (display.nextPage());
}

// =============================================================================
// Sub-Screens
// =============================================================================
void drawWiFiScreen() {
    int w = display.width();
    int h = display.height();
    int centerX = w / 2;
    
    display.fillScreen(GxEPD_WHITE);
    drawSettingsHeader("WiFi");
    
    int y = 120;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    if (wifiManager.isConnected()) {
        // Connected state
        display.setCursor(centerX - 80, y);
        display.print("Connected to:");
        y += 40;
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(centerX - 100, y);
        display.print(wifiManager.getSSID());
        y += 50;
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(centerX - 60, y);
        display.print("IP: ");
        display.print(wifiManager.getIP());
        
        y += 80;
        display.fillRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 60, 10, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(centerX - 60, y + 38);
        display.print("Disconnect");
    } else if (wifiManager.hasCredentials()) {
        // Has credentials but not connected
        display.setCursor(centerX - 100, y);
        display.print("Saved Network:");
        y += 40;
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(centerX - 80, y);
        display.print(wifiManager.getSavedSSID());
        
        y += 80;
        display.fillRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 60, 10, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(centerX - 50, y + 38);
        display.print("Connect");
    } else {
        // No credentials
        display.setCursor(centerX - 120, y);
        display.print("No WiFi configured");
        y += 50;
        display.setFont(&FreeSans9pt7b);
        display.setCursor(centerX - 140, y);
        display.print("Use Portal to add WiFi network");
    }
    
    drawSettingsFooter();
}

void drawPortalScreen() {
    int w = display.width();
    int h = display.height();
    int centerX = w / 2;
    SettingsState& state = settingsGetState();
    
    display.fillScreen(GxEPD_WHITE);
    drawSettingsHeader("Open Portal");
    
    int y = 100;
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    if (portal_active) {
        // Portal is active - show connection info
        display.setCursor(centerX - 80, y);
        display.print("Portal Active!");
        y += 50;
        
        // Show connection mode
        display.setFont(&FreeSans9pt7b);
        if (wifiManager.isConnected()) {
            // Home WiFi mode
            display.setCursor(settingsLayout.margin, y);
            display.print("Connected via Home WiFi:");
            y += 30;
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(settingsLayout.margin + 20, y);
            display.print(wifiManager.getSSID());
            y += 40;
            display.setFont(&FreeSans9pt7b);
            display.setCursor(settingsLayout.margin, y);
            display.print("Open browser to:");
            y += 30;
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(settingsLayout.margin + 20, y);
            display.print("http://");
            display.print(wifiManager.getIP());
            y += 25;
            display.setFont(&FreeSans9pt7b);
            display.setCursor(settingsLayout.margin + 20, y);
            display.print("or http://sumi.local");
        } else {
            // Hotspot mode
            display.setCursor(settingsLayout.margin, y);
            display.print("1. Connect your phone to WiFi:");
            y += 30;
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(settingsLayout.margin + 20, y);
            display.print(wifiManager.getAPName());
            y += 45;
            display.setFont(&FreeSans9pt7b);
            display.setCursor(settingsLayout.margin, y);
            display.print("2. Open browser to:");
            y += 30;
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(settingsLayout.margin + 20, y);
            display.print("http://sumi.local");
            y += 25;
            display.setFont(&FreeSans9pt7b);
            display.setCursor(settingsLayout.margin + 20, y);
            display.print("or http://");
            display.print(settingsGetPortalIP());
        }
        
        // Stop button
        y += 70;
        display.fillRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 55, 10, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(centerX - 60, y + 36);
        display.print("Stop Portal");
    } else {
        // Portal not active - show mode selection
        display.setCursor(centerX - 120, y);
        display.print("Choose connection:");
        y += 50;
        
        // Option 1: Hotspot
        bool hotspotSelected = (state.portalModeSelection == PORTAL_MODE_HOTSPOT);
        if (hotspotSelected) {
            display.fillRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 80, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 80, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(settingsLayout.margin + 20, y + 35);
        display.print("Create Hotspot");
        display.setFont(&FreeSans9pt7b);
        display.setCursor(settingsLayout.margin + 20, y + 60);
        display.print("Phone connects to SUMI");
        
        y += 95;
        
        // Option 2: Home WiFi
        bool homeSelected = (state.portalModeSelection == PORTAL_MODE_HOME_WIFI);
        if (homeSelected) {
            display.fillRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 80, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(settingsLayout.margin, y, w - settingsLayout.margin*2, 80, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(settingsLayout.margin + 20, y + 35);
        display.print("Use Home WiFi");
        display.setFont(&FreeSans9pt7b);
        display.setCursor(settingsLayout.margin + 20, y + 60);
        if (wifiManager.hasCredentials()) {
            String ssidInfo = "Connect via " + wifiManager.getSavedSSID();
            display.print(ssidInfo.substring(0, 30));
        } else {
            display.print("No saved network");
        }
        
        y += 110;
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(centerX - 80, y);
        display.print("Press OK to start");
    }
    
    drawSettingsFooter();
}

void drawAboutScreen() {
    int w = display.width();
    int h = display.height();
    
    display.fillScreen(GxEPD_WHITE);
    drawSettingsHeader("About");
    
    int y = 110;
    int margin = settingsLayout.margin;
    
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(margin, y);
    display.print("Sumi E-Reader");
    y += 30;
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(margin, y);
    display.printf("Version: %s", SUMI_VERSION);
    y += 40;
    
    // System info
    display.drawLine(margin, y, w - margin, y, GxEPD_BLACK);
    y += 25;
    
    display.setCursor(margin, y);
    display.printf("Free Memory: %d KB", ESP.getFreeHeap() / 1024);
    y += 25;
    
    display.setCursor(margin, y);
    display.printf("Min Free: %d KB", ESP.getMinFreeHeap() / 1024);
    y += 25;
    
    display.setCursor(margin, y);
    display.printf("SD Card: %s", sd_card_present ? "Detected" : "Not Found");
    y += 25;
    
    display.setCursor(margin, y);
    display.printf("WiFi: %s", wifiManager.isConnected() ? wifiManager.getSSID().c_str() : "Not Connected");
    y += 25;
    
    display.setCursor(margin, y);
    display.printf("Portal: %s", portal_active ? "Active" : "Inactive");
    y += 40;
    
    // Battery
    display.drawLine(margin, y, w - margin, y, GxEPD_BLACK);
    y += 25;
    display.setCursor(margin, y);
    display.printf("Battery: %.2fV (%d%%)", batteryMonitor.getVoltage(), batteryMonitor.getPercent());
    
    drawSettingsFooter();
}

// =============================================================================
// Main Settings Screen
// =============================================================================
void showSettingsScreen() {
    SettingsState& state = settingsGetState();
    int w = display.width();
    int h = display.height();
    
    // Determine if partial refresh suffices for selection change
    if (!state.needsFullRefresh && state.prevSelection != state.selection) {
        int itemCount = settingsGetItemCount();
        settingsRefreshSelection(state.prevSelection, state.selection, itemCount);
        state.prevSelection = state.selection;
        return;
    }
    
    // Full screen refresh
    display.setFullWindow();
    display.firstPage();
    do {
        switch (state.screen) {
            case SETTINGS_SCREEN_MAIN:
                display.fillScreen(GxEPD_WHITE);
                drawSettingsHeader("Settings");
                for (int i = 0; i < MAIN_ITEM_COUNT; i++) {
                    drawSettingsItem(i, i == state.selection, settingsGetMainLabel(i));
                }
                drawSettingsFooter();
                break;
                
            case SETTINGS_SCREEN_WIFI:
                drawWiFiScreen();
                break;
                
            case SETTINGS_SCREEN_PORTAL:
                drawPortalScreen();
                break;
                
            case SETTINGS_SCREEN_DISPLAY:
                display.fillScreen(GxEPD_WHITE);
                drawSettingsHeader("Display");
                for (int i = 0; i < DISP_ITEM_COUNT; i++) {
                    drawSettingsItem(i, i == state.selection, settingsGetDisplayLabel(i));
                }
                drawSettingsFooter();
                break;
                
            case SETTINGS_SCREEN_ABOUT:
                drawAboutScreen();
                break;
        }
    } while (display.nextPage());
    
    state.needsFullRefresh = false;
    state.prevSelection = state.selection;
}

// =============================================================================
// App Placeholder
// =============================================================================
void showAppPlaceholder(const char* appName) {
    int w = display.width();
    int h = display.height();
    int centerX = w / 2;
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        display.fillRect(0, 0, w, 60, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(appName, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw / 2, 40);
        display.print(appName);
        display.setTextColor(GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(centerX - 80, h / 2);
        display.print("Feature not available");
        display.setCursor(centerX - 80, h - 50);
        display.print("Press BACK to return");
        
    } while (display.nextPage());
}
