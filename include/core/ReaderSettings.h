/**
 * @file ReaderSettings.h
 * @brief Reader Settings Definitions
 * @version 1.4.3
 *
 * Contains all reader-related settings structures and enums.
 * Settings are stored in binary format on SD card for quick load.
 * 
 * Layout system optimized for e-ink rendering:
 * - Viewable margins (hardware) + screen margin (user preference)
 * - Line height compression for font-specific tuning
 * - Optional paragraph spacing vs indent
 */

#ifndef SUMI_READER_SETTINGS_H
#define SUMI_READER_SETTINGS_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

// =============================================================================
// Enums
// =============================================================================

// Font size options (maps to actual pixel sizes)
enum class FontSize : uint8_t {
    SMALL = 0,      // 12pt equivalent
    MEDIUM = 1,     // 14pt equivalent (default)
    LARGE = 2,      // 16pt equivalent
    EXTRA_LARGE = 3 // 18pt equivalent
};

// Line spacing options (compression multiplier)
enum class LineSpacing : uint8_t {
    TIGHT = 0,      // 0.95x (more lines per page)
    NORMAL = 1,     // 1.0x (default)
    WIDE = 2        // 1.1x (easier reading)
};

// Text alignment
enum class TextAlign : uint8_t {
    JUSTIFIED = 0,  // Full justification (default)
    LEFT = 1,       // Left aligned
    CENTER = 2,     // Center aligned (for headers)
    RIGHT = 3       // Right aligned
};

// Font style (for inline formatting)
enum class FontStyle : uint8_t {
    NORMAL = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3
};

// =============================================================================
// Viewable Margins
// These account for the e-ink panel's non-viewable edges
// =============================================================================
namespace ViewableMargins {
    // Hardware-specific margins (pixels lost at edges of e-ink panel)
    constexpr int TOP = 9;
    constexpr int RIGHT = 3;
    constexpr int BOTTOM = 3;
    constexpr int LEFT = 3;
    
    // Status bar area at bottom (for page numbers, chapter title)
    constexpr int STATUS_BAR_HEIGHT = 22;
}

// =============================================================================
// Reader Settings Structure
// =============================================================================

#define READER_SETTINGS_MAGIC   0x52534554  // "RSET"
#define READER_SETTINGS_VERSION 3           // Bumped for new fields
#define READER_SETTINGS_PATH    "/.sumi/reader.bin"

struct LibReaderSettings {
    uint32_t magic;
    uint16_t version;
    
    // === Font Settings ===
    FontSize fontSize;
    LineSpacing lineSpacing;
    TextAlign textAlign;
    
    // === Layout Settings ===
    uint8_t screenMargin;           // User margin added to viewable margins (0-20, default 5)
    bool extraParagraphSpacing;     // true = add space between paragraphs, false = use indent
    
    // === Display Settings ===
    bool showPageNumbers;
    bool showChapterTitle;
    uint8_t refreshFrequency;       // Pages between full refresh (default 15)
    
    // Reserved for future use
    uint8_t reserved[12];
    
    LibReaderSettings() {
        setDefaults();
    }
    
    void setDefaults() {
        magic = READER_SETTINGS_MAGIC;
        version = READER_SETTINGS_VERSION;
        fontSize = FontSize::MEDIUM;
        lineSpacing = LineSpacing::NORMAL;
        textAlign = TextAlign::JUSTIFIED;
        screenMargin = 5;
        extraParagraphSpacing = true;
        showPageNumbers = true;
        showChapterTitle = true;
        refreshFrequency = 15;
        memset(reserved, 0, sizeof(reserved));
    }
    
    // === Computed Layout Values ===
    
    // Get base line height for font size (before compression)
    int getBaseFontHeight() const {
        switch (fontSize) {
            case FontSize::SMALL:       return 22;  // ~12pt
            case FontSize::MEDIUM:      return 26;  // ~14pt
            case FontSize::LARGE:       return 30;  // ~16pt
            case FontSize::EXTRA_LARGE: return 34;  // ~18pt
            default: return 26;
        }
    }
    
    // Get line compression multiplier
    float getLineCompression() const {
        switch (lineSpacing) {
            case LineSpacing::TIGHT:  return 0.95f;
            case LineSpacing::NORMAL: return 1.0f;
            case LineSpacing::WIDE:   return 1.1f;
            default: return 1.0f;
        }
    }
    
    // Get actual line height (base height * compression)
    int getLineHeight() const {
        return (int)(getBaseFontHeight() * getLineCompression());
    }
    
    // Get paragraph spacing (half line height when enabled)
    int getParagraphSpacing() const {
        if (!extraParagraphSpacing) return 0;
        return getLineHeight() / 2;
    }
    
    // Get total left margin (viewable + user)
    int getMarginLeft() const {
        return ViewableMargins::LEFT + screenMargin;
    }
    
    // Get total right margin (viewable + user)
    int getMarginRight() const {
        return ViewableMargins::RIGHT + screenMargin;
    }
    
    // Get total top margin (viewable + user)
    int getMarginTop() const {
        return ViewableMargins::TOP + screenMargin;
    }
    
    // Get total bottom margin (viewable + user + status bar)
    int getMarginBottom() const {
        return ViewableMargins::BOTTOM + screenMargin + ViewableMargins::STATUS_BAR_HEIGHT;
    }
    
    // Get content width for given screen width
    int getContentWidth(int screenWidth) const {
        return screenWidth - getMarginLeft() - getMarginRight();
    }
    
    // Get content height for given screen height
    int getContentHeight(int screenHeight) const {
        return screenHeight - getMarginTop() - getMarginBottom();
    }
    
    // Calculate approximate lines per page
    int getLinesPerPage(int screenHeight) const {
        int contentHeight = getContentHeight(screenHeight);
        return contentHeight / getLineHeight();
    }
    
    // === UI Helpers ===
    
    static const char* getFontSizeName(FontSize fs) {
        switch (fs) {
            case FontSize::SMALL:       return "Small";
            case FontSize::MEDIUM:      return "Medium";
            case FontSize::LARGE:       return "Large";
            case FontSize::EXTRA_LARGE: return "Extra Large";
            default: return "?";
        }
    }
    
    static const char* getLineSpacingName(LineSpacing ls) {
        switch (ls) {
            case LineSpacing::TIGHT:  return "Tight";
            case LineSpacing::NORMAL: return "Normal";
            case LineSpacing::WIDE:   return "Wide";
            default: return "?";
        }
    }
    
    static const char* getTextAlignName(TextAlign ta) {
        switch (ta) {
            case TextAlign::JUSTIFIED: return "Justified";
            case TextAlign::LEFT:      return "Left";
            case TextAlign::CENTER:    return "Center";
            case TextAlign::RIGHT:     return "Right";
            default: return "?";
        }
    }
    
    // For backwards compatibility with old code
    int getMarginPx() const { return screenMargin + ViewableMargins::LEFT; }
    int getParaSpacing() const { return getParagraphSpacing(); }
    uint8_t pagesPerFullRefresh() const { return refreshFrequency; }
    bool justifyText() const { return textAlign == TextAlign::JUSTIFIED; }
};

// =============================================================================
// Settings Manager
// =============================================================================

class ReaderSettingsManager {
public:
    ReaderSettingsManager() : _dirty(false) {}
    
    LibReaderSettings& get() { return _settings; }
    const LibReaderSettings& get() const { return _settings; }
    
    bool load() {
        File f = SD.open(READER_SETTINGS_PATH, FILE_READ);
        if (!f) {
            Serial.println("[SETTINGS] No reader settings, using defaults");
            _settings.setDefaults();
            return false;
        }
        
        LibReaderSettings temp;
        if (f.read((uint8_t*)&temp, sizeof(LibReaderSettings)) != sizeof(LibReaderSettings)) {
            f.close();
            _settings.setDefaults();
            return false;
        }
        f.close();
        
        // Validate magic and migrate if needed
        if (temp.magic != READER_SETTINGS_MAGIC) {
            Serial.println("[SETTINGS] Invalid reader settings magic, using defaults");
            _settings.setDefaults();
            return false;
        }
        
        // Handle version migration
        if (temp.version < READER_SETTINGS_VERSION) {
            Serial.printf("[SETTINGS] Migrating from version %d to %d\n", temp.version, READER_SETTINGS_VERSION);
            // Copy compatible fields
            _settings.fontSize = temp.fontSize;
            _settings.showPageNumbers = temp.showPageNumbers;
            _settings.showChapterTitle = temp.showChapterTitle;
            _settings.refreshFrequency = temp.refreshFrequency;
            // Use defaults for new fields
            _settings.magic = READER_SETTINGS_MAGIC;
            _settings.version = READER_SETTINGS_VERSION;
            save();  // Save migrated settings
            return true;
        }
        
        _settings = temp;
        _dirty = false;
        Serial.println("[SETTINGS] Loaded reader settings");
        return true;
    }
    
    bool save() {
        // Ensure directory exists
        if (!SD.exists("/.sumi")) {
            SD.mkdir("/.sumi");
        }
        
        File f = SD.open(READER_SETTINGS_PATH, FILE_WRITE);
        if (!f) {
            Serial.println("[SETTINGS] Failed to save reader settings");
            return false;
        }
        
        f.write((const uint8_t*)&_settings, sizeof(LibReaderSettings));
        f.close();
        
        _dirty = false;
        Serial.println("[SETTINGS] Saved reader settings");
        return true;
    }
    
    void markDirty() { _dirty = true; }
    bool isDirty() const { return _dirty; }
    
    // Save if dirty (call periodically or on exit)
    void saveIfDirty() {
        if (_dirty) {
            save();
        }
    }
    
private:
    LibReaderSettings _settings;
    bool _dirty;
};

// =============================================================================
// Global Instance
// =============================================================================
extern ReaderSettingsManager readerSettings;

#endif // SUMI_READER_SETTINGS_H
