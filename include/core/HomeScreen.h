#ifndef SUMI_HOME_SCREEN_H
#define SUMI_HOME_SCREEN_H

/**
 * @file HomeScreen.h
 * @brief Home screen display and navigation - Optimized for instant rendering
 * @version 1.5.0
 * 
 * Optimizations:
 * - Pre-cached scaled cover image on SD card
 * - Persistent HomeState struct (no recalculation on navigation)
 * - Pre-render during deploy screen
 * - Per-widget partial refresh
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"
#include "core/HomeItems.h"

// =============================================================================
// Constants
// =============================================================================
#define MAX_HOME_ITEMS 8
#define HOME_COVER_CACHE_PATH "/.sumi/cover_home.bin"  // Pre-scaled cover cache
#ifndef WEATHER_CACHE_PATH
#define WEATHER_CACHE_PATH "/.sumi/weather_cache.bin"
#endif
#define LAST_BOOK_PATH "/.sumi/lastbook.bin"

// =============================================================================
// Home Screen State (extern - defined in HomeScreen.cpp)
// =============================================================================
extern uint8_t enabledItemIndices[MAX_HOME_ITEMS];
extern int enabledItemCount;
extern int homeSelection;
extern int homePageIndex;
extern int itemsPerPage;
extern int homeCols;
extern int homeRows;

// Widget selection (-1 = none/grid, 0 = book widget, 1 = weather widget, etc)
extern int widgetSelection;

// =============================================================================
// Cell Geometry Structure
// =============================================================================
struct CellGeometry {
    int cols, rows, statusBarHeight, gridPadding, cellGap, gridTop;
    int cellWidth, cellHeight, cornerRadius;
    uint16_t bgColor, fgColor;
};

// =============================================================================
// Persistent Home State (pre-computed, survives across calls)
// =============================================================================
struct HomeState {
    // Flags
    bool initialized;           // True after first build
    bool dirty;                 // True if needs full redraw
    bool coverCached;           // True if pre-scaled cover exists
    
    // Grid layout (computed once)
    CellGeometry geo;
    int totalPages;
    int itemsOnPage;
    
    // Widget state
    bool hasBook;
    bool hasWeather;
    bool hasOrient;
    int widgetCount;
    
    // Book widget data
    char bookTitle[64];
    char bookCoverPath[96];
    float bookProgress;
    
    // Weather widget data
    float weatherTemp;
    int weatherCode;
    int weatherHumidity;
    char weatherLocation[48];
    bool weatherCelsius;
    float weatherHigh;          // Today's high
    float weatherLow;           // Today's low
    char sunrise[12];           // e.g. "6:45 AM"
    char sunset[12];            // e.g. "5:30 PM"
    
    // 3-day forecast
    float forecastHigh[3];      // Next 3 days high temps
    float forecastLow[3];       // Next 3 days low temps
    char forecastDay[3][4];     // Day names (e.g. "Wed", "Thu", "Fri")
    
    // Cached cover info
    int cachedCoverW;
    int cachedCoverH;
    int cachedCoverX;
    int cachedCoverY;
    
    // Orientation
    bool isLandscape;
};

extern HomeState homeState;

// =============================================================================
// Home Screen Functions
// =============================================================================

// Initialization and building
void initHomeState();                    // Call once at startup
void buildHomeScreenItems();             // Full rebuild (settings change, deploy)
void prepareHomeScreen();                // Pre-render all data (call during deploy screen)
void updateGridLayout();

// Page/item calculations  
int getTotalPages();
int getItemsOnCurrentPage();
uint8_t getItemIndexForPosition(int position);

// Cell geometry
void calculateCellGeometry(CellGeometry& geo);
void getCellPosition(const CellGeometry& geo, int cellIndex, int& cellX, int& cellY);

// Drawing functions - optimized
void showHomeScreen();                           // Full refresh (first show, orientation change)
void showHomeScreenPartial(bool partialRefresh); // Standard partial refresh
void showHomeScreenFast();                       // Ultra-fast using cached data
void refreshGridOnly();                          // Refresh just the grid area
void refreshCellSelection(int oldSel, int newSel);  // Refresh just changed cells
void refreshWidgetSelection(int oldWidget, int newWidget);  // Refresh widget selection

// Legacy compatibility
void showHomeScreenPartialFast();
void refreshChangedCells(int oldSelection, int newSelection);
void drawSingleCell(int cellIndex, bool selected);

// Widget functions
bool hasBookWidget();
bool hasWeatherWidget();
bool hasOrientWidget();
int getWidgetCount();
void toggleOrientation();
void activateWidget(int widget);

// Cover caching
bool createCachedCover(const char* sourcePath, int targetW, int targetH);
bool drawCachedCover(int x, int y);

// Weather cache (called from Weather app)
void saveWeatherCache(float temp, int code, int humidity, float wind, 
                      const char* location, bool celsius,
                      float high, float low,
                      float forecastHigh[3], float forecastLow[3],
                      const char forecastDay[3][4],
                      const char* sunrise, const char* sunset);

// Widget loading
void loadLastBookWidget();
void loadWeatherWidget();

#endif // SUMI_HOME_SCREEN_H
