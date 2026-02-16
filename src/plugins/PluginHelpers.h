#pragma once

/**
 * @file PluginHelpers.h
 * @brief Standardized helpers for all plugins
 *
 * Ported from SUMI's PluginHelpers.h — adapted to use PluginRenderer
 * instead of direct GxEPD2 calls.
 */

#include "../config.h"

#if FEATURE_PLUGINS

#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

// =============================================================================
// Plugin UI Constants
// =============================================================================
static constexpr int PLUGIN_HEADER_H = 28;
static constexpr int PLUGIN_FOOTER_H = 20;
static constexpr int PLUGIN_MARGIN = 6;
static constexpr int PLUGIN_PADDING = 4;
static constexpr int PLUGIN_ITEM_H = 32;

// =============================================================================
// Orientation Detection
// =============================================================================
inline bool isLandscapeMode(int screenW, int screenH) { return screenW > screenH; }

// =============================================================================
// Orientation-Aware Button Mapping
// =============================================================================
inline PluginButton remapButtonForOrientation(PluginButton btn, bool landscape) {
  if (landscape) return btn;
  switch (btn) {
    case PluginButton::Up:
      return PluginButton::Left;
    case PluginButton::Down:
      return PluginButton::Right;
    case PluginButton::Left:
      return PluginButton::Down;
    case PluginButton::Right:
      return PluginButton::Up;
    default:
      return btn;
  }
}

// =============================================================================
// Grid Layout Calculator
// =============================================================================
struct GridLayout {
  int cellSize;
  int offsetX;
  int offsetY;
  int cols;
  int rows;
  int gridW;
  int gridH;
  bool landscape;
};

inline GridLayout calculateGrid(int screenW, int screenH, int cols, int rows, bool hasHeader = true,
                                bool hasFooter = true) {
  GridLayout g;
  g.cols = cols;
  g.rows = rows;
  g.landscape = isLandscapeMode(screenW, screenH);

  int availW = screenW - 2 * PLUGIN_MARGIN;
  int availH = screenH - 2 * PLUGIN_MARGIN;

  int topSpace = hasHeader ? PLUGIN_HEADER_H : 0;
  int bottomSpace = hasFooter ? PLUGIN_FOOTER_H : 0;
  availH -= (topSpace + bottomSpace);

  int cellW = availW / cols;
  int cellH = availH / rows;
  g.cellSize = (cellW < cellH) ? cellW : cellH;

  g.gridW = cols * g.cellSize;
  g.gridH = rows * g.cellSize;

  g.offsetX = (screenW - g.gridW) / 2;
  g.offsetY = topSpace + (availH - g.gridH) / 2 + PLUGIN_MARGIN;

  return g;
}

// =============================================================================
// UI Drawing Helpers
// =============================================================================
namespace PluginUI {

inline void drawHeader(PluginRenderer& d, const char* title, int screenW) {
  d.fillRect(0, 0, screenW, PLUGIN_HEADER_H, GxEPD_WHITE);
  d.setTextColor(GxEPD_BLACK);

  int16_t tx, ty;
  uint16_t tw, th;
  d.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
  d.setCursor((screenW - tw) / 2, PLUGIN_HEADER_H - 8);
  d.print(title);

  d.drawFastHLine(4, PLUGIN_HEADER_H - 1, screenW - 8, GxEPD_BLACK);
  d.drawFastHLine(4, PLUGIN_HEADER_H - 2, screenW - 8, GxEPD_BLACK);
}

inline void drawFooter(PluginRenderer& d, const char* leftText, const char* rightText, int screenW, int screenH) {
  int y = screenH - PLUGIN_FOOTER_H;
  d.drawLine(0, y, screenW, y, GxEPD_BLACK);

  if (leftText && leftText[0]) {
    d.setCursor(PLUGIN_MARGIN, screenH - 5);
    d.print(leftText);
  }

  if (rightText && rightText[0]) {
    int16_t tx, ty;
    uint16_t tw, th;
    d.getTextBounds(rightText, 0, 0, &tx, &ty, &tw, &th);
    d.setCursor(screenW - tw - PLUGIN_MARGIN, screenH - 5);
    d.print(rightText);
  }
}

inline void drawCursor(PluginRenderer& d, int x, int y, int w, int h) {
  d.drawRect(x, y, w, h, GxEPD_BLACK);
  d.drawRect(x + 1, y + 1, w - 2, h - 2, GxEPD_BLACK);
  d.drawRect(x + 2, y + 2, w - 4, h - 4, GxEPD_BLACK);
}

inline void drawTextCentered(PluginRenderer& d, const char* text, int x, int y, int w, int h) {
  int16_t tx, ty;
  uint16_t tw, th;
  d.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
  d.setCursor(x + (w - tw) / 2, y + (h + th) / 2);
  d.print(text);
}

inline void drawMenuItem(PluginRenderer& d, const char* text, int x, int y, int w, int h, bool selected) {
  if (selected) {
    d.fillRect(x, y, w, h, GxEPD_BLACK);
    d.setTextColor(GxEPD_WHITE);
  } else {
    d.drawRect(x, y, w, h, GxEPD_BLACK);
    d.setTextColor(GxEPD_BLACK);
  }

  int16_t tx, ty;
  uint16_t tw, th;
  d.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
  d.setCursor(x + PLUGIN_PADDING, y + (h + th) / 2);
  d.print(text);

  d.setTextColor(GxEPD_BLACK);
}

inline void drawDialog(PluginRenderer& d, const char* title, const char* msg, int screenW, int screenH) {
  int dw = (320 < screenW - 40) ? 320 : screenW - 40;
  int dh = 90;
  int dx = (screenW - dw) / 2;
  int dy = (screenH - dh) / 2;

  d.fillRect(dx, dy, dw, dh, GxEPD_WHITE);
  d.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
  d.drawRect(dx + 1, dy + 1, dw - 2, dh - 2, GxEPD_BLACK);

  d.fillRect(dx + 2, dy + 2, dw - 4, 22, GxEPD_BLACK);
  d.setTextColor(GxEPD_WHITE);
  d.setCursor(dx + 8, dy + 17);
  d.print(title);
  d.setTextColor(GxEPD_BLACK);

  d.setCursor(dx + 8, dy + 45);
  d.print(msg);

  d.setCursor(dx + 8, dy + dh - 10);
  d.print("OK to continue");
}

inline void drawConfirm(PluginRenderer& d, const char* question, int screenW, int screenH, int selected = 0) {
  int dw = (280 < screenW - 40) ? 280 : screenW - 40;
  int dh = 80;
  int dx = (screenW - dw) / 2;
  int dy = (screenH - dh) / 2;

  d.fillRect(dx, dy, dw, dh, GxEPD_WHITE);
  d.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
  d.drawRect(dx + 1, dy + 1, dw - 2, dh - 2, GxEPD_BLACK);

  d.setCursor(dx + 10, dy + 25);
  d.print(question);

  int btnW = 60;
  int btnH = 24;
  int btnY = dy + dh - btnH - 10;
  int yesX = dx + dw / 2 - btnW - 10;
  int noX = dx + dw / 2 + 10;

  drawMenuItem(d, "Yes", yesX, btnY, btnW, btnH, selected == 0);
  drawMenuItem(d, "No", noX, btnY, btnW, btnH, selected == 1);
}

inline void drawCheckerSquare(PluginRenderer& d, int x, int y, int size, bool dark) {
  if (dark) {
    d.fillRect(x, y, size, size, GxEPD_BLACK);
  }
  d.drawRect(x, y, size, size, GxEPD_BLACK);
}

inline void drawGameOver(PluginRenderer& d, const char* result, const char* stats, int screenW, int screenH) {
  int dw = 260;
  int dh = 100;
  int dx = (screenW - dw) / 2;
  int dy = (screenH - dh) / 2;

  d.fillRect(dx, dy, dw, dh, GxEPD_WHITE);
  d.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
  d.drawRect(dx + 2, dy + 2, dw - 4, dh - 4, GxEPD_BLACK);

  d.fillRect(dx + 4, dy + 4, dw - 8, 26, GxEPD_BLACK);
  d.setTextColor(GxEPD_WHITE);
  int16_t tx, ty;
  uint16_t tw, th;
  d.getTextBounds("GAME OVER", 0, 0, &tx, &ty, &tw, &th);
  d.setCursor(dx + (dw - tw) / 2, dy + 22);
  d.print("GAME OVER");
  d.setTextColor(GxEPD_BLACK);

  d.getTextBounds(result, 0, 0, &tx, &ty, &tw, &th);
  d.setCursor(dx + (dw - tw) / 2, dy + 50);
  d.print(result);

  if (stats && stats[0]) {
    d.getTextBounds(stats, 0, 0, &tx, &ty, &tw, &th);
    d.setCursor(dx + (dw - tw) / 2, dy + 70);
    d.print(stats);
  }

  d.setCursor(dx + 8, dy + dh - 10);
  d.print("OK: New  BACK: Exit");
}

}  // namespace PluginUI

// Common Game States
enum class GameState : uint8_t {
  Playing,
  Paused,
  Over,
  Win,
  Menu,
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
