#pragma once

#include <EInkDisplay.h>
#include <EpdFontFamily.h>
#include <ThaiCluster.h>

#include <array>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "Bitmap.h"

class ExternalFont;
class StreamingEpdFont;

class GfxRenderer {
 public:
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

 private:
  EInkDisplay& einkDisplay;
  RenderMode renderMode;
  Orientation orientation;
  std::map<int, EpdFontFamily> fontMap;
  mutable std::map<int, std::array<StreamingEpdFont*, 3>> _streamingFonts;
  ExternalFont* _externalFont = nullptr;
  using FontStyleResolver = void (*)(void* ctx, int fontId, int styleIdx);
  mutable FontStyleResolver _fontStyleResolver = nullptr;
  mutable void* _fontStyleResolverCtx = nullptr;
  static constexpr size_t MAX_WIDTH_CACHE_SIZE = 256;
  mutable std::unordered_map<uint64_t, int16_t> wordWidthCache;
  static uint8_t frameBuffer_[EInkDisplay::BUFFER_SIZE];

  // Count UTF-8 characters
  static int utf8Len(const char* text) {
    if (!text) return 0;
    int count = 0;
    while (*text) {
      if ((*text & 0xC0) != 0x80) count++;
      text++;
    }
    return count;
  }

 public:
  static constexpr int VIEWABLE_MARGIN_TOP = 9;
  static constexpr int VIEWABLE_MARGIN_RIGHT = 3;
  static constexpr int VIEWABLE_MARGIN_BOTTOM = 3;
  static constexpr int VIEWABLE_MARGIN_LEFT = 3;

  explicit GfxRenderer(EInkDisplay& einkDisplay) : einkDisplay(einkDisplay), renderMode(BW), orientation(Portrait) {}
  ~GfxRenderer() = default;

  // Setup
  void begin() {}
  void insertFont(int fontId, EpdFontFamily font) { fontMap.emplace(fontId, font); }
  void removeFont(int fontId) { fontMap.erase(fontId); }
  void clearWidthCache() { wordWidthCache.clear(); }
  void setExternalFont(ExternalFont* font) { _externalFont = font; }
  ExternalFont* getExternalFont() const { return _externalFont; }

  void setFontStyleResolver(FontStyleResolver resolver, void* ctx) {
    _fontStyleResolver = resolver;
    _fontStyleResolverCtx = ctx;
  }
  void updateFontFamily(int fontId, EpdFontFamily::Style style, const EpdFont* font) {
    auto it = fontMap.find(fontId);
    if (it != fontMap.end()) it->second.setFont(style, font);
  }

  void setStreamingFont(int fontId, EpdFontFamily::Style style, StreamingEpdFont* font) {
    int idx = (style == EpdFontFamily::BOLD_ITALIC) ? EpdFontFamily::BOLD : style;
    _streamingFonts[fontId][idx] = font;
  }
  void setStreamingFont(int fontId, StreamingEpdFont* font) { _streamingFonts[fontId][EpdFontFamily::REGULAR] = font; }
  void removeStreamingFont(int fontId) { _streamingFonts.erase(fontId); }
  StreamingEpdFont* getStreamingFont(int fontId, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    auto it = _streamingFonts.find(fontId);
    if (it == _streamingFonts.end()) return nullptr;
    int idx = (style == EpdFontFamily::BOLD_ITALIC) ? EpdFontFamily::BOLD : style;
    StreamingEpdFont* sf = it->second[idx];
    if (!sf && idx != EpdFontFamily::REGULAR && _fontStyleResolver) {
      _fontStyleResolver(_fontStyleResolverCtx, fontId, idx);
      sf = it->second[idx];
    }
    return sf ? sf : it->second[EpdFontFamily::REGULAR];
  }

  void setOrientation(const Orientation o) { orientation = o; }
  Orientation getOrientation() const { return orientation; }

  // Screen ops - fixed metrics
  int getScreenWidth() const {
    return (orientation == Portrait || orientation == PortraitInverted) ? EInkDisplay::DISPLAY_HEIGHT
                                                                       : EInkDisplay::DISPLAY_WIDTH;
  }
  int getScreenHeight() const {
    return (orientation == Portrait || orientation == PortraitInverted) ? EInkDisplay::DISPLAY_WIDTH
                                                                       : EInkDisplay::DISPLAY_HEIGHT;
  }
  void displayBuffer(EInkDisplay::RefreshMode = EInkDisplay::FAST_REFRESH, bool = false) const {}
  void displayWindow(int, int, int, int, bool = false) const {}
  void invertScreen() const {}
  void clearScreen(uint8_t = 0xFF) const {}
  void clearArea(int, int, int, int, uint8_t = 0xFF) const {}

  // Drawing - all no-ops
  void drawPixel(int, int, bool = true) const {}
  void drawLine(int, int, int, int, bool = true) const {}
  void drawRect(int, int, int, int, bool = true) const {}
  void fillRect(int, int, int, int, bool = true) const {}
  void drawImage(const uint8_t*, int, int, int, int) const {}
  void drawBitmap(const Bitmap&, int, int, int, int) const {}

  // Text - fixed-width metrics (8px per UTF-8 char)
  int getTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return utf8Len(text) * 8;
  }
  void drawCenteredText(int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}
  void drawText(int, int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}
  int getSpaceWidth(int) const { return 5; }
  int getFontAscenderSize(int) const { return 16; }
  int getLineHeight(int) const { return 20; }
  std::string truncatedText(int, const char* text, int, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return text ? text : "";
  }

  std::vector<std::string> breakWordWithHyphenation(int, const char* word, int maxWidth,
                                                     EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    std::vector<std::string> result;
    if (!word || maxWidth <= 0) return result;

    std::string w(word);
    int charWidth = 8;
    int hyphenWidth = 8;
    int maxChars = (maxWidth - hyphenWidth) / charWidth;
    if (maxChars < 1) maxChars = 1;

    // Break UTF-8 aware
    size_t i = 0;
    while (i < w.size()) {
      int count = 0;
      size_t start = i;
      while (i < w.size() && count < maxChars) {
        if ((w[i] & 0xC0) != 0x80) count++;
        i++;
        // Advance past continuation bytes
        while (i < w.size() && (w[i] & 0xC0) == 0x80) i++;
      }
      if (i < w.size()) {
        result.push_back(w.substr(start, i - start) + "-");
      } else {
        result.push_back(w.substr(start));
      }
    }
    if (result.empty()) result.push_back(w);
    return result;
  }

  std::vector<std::string> wrapTextWithHyphenation(int, const char* text, int, int,
                                                    EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    std::vector<std::string> result;
    if (text) result.emplace_back(text);
    return result;
  }

  bool fontSupportsGrayscale(int) const { return false; }

  // Thai text - use same fixed metrics
  int getThaiTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return utf8Len(text) * 8;
  }
  void drawThaiText(int, int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}

  // Arabic text
  int getArabicTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return utf8Len(text) * 8;
  }
  void drawArabicText(int, int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}

  // UI Components
  void drawButtonHints(int, const char*, const char*, const char*, const char*, bool = true) const {}

  // Grayscale - no-ops
  void setRenderMode(const RenderMode mode) { this->renderMode = mode; }
  void copyGrayscaleLsbBuffers() const {}
  void copyGrayscaleMsbBuffers() const {}
  void displayGrayBuffer(bool = false) const {}
  bool storeBwBuffer() { return true; }
  void restoreBwBuffer() {}
  void cleanupGrayscaleWithFrameBuffer() const {}

  // Low level
  uint8_t* getFrameBuffer() const { return frameBuffer_; }
  static size_t getBufferSize() { return EInkDisplay::BUFFER_SIZE; }
  void grayscaleRevert() const {}
  void getOrientedViewableTRBL(int* outTop, int* outRight, int* outBottom, int* outLeft) const {
    *outTop = VIEWABLE_MARGIN_TOP;
    *outRight = VIEWABLE_MARGIN_RIGHT;
    *outBottom = VIEWABLE_MARGIN_BOTTOM;
    *outLeft = VIEWABLE_MARGIN_LEFT;
  }
};
