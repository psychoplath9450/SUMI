#ifndef SUMI_HOME_SCREEN_H
#define SUMI_HOME_SCREEN_H

/**
 * @file HomeScreen.h
 * @brief Home screen display and navigation
 * @version 2.1.16
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"
#include "core/HomeItems.h"

// =============================================================================
// Home Screen State (extern - defined in main.cpp for now)
// =============================================================================
#define MAX_HOME_ITEMS 32

extern uint8_t enabledItemIndices[MAX_HOME_ITEMS];
extern int enabledItemCount;
extern int homeSelection;
extern int homePageIndex;
extern int itemsPerPage;
extern int homeCols;
extern int homeRows;

// =============================================================================
// Cell Geometry Structure
// =============================================================================
struct CellGeometry {
    int cols, rows, statusBarHeight, gridPadding, cellGap, gridTop;
    int cellWidth, cellHeight, cornerRadius;
    uint16_t bgColor, fgColor;
};

// =============================================================================
// Home Screen Functions
// =============================================================================

// Building home screen items
void buildHomeScreenItems();
void updateGridLayout();

// Page/item calculations
int getTotalPages();
int getItemsOnCurrentPage();
uint8_t getItemIndexForPosition(int position);

// Cell geometry
void calculateCellGeometry(CellGeometry& geo);
void getCellPosition(const CellGeometry& geo, int cellIndex, int& cellX, int& cellY);

// Drawing functions
void showHomeScreen();
void showHomeScreenPartial(bool partialRefresh);
void refreshChangedCells(int oldSelection, int newSelection);
void drawSingleCell(int cellIndex, bool selected);

#endif // SUMI_HOME_SCREEN_H
