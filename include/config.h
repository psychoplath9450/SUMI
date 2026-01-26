/**
 * @file config.h
 * @brief SUMI Configuration - Optimized for 380KB RAM
 * @version 1.4.2
 *
 * Critical memory-conscious configuration for ESP32-C3.
 * Every setting here considers the 380KB RAM constraint.
 */

#ifndef SUMI_CONFIG_H
#define SUMI_CONFIG_H

#include <Arduino.h>
#include <GxEPD2_BW.h>

// =============================================================================
// Display Buffer - PAGED MODE FOR MEMORY SAVINGS
// =============================================================================
// Full buffer = 480 lines = 48KB RAM
// Paged buffer = 100 lines = 10KB RAM (saves ~38KB!)
// Trade-off: Multiple render passes, but memory is more critical
#define DISPLAY_BUFFER_HEIGHT 100

// =============================================================================
// Fonts - Essential only to save flash
// =============================================================================
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// =============================================================================
// Firmware Identity
// =============================================================================
#define SUMI_NAME       "Sumi"
#define SUMI_TAGLINE    "Ink, simplified."

#ifndef SUMI_VERSION
#define SUMI_VERSION    "1.4.2"
#endif

#define SUMI_VARIANT    "standard"
#define PORTAL_VERSION  "1.4.2"

// =============================================================================
// Hardware Pins - Xteink X4
// =============================================================================

// Display SPI
#define EPD_SCLK    8
#define EPD_MOSI    10
#define EPD_CS      21
#define EPD_DC      4
#define EPD_RST     5
#define EPD_BUSY    6

// SD Card SPI (shares clock/mosi with display)
#define SD_CS       12
#define SD_SPI_CS   12
#define SD_SPI_MISO 7
#define SD_SCK      EPD_SCLK
#define SD_MISO     SD_SPI_MISO
#define SD_MOSI     EPD_MOSI

// Battery & Power
#define BAT_ADC     0
#define PIN_BATTERY BAT_ADC
#define PIN_PWR_HOLD  9
#define PIN_PWR_BTN   20

// Buttons
#define BTN_GPIO1   1
#define BTN_GPIO2   2
#define BTN_GPIO3   3

// SPI Frequency
#define SPI_FQ 40000000

// =============================================================================
// Display Dimensions
// =============================================================================
#define DISPLAY_WIDTH  800
#define DISPLAY_HEIGHT 480

// Display Type
typedef GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> DisplayType;

// =============================================================================
// Button Configuration
// =============================================================================
#define BTN_THRESHOLD       100
#define BTN_RIGHT_VAL       3
#define BTN_LEFT_VAL        1470
#define BTN_CONFIRM_VAL     2655
#define BTN_BACK_VAL        3470
#define BTN_DOWN_VAL        3
#define BTN_UP_VAL          2205

#define BTN_DEBOUNCE_MS         50
#define BTN_LONG_PRESS_MS       800
#define BTN_REPEAT_DELAY_MS     500
#define BTN_REPEAT_RATE_MS      150
#define BTN_POWER_RESTART_MS    5000
#define BTN_BACK_LONG_MS        400

enum Button {
    BTN_NONE = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_CONFIRM,
    BTN_BACK,
    BTN_POWER
};

enum BackPressType {
    BACK_NONE = 0,
    BACK_SHORT,
    BACK_LONG
};

// =============================================================================
// Storage Paths
// =============================================================================
#define PATH_BOOKS          "/books"
#define PATH_FLASHCARDS     "/flashcards"
#define PATH_IMAGES         "/images"
#define PATH_NOTES          "/notes"
#define PATH_SAVES          "/saves"
#define PATH_CACHE          "/.cache"
#define PATH_CONFIG         "/.config"
#define PATH_SUMI_CACHE     "/.sumi"
#define PATH_BOOK_CACHE     "/.sumi/books"

// =============================================================================
// WiFi Configuration
// =============================================================================
#define WIFI_AP_SSID_PREFIX     "Sumi-Setup-"
#define WIFI_AP_PASSWORD        "sumisetup"
#define WIFI_CONNECT_TIMEOUT    15000
#define WIFI_MAX_NETWORKS       5

// WiFi uses ~100KB RAM and fragments heap
// After WiFi operations, may need restart for contiguous memory
#define WIFI_RAM_USAGE_KB       100

// =============================================================================
// Web Server Configuration
// =============================================================================
#define WEB_SERVER_PORT     80
#define WEBSOCKET_PORT      81

// =============================================================================
// Reader Configuration - Memory Conscious
// =============================================================================
#define READER_MAX_CHAPTERS     200     // Max spine entries
#define READER_MAX_TOC          100     // Max TOC entries
#define READER_CHUNK_SIZE       1024    // Stream chunk size (1KB)
#define READER_MAX_BOOKMARKS    20      // Per book

// Refresh intervals (pages between refreshes)
#define FULL_REFRESH_PAGES      15
#define HALF_REFRESH_PAGES      5

// Navigation timing
#define CHAPTER_SKIP_MS         700
#define PAGE_SKIP_MS            700
#define PAGES_PER_SKIP          10

// =============================================================================
// Library Configuration
// =============================================================================
#define LIBRARY_MAX_BOOKS       200     // Binary index on SD, not RAM

// =============================================================================
// Home Screen Configuration
// =============================================================================
#define HOME_ITEMS_MAX          16      // Maximum number of home items
#define HOME_ITEMS_BYTES        ((HOME_ITEMS_MAX + 7) / 8)  // Bytes for bitmap

// =============================================================================
// Plugin Configuration
// =============================================================================
#define FC_MAX_CARDS            500     // Max flashcards per deck
#define WEATHER_FORECAST_DAYS   5       // Days of weather forecast

// =============================================================================
// Feature Flags
// =============================================================================
#ifndef FEATURE_WIFI
#define FEATURE_WIFI        1
#endif

#ifndef FEATURE_WEBSERVER
#define FEATURE_WEBSERVER   1
#endif

#ifndef FEATURE_READER
#define FEATURE_READER      1
#endif

#ifndef FEATURE_FLASHCARDS
#define FEATURE_FLASHCARDS  1
#endif

#ifndef FEATURE_WEATHER
#define FEATURE_WEATHER     1
#endif

#ifndef FEATURE_GAMES
#define FEATURE_GAMES       1
#endif

#ifndef FEATURE_LOCKSCREEN
#define FEATURE_LOCKSCREEN  0
#endif

#ifndef FEATURE_BLUETOOTH
#define FEATURE_BLUETOOTH   0
#endif

// =============================================================================
// Settings Validation
// =============================================================================
#define FONT_SIZE_MIN       12
#define FONT_SIZE_MAX       32
#define LINE_HEIGHT_MIN     100
#define LINE_HEIGHT_MAX     200
#define MARGIN_MIN          5
#define MARGIN_MAX          50
#define SLEEP_MIN           0
#define SLEEP_MAX           120
#define REFRESH_PAGES_MIN   0
#define REFRESH_PAGES_MAX   50

// =============================================================================
// Memory Management - CRITICAL
// =============================================================================

/**
 * Memory debugging macros
 * Use MEM_LOG() liberally to track memory usage
 */
#ifndef SUMI_MEM_DEBUG
#define SUMI_MEM_DEBUG 1  // Enable by default for development
#endif

#if SUMI_MEM_DEBUG
    #define MEM_LOG(tag) do { \
        Serial.printf("[MEM:%s] Free=%d Min=%d\n", \
            tag, ESP.getFreeHeap(), ESP.getMinFreeHeap()); \
    } while(0)
    
    #define MEM_LOG_DETAIL(tag, extra) do { \
        Serial.printf("[MEM:%s] Free=%d Min=%d | %s\n", \
            tag, ESP.getFreeHeap(), ESP.getMinFreeHeap(), extra); \
    } while(0)
    
    #define MEM_CHECK_CRITICAL(tag, minRequired) do { \
        int freeHeap = ESP.getFreeHeap(); \
        if (freeHeap < minRequired) { \
            Serial.printf("[MEM:%s] CRITICAL! Free=%d < Required=%d\n", \
                tag, freeHeap, minRequired); \
        } \
    } while(0)
#else
    #define MEM_LOG(tag)
    #define MEM_LOG_DETAIL(tag, extra)
    #define MEM_CHECK_CRITICAL(tag, minRequired)
#endif

/**
 * Memory thresholds
 */
#define MEM_CRITICAL_THRESHOLD  30000   // 30KB - danger zone
#define MEM_WARNING_THRESHOLD   50000   // 50KB - caution
#define MEM_EPUB_MINIMUM        60000   // 60KB needed for EPUB operations
#define MEM_WIFI_MINIMUM        120000  // 120KB needed before enabling WiFi

/**
 * Check if operation is safe to proceed
 */
inline bool memoryIsSafe(int requiredBytes = MEM_WARNING_THRESHOLD) {
    return ESP.getFreeHeap() >= requiredBytes;
}

/**
 * Check if WiFi can be safely enabled
 */
inline bool canEnableWifi() {
    return ESP.getFreeHeap() >= MEM_WIFI_MINIMUM;
}

// =============================================================================
// Mode Management
// =============================================================================

/**
 * Operating modes with different memory profiles:
 * 
 * READER_MODE:
 * - ZIP buffers allocated (43KB)
 * - WiFi disabled
 * - Full EPUB capability
 * 
 * PORTAL_MODE:
 * - ZIP buffers freed
 * - WiFi enabled (~100KB)
 * - Web server active
 * - No EPUB reading (insufficient memory)
 * 
 * IDLE_MODE:
 * - Minimal memory usage
 * - Ready for any operation
 */
enum class OperatingMode {
    IDLE,
    READER,
    PORTAL,
    GAME
};

// =============================================================================
// Debug Configuration
// =============================================================================
#ifndef SUMI_DEBUG
#define SUMI_DEBUG 0
#endif

#if SUMI_DEBUG
    #define SUMI_LOG(x)       Serial.print(x)
    #define SUMI_LOGLN(x)     Serial.println(x)
    #define SUMI_LOGF(...)    Serial.printf(__VA_ARGS__)
#else
    #define SUMI_LOG(x)
    #define SUMI_LOGLN(x)
    #define SUMI_LOGF(...)
#endif

// =============================================================================
// Utility Macros
// =============================================================================

// Safe string copy with null termination
#define SAFE_STRCPY(dest, src, size) do { \
    strncpy(dest, src, (size) - 1); \
    dest[(size) - 1] = '\0'; \
} while(0)

// Array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Min/Max for non-standard compilers
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Constrain value to range
#ifndef CONSTRAIN
#define CONSTRAIN(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

// =============================================================================
// Build-time Checks
// =============================================================================
static_assert(DISPLAY_BUFFER_HEIGHT <= 200, 
    "Display buffer too large - will consume too much RAM");

static_assert(READER_CHUNK_SIZE <= 2048,
    "Stream chunk size too large - use 1KB or 2KB max");

static_assert(LIBRARY_MAX_BOOKS <= 200,
    "Library size too large - will slow down scanning");

#endif // SUMI_CONFIG_H
