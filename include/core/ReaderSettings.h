/**
 * @file ReaderSettings.h
 * @brief Reader Settings Management for Sumi E-Reader
 * 
 * VERSION 2.1.6: Now connected to SettingsManager for portal sync
 * 
 * Features:
 * - Font size selection
 * - Margin adjustment
 * - Line spacing options
 * - Justify text toggle
 * - SYNCS WITH WEB PORTAL via SettingsManager
 * 
 * Version: 2.1.16
 */

#ifndef SUMI_READER_SETTINGS_H
#define SUMI_READER_SETTINGS_H

#include <Arduino.h>
#include <SD.h>

// Forward declaration
class SettingsManager;
extern SettingsManager settingsManager;

// =============================================================================
// Font Size Options
// =============================================================================
enum class FontSize : uint8_t {
    SMALL = 0,    // 9pt - More text per page
    MEDIUM = 1,   // 12pt - Default balance
    LARGE = 2     // 14pt - Easier reading
};

// =============================================================================
// Margin Options
// =============================================================================
enum class MarginSize : uint8_t {
    NARROW = 0,   // 10px - Maximum reading area
    NORMAL = 1,   // 20px - Default
    WIDE = 2      // 35px - More white space
};

// =============================================================================
// Line Spacing Options
// =============================================================================
enum class LineSpacing : uint8_t {
    COMPACT = 0,  // 1.2x - Dense text
    NORMAL = 1,   // 1.5x - Default
    RELAXED = 2   // 1.8x - Easy on eyes
};

// =============================================================================
// Refresh Mode (3 levels: fast, half, full)
// =============================================================================
enum class RefreshMode : uint8_t {
    FAST = 0,     // Quick partial (ghosting builds up)
    HALF = 1,     // Medium clean (clears ghosting, moderate speed)
    FULL = 2      // Complete clean (slow, no ghosting)
};

// =============================================================================
// Reader Settings Structure (for Library app)
// =============================================================================
struct LibReaderSettings {
    // Magic number for validation
    static const uint32_t MAGIC = 0x52534554;  // "RSET"
    static const uint8_t VERSION = 2;  // Bumped for portal sync
    
    uint32_t magic;
    uint8_t version;
    
    // Display settings
    FontSize fontSize;
    MarginSize margins;
    LineSpacing lineSpacing;
    bool justifyText;
    
    // Reading behavior
    bool showProgress;         // Show progress percentage
    bool showChapterName;      // Show chapter in status bar
    bool showBattery;          // Show battery in status bar
    uint8_t pagesPerFullRefresh;  // Full refresh interval
    uint8_t pagesPerHalfRefresh;  // Half refresh interval (clears ghosting)
    
    // Reserved for future use
    uint8_t reserved[14];
    
    // Initialize with defaults
    LibReaderSettings() {
        magic = MAGIC;
        version = VERSION;
        fontSize = FontSize::MEDIUM;
        margins = MarginSize::NORMAL;
        lineSpacing = LineSpacing::NORMAL;
        justifyText = true;  // Justified text by default
        showProgress = true;
        showChapterName = true;
        showBattery = true;
        pagesPerFullRefresh = 15;   // Full refresh every 15 pages
        pagesPerHalfRefresh = 5;    // Half refresh every 5 pages
        memset(reserved, 0, sizeof(reserved));
    }
    
    // Validate settings
    bool isValid() const {
        return magic == MAGIC && version <= VERSION;
    }
    
    // Get actual pixel values
    int getFontHeight() const {
        switch (fontSize) {
            case FontSize::SMALL:  return 18;
            case FontSize::MEDIUM: return 22;
            case FontSize::LARGE:  return 28;
            default: return 22;
        }
    }
    
    int getMarginPx() const {
        // Side margins - status bar handled separately
        switch (margins) {
            case MarginSize::NARROW: return 5;   // Minimal margins
            case MarginSize::NORMAL: return 10;  // Default
            case MarginSize::WIDE:   return 20;  // More white space
            default: return 10;
        }
    }
    
    int getLineHeight() const {
        int base = getFontHeight();
        switch (lineSpacing) {
            case LineSpacing::COMPACT: return base + 2;
            case LineSpacing::NORMAL:  return base + 6;
            case LineSpacing::RELAXED: return base + 10;
            default: return base + 6;
        }
    }
    
    int getParaSpacing() const {
        switch (lineSpacing) {
            case LineSpacing::COMPACT: return 4;
            case LineSpacing::NORMAL:  return 8;
            case LineSpacing::RELAXED: return 12;
            default: return 8;
        }
    }
    
    // Get display names
    static const char* getFontSizeName(FontSize size) {
        switch (size) {
            case FontSize::SMALL:  return "Small";
            case FontSize::MEDIUM: return "Medium";
            case FontSize::LARGE:  return "Large";
            default: return "Medium";
        }
    }
    
    static const char* getMarginName(MarginSize size) {
        switch (size) {
            case MarginSize::NARROW: return "Narrow";
            case MarginSize::NORMAL: return "Normal";
            case MarginSize::WIDE:   return "Wide";
            default: return "Normal";
        }
    }
    
    static const char* getSpacingName(LineSpacing spacing) {
        switch (spacing) {
            case LineSpacing::COMPACT: return "Compact";
            case LineSpacing::NORMAL:  return "Normal";
            case LineSpacing::RELAXED: return "Relaxed";
            default: return "Normal";
        }
    }
};

// =============================================================================
// Settings Manager - NOW SYNCS WITH WEB PORTAL
// =============================================================================
class LibReaderSettingsManager {
public:
    static const char* SETTINGS_PATH;
    
    LibReaderSettingsManager() : _loaded(false) {}
    
    // Load settings from SD AND sync with portal
    bool load();
    
    // Save settings to SD AND sync with portal
    bool save();
    
    // Sync from portal settings (call after portal changes)
    void syncFromPortal();
    
    // Sync to portal settings (call before portal reads)
    void syncToPortal();
    
    // Get current settings
    LibReaderSettings& get() {
        if (!_loaded) load();
        return _settings;
    }
    
    // Apply settings to text layout
    void applyToLayout(class TextLayout& layout);
    
    // Debug logging
    void logSettings(const char* context);
    
    // Check if settings require cache rebuild
    bool requiresCacheRebuild(const LibReaderSettings& oldSettings) const {
        return _settings.fontSize != oldSettings.fontSize ||
               _settings.margins != oldSettings.margins ||
               _settings.lineSpacing != oldSettings.lineSpacing ||
               _settings.justifyText != oldSettings.justifyText;
    }
    
    // Get refresh mode for current page
    RefreshMode getRefreshMode(int& pagesUntilHalf, int& pagesUntilFull) const {
        if (pagesUntilFull <= 1) {
            pagesUntilFull = _settings.pagesPerFullRefresh;
            pagesUntilHalf = _settings.pagesPerHalfRefresh;
            return RefreshMode::FULL;
        }
        if (pagesUntilHalf <= 1) {
            pagesUntilHalf = _settings.pagesPerHalfRefresh;
            pagesUntilFull--;
            return RefreshMode::HALF;
        }
        pagesUntilHalf--;
        pagesUntilFull--;
        return RefreshMode::FAST;
    }
    
private:
    LibReaderSettings _settings;
    bool _loaded;
};

// Global instance
extern LibReaderSettingsManager readerSettings;

#endif // SUMI_READER_SETTINGS_H
