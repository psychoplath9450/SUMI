#ifndef SUMI_SETTINGS_SCREEN_H
#define SUMI_SETTINGS_SCREEN_H

/**
 * @file SettingsScreen.h
 * @brief Settings menu UI and subscreens
 * @version 1.3.0
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"

// =============================================================================
// Settings Layout
// =============================================================================
struct SettingsLayout {
    int headerHeight = 70;
    int margin = 20;
    int itemHeight = 65;
    int itemSpacing = 8;
    int startY = 85;
    int footerHeight = 50;
};

extern SettingsLayout settingsLayout;

// =============================================================================
// Settings Screen Functions
// =============================================================================

// Get Y position for menu item at index
int getSettingsItemY(int index);

// Draw individual settings item
void drawSettingsItem(int index, bool selected, const String& label, bool partialOnly = false);

// Draw header with title
void drawSettingsHeader(const char* title);

// Draw footer with navigation hints
void drawSettingsFooter();

// Partial refresh for selection change
void settingsRefreshSelection(int oldSel, int newSel, int itemCount);

// Sub-screens
void drawWiFiScreen();
void drawPortalScreen();
void drawAboutScreen();

// Main settings draw function
void showSettingsScreen();

// Show placeholder screen for disabled features
void showAppPlaceholder(const char* appName);

#endif // SUMI_SETTINGS_SCREEN_H
