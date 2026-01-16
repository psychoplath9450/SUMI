/**
 * @file HomeScreen.cpp
 * @brief Home screen display and navigation implementation
 * @version 2.1.16
 */

#include "core/HomeScreen.h"
#include "core/SettingsManager.h"
#include "core/BatteryMonitor.h"
#include "core/RefreshManager.h"

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern RefreshManager refreshManager;

// =============================================================================
// Home Screen State
// =============================================================================
uint8_t enabledItemIndices[MAX_HOME_ITEMS];
int enabledItemCount = 0;
int homeSelection = 0;
int homePageIndex = 0;
int itemsPerPage = 8;
int homeCols = 2;
int homeRows = 4;

// Internal state
static bool needsFullRedraw = true;
static int previousSelection = -1;
static int previousPage = -1;

// =============================================================================
// Building Home Screen Items
// =============================================================================
void buildHomeScreenItems() {
    enabledItemCount = 0;
    settingsManager.getEnabledHomeItems(enabledItemIndices, &enabledItemCount, MAX_HOME_ITEMS);
    
    Serial.printf("[HOME] Got %d items from settings\n", enabledItemCount);
    
    // Ensure Settings is always included (so user can access setup)
    bool hasSettings = false;
    for (int i = 0; i < enabledItemCount; i++) {
        if (enabledItemIndices[i] == HOME_ITEM_SETTINGS) {
            hasSettings = true;
            break;
        }
    }
    if (!hasSettings && enabledItemCount < MAX_HOME_ITEMS) {
        enabledItemIndices[enabledItemCount++] = HOME_ITEM_SETTINGS;
        Serial.println("[HOME] Added Settings to home screen");
    }
    
    // NO hardcoded fallback - if setup isn't complete, user must go through portal
    // Settings icon is always available so they can access the portal
    if (enabledItemCount == 0) {
        Serial.println("[HOME] WARNING: No home items configured! Adding Settings only.");
        enabledItemIndices[enabledItemCount++] = HOME_ITEM_SETTINGS;
    }
    
    Serial.printf("[HOME] Final: %d items enabled\n", enabledItemCount);
    for (int i = 0; i < enabledItemCount; i++) {
        Serial.printf("[HOME]   [%d] = item %d\n", i, enabledItemIndices[i]);
    }
}

void updateGridLayout() {
    bool isHorizontal = (settingsManager.display.orientation == 0);
    
    if (isHorizontal) {
        homeCols = settingsManager.display.hItemsPerRow;
        homeRows = (homeCols <= 4) ? 2 : 3;
    } else {
        homeCols = settingsManager.display.vItemsPerRow;
        homeRows = (homeCols <= 2) ? 4 : 5;
    }
    
    homeCols = constrain(homeCols, 2, 5);
    homeRows = constrain(homeRows, 2, 5);
    itemsPerPage = homeCols * homeRows;
    
    if (homeSelection >= getItemsOnCurrentPage()) homeSelection = 0;
    if (homePageIndex >= getTotalPages()) homePageIndex = 0;
}

// =============================================================================
// Page/Item Calculations
// =============================================================================
int getTotalPages() {
    if (enabledItemCount == 0) return 1;
    return (enabledItemCount + itemsPerPage - 1) / itemsPerPage;
}

int getItemsOnCurrentPage() {
    int startIdx = homePageIndex * itemsPerPage;
    int remaining = enabledItemCount - startIdx;
    if (remaining <= 0) return 0;
    return min(remaining, itemsPerPage);
}

uint8_t getItemIndexForPosition(int position) {
    int actualIndex = homePageIndex * itemsPerPage + position;
    if (actualIndex >= 0 && actualIndex < enabledItemCount) {
        return enabledItemIndices[actualIndex];
    }
    return 0xFF;
}

// =============================================================================
// Cell Geometry
// =============================================================================
void calculateCellGeometry(CellGeometry& geo) {
    int w = display.width();
    int h = display.height();
    
    bool invertColors = settingsManager.display.invertColors;
    geo.bgColor = invertColors ? GxEPD_BLACK : GxEPD_WHITE;
    geo.fgColor = invertColors ? GxEPD_WHITE : GxEPD_BLACK;
    
    geo.cols = homeCols;
    geo.rows = homeRows;
    
    bool showStatusBar = settingsManager.display.showClockHome || 
                         settingsManager.display.showBatteryHome;
    geo.statusBarHeight = showStatusBar ? 30 : 0;
    geo.gridPadding = 12;
    geo.cellGap = 10;
    geo.gridTop = geo.statusBarHeight + 8;
    
    int pageIndicatorHeight = (getTotalPages() > 1) ? 30 : 10;
    int gridHeight = h - geo.gridTop - pageIndicatorHeight;
    
    geo.cellWidth = (w - geo.gridPadding * 2 - geo.cellGap * (geo.cols - 1)) / geo.cols;
    geo.cellHeight = (gridHeight - geo.cellGap * (geo.rows - 1)) / geo.rows;
    
    uint8_t buttonShape = settingsManager.display.buttonShape;
    switch (buttonShape) {
        case 1: geo.cornerRadius = min(geo.cellWidth, geo.cellHeight) / 2; break;
        case 2: geo.cornerRadius = 2; break;
        default: geo.cornerRadius = 10; break;
    }
}

void getCellPosition(const CellGeometry& geo, int cellIndex, int& cellX, int& cellY) {
    int row = cellIndex / geo.cols;
    int col = cellIndex % geo.cols;
    cellX = geo.gridPadding + col * (geo.cellWidth + geo.cellGap);
    cellY = geo.gridTop + row * (geo.cellHeight + geo.cellGap);
}

// =============================================================================
// Drawing Functions
// =============================================================================
void showHomeScreen() {
    needsFullRedraw = true;
    showHomeScreenPartial(false);
    refreshManager.recordFullRefresh();  // Track full refresh
    previousSelection = homeSelection;
    previousPage = homePageIndex;
    needsFullRedraw = false;
}

void refreshChangedCells(int oldSel, int newSel) {
    // Determine if full refresh is required instead (ghosting prevention)
    if (refreshManager.mustFullRefresh() && refreshManager.canFullRefresh()) {
        showHomeScreen();  // Full refresh
        return;
    }
    
    CellGeometry geo;
    calculateCellGeometry(geo);
    
    int oldX, oldY, newX, newY;
    getCellPosition(geo, oldSel, oldX, oldY);
    getCellPosition(geo, newSel, newX, newY);
    
    int margin = 2;
    int minX = min(oldX, newX) - margin;
    int minY = min(oldY, newY) - margin;
    int maxX = max(oldX, newX) + geo.cellWidth + margin;
    int maxY = max(oldY, newY) + geo.cellHeight + margin;
    
    display.setPartialWindow(minX, minY, maxX - minX, maxY - minY);
    
    display.firstPage();
    do {
        display.fillRect(minX, minY, maxX - minX, maxY - minY, geo.bgColor);
        
        // Old cell (deselected)
        display.fillRoundRect(oldX, oldY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.bgColor);
        display.drawRoundRect(oldX, oldY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.fgColor);
        
        uint8_t oldIdx = getItemIndexForPosition(oldSel);
        const HomeItemInfo* oldInfo = getHomeItemByIndex(oldIdx);
        const char* oldLabel = oldInfo ? oldInfo->label : "???";
        
        display.setFont(&FreeSansBold12pt7b);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(oldLabel, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(oldX + (geo.cellWidth - tw) / 2, oldY + (geo.cellHeight + th) / 2);
        display.print(oldLabel);
        
        // New cell (selected)
        display.fillRoundRect(newX, newY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.bgColor);
        
        uint8_t newIdx = getItemIndexForPosition(newSel);
        const HomeItemInfo* newInfo = getHomeItemByIndex(newIdx);
        const char* newLabel = newInfo ? newInfo->label : "???";
        
        display.getTextBounds(newLabel, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(newX + (geo.cellWidth - tw) / 2, newY + (geo.cellHeight + th) / 2);
        display.print(newLabel);
        
    } while (display.nextPage());
    
    refreshManager.recordPartialRefresh();
}

void showHomeScreenPartial(bool partialRefresh) {
    int w = display.width();
    int h = display.height();
    
    CellGeometry geo;
    calculateCellGeometry(geo);
    
    int totalPages = getTotalPages();
    int pageIndicatorHeight = (totalPages > 1) ? 30 : 10;
    int gridHeight = h - geo.gridTop - pageIndicatorHeight;
    
    if (partialRefresh) {
        display.setPartialWindow(0, geo.gridTop, w, gridHeight + pageIndicatorHeight);
    } else {
        display.setFullWindow();
    }
    
    display.firstPage();
    do {
        display.fillScreen(geo.bgColor);
        
        // Status bar
        if (!partialRefresh && geo.statusBarHeight > 0) {
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(geo.fgColor);
            
            if (settingsManager.display.showClockHome) {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    char timeStr[16];
                    strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
                    display.setCursor(12, 22);
                    display.print(timeStr);
                }
            }
            
            if (settingsManager.display.showBatteryHome) {
                char battStr[8];
                snprintf(battStr, sizeof(battStr), "%d%%", batteryMonitor.getPercent());
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(battStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(w - tw - 10, 22);
                display.print(battStr);
            }
        }
        
        // Draw cells
        int itemsOnPage = getItemsOnCurrentPage();
        for (int i = 0; i < itemsOnPage; i++) {
            int cellX, cellY;
            getCellPosition(geo, i, cellX, cellY);
            
            uint8_t itemIndex = getItemIndexForPosition(i);
            const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
            const char* label = itemInfo ? itemInfo->label : "???";
            
            bool selected = (i == homeSelection);
            bool highContrast = (settingsManager.display.bgTheme == 2);
            
            if (selected) {
                display.fillRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
                display.setTextColor(geo.bgColor);
            } else {
                display.fillRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.bgColor);
                display.drawRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
                // High contrast mode: draw thicker border
                if (highContrast) {
                    display.drawRoundRect(cellX+1, cellY+1, geo.cellWidth-2, geo.cellHeight-2, geo.cornerRadius > 2 ? geo.cornerRadius-1 : geo.cornerRadius, geo.fgColor);
                }
                display.setTextColor(geo.fgColor);
            }
            
            // Select font based on fontSize setting and cell size
            // Smaller cells or smaller fontSize = smaller font
            uint8_t fontSize = settingsManager.display.fontSize;
            if (fontSize <= 14 || geo.cellWidth < 80) {
                display.setFont(&FreeSansBold9pt7b);
            } else if (fontSize >= 20 || geo.cellWidth > 120) {
                display.setFont(&FreeSansBold12pt7b);
            } else {
                display.setFont(&FreeSansBold12pt7b);  // Default medium
            }
            
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
            
            // Truncate if needed
            char truncLabel[16];
            strncpy(truncLabel, label, sizeof(truncLabel) - 1);
            truncLabel[sizeof(truncLabel) - 1] = '\0';
            while (tw > geo.cellWidth - 10 && strlen(truncLabel) > 3) {
                truncLabel[strlen(truncLabel) - 1] = '\0';
                display.getTextBounds(truncLabel, 0, 0, &tx, &ty, &tw, &th);
            }
            
            display.setCursor(cellX + (geo.cellWidth - tw) / 2, cellY + (geo.cellHeight + th) / 2);
            display.print(truncLabel);
        }
        
        // Page indicator
        if (totalPages > 1) {
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(geo.fgColor);
            char pageStr[16];
            snprintf(pageStr, sizeof(pageStr), "Page %d/%d", homePageIndex + 1, totalPages);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(pageStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(w / 2 - tw / 2, h - 15);
            display.print(pageStr);
        }
        
    } while (display.nextPage());
}

void drawSingleCell(int cellIndex, bool selected) {
    CellGeometry geo;
    calculateCellGeometry(geo);
    
    int cellX, cellY;
    getCellPosition(geo, cellIndex, cellX, cellY);
    
    uint8_t itemIndex = getItemIndexForPosition(cellIndex);
    const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
    const char* label = itemInfo ? itemInfo->label : "???";
    
    if (selected) {
        display.fillRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.bgColor);
    } else {
        display.fillRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.bgColor);
        display.drawRoundRect(cellX, cellY, geo.cellWidth, geo.cellHeight, geo.cornerRadius, geo.fgColor);
        display.setTextColor(geo.fgColor);
    }
    
    display.setFont(&FreeSansBold12pt7b);
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(cellX + (geo.cellWidth - tw) / 2, cellY + (geo.cellHeight + th) / 2);
    display.print(label);
}
