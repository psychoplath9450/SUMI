#ifndef SUMI_SETTINGS_MANAGER_H
#define SUMI_SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"
#include "HomeItems.h"

// =============================================================================
// Settings Structures - All Portal-Configurable Options
// =============================================================================

struct DisplaySettings {
    uint8_t rotation;           // 0-3 (0°, 90°, 180°, 270°)
    uint8_t sleepMinutes;       // 0 = never, 2-60
    uint8_t fullRefreshPages;   // Pages between full refresh (0=manual)
    bool deepSleep;             // Use deep sleep mode
    bool showBatteryHome;       // On home screen
    bool showBatterySleep;      // On sleep screen
    bool showClockHome;         // On home screen
    bool showDate;              // On sleep screen
    bool showWifi;              // On home screen
    uint8_t sleepStyle;         // 0=Default (SUMI), 1=Images, 2=Covers
    uint8_t clockStyle;         // 0=digital, 1=analog, 2=minimal
    uint8_t homeLayout;         // 0=grid, 1=list
    bool invertColors;          // Dark mode
    bool bootToLastBook;        // Skip home screen, open last book
    
    // === Widget Visibility Settings ===
    bool showBookWidget;        // Show book cover widget
    bool showWeatherWidget;     // Show weather widget
    bool showOrientWidget;      // Show orientation toggle widget
    
    // === Lock Screen Settings ===
    uint8_t lockStyle;          // 0=clock, 1=photo, 2=quote, 3=minimal
    uint8_t lockPhotoSource;    // 0=shuffle, 1=single, 2=folder
    bool showBatteryLock;       // Show battery on lock screen
    bool showWeatherLock;       // Show weather on lock screen
    
    // Portal Customization Options
    uint8_t orientation;        // 0=horizontal, 1=vertical
    uint8_t buttonShape;        // 0=rounded, 1=circle, 2=square
    uint8_t fontStyle;          // 0=sans, 1=serif, 2=mono
    uint8_t bgTheme;            // 0=light, 1=gray, 2=sepia, 3=dark
    uint8_t accentColor;        // 0=orange, 1=blue, 2=green, 3=purple, 4=red, 5=teal
    uint8_t hItemsPerRow;       // Horizontal mode: 3-5
    uint8_t vItemsPerRow;       // Vertical mode: 2-3
    
    // Display Performance Settings
    uint8_t refreshMode;        // 0=auto, 1=fast, 2=quality, 3=full-only
    uint8_t transitionStyle;    // 0=none, 1=fade, 2=slide, 3=wipe
    uint8_t ghostingThreshold;  // 10-50, partial refreshes before forced full
    uint8_t partialQuality;     // 1-4, higher = better but slower
    bool enableDirtyRects;      // Use dirty rectangle optimization
    bool enableTransitions;     // Enable animated transitions
    bool readingModeQuality;    // Use higher quality in reading mode
    
    // === Sleep Screen Settings ===
    uint8_t sleepPhotoSource;   // 0=shuffle, 1=single, 2=folder (legacy)
    char sleepSelectedImage[64]; // Path to selected sleep image
    
    // Additional Portal Settings
    bool showStatusBar;         // Show status bar on home screen
    uint8_t fontSize;           // Global font size (10-24)
    bool sleepRefresh;          // Refresh display during sleep (hourly)
    uint8_t wakeButton;         // 0=any, 1=select, 2=power
};

struct ReaderSettings {
    // === PORTAL-EXPOSED SETTINGS (all connected and working) ===
    uint8_t fontSize;           // 12-32 → syncs to LibReaderSettings.fontSize enum
    uint8_t lineHeight;         // 100-200% → syncs to LibReaderSettings.lineSpacing enum
    uint8_t margins;            // 5-40 pixels → syncs to LibReaderSettings.screenMargin
    uint8_t textAlign;          // 0=left, 1=justify → syncs to LibReaderSettings.textAlign
    // Note: All EPUBs must be preprocessed via portal - no fallback parsing
    
    // === LEGACY/UNUSED SETTINGS (kept for compatibility, not exposed in portal) ===
    uint8_t paraSpacing;        // Not synced - LibReaderSettings has own extraParagraphSpacing bool
    uint8_t sceneBreakSpacing;  // Never implemented
    bool hyphenation;           // Now handled by portal during preprocessing
    bool showProgress;          // Status bar always shows progress
    bool showChapter;           // Synced but LibReaderSettings.showChapterTitle not read
    bool showPages;             // Synced but LibReaderSettings.showPageNumbers not read
    uint8_t pageTurn;           // Device has fixed button mapping
    uint8_t tapZones;           // Device has buttons, not touch screen
};

struct FlashcardSettings {
    uint8_t newPerDay;          // 0-100
    uint16_t reviewLimit;       // 0-500
    uint8_t retention;          // 70-99 percent target
    bool useFsrs;               // Use FSRS algorithm
    bool showTimer;             // Time spent on card
    bool autoFlip;              // Auto show answer
    bool shuffle;               // Shuffle new cards
    uint8_t fontSize;           // 0=Small, 1=Medium, 2=Large, 3=Extra Large
    bool centerText;            // Center question/answer in their areas
    bool showProgressBar;       // Show progress bar at top
    bool showStats;             // Show correct/incorrect count
};

struct WeatherSettings {
    float latitude;
    float longitude;
    char location[64];          // City name for display
    char zipCode[6];            // Saved ZIP code for manual location
    bool celsius;               // true=C, false=F
    uint8_t updateHours;        // Refresh interval
    int32_t timezoneOffset;     // Timezone offset from UTC in seconds
};

struct SyncSettings {
    char kosyncUrl[64];
    char kosyncUser[32];
    char kosyncPass[32];
    bool kosyncEnabled;
};

// =============================================================================
// Bluetooth Settings
// =============================================================================
struct BluetoothSettings {
    bool enabled;               // Bluetooth enabled on boot
    bool autoConnect;           // Auto-connect to paired devices
    uint8_t keyboardLayout;     // 0=US, 1=UK, 2=DE, 3=FR, 4=ES, 5=IT
    bool showInStatusBar;       // Show BT icon in status bar
};

// =============================================================================
// Image Settings
// =============================================================================
struct ImageSettings {
    bool grayscale;             // Convert to grayscale for e-ink
    bool dither;                // Apply dithering for better gradients
    uint8_t contrast;           // 0-255, 128 = normal
    uint8_t brightness;         // 0-255, 128 = normal
};

struct APISettings {
    char stockApiKey[48];
    char stockSymbols[64];      // Comma-separated
    char newsApiKey[48];
    uint8_t newsCategory;       // 0=general, 1=tech, 2=business, 3=science
};

// =============================================================================
// WiFi Credential Storage (Multi-Network Support)
// =============================================================================
#define WIFI_SSID_MAX_LEN     33
#define WIFI_PASS_MAX_LEN     65

struct WiFiCredential {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
    int8_t lastRSSI;            // Last known signal strength
    bool isActive;              // Is this slot in use?
};

struct WiFiCredentials {
    WiFiCredential networks[WIFI_MAX_NETWORKS];
    uint8_t preferredIndex;     // Which network to try first
    uint8_t savedCount;         // How many networks are saved
};

// =============================================================================
// Plugin Enable Flags (bitfield for compact storage)
// Note: This is a legacy system. The homeScreenEnabled bitmap is now preferred.
// =============================================================================
#define PLUGIN_READER       (1 << 0)
#define PLUGIN_FLASHCARDS   (1 << 1)
#define PLUGIN_LIBRARY      (1 << 2)
#define PLUGIN_CHESS        (1 << 3)
#define PLUGIN_MINESWEEPER  (1 << 4)
#define PLUGIN_NOTES        (1 << 5)
#define PLUGIN_WEATHER      (1 << 6)
#define PLUGIN_CHECKERS     (1 << 7)
#define PLUGIN_SUDOKU       (1 << 8)
#define PLUGIN_SOLITAIRE    (1 << 9)
#define PLUGIN_TODO         (1 << 10)
#define PLUGIN_IMAGES       (1 << 11)
#define PLUGIN_TOOLS        (1 << 12)

// Default enabled plugins (sensible defaults)
#define PLUGINS_DEFAULT (PLUGIN_READER | PLUGIN_FLASHCARDS | PLUGIN_LIBRARY | \
                         PLUGIN_NOTES | PLUGIN_WEATHER)

// =============================================================================
// Settings Manager Class
// =============================================================================
class SettingsManager {
public:
    SettingsManager();
    
    void begin();
    void save();
    void load();
    void reset();
    
    // All settings sections (public for direct access)
    DisplaySettings display;
    ReaderSettings reader;
    FlashcardSettings flashcards;
    WeatherSettings weather;
    SyncSettings sync;
    APISettings api;
    BluetoothSettings bluetooth;
    WiFiCredentials wifi;
    ImageSettings images;        // Image processing settings
    
    // Plugin management (legacy - use homeScreenEnabled for new code)
    uint32_t enabledPlugins;
    
    bool isPluginEnabled(const char* pluginId);
    void setPluginEnabled(const char* pluginId, bool enabled);
    void clearPlugins();
    uint32_t getPluginBit(const char* pluginId);
    
    // ==========================================================================
    // Setup State & Home Screen
    // ==========================================================================
    
    // Setup completion state
    bool setupComplete;         // Device locked until portal setup is done
    
    bool isSetupComplete() const { return setupComplete; }
    void setSetupComplete(bool complete);
    
    // Home screen item bitmap (64 items)
    uint8_t homeScreenEnabled[HOME_ITEMS_BYTES];
    
    bool isHomeItemEnabled(uint8_t itemIndex) const;
    void setHomeItemEnabled(uint8_t itemIndex, bool enabled);
    int getEnabledHomeItemCount() const;
    void getEnabledHomeItems(uint8_t* outIndices, int* outCount, int maxCount) const;
    void setHomeItemsFromJSON(JsonArrayConst items);
    void homeItemsToJSON(JsonArray items) const;
    
    // Theme selection (portal-controlled)
    uint8_t themeIndex;         // 0=Default, 1=Compact, 2=Reader, 3=HighContrast
    
    // Auto-save
    void markDirty();
    void checkAutoSave();
    bool isDirty() const { return _dirty; }
    
    // Plugin ordering (stores order of plugins on home screen)
    uint8_t pluginOrder[32];    // Plugin indices in display order
    uint8_t pluginOrderCount;   // Number of plugins in order list
    
    // Orientation helpers
    bool isHorizontal() const { return display.orientation == 0; }
    bool isVertical() const { return display.orientation == 1; }
    int getItemsPerRow() const { return isHorizontal() ? display.hItemsPerRow : display.vItemsPerRow; }
    int getRowCount() const { return isHorizontal() ? 2 : 4; }
    
    // ==========================================================================
    // Multi-WiFi Management
    // ==========================================================================
    
    int addWiFiNetwork(const char* ssid, const char* password);
    bool removeWiFiNetwork(int index);
    bool removeWiFiNetworkBySSID(const char* ssid);
    int getWiFiNetworkCount() const;
    const WiFiCredential* getWiFiNetwork(int index) const;
    void setPreferredWifi(int index);
    int getPreferredWifi() const { return wifi.preferredIndex; }
    const char* getWiFiPassword(const char* ssid) const;
    void updateWiFiRSSI(const char* ssid, int8_t rssi);
    
    // ==========================================================================
    // Generic Access & Export
    // ==========================================================================
    
    // Generic get/set by key (for web API)
    bool setByKey(const String& key, const String& value);
    String getByKey(const String& key);
    
    // JSON export/import (for backup/restore)
    void toJSON(JsonObject doc);
    bool fromJSON(JsonObjectConst doc);
    void syncReaderSettings();  // Sync portal settings to Library's internal format
    
    // SD card backup
    bool backupToSD(const char* path = "/.config/settings.json");
    bool restoreFromSD(const char* path = "/.config/settings.json");
    
    // Settings version (increment when structure changes)
    static const uint8_t SETTINGS_VERSION = 6;

private:
    Preferences _prefs;
    bool _dirty;
    unsigned long _lastChange;  // Track last change time
    
    void loadDefaults();
    void loadWiFiDefaults();
    void saveWiFi();
    void loadWiFi();
};

// Global instance
extern SettingsManager settingsManager;

#endif // SUMI_SETTINGS_MANAGER_H
