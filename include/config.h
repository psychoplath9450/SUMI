#ifndef SUMI_CONFIG_H
#define SUMI_CONFIG_H

#include <Arduino.h>
#include <GxEPD2_BW.h>

// =============================================================================
// Display Buffer Configuration - PAGED MODE FOR MEMORY SAVINGS
// =============================================================================
// Full buffer = 480 lines = 48KB RAM
// Paged buffer = 100 lines = 10KB RAM (saves ~38KB!)
// GxEPD2 will render in multiple passes using firstPage()/nextPage()
#define DISPLAY_BUFFER_HEIGHT 100

// =============================================================================
// Include fonts - Only essential fonts to save flash
// =============================================================================
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// =============================================================================
// Firmware Identity
// =============================================================================
#define SUMI_NAME       "Sumi"
#define SUMI_TAGLINE    "Ink, simplified."

#ifndef SUMI_VERSION
#define SUMI_VERSION    "2.2.0"
#endif

#define SUMI_VARIANT    "standard"
#define PORTAL_VERSION  "2.2.0"

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
#define SD_SCK      EPD_SCLK
#define SD_MISO     SD_SPI_MISO
#define SD_MOSI     EPD_MOSI

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
// Display Type - Using paged buffer mode
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
#define CHAPTER_SKIP_MS      700
#define PAGE_SKIP_MS         700
#define PAGES_PER_SKIP       10
#define FULL_REFRESH_PAGES   15

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
#define PATH_SUMI_CACHE  "/.sumi"
#define PATH_BOOK_CACHE  "/.sumi/books"

// =============================================================================
// WiFi Configuration
// =============================================================================
#define WIFI_AP_SSID_PREFIX  "Sumi-Setup-"
#define WIFI_AP_PASSWORD     "sumisetup"
#define WIFI_CONNECT_TIMEOUT 15000
#define WIFI_MAX_NETWORKS    5

// =============================================================================
// Web Server Configuration
// =============================================================================
#define WEB_SERVER_PORT     80
#define WEBSOCKET_PORT      81

// =============================================================================
// Home Screen Item Constants
// =============================================================================
#define HOME_ITEMS_MAX       64
#define HOME_ITEMS_BYTES     8

// =============================================================================
// Feature Flags - Defaults (can be overridden by platformio.ini)
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
#define READER_PAGE_BUFFER  8192
#define READER_MAX_CHAPTERS 100

#ifndef FEATURE_FLASHCARDS
#define FEATURE_FLASHCARDS  1
#endif
#define FC_MAX_CARDS        500
#define FC_MAX_DECKS        20

#ifndef FEATURE_WEATHER
#define FEATURE_WEATHER     1
#endif
#define WEATHER_FORECAST_DAYS 7

#ifndef FEATURE_GAMES
#define FEATURE_GAMES       1
#endif

#ifndef FEATURE_LOCKSCREEN
#define FEATURE_LOCKSCREEN  0
#endif

#ifndef FEATURE_BLUETOOTH
#define FEATURE_BLUETOOTH   0
#endif

#ifndef FEATURE_EPUB_FOLDER
#define FEATURE_EPUB_FOLDER 0
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
// Memory Debugging
// =============================================================================
#ifndef SUMI_MEM_DEBUG
#define SUMI_MEM_DEBUG 0
#endif

#if SUMI_MEM_DEBUG
    #define MEM_LOG(tag) Serial.printf("[MEM:%s] Free: %d, Min: %d\n", tag, ESP.getFreeHeap(), ESP.getMinFreeHeap())
    #define MEM_LOG_DETAIL(tag, extra) Serial.printf("[MEM:%s] Free: %d, Min: %d | %s\n", tag, ESP.getFreeHeap(), ESP.getMinFreeHeap(), extra)
#else
    #define MEM_LOG(tag)
    #define MEM_LOG_DETAIL(tag, extra)
#endif

// =============================================================================
// Debug
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

#endif // SUMI_CONFIG_H
