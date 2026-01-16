#ifndef SUMI_CONFIG_H
#define SUMI_CONFIG_H

#include <Arduino.h>
#include <GxEPD2_BW.h>

// =============================================================================
// Memory Configuration for ESP32-C3 - FORCE LOW MEMORY
// =============================================================================
#ifndef SUMI_LOW_MEMORY
    #if CONFIG_IDF_TARGET_ESP32C3
        #define SUMI_LOW_MEMORY 1
    #else
        #define SUMI_LOW_MEMORY 0
    #endif
#endif

#if SUMI_LOW_MEMORY
    // Use FULL buffer to fix GxEPD2 rotation bug with paged mode
    // This uses ~48KB RAM but is required for correct portrait rendering
    #define DISPLAY_BUFFER_HEIGHT 480   
#else
    #define DISPLAY_BUFFER_HEIGHT 480   // Full buffer for ESP32/S3
#endif

// =============================================================================
// Include fonts - MINIMAL set only (saves ~30KB flash)
// =============================================================================
#if SUMI_LOW_MEMORY
    // Only 3 fonts for minimal UI
    #include <Fonts/FreeSans9pt7b.h>
    #include <Fonts/FreeSansBold9pt7b.h>
    #include <Fonts/FreeSansBold12pt7b.h>
#else
    // Full font set for ESP32/S3
    #include <Fonts/FreeMonoBold9pt7b.h>
    #include <Fonts/FreeMonoBold12pt7b.h>
    #include <Fonts/FreeMonoBold18pt7b.h>
    #include <Fonts/FreeMonoBold24pt7b.h>
    #include <Fonts/FreeMono9pt7b.h>
    #include <Fonts/FreeMono12pt7b.h>
    #include <Fonts/FreeSans9pt7b.h>
    #include <Fonts/FreeSans12pt7b.h>
    #include <Fonts/FreeSansBold9pt7b.h>
    #include <Fonts/FreeSansBold12pt7b.h>
#endif

// =============================================================================
// Firmware Identity
// =============================================================================
#define SUMI_NAME       "Sumi"
#define SUMI_TAGLINE    "Ink, simplified."

#ifndef SUMI_VERSION
#define SUMI_VERSION    "2.1.30"
#endif

#ifndef SUMI_VARIANT
    #if SUMI_LOW_MEMORY
        #define SUMI_VARIANT    "lite"
    #else
        #define SUMI_VARIANT    "standard"
    #endif
#endif

#define PORTAL_VERSION  "2.1.30"

// =============================================================================
// SPI Configuration
// =============================================================================
#define SPI_FQ 40000000

// =============================================================================
// Display SPI Pins
// =============================================================================
#define EPD_SCLK 8
#define EPD_MOSI 10
#define EPD_CS   21
#define EPD_DC   4
#define EPD_RST  5
#define EPD_BUSY 6

// =============================================================================
// SD Card Pins
// =============================================================================
#define SD_CS       12
#define SD_SPI_CS   12
#define SD_SPI_MISO 7
#define SD_SCK      EPD_SCLK    // Shared SPI clock with display
#define SD_MISO     SD_SPI_MISO
#define SD_MOSI     EPD_MOSI    // Shared SPI MOSI with display

// =============================================================================
// GPIO Pins
// =============================================================================
#define BAT_ADC     0
#define BAT_GPIO0   0
#define BTN_GPIO1   1
#define BTN_GPIO2   2
#define BTN_GPIO3   3
#define PIN_BATTERY BAT_ADC

#define PIN_PWR_HOLD  9
#define PIN_PWR_BTN   20

// =============================================================================
// Display Dimensions
// =============================================================================
#define DISPLAY_WIDTH  800
#define DISPLAY_HEIGHT 480

// =============================================================================
// Display Type - Uses paged mode for low-memory devices
// =============================================================================
typedef GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> DisplayType;

// =============================================================================
// Button ADC Thresholds
// =============================================================================
#define BTN_THRESHOLD    100
#define BTN_RIGHT_VAL    3
#define BTN_LEFT_VAL     1470
#define BTN_CONFIRM_VAL  2655
#define BTN_BACK_VAL     3470
#define BTN_DOWN_VAL     3
#define BTN_UP_VAL       2205

#define BTN_DEBOUNCE_MS      50
#define BTN_LONG_PRESS_MS    800
#define BTN_REPEAT_DELAY_MS  500
#define BTN_REPEAT_RATE_MS   150
#define BTN_POWER_RESTART_MS 5000
#define BTN_BACK_LONG_MS     400

// =============================================================================
// Reader Navigation Timing
// =============================================================================
#define CHAPTER_SKIP_MS      700    // Hold time to skip chapter
#define PAGE_SKIP_MS         700    // Hold time to skip multiple pages
#define PAGES_PER_SKIP       10     // Pages to skip on long press in browser
#define FULL_REFRESH_PAGES   15     // Full refresh every N pages (reduce ghosting)

// =============================================================================
// Button Enum
// =============================================================================
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
#define PATH_BOOKS       "/books"
#define PATH_FLASHCARDS  "/flashcards"
#define PATH_IMAGES      "/images"
#define PATH_NOTES       "/notes"
#define PATH_SAVES       "/saves"
#define PATH_CACHE       "/.cache"
#define PATH_CONFIG      "/.config"
#define PATH_THEMES      "/themes"
#define PATH_SRS         "/srs"
#define PATH_SUMI_CACHE  "/.sumi"           // Cache directory
#define PATH_BOOK_CACHE  "/.sumi/books"     // Per-book cache storage

// =============================================================================
// WiFi Configuration
// =============================================================================
#define WIFI_AP_SSID_PREFIX  "Sumi-Setup-"
#define WIFI_AP_PASSWORD     "sumisetup"
#define WIFI_CONNECT_TIMEOUT 15000

#if SUMI_LOW_MEMORY
    #define WIFI_MAX_NETWORKS    1
#else
    #define WIFI_MAX_NETWORKS    5
#endif

// =============================================================================
// Web Server Configuration
// =============================================================================
#define WEB_SERVER_PORT     80
#define WEBSOCKET_PORT      81

// =============================================================================
// Home Screen Item Constants
// =============================================================================
#if SUMI_LOW_MEMORY
    #define HOME_ITEMS_MAX       32
    #define HOME_ITEMS_BYTES     4
#else
    #define HOME_ITEMS_MAX       64
    #define HOME_ITEMS_BYTES     8
#endif

// =============================================================================
// Feature Flags - MINIMAL for ESP32-C3, expanded for ESP32/S3
// =============================================================================

// WiFi - ALWAYS enabled (required for setup portal)
#ifndef FEATURE_WIFI
#define FEATURE_WIFI        1
#endif

// WebServer - ALWAYS enabled (required for setup portal)
#ifndef FEATURE_WEBSERVER
#define FEATURE_WEBSERVER   1
#endif

// =============================================================================
// Feature flags - use lite versions on low memory
// =============================================================================

// Reader - ENABLED on all devices, use lite renderer on low memory
#ifndef FEATURE_READER
    #define FEATURE_READER      1
#endif
#if SUMI_LOW_MEMORY && FEATURE_READER
    #define READER_LITE         1   // Simplified text renderer
    #define READER_PAGE_BUFFER  2048 // Smaller page buffer (vs 8192)
    #define READER_MAX_CHAPTERS 20   // Limit chapters in memory
#else
    #define READER_LITE         0
    #define READER_PAGE_BUFFER  8192
    #define READER_MAX_CHAPTERS 100
#endif

// Flashcards - ENABLED on all devices, reduced deck size on low memory
#ifndef FEATURE_FLASHCARDS
    #define FEATURE_FLASHCARDS  1
#endif
#if SUMI_LOW_MEMORY
    #define FC_MAX_CARDS        100  // Limit cards in memory
    #define FC_MAX_DECKS        5    // Limit loaded decks
#else
    #define FC_MAX_CARDS        500
    #define FC_MAX_DECKS        20
#endif

// Weather - ENABLED on all devices, reduced forecast on low memory
#ifndef FEATURE_WEATHER
    #define FEATURE_WEATHER     1
#endif
#if SUMI_LOW_MEMORY
    #define WEATHER_LITE        1   // Current + 3 day only (no hourly)
    #define WEATHER_FORECAST_DAYS 3
#else
    #define WEATHER_LITE        0
    #define WEATHER_FORECAST_DAYS 7
#endif

// Games - DISABLED on low memory devices (saves ~100KB RAM)
#ifndef FEATURE_GAMES
    #if SUMI_LOW_MEMORY
        #define FEATURE_GAMES       0
    #else
        #define FEATURE_GAMES       1
    #endif
#endif

// Lockscreen - ENABLED on all devices, simplified on low memory
#ifndef FEATURE_LOCKSCREEN
    #define FEATURE_LOCKSCREEN  1
#endif
#if SUMI_LOW_MEMORY
    #define LOCKSCREEN_LITE     1   // No photo mode, clock/quote only
#else
    #define LOCKSCREEN_LITE     0
#endif

// Bluetooth - ENABLED on all devices (ESP32-C3 has BLE only, which is fine for HID keyboards)
// Note: BLE stack adds ~25KB RAM usage
#ifndef FEATURE_BLUETOOTH
    #define FEATURE_BLUETOOTH   1  // Enable for all, including C3
#endif

// EPUB folder support - disabled on low-memory (recursive parsing is expensive)
#ifndef FEATURE_EPUB_FOLDER
    #if SUMI_LOW_MEMORY
        #define FEATURE_EPUB_FOLDER 0
    #else
        #define FEATURE_EPUB_FOLDER 1
    #endif
#endif

// =============================================================================
// Settings Validation Limits
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
// Debug
// =============================================================================
#ifndef SUMI_DEBUG
#define SUMI_DEBUG 0  // Set to 1 for debug output
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

#endif // SUMI_CONFIG_H
