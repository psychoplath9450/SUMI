#pragma once

/**
 * @file PluginRenderer.h
 * @brief GxEPD2-style drawing API adapter for SUMI GfxRenderer
 *
 * SUMI plugins draw using GxEPD2 calls: display.fillScreen(), display.print(),
 * display.drawRect(), etc. inside firstPage()/nextPage() loops.
 *
 * This adapter provides that same API surface, translating to GfxRenderer calls.
 * GfxRenderer uses a single-pass framebuffer, so firstPage()/nextPage() are no-ops.
 *
 * Usage in ported plugins:
 *   Replace `extern GxEPD2_BW<...> display;` with `extern PluginRenderer display;`
 *   Everything else works unchanged.
 */

#include <GfxRenderer.h>

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <algorithm>

#include "../config.h"

#if FEATURE_PLUGINS

namespace sumi {

// Color constants matching GxEPD2
static constexpr bool GxEPD_BLACK = true;
static constexpr bool GxEPD_WHITE = false;

class PluginRenderer {
 public:
  explicit PluginRenderer(GfxRenderer& gfx) : gfx_(gfx) {}

  // --- Screen dimensions ---
  int width() const { return gfx_.getScreenWidth(); }
  int height() const { return gfx_.getScreenHeight(); }

  // --- Screen operations ---
  void fillScreen(bool color) { gfx_.clearScreen(color ? 0x00 : 0xFF); }

  // --- GxEPD2 page loop compatibility (single-pass in GfxRenderer) ---
  void setFullWindow() {}  // no-op
  void setPartialWindow(int, int, int, int) {}  // no-op
  bool firstPage() {
    pageActive_ = true;
    return true;
  }
  bool nextPage() {
    pageActive_ = false;
    return false;  // Always single-pass — return false to exit loop
  }

  // --- Text state ---
  void setTextColor(bool color) { textBlack_ = color; }

  void setCursor(int x, int y) {
    cursorX_ = x;
    cursorY_ = y;
  }

  // We use SUMI fonts for plugins. setFont() from Adafruit GFX maps to 
  // SUMI font IDs. nullptr = small/builtin font, non-null = regular font.
  void setFont(const void* font) {
    if (font == nullptr) {
      pluginFontId_ = smallFontId_;  // Adafruit default = small font
    } else {
      pluginFontId_ = regularFontId_;  // Named SUMI fonts = regular
    }
  }

  void setFontId(int fontId) { pluginFontId_ = fontId; }
  void setSmallFontId(int fontId) { smallFontId_ = fontId; }
  void setRegularFontId(int fontId) { regularFontId_ = fontId; pluginFontId_ = fontId; }
  int fontId() const { return pluginFontId_; }

  // --- Text output ---
  // NOTE: In GxEPD2, setCursor sets the BASELINE y position.
  // GfxRenderer::drawText treats y as TOP of text and adds ascender internally.
  // So we subtract ascender to convert baseline -> top coordinate.
  void print(const char* text) {
    if (!text || !text[0]) return;
    int topY = cursorY_ - gfx_.getFontAscenderSize(pluginFontId_);
    gfx_.drawText(pluginFontId_, cursorX_, topY, text, textBlack_);
    // Advance cursor by text width
    cursorX_ += gfx_.getTextWidth(pluginFontId_, text);
  }

  void print(int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    print(buf);
  }

  void print(unsigned int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", value);
    print(buf);
  }

  void print(long value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", value);
    print(buf);
  }

  void print(float value, int decimals = 2) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
    print(buf);
  }

  void print(char c) {
    char buf[2] = {c, '\0'};
    print(buf);
  }

  void println(const char* text = "") {
    print(text);
    cursorY_ += getLineHeight();
    cursorX_ = 0;
  }

  void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    print(buf);
  }

  // --- Text measurement ---
  // Approximation of Adafruit GFX getTextBounds
  void getTextBounds(const char* text, int /*x*/, int /*y*/, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    int tw = gfx_.getTextWidth(pluginFontId_, text);
    int th = gfx_.getLineHeight(pluginFontId_);
    if (x1) *x1 = 0;
    if (y1) *y1 = static_cast<int16_t>(-gfx_.getFontAscenderSize(pluginFontId_));
    if (w) *w = static_cast<uint16_t>(tw);
    if (h) *h = static_cast<uint16_t>(th);
  }

  int getTextWidth(const char* text) { return gfx_.getTextWidth(pluginFontId_, text); }

  int getLineHeight() { return gfx_.getLineHeight(pluginFontId_); }

  // --- Drawing primitives ---
  void drawRect(int x, int y, int w, int h, bool color) { gfx_.drawRect(x, y, w, h, color); }

  void fillRect(int x, int y, int w, int h, bool color) { gfx_.fillRect(x, y, w, h, color); }

  // Round rect - approximate with regular rect (e-ink doesn't benefit much from rounded corners)
  void drawRoundRect(int x, int y, int w, int h, int r, bool color) {
    (void)r;
    gfx_.drawRect(x, y, w, h, color);
  }

  void fillRoundRect(int x, int y, int w, int h, int r, bool color) {
    (void)r;
    gfx_.fillRect(x, y, w, h, color);
  }

  // Text size multiplier (GxEPD2 compat - rarely used, default 1)
  void setTextSize(int size) { (void)size; /* single font size on e-ink */ }

  void drawLine(int x1, int y1, int x2, int y2, bool color) { gfx_.drawLine(x1, y1, x2, y2, color); }

  void drawFastHLine(int x, int y, int w, bool color) { gfx_.drawLine(x, y, x + w - 1, y, color); }

  void drawFastVLine(int x, int y, int h, bool color) { gfx_.drawLine(x, y, x, y + h - 1, color); }

  void drawPixel(int x, int y, bool color) { gfx_.drawPixel(x, y, color); }

  // Circle drawing (not in GfxRenderer — implement with Bresenham)
  void drawCircle(int cx, int cy, int r, bool color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
      gfx_.drawPixel(cx + x, cy + y, color);
      gfx_.drawPixel(cx - x, cy + y, color);
      gfx_.drawPixel(cx + x, cy - y, color);
      gfx_.drawPixel(cx - x, cy - y, color);
      gfx_.drawPixel(cx + y, cy + x, color);
      gfx_.drawPixel(cx - y, cy + x, color);
      gfx_.drawPixel(cx + y, cy - x, color);
      gfx_.drawPixel(cx - y, cy - x, color);
      if (d < 0) {
        d += 4 * x + 6;
      } else {
        d += 4 * (x - y) + 10;
        y--;
      }
      x++;
    }
  }

  void fillCircle(int cx, int cy, int r, bool color) {
    for (int y = -r; y <= r; y++) {
      int dx = static_cast<int>(sqrt(r * r - y * y));
      gfx_.drawLine(cx - dx, cy + y, cx + dx, cy + y, color);
    }
  }

  // Triangle
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
  }

  // Filled triangle (scanline fill)
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool color) {
    // Sort vertices by Y
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 == y2) { // Degenerate
      int minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
      drawFastHLine(minx, y0, maxx - minx + 1, color);
      return;
    }
    int totalH = y2 - y0;
    for (int i = 0; i <= totalH; i++) {
      bool secondHalf = (i > y1 - y0) || (y1 == y0);
      int segH = secondHalf ? (y2 - y1) : (y1 - y0);
      if (segH == 0) segH = 1;
      float alpha = (float)i / totalH;
      float beta = secondHalf ? (float)(i - (y1 - y0)) / segH : (float)i / segH;
      int ax = x0 + (int)((x2 - x0) * alpha);
      int bx = secondHalf ? x1 + (int)((x2 - x1) * beta) : x0 + (int)((x1 - x0) * beta);
      if (ax > bx) std::swap(ax, bx);
      drawFastHLine(ax, y0 + i, bx - ax + 1, color);
    }
  }

  // Cursor position query (GxEPD2 compat)
  int getCursorX() const { return cursorX_; }
  int getCursorY() const { return cursorY_; }

  // Rotation (no-op — SUMI handles rotation at display level)
  void setRotation(int r) { (void)r; }

  // Bitmap drawing (1-bit packed, MSB first — GxEPD2 format)
  void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, bool color) {
    if (!bitmap) return;
    int byteWidth = (w + 7) / 8;
    for (int j = 0; j < h; j++) {
      for (int i = 0; i < w; i++) {
        if (bitmap[j * byteWidth + i / 8] & (0x80 >> (i & 7))) {
          gfx_.drawPixel(x + i, y + j, color);
        }
      }
    }
  }

  // 7-arg drawBitmap: separate fg/bg colors (GxEPD2 compat)
  void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, bool bgColor, bool fgColor) {
    if (!bitmap) return;
    int byteWidth = (w + 7) / 8;
    for (int j = 0; j < h; j++) {
      for (int i = 0; i < w; i++) {
        bool bit = bitmap[j * byteWidth + i / 8] & (0x80 >> (i & 7));
        gfx_.drawPixel(x + i, y + j, bit ? fgColor : bgColor);
      }
    }
  }

  // --- Display refresh ---
  void display() { gfx_.displayBuffer(); }

  void displayPartial() { gfx_.displayBuffer(EInkDisplay::FAST_REFRESH); }

  // Windowed partial update
  void displayWindow(int x, int y, int w, int h) { gfx_.displayWindow(x, y, w, h); }

  // --- Direct GfxRenderer access for advanced usage ---
  GfxRenderer& gfx() { return gfx_; }
  const GfxRenderer& gfx() const { return gfx_; }

 private:
  GfxRenderer& gfx_;
  int cursorX_ = 0;
  int cursorY_ = 0;
  bool textBlack_ = true;
  bool pageActive_ = false;
  int pluginFontId_ = -731562571;  // UI_FONT_ID default
  int regularFontId_ = -731562571;
  int smallFontId_ = -731562571;  // Will be set to SMALL_FONT_ID by host
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
