/**
 * @file PluginHelpers.h
 * @brief Standardized helpers for all Sumi plugins
 * @version 1.3.0
 *
 * Provides:
 * - Orientation-aware button mapping
 * - Standard UI drawing helpers
 * - Grid/cell size calculations
 * - Common plugin patterns
 */

#ifndef SUMI_PLUGIN_HELPERS_H
#define SUMI_PLUGIN_HELPERS_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"

// Forward declare display
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// Plugin UI Constants
// =============================================================================
#define PLUGIN_HEADER_H     28      // Standard header bar height
#define PLUGIN_FOOTER_H     20      // Standard footer/status bar
#define PLUGIN_MARGIN       6       // Standard margin
#define PLUGIN_PADDING      4       // Standard padding inside elements
#define PLUGIN_ITEM_H       32      // Standard list item height

// =============================================================================
// Orientation Detection
// =============================================================================
inline bool isLandscapeMode(int screenW, int screenH) {
    return screenW > screenH;
}

// =============================================================================
// Orientation-Aware Button Mapping
// =============================================================================
// Physical buttons are arranged for landscape mode.
// In portrait mode, buttons are remapped for natural navigation.
//
// LANDSCAPE (800x480) - no remapping needed:
//   UP = move up, DOWN = move down, LEFT = move left, RIGHT = move right
//
// PORTRAIT (480x800) - device rotated 90° CCW from landscape:
//   Physical UP    -> Logical LEFT  (screen left is physical up)
//   Physical DOWN  -> Logical RIGHT (screen right is physical down)
//   Physical LEFT  -> Logical DOWN  (screen down is physical left)
//   Physical RIGHT -> Logical UP    (screen up is physical right)

inline Button remapButtonForOrientation(Button btn, bool landscape) {
    if (landscape) return btn;  // No remapping needed
    
    // Portrait mode: remap for natural feel
    switch (btn) {
        case BTN_UP:    return BTN_LEFT;
        case BTN_DOWN:  return BTN_RIGHT;
        case BTN_LEFT:  return BTN_DOWN;
        case BTN_RIGHT: return BTN_UP;
        default:        return btn;
    }
}

// =============================================================================
// Standard Plugin Structure Template
// =============================================================================
// All plugins should implement these methods:
//
//   void init(int screenW, int screenH)
//     - Store dimensions, calculate layout, reset state
//     - Determine if landscape or portrait
//     - Calculate cell sizes, grid offsets, etc.
//
//   void draw()
//     - Draw everything (header, grid, cursor, status)
//     - Called inside display.firstPage()/nextPage() loop
//     - Should NOT call display refresh functions
//
//   bool handleInput(Button btn)
//     - Handle raw button input
//     - Remap buttons if needed for orientation
//     - Return true if button was consumed
//     - Return false to let parent handle (e.g., BTN_BACK exits)
//
//   bool update() [optional]
//     - For animations/timers
//     - Return true if display needs refresh

// =============================================================================
// Grid Layout Calculator
// =============================================================================
struct GridLayout {
    int cellSize;       // Size of each cell (square)
    int offsetX;        // X offset to center grid
    int offsetY;        // Y offset to center grid
    int cols;           // Number of columns
    int rows;           // Number of rows
    int gridW;          // Total grid width
    int gridH;          // Total grid height
    bool landscape;     // Is landscape orientation
};

inline GridLayout calculateGrid(int screenW, int screenH, int cols, int rows, 
                                bool hasHeader = true, bool hasFooter = true) {
    GridLayout g;
    g.cols = cols;
    g.rows = rows;
    g.landscape = isLandscapeMode(screenW, screenH);
    
    // Calculate available space
    int availW = screenW - 2 * PLUGIN_MARGIN;
    int availH = screenH - 2 * PLUGIN_MARGIN;
    
    int topSpace = hasHeader ? PLUGIN_HEADER_H : 0;
    int bottomSpace = hasFooter ? PLUGIN_FOOTER_H : 0;
    availH -= (topSpace + bottomSpace);
    
    // Calculate cell size (keep square)
    int cellW = availW / cols;
    int cellH = availH / rows;
    g.cellSize = min(cellW, cellH);
    
    // Calculate grid dimensions
    g.gridW = cols * g.cellSize;
    g.gridH = rows * g.cellSize;
    
    // Center the grid
    g.offsetX = (screenW - g.gridW) / 2;
    g.offsetY = topSpace + (availH - g.gridH) / 2 + PLUGIN_MARGIN;
    
    return g;
}

// =============================================================================
// UI Drawing Helpers
// =============================================================================
namespace PluginUI {

// Draw a standard header bar with title
inline void drawHeader(const char* title, int screenW) {
    display.fillRect(0, 0, screenW, PLUGIN_HEADER_H, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((screenW - tw) / 2, PLUGIN_HEADER_H - 8);
    display.print(title);
    
    display.setTextColor(GxEPD_BLACK);
}

// Draw a standard footer with status text
inline void drawFooter(const char* leftText, const char* rightText, int screenW, int screenH) {
    int y = screenH - PLUGIN_FOOTER_H;
    display.drawLine(0, y, screenW, y, GxEPD_BLACK);
    
    // Left-aligned text
    if (leftText && leftText[0]) {
        display.setCursor(PLUGIN_MARGIN, screenH - 5);
        display.print(leftText);
    }
    
    // Right-aligned text
    if (rightText && rightText[0]) {
        int16_t tx, ty;
        uint16_t tw, th;
        display.getTextBounds(rightText, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - tw - PLUGIN_MARGIN, screenH - 5);
        display.print(rightText);
    }
}

// Draw a cursor box (thick outline)
inline void drawCursor(int x, int y, int w, int h) {
    display.drawRect(x, y, w, h, GxEPD_BLACK);
    display.drawRect(x + 1, y + 1, w - 2, h - 2, GxEPD_BLACK);
    display.drawRect(x + 2, y + 2, w - 4, h - 4, GxEPD_BLACK);
}

// Draw a selection highlight (filled or inverted)
inline void drawSelection(int x, int y, int w, int h) {
    // Draw inner highlight box
    display.drawRect(x + 3, y + 3, w - 6, h - 6, GxEPD_BLACK);
    display.drawRect(x + 4, y + 4, w - 8, h - 8, GxEPD_BLACK);
}

// Draw centered text in a region
inline void drawTextCentered(const char* text, int x, int y, int w, int h) {
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(x + (w - tw) / 2, y + (h + th) / 2);
    display.print(text);
}

// Draw a menu item (with optional selection)
inline void drawMenuItem(const char* text, int x, int y, int w, int h, bool selected) {
    if (selected) {
        display.fillRect(x, y, w, h, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
    } else {
        display.drawRect(x, y, w, h, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
    }
    
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(x + PLUGIN_PADDING, y + (h + th) / 2);
    display.print(text);
    
    display.setTextColor(GxEPD_BLACK);
}

// Draw a message dialog
inline void drawDialog(const char* title, const char* msg, int screenW, int screenH) {
    int dw = min(320, screenW - 40);
    int dh = 90;
    int dx = (screenW - dw) / 2;
    int dy = (screenH - dh) / 2;
    
    // Background
    display.fillRect(dx, dy, dw, dh, GxEPD_WHITE);
    display.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
    display.drawRect(dx + 1, dy + 1, dw - 2, dh - 2, GxEPD_BLACK);
    
    // Title bar
    display.fillRect(dx + 2, dy + 2, dw - 4, 22, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(dx + 8, dy + 17);
    display.print(title);
    display.setTextColor(GxEPD_BLACK);
    
    // Message
    display.setCursor(dx + 8, dy + 45);
    display.print(msg);
    
    // Hint
    display.setCursor(dx + 8, dy + dh - 10);
    display.print("OK to continue");
}

// Draw a simple confirmation prompt
inline void drawConfirm(const char* question, int screenW, int screenH, int selected = 0) {
    int dw = min(280, screenW - 40);
    int dh = 80;
    int dx = (screenW - dw) / 2;
    int dy = (screenH - dh) / 2;
    
    display.fillRect(dx, dy, dw, dh, GxEPD_WHITE);
    display.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
    display.drawRect(dx + 1, dy + 1, dw - 2, dh - 2, GxEPD_BLACK);
    
    // Question
    display.setCursor(dx + 10, dy + 25);
    display.print(question);
    
    // Buttons
    int btnW = 60;
    int btnH = 24;
    int btnY = dy + dh - btnH - 10;
    int yesX = dx + dw/2 - btnW - 10;
    int noX = dx + dw/2 + 10;
    
    drawMenuItem("Yes", yesX, btnY, btnW, btnH, selected == 0);
    drawMenuItem("No", noX, btnY, btnW, btnH, selected == 1);
}

// Draw a checkerboard square (for chess, checkers)
inline void drawCheckerSquare(int x, int y, int size, bool dark) {
    if (dark) {
        display.fillRect(x, y, size, size, GxEPD_BLACK);
    }
    display.drawRect(x, y, size, size, GxEPD_BLACK);
}

// Draw game over screen
inline void drawGameOver(const char* result, const char* stats, int screenW, int screenH) {
    int dw = 260;
    int dh = 100;
    int dx = (screenW - dw) / 2;
    int dy = (screenH - dh) / 2;
    
    display.fillRect(dx, dy, dw, dh, GxEPD_WHITE);
    display.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
    display.drawRect(dx + 2, dy + 2, dw - 4, dh - 4, GxEPD_BLACK);
    
    // Title
    display.fillRect(dx + 4, dy + 4, dw - 8, 26, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds("GAME OVER", 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(dx + (dw - tw) / 2, dy + 22);
    display.print("GAME OVER");
    display.setTextColor(GxEPD_BLACK);
    
    // Result
    display.getTextBounds(result, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(dx + (dw - tw) / 2, dy + 50);
    display.print(result);
    
    // Stats
    if (stats && stats[0]) {
        display.getTextBounds(stats, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(dx + (dw - tw) / 2, dy + 70);
        display.print(stats);
    }
    
    // Hint
    display.setCursor(dx + 8, dy + dh - 10);
    display.print("OK: New  BACK: Exit");
}

}  // namespace PluginUI

// =============================================================================
// Common Game States
// =============================================================================
enum GameState {
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER,
    GAME_WIN,
    GAME_MENU
};

#endif // SUMI_PLUGIN_HELPERS_H
