/**
 * @file ReaderSettings.cpp
 * @brief Reader Settings Implementation with Portal Sync
 * 
 * VERSION 2.1.6: Now syncs with SettingsManager for web portal
 * 
 * Version: 2.1.16
 */

#include <SD.h>
#include "core/ReaderSettings.h"
#include "core/TextLayout.h"
#include "core/SettingsManager.h"

// Settings file path
const char* LibReaderSettingsManager::SETTINGS_PATH = "/.sumi/reader.bin";

// Global instance
LibReaderSettingsManager readerSettings;

// =============================================================================
// Load settings from SD AND sync with portal
// =============================================================================
bool LibReaderSettingsManager::load() {
    File file = SD.open(SETTINGS_PATH, FILE_READ);
    if (!file) {
        Serial.println("[READER] No settings file, syncing from portal");
        _settings = LibReaderSettings();
        syncFromPortal();  // Get initial values from portal
        _loaded = true;
        logSettings("after default init");
        return false;
    }
    
    size_t read = file.read((uint8_t*)&_settings, sizeof(LibReaderSettings));
    file.close();
    
    if (read != sizeof(LibReaderSettings) || !_settings.isValid()) {
        Serial.println("[READER] Invalid settings, using defaults + portal sync");
        _settings = LibReaderSettings();
        syncFromPortal();
    } else {
        Serial.println("[READER] Settings loaded from SD");
        // Also sync from portal in case user changed settings via web
        syncFromPortal();
    }
    
    _loaded = true;
    logSettings("after load");
    return true;
}

void LibReaderSettingsManager::logSettings(const char* context) {
    Serial.printf("[READER] Settings (%s):\n", context);
    Serial.printf("[READER]   fontSize=%d, margins=%d, lineSpacing=%d\n",
                  (int)_settings.fontSize, (int)_settings.margins, (int)_settings.lineSpacing);
    Serial.printf("[READER]   justify=%d, showProgress=%d, showChapterName=%d, showBattery=%d\n",
                  _settings.justifyText, _settings.showProgress, _settings.showChapterName, _settings.showBattery);
    Serial.printf("[READER]   marginPx=%d, lineHeight=%d, fontHeight=%d\n",
                  _settings.getMarginPx(), _settings.getLineHeight(), _settings.getFontHeight());
}

// =============================================================================
// Save settings to SD AND sync to portal
// =============================================================================
bool LibReaderSettingsManager::save() {
    // Ensure directory exists
    if (!SD.exists("/.sumi")) {
        SD.mkdir("/.sumi");
    }
    
    File file = SD.open(SETTINGS_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("[READER] Failed to save settings");
        return false;
    }
    
    size_t written = file.write((uint8_t*)&_settings, sizeof(LibReaderSettings));
    file.close();
    
    if (written != sizeof(LibReaderSettings)) {
        Serial.println("[READER] Settings write incomplete");
        return false;
    }
    
    // Sync to portal so web UI shows current values
    syncToPortal();
    settingsManager.save();  // Persist portal settings too
    
    Serial.println("[READER] Settings saved and synced to portal");
    return true;
}

// =============================================================================
// Sync from portal settings (portal → LibReaderSettings)
// =============================================================================
void LibReaderSettingsManager::syncFromPortal() {
    Serial.println("[READER] Syncing from portal settings...");
    
    // Map portal fontSize (int) to LibReaderSettings::FontSize
    // Portal uses actual point sizes: 14, 16, 18, 20, 22, etc.
    int portalFontSize = settingsManager.reader.fontSize;
    if (portalFontSize <= 14) {
        _settings.fontSize = FontSize::SMALL;
    } else if (portalFontSize <= 18) {
        _settings.fontSize = FontSize::MEDIUM;
    } else {
        _settings.fontSize = FontSize::LARGE;
    }
    
    // Map portal margins (int px) to MarginSize enum
    int portalMargins = settingsManager.reader.margins;
    if (portalMargins <= 12) {
        _settings.margins = MarginSize::NARROW;
    } else if (portalMargins <= 25) {
        _settings.margins = MarginSize::NORMAL;
    } else {
        _settings.margins = MarginSize::WIDE;
    }
    
    // Map portal lineHeight (percentage 100-200) to LineSpacing
    int portalLineHeight = settingsManager.reader.lineHeight;
    if (portalLineHeight <= 130) {
        _settings.lineSpacing = LineSpacing::COMPACT;
    } else if (portalLineHeight <= 160) {
        _settings.lineSpacing = LineSpacing::NORMAL;
    } else {
        _settings.lineSpacing = LineSpacing::RELAXED;
    }
    
    // Justify: portal textAlign 1 = justify
    _settings.justifyText = (settingsManager.reader.textAlign == 1);
    
    // Status bar settings
    _settings.showProgress = settingsManager.reader.showProgress;
    _settings.showChapterName = settingsManager.reader.showChapter;
    
    // Refresh settings from display
    _settings.pagesPerFullRefresh = settingsManager.display.fullRefreshPages;
    _settings.pagesPerHalfRefresh = settingsManager.display.fullRefreshPages / 3;
    if (_settings.pagesPerHalfRefresh < 5) _settings.pagesPerHalfRefresh = 5;
    
    Serial.printf("[READER] Synced: font=%d, margins=%d, spacing=%d, justify=%d\n",
                  (int)_settings.fontSize, (int)_settings.margins, 
                  (int)_settings.lineSpacing, _settings.justifyText);
}

// =============================================================================
// Sync to portal settings (LibReaderSettings → portal)
// =============================================================================
void LibReaderSettingsManager::syncToPortal() {
    Serial.println("[READER] Syncing to portal settings...");
    
    // Map FontSize enum to portal fontSize (point size)
    switch (_settings.fontSize) {
        case FontSize::SMALL:
            settingsManager.reader.fontSize = 14;
            break;
        case FontSize::MEDIUM:
            settingsManager.reader.fontSize = 18;
            break;
        case FontSize::LARGE:
            settingsManager.reader.fontSize = 22;
            break;
    }
    
    // Map MarginSize to portal margins (px)
    switch (_settings.margins) {
        case MarginSize::NARROW:
            settingsManager.reader.margins = 10;
            break;
        case MarginSize::NORMAL:
            settingsManager.reader.margins = 20;
            break;
        case MarginSize::WIDE:
            settingsManager.reader.margins = 35;
            break;
    }
    
    // Map LineSpacing to portal lineHeight (percentage)
    switch (_settings.lineSpacing) {
        case LineSpacing::COMPACT:
            settingsManager.reader.lineHeight = 120;
            break;
        case LineSpacing::NORMAL:
            settingsManager.reader.lineHeight = 150;
            break;
        case LineSpacing::RELAXED:
            settingsManager.reader.lineHeight = 180;
            break;
    }
    
    // Justify → textAlign
    settingsManager.reader.textAlign = _settings.justifyText ? 1 : 0;
    
    // Status bar settings
    settingsManager.reader.showProgress = _settings.showProgress;
    settingsManager.reader.showChapter = _settings.showChapterName;
}

// =============================================================================
// Apply settings to text layout engine
// =============================================================================
void LibReaderSettingsManager::applyToLayout(TextLayout& layout) {
    int marginPx = _settings.getMarginPx();
    int lineHeight = _settings.getLineHeight();
    int paraSpacing = _settings.getParaSpacing();
    
    // Calculate margins consistently with applyFontSettings in Library.h
    const int statusBarHeight = 35;
    int topMargin = marginPx + 5;
    int bottomMargin = statusBarHeight + marginPx;
    
    Serial.printf("[READER_SETTINGS] Applying to layout:\n");
    Serial.printf("[READER_SETTINGS]   marginPx=%d (setting=%d)\n", marginPx, (int)_settings.margins);
    Serial.printf("[READER_SETTINGS]   margins: L/R=%d, T=%d, B=%d\n", marginPx, topMargin, bottomMargin);
    Serial.printf("[READER_SETTINGS]   lineHeight=%d\n", lineHeight);
    Serial.printf("[READER_SETTINGS]   paraSpacing=%d\n", paraSpacing);
    Serial.printf("[READER_SETTINGS]   justify=%d\n", _settings.justifyText);
    Serial.printf("[READER_SETTINGS]   fontSize=%d\n", (int)_settings.fontSize);
    
    // setMargins(left, right, top, bottom)
    layout.setMargins(marginPx, marginPx, topMargin, bottomMargin);
    layout.setLineHeight(lineHeight);
    layout.setParaSpacing(paraSpacing);
    layout.setJustify(_settings.justifyText);
    
    // Font selection based on size
    extern const GFXfont FreeSans9pt7b;
    extern const GFXfont FreeSans12pt7b;
    
    switch (_settings.fontSize) {
        case FontSize::SMALL:
            layout.setFont(&FreeSans9pt7b);
            break;
        case FontSize::MEDIUM:
        case FontSize::LARGE:
        default:
            layout.setFont(&FreeSans12pt7b);
            break;
    }
}
