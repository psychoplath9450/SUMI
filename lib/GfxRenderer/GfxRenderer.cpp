#include "GfxRenderer.h"

#include <ArabicShaper.h>
#include <ExternalFont.h>
#include <MemoryArena.h>
#include <ScriptDetector.h>
#include <StreamingEpdFont.h>
#include <ThaiShaper.h>
#include <Utf8.h>

#include <cassert>

#ifdef ARDUINO
TaskHandle_t GfxRenderer::s_cacheTaskHandle_ = nullptr;
#endif

void GfxRenderer::warnIfNonOwner(const char* methodName) {
#ifdef ARDUINO
  TaskHandle_t bg = s_cacheTaskHandle_;
  if (!bg) return;
  if (xTaskGetCurrentTaskHandle() == bg) return;  // legitimate cacheTask call
  // Rate-limit to once every 5 s. Without this, a render-loop bug would
  // flood the serial console — and the same bug fires on every frame.
  static uint32_t lastWarnMs = 0;
  uint32_t now = millis();
  if (now - lastWarnMs < 5000) return;
  lastWarnMs = now;
  Serial.printf(
      "[GFX] BUG: %s called from main task while cacheTask is still active. "
      "Caller forgot stopBackgroundCaching(). CONCURRENCY.md C1.\n",
      methodName);
#else
  (void)methodName;
#endif
}

void GfxRenderer::insertFont(const int fontId, EpdFontFamily font) { fontMap.insert({fontId, font}); }

void GfxRenderer::removeFont(const int fontId) {
  fontMap.erase(fontId);
  _streamingFonts.erase(fontId);
  wordWidthCache.clear();
}

static inline void rotateCoordinates(const GfxRenderer::Orientation orientation, const int x, const int y,
                                     const int panelWidth, const int panelHeight,
                                     int* rotatedX, int* rotatedY) {
  // Panel dimensions are runtime values (X4=800x480, X3=792x528).
  switch (orientation) {
    case GfxRenderer::Portrait: {
      // Logical portrait → panel landscape. Rotation: 90 degrees clockwise
      *rotatedX = y;
      *rotatedY = panelHeight - 1 - x;
      break;
    }
    case GfxRenderer::LandscapeClockwise: {
      // Logical landscape rotated 180 degrees (swap top/bottom and left/right)
      *rotatedX = panelWidth - 1 - x;
      *rotatedY = panelHeight - 1 - y;
      break;
    }
    case GfxRenderer::PortraitInverted: {
      // Logical portrait → panel landscape. Rotation: 90 degrees counter-clockwise
      *rotatedX = panelWidth - 1 - y;
      *rotatedY = x;
      break;
    }
    case GfxRenderer::LandscapeCounterClockwise: {
      // Logical landscape aligned with panel orientation
      *rotatedX = x;
      *rotatedY = y;
      break;
    }
  }
}

void GfxRenderer::begin() {
  frameBuffer = einkDisplay.getFrameBuffer();
  assert(frameBuffer && "GfxRenderer::begin() called before display.begin()");

  // Migrate bitmap row buffers from heap to arena if arena is now available
  // (constructor runs before arena init, so initial allocation uses malloc)
  if (bitmapRowsOwnMemory_ && sumi::MemoryArena::isInitialized() && sumi::MemoryArena::imageRowRegion) {
    freeBitmapRowBuffers();
    allocateBitmapRowBuffers();
  }
}

void GfxRenderer::drawPixel(const int x, const int y, const bool state) const {
  // Runtime panel dimensions (X4=800x480, X3=792x528)
  const int panelWidth = einkDisplay.getDisplayWidth();
  const int panelHeight = einkDisplay.getDisplayHeight();
  const int panelWidthBytes = einkDisplay.getDisplayWidthBytes();

  int rotatedX = 0;
  int rotatedY = 0;
  rotateCoordinates(orientation, x, y, panelWidth, panelHeight, &rotatedX, &rotatedY);

  // Bounds checking against physical panel dimensions
  if (rotatedX < 0 || rotatedX >= panelWidth || rotatedY < 0 ||
      rotatedY >= panelHeight) {
    return;
  }

  // Calculate byte position and bit position
  const uint16_t byteIndex = rotatedY * panelWidthBytes + (rotatedX / 8);
  const uint8_t bitPosition = 7 - (rotatedX % 8);  // MSB first

  if (state) {
    frameBuffer[byteIndex] &= ~(1 << bitPosition);  // Clear bit
  } else {
    frameBuffer[byteIndex] |= 1 << bitPosition;  // Set bit
  }
}

int GfxRenderer::getTextWidth(const int fontId, const char* text, const EpdFontFamily::Style style) const {
  warnIfNonOwner("getTextWidth");
  if (!text || !*text) return 0;

  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return 0;
  }

  // Check cache first (significant speedup during EPUB section creation)
  const uint64_t key = makeWidthCacheKey(fontId, text, style);
  auto it = wordWidthCache.find(key);
  if (it != wordWidthCache.end()) {
    return it->second;
  }

  // Check if text contains Arabic or Thai - use specialized width calculation
  int w = 0;
  if (ScriptDetector::containsArabic(text)) {
    w = getArabicTextWidth(fontId, text, style);
  } else if (ScriptDetector::containsThai(text)) {
    w = getThaiTextWidth(fontId, text, style);
  } else if (_externalFont && _externalFont->isLoaded()) {
    // Character-by-character calculation with external font fallback
    const auto& font = fontMap.at(fontId);
    const char* ptr = text;
    uint32_t cp;
    while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&ptr)))) {
      const EpdGlyph* glyph = font.getGlyph(cp, style);
      if (glyph) {
        w += glyph->advanceX;
      } else {
        // Try external font
        const int extWidth = getExternalGlyphWidth(cp);
        if (extWidth > 0) {
          w += extWidth;
          continue;
        }
        // Unicode whitespace: measure as a space (1x or 2x) to match
        // renderChar's fallback, so width measurement and rendering stay
        // in sync — otherwise a line measured as N '?'s wide would
        // actually render as N spaces wide and layout breaks.
        const bool isUnicodeWhitespace = (cp == 0x00A0) ||
                                         (cp >= 0x2000 && cp <= 0x200A) ||
                                         (cp == 0x202F) ||
                                         (cp == 0x205F) ||
                                         (cp == 0x3000);
        if (isUnicodeWhitespace) {
          const EpdGlyph* spaceGlyph = font.getGlyph(' ', style);
          if (spaceGlyph) {
            w += spaceGlyph->advanceX;
            if (cp == 0x2003 || cp == 0x3000) w += spaceGlyph->advanceX;
            continue;
          }
        }
        // Fall back to '?' glyph width
        const EpdGlyph* fallback = font.getGlyph('?', style);
        if (fallback) {
          w += fallback->advanceX;
        }
      }
    }
  } else {
    int h = 0;
    fontMap.at(fontId).getTextDimensions(text, &w, &h, style);
  }

  // FIFO eviction (audit #42). At capacity, drop the oldest entry —
  // not the whole cache. Pre-Batch-9-followup the bulk clear() trashed
  // 256 entries on every overflow and hit-rate dropped to ~0% on
  // long pages; FIFO keeps the most recent N warm.
  if (wordWidthCache.size() >= MAX_WIDTH_CACHE_SIZE && !wordWidthOrder.empty()) {
    const uint64_t oldest = wordWidthOrder.front();
    wordWidthOrder.pop_front();
    wordWidthCache.erase(oldest);
  }

  wordWidthCache[key] = static_cast<int32_t>(w);
  wordWidthOrder.push_back(key);
  return w;
}

void GfxRenderer::drawCenteredText(const int fontId, const int y, const char* text, const bool black,
                                   const EpdFontFamily::Style style) const {
  const int x = (getScreenWidth() - getTextWidth(fontId, text, style)) / 2;
  drawText(fontId, x, y, text, black, style);
}

void GfxRenderer::drawText(const int fontId, const int x, const int y, const char* text, const bool black,
                           const EpdFontFamily::Style style) const {
  warnIfNonOwner("drawText");
  // cannot draw a NULL / empty string
  if (text == nullptr || *text == '\0') {
    return;
  }

  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return;
  }
  const auto font = fontMap.at(fontId);

  // Check if text contains Arabic script - use Arabic rendering path
  if (ScriptDetector::containsArabic(text)) {
    drawArabicText(fontId, x, y, text, black, style);
    return;
  }

  // Check if text contains Thai script - use Thai rendering path if so
  if (ScriptDetector::containsThai(text)) {
    drawThaiText(fontId, x, y, text, black, style);
    return;
  }

  // Standard rendering path for non-Thai text
  const int yPos = y + getFontAscenderSize(fontId);
  int xpos = x;

  uint32_t cp;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&text)))) {
    renderChar(font, cp, &xpos, &yPos, black, style, fontId);
  }
}

void GfxRenderer::drawLine(int x1, int y1, int x2, int y2, const bool state) const {
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;

  while (true) {
    drawPixel(x1, y1, state);

    if (x1 == x2 && y1 == y2) break;

    int e2 = 2 * err;

    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }

    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void GfxRenderer::drawRect(const int x, const int y, const int width, const int height, const bool state) const {
  drawLine(x, y, x + width - 1, y, state);
  drawLine(x + width - 1, y, x + width - 1, y + height - 1, state);
  drawLine(x + width - 1, y + height - 1, x, y + height - 1, state);
  drawLine(x, y, x, y + height - 1, state);
}

void GfxRenderer::fillRect(const int x, const int y, const int width, const int height, const bool state) const {
  for (int fillY = y; fillY < y + height; fillY++) {
    drawLine(x, fillY, x + width - 1, fillY, state);
  }
}

void GfxRenderer::drawImage(const uint8_t bitmap[], const int x, const int y, const int width, const int height) const {
  warnIfNonOwner("drawImage");
  // TODO: Rotate bits
  int rotatedX = 0;
  int rotatedY = 0;
  rotateCoordinates(orientation, x, y, einkDisplay.getDisplayWidth(), einkDisplay.getDisplayHeight(),
                    &rotatedX, &rotatedY);
  einkDisplay.drawImage(bitmap, rotatedX, rotatedY, width, height);
}

void GfxRenderer::drawBitmap(const Bitmap& bitmap, const int x, const int y, const int maxWidth,
                             const int maxHeight) const {
  float scale = 1.0f;
  bool isScaled = false;
  if (maxWidth > 0 && bitmap.getWidth() > maxWidth) {
    scale = static_cast<float>(maxWidth) / static_cast<float>(bitmap.getWidth());
    isScaled = true;
  }
  if (maxHeight > 0 && bitmap.getHeight() > maxHeight) {
    scale = std::min(scale, static_cast<float>(maxHeight) / static_cast<float>(bitmap.getHeight()));
    isScaled = true;
  }

  // Use pre-allocated row buffers to avoid per-call heap allocation
  // Verify bitmap fits within our pre-allocated buffer sizes
  const size_t outputRowSize = static_cast<size_t>((bitmap.getWidth() + 3) / 4);
  const size_t rowBytesSize = static_cast<size_t>(bitmap.getRowBytes());

  if (!bitmapOutputRow_ || !bitmapRowBytes_) {
    Serial.printf("[%lu] [GFX] !! Bitmap row buffers not allocated\n", millis());
    return;
  }

  if (outputRowSize > BITMAP_OUTPUT_ROW_SIZE || rowBytesSize > BITMAP_ROW_BYTES_SIZE) {
    Serial.printf("[%lu] [GFX] !! Bitmap too large for pre-allocated buffers (%zu > %zu or %zu > %zu)\n", millis(),
                  outputRowSize, BITMAP_OUTPUT_ROW_SIZE, rowBytesSize, BITMAP_ROW_BYTES_SIZE);
    return;
  }

  for (int bmpY = 0; bmpY < bitmap.getHeight(); bmpY++) {
    // The BMP's (0, 0) is the bottom-left corner (if the height is positive, top-left if negative).
    // Screen's (0, 0) is the top-left corner.
    int bmpOffset = bitmap.isTopDown() ? bmpY : bitmap.getHeight() - 1 - bmpY;
    int screenY = isScaled ? y + static_cast<int>(std::floor(bmpOffset * scale)) : y + bmpOffset;
    if (screenY >= getScreenHeight()) {
      break;
    }

    if (bitmap.readRow(bitmapOutputRow_, bitmapRowBytes_, bmpY) != BmpReaderError::Ok) {
      Serial.printf("[%lu] [GFX] Failed to read row %d from bitmap\n", millis(), bmpY);
      return;
    }

    for (int bmpX = 0; bmpX < bitmap.getWidth(); bmpX++) {
      int screenX = isScaled ? x + static_cast<int>(std::floor(bmpX * scale)) : x + bmpX;
      if (screenX >= getScreenWidth()) {
        break;
      }

      const uint8_t val = bitmapOutputRow_[bmpX / 4] >> (6 - ((bmpX * 2) % 8)) & 0x3;

      if (renderMode == BW && val < 3) {
        drawPixel(screenX, screenY);
      } else if (renderMode == GRAYSCALE_MSB && (val == 1 || val == 2)) {
        drawPixel(screenX, screenY, false);
      } else if (renderMode == GRAYSCALE_LSB && val == 1) {
        drawPixel(screenX, screenY, false);
      }
    }
  }
}

static unsigned long renderStartMs = 0;

void GfxRenderer::clearScreen(const uint8_t color) const {
  warnIfNonOwner("clearScreen");
  renderStartMs = millis();
  einkDisplay.clearScreen(color);
}

void GfxRenderer::clearArea(const int x, const int y, const int width, const int height, const uint8_t color) const {
  if (width <= 0 || height <= 0) {
    return;
  }

  // Runtime panel dimensions (X4=800x480, X3=792x528)
  const int panelWidth = einkDisplay.getDisplayWidth();
  const int panelHeight = einkDisplay.getDisplayHeight();
  const int panelWidthBytes = einkDisplay.getDisplayWidthBytes();

  // Validate bounds - region entirely outside display
  if (x >= panelWidth || y >= panelHeight || x + width <= 0 || y + height <= 0) {
    return;
  }

  // Clamp to display boundaries
  const int x_start = std::max(x, 0);
  const int y_start = std::max(y, 0);
  const int x_end = std::min(x + width - 1, panelWidth - 1);
  const int y_end = std::min(y + height - 1, panelHeight - 1);

  // Calculate byte boundaries (8 pixels per byte)
  const int x_byte_start = x_start / 8;
  const int x_byte_end = x_end / 8;
  const int byte_width = x_byte_end - x_byte_start + 1;

  // Clear each row in the region
  for (int row = y_start; row <= y_end; row++) {
    const uint32_t buffer_offset = row * panelWidthBytes + x_byte_start;
    memset(&frameBuffer[buffer_offset], color, byte_width);
  }
}

void GfxRenderer::invertScreen() const {
  const uint32_t bufSize = einkDisplay.getBufferSize();
  for (uint32_t i = 0; i < bufSize; i++) {
    frameBuffer[i] = ~frameBuffer[i];
  }
}

void GfxRenderer::displayBuffer(EInkDisplay::RefreshMode refreshMode, bool turnOffScreen) const {
  warnIfNonOwner("displayBuffer");
  if (renderStartMs > 0) {
    Serial.printf("[%lu] [GFX] Render took %lu ms\n", millis(), millis() - renderStartMs);
    renderStartMs = 0;
  }

  // Periodic refresh: auto-promote FAST_REFRESH to HALF_REFRESH to clear
  // accumulated e-ink ghosting in states that don't manage their own counter
  if (periodicRefreshInterval_ > 0 && refreshMode == EInkDisplay::FAST_REFRESH) {
    fastRefreshCount_++;
    if (fastRefreshCount_ >= periodicRefreshInterval_) {
      refreshMode = EInkDisplay::HALF_REFRESH;
      fastRefreshCount_ = 0;
    }
  }

  // X3 grayscale washout fix (CrossPoint 1.3.0 parity). On X3, every
  // HALF_REFRESH must force a full sync + one settle pass; otherwise the
  // fast-diff path accumulates residue and AA pixels turn into haze after
  // a handful of partial refreshes. requestResync() self-gates on
  // _x3Mode (no-op on X4). The README's "X3 grayscale/AA may show
  // artifacts — turn off Text Anti-Aliasing" caveat goes away once this
  // is in place. EInkDisplay already has the consumer side wired up
  // (lib/EInkDisplay/src/EInkDisplay.cpp:813,873,919) — nothing in src/
  // had been calling requestResync() yet.
  if (refreshMode == EInkDisplay::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.displayBuffer(refreshMode, turnOffScreen || fadingFix_);
}

void GfxRenderer::displayWindow(int x, int y, int width, int height, bool turnOffScreen) const {
  warnIfNonOwner("displayWindow");
  einkDisplay.displayWindow(x, y, width, height, turnOffScreen || fadingFix_);
}

std::string GfxRenderer::truncatedText(const int fontId, const char* text, const int maxWidth,
                                       const EpdFontFamily::Style style) const {
  std::string item = text;
  int itemWidth = getTextWidth(fontId, item.c_str(), style);
  while (itemWidth > maxWidth && item.length() > 8) {
    // Remove "..." first, then remove one UTF-8 char, then add "..." back
    if (item.length() >= 3 && item.substr(item.length() - 3) == "...") {
      item.resize(item.length() - 3);
    }
    utf8RemoveLastChar(item);
    item.append("...");
    itemWidth = getTextWidth(fontId, item.c_str(), style);
  }
  return item;
}

std::vector<std::string> GfxRenderer::breakWordWithHyphenation(const int fontId, const char* word, const int maxWidth,
                                                               const EpdFontFamily::Style style) const {
  std::vector<std::string> chunks;
  if (!word || *word == '\0') return chunks;

  std::string remaining = word;
  while (!remaining.empty()) {
    const int remainingWidth = getTextWidth(fontId, remaining.c_str(), style);
    if (remainingWidth <= maxWidth) {
      chunks.push_back(remaining);
      break;
    }

    // Find max chars that fit with hyphen
    std::string chunk;
    const char* ptr = remaining.c_str();
    const char* lastGoodPos = ptr;

    while (*ptr) {
      const char* nextChar = ptr;
      utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&nextChar));

      std::string testChunk = chunk;
      testChunk.append(ptr, nextChar - ptr);
      const int testWidth = getTextWidth(fontId, (testChunk + "-").c_str(), style);

      if (testWidth > maxWidth && !chunk.empty()) break;

      chunk = testChunk;
      lastGoodPos = nextChar;
      ptr = nextChar;
    }

    if (chunk.empty()) {
      // Single char too wide - force it
      const char* nextChar = remaining.c_str();
      utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&nextChar));
      chunk.append(remaining.c_str(), nextChar - remaining.c_str());
      lastGoodPos = nextChar;
    }

    if (lastGoodPos < remaining.c_str() + remaining.size()) {
      chunks.push_back(chunk + "-");
      remaining = remaining.substr(lastGoodPos - remaining.c_str());
    } else {
      chunks.push_back(chunk);
      remaining.clear();
    }
  }
  return chunks;
}

std::vector<std::string> GfxRenderer::wrapTextWithHyphenation(const int fontId, const char* text, const int maxWidth,
                                                              const int maxLines,
                                                              const EpdFontFamily::Style style) const {
  std::vector<std::string> lines;
  if (!text || *text == '\0' || maxLines <= 0) {
    return lines;
  }

  std::string remaining = text;

  while (!remaining.empty() && static_cast<int>(lines.size()) < maxLines) {
    const bool isLastLine = static_cast<int>(lines.size()) == maxLines - 1;

    // Check if remaining text fits on current line
    const int remainingWidth = getTextWidth(fontId, remaining.c_str(), style);
    if (remainingWidth <= maxWidth) {
      lines.push_back(remaining);
      break;
    }

    // Find where to break the line
    std::string currentLine;
    const char* ptr = remaining.c_str();
    const char* lastBreakPoint = nullptr;
    std::string lineAtBreak;

    while (*ptr) {
      // Skip to end of current word. Stop at ASCII space or any unicode
      // whitespace codepoint (NBSP, narrow NBSP, EN/EM/thin space, etc.)
      // so French/Spanish typography splits at its actual word boundaries
      // instead of accumulating 'mot\u202F:' as a single monster word
      // that then trips breakWordWithHyphenation.
      const char* wordEnd = ptr;
      while (*wordEnd && *wordEnd != ' ') {
        const int uws = utf8UnicodeWhitespaceBytes(wordEnd, 4);
        if (uws > 0) break;
        utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&wordEnd));
      }

      // Build line up to this word
      std::string testLine = currentLine;
      if (!testLine.empty()) {
        testLine += ' ';
      }
      testLine.append(ptr, wordEnd - ptr);

      const int testWidth = getTextWidth(fontId, testLine.c_str(), style);

      if (testWidth <= maxWidth) {
        // Word fits, update current line and remember this as potential break point
        currentLine = testLine;
        lastBreakPoint = wordEnd;
        lineAtBreak = currentLine;

        // Move past the word and any whitespace (ASCII or unicode).
        ptr = wordEnd;
        while (*ptr) {
          if (*ptr == ' ') { ptr++; continue; }
          const int uws = utf8UnicodeWhitespaceBytes(ptr, 4);
          if (uws > 0) { ptr += uws; continue; }
          break;
        }
      } else {
        // Word doesn't fit
        if (currentLine.empty()) {
          // Word alone is too long - use helper
          auto wordChunks = breakWordWithHyphenation(fontId, std::string(ptr, wordEnd - ptr).c_str(), maxWidth, style);
          for (size_t i = 0; i < wordChunks.size() && static_cast<int>(lines.size()) < maxLines; i++) {
            lines.push_back(wordChunks[i]);
          }
          // Update remaining to skip past the word and any whitespace.
          ptr = wordEnd;
          while (*ptr) {
            if (*ptr == ' ') { ptr++; continue; }
            const int uws = utf8UnicodeWhitespaceBytes(ptr, 4);
            if (uws > 0) { ptr += uws; continue; }
            break;
          }
          remaining = ptr;
          break;
        } else if (lastBreakPoint) {
          // Line has content, break at last good point
          lines.push_back(lineAtBreak);
          const char* nextStart = lastBreakPoint;
          while (*nextStart) {
            if (*nextStart == ' ') { nextStart++; continue; }
            const int uws = utf8UnicodeWhitespaceBytes(nextStart, 4);
            if (uws > 0) { nextStart += uws; continue; }
            break;
          }
          remaining = nextStart;
          break;
        }
      }

      if (*ptr == '\0') {
        // Reached end of text
        if (!currentLine.empty()) {
          lines.push_back(currentLine);
        }
        remaining.clear();
        break;
      }
    }

    // Handle last line truncation
    if (isLastLine && !remaining.empty() && static_cast<int>(lines.size()) == maxLines) {
      // Last line but text remains - truncate with "..."
      std::string& lastLine = lines.back();
      lastLine = truncatedText(fontId, lastLine.c_str(), maxWidth, style);
    }
  }

  // If we have remaining text and hit maxLines, truncate the last line
  if (!remaining.empty() && static_cast<int>(lines.size()) == maxLines) {
    std::string& lastLine = lines.back();
    // Append remaining text and truncate
    if (getTextWidth(fontId, lastLine.c_str(), style) < maxWidth) {
      std::string combined = lastLine + " " + remaining;
      lastLine = truncatedText(fontId, combined.c_str(), maxWidth, style);
    } else {
      lastLine = truncatedText(fontId, lastLine.c_str(), maxWidth, style);
    }
  }

  return lines;
}

// Note: Internal driver treats screen in command orientation; this library exposes a logical orientation
int GfxRenderer::getScreenWidth() const {
  // Native panel dimensions at runtime — X4 = 800x480, X3 = 792x528.
  // Portrait rotates so "screen width" is panel height and vice versa.
  const int panelWidth = einkDisplay.getDisplayWidth();
  const int panelHeight = einkDisplay.getDisplayHeight();
  switch (orientation) {
    case Portrait:
    case PortraitInverted:
      // Portrait: X4=480 wide, X3=528 wide (panel height becomes logical width)
      return panelHeight;
    case LandscapeClockwise:
    case LandscapeCounterClockwise:
      // Landscape: X4=800 wide, X3=792 wide (panel width = logical width)
      return panelWidth;
  }
  return panelHeight;
}

int GfxRenderer::getScreenHeight() const {
  // Native panel dimensions at runtime — X4 = 800x480, X3 = 792x528.
  const int panelWidth = einkDisplay.getDisplayWidth();
  const int panelHeight = einkDisplay.getDisplayHeight();
  switch (orientation) {
    case Portrait:
    case PortraitInverted:
      // Portrait: X4=800 tall, X3=792 tall (panel width becomes logical height)
      return panelWidth;
    case LandscapeClockwise:
    case LandscapeCounterClockwise:
      // Landscape: X4=480 tall, X3=528 tall (panel height = logical height)
      return panelHeight;
  }
  return panelWidth;
}

int GfxRenderer::getSpaceWidth(const int fontId) const {
  auto it = fontMap.find(fontId);
  if (it == fontMap.end()) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return 0;
  }

  const EpdGlyph* glyph = it->second.getGlyph(' ', EpdFontFamily::REGULAR);
  return glyph ? glyph->advanceX : 0;
}

int GfxRenderer::getFontAscenderSize(const int fontId) const {
  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return 0;
  }

  return fontMap.at(fontId).getData(EpdFontFamily::REGULAR)->ascender;
}

int GfxRenderer::getLineHeight(const int fontId) const {
  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return 0;
  }

  return fontMap.at(fontId).getData(EpdFontFamily::REGULAR)->advanceY;
}

bool GfxRenderer::fontSupportsGrayscale(const int fontId) const {
  auto it = fontMap.find(fontId);
  if (it == fontMap.end()) {
    return false;
  }
  const EpdFontData* data = it->second.getData();
  return data != nullptr && data->is2Bit;
}

void GfxRenderer::drawButtonHints(const int fontId, const char* btn1, const char* btn2, const char* btn3,
                                  const char* btn4, const bool black) const {
  const int pageHeight = getScreenHeight();
  constexpr int buttonWidth = 106;
  constexpr int buttonHeight = 46;
  constexpr int buttonY = 50;      // Distance from bottom
  constexpr int textYOffset = 10;  // Distance from top of button to text baseline
  constexpr int buttonPositions[] = {25, 130, 245, 350};
  const char* labels[] = {btn1, btn2, btn3, btn4};

  for (int i = 0; i < 4; i++) {
    // Only draw if the label is non-empty
    if (labels[i] != nullptr && labels[i][0] != '\0') {
      const int x = buttonPositions[i];
      drawRect(x, pageHeight - buttonY, buttonWidth, buttonHeight, black);
      const int textWidth = getTextWidth(fontId, labels[i]);
      const int textX = x + (buttonWidth - 1 - textWidth) / 2;
      drawText(fontId, textX, pageHeight - buttonY + textYOffset, labels[i], black);
    }
  }
}

uint8_t* GfxRenderer::getFrameBuffer() const { return frameBuffer; }

size_t GfxRenderer::getBufferSize() const { return einkDisplay.getBufferSize(); }

void GfxRenderer::grayscaleRevert() const { einkDisplay.grayscaleRevert(); }

void GfxRenderer::copyGrayscaleLsbBuffers() const { einkDisplay.copyGrayscaleLsbBuffers(frameBuffer); }

void GfxRenderer::copyGrayscaleMsbBuffers() const { einkDisplay.copyGrayscaleMsbBuffers(frameBuffer); }

void GfxRenderer::displayGrayBuffer(bool turnOffScreen) const {
  // X3: force a fresh sync before flushing the grayscale composite. The
  // 2-pass AA pipeline (LSB + MSB grayscale renders) is the path that
  // most reliably triggers X3 washout under partial-refresh accumulation
  // — see displayBuffer above for the broader rationale. requestResync()
  // is a no-op on X4.
  einkDisplay.requestResync(1);
  einkDisplay.displayGrayBuffer(turnOffScreen || fadingFix_);
}

void GfxRenderer::freeBwBufferChunks() {
  for (auto& bwBufferChunk : bwBufferChunks) {
    if (bwBufferChunk) {
      free(bwBufferChunk);
      bwBufferChunk = nullptr;
    }
  }
}

/**
 * This should be called before grayscale buffers are populated.
 * A `restoreBwBuffer` call should always follow the grayscale render if this method was called.
 * Uses chunked allocation to avoid needing 48KB of contiguous memory.
 * Returns true if buffer was stored successfully, false if allocation failed.
 */
bool GfxRenderer::storeBwBuffer() {
  // Allocate and copy each chunk
  for (size_t i = 0; i < BW_BUFFER_NUM_CHUNKS; i++) {
    // Check if any chunks are already allocated
    if (bwBufferChunks[i]) {
      Serial.printf("[%lu] [GFX] !! BW buffer chunk %zu already stored - this is likely a bug, freeing chunk\n",
                    millis(), i);
      free(bwBufferChunks[i]);
      bwBufferChunks[i] = nullptr;
    }

    const size_t offset = i * BW_BUFFER_CHUNK_SIZE;
    bwBufferChunks[i] = static_cast<uint8_t*>(malloc(BW_BUFFER_CHUNK_SIZE));

    if (!bwBufferChunks[i]) {
      Serial.printf("[%lu] [GFX] !! Failed to allocate BW buffer chunk %zu (%zu bytes)\n", millis(), i,
                    BW_BUFFER_CHUNK_SIZE);
      // Free previously allocated chunks
      freeBwBufferChunks();
      return false;
    }

    memcpy(bwBufferChunks[i], frameBuffer + offset, BW_BUFFER_CHUNK_SIZE);
  }

  Serial.printf("[%lu] [GFX] Stored BW buffer in %zu chunks (%zu bytes each)\n", millis(), BW_BUFFER_NUM_CHUNKS,
                BW_BUFFER_CHUNK_SIZE);
  return true;
}

/**
 * This can only be called if `storeBwBuffer` was called prior to the grayscale render.
 * It should be called to restore the BW buffer state after grayscale rendering is complete.
 * Uses chunked restoration to match chunked storage.
 */
void GfxRenderer::restoreBwBuffer() {
  // Check if any all chunks are allocated
  bool missingChunks = false;
  for (const auto& bwBufferChunk : bwBufferChunks) {
    if (!bwBufferChunk) {
      missingChunks = true;
      break;
    }
  }

  if (missingChunks) {
    freeBwBufferChunks();
    return;
  }

  for (size_t i = 0; i < BW_BUFFER_NUM_CHUNKS; i++) {
    // Check if chunk is missing
    if (!bwBufferChunks[i]) {
      Serial.printf("[%lu] [GFX] !! BW buffer chunks not stored - this is likely a bug\n", millis());
      freeBwBufferChunks();
      return;
    }

    const size_t offset = i * BW_BUFFER_CHUNK_SIZE;
    memcpy(frameBuffer + offset, bwBufferChunks[i], BW_BUFFER_CHUNK_SIZE);
  }

  einkDisplay.cleanupGrayscaleBuffers(frameBuffer);

  freeBwBufferChunks();
  Serial.printf("[%lu] [GFX] Restored and freed BW buffer chunks\n", millis());
}

/**
 * Cleanup grayscale buffers using the current frame buffer.
 * Use this when BW buffer was re-rendered instead of stored/restored.
 */
void GfxRenderer::cleanupGrayscaleWithFrameBuffer() const { einkDisplay.cleanupGrayscaleBuffers(frameBuffer); }

void GfxRenderer::renderChar(const EpdFontFamily& fontFamily, const uint32_t cp, int* x, const int* y,
                             const bool pixelState, const EpdFontFamily::Style style, const int fontId) const {
  const EpdGlyph* glyph = fontFamily.getGlyph(cp, style);
  if (!glyph) {
    // Try external font fallback (for CJK characters)
    if (_externalFont && _externalFont->isLoaded()) {
      renderExternalGlyph(cp, x, *y, pixelState);
      return;
    }

    // For whitespace characters missing from font, advance by space width
    // instead of rendering '?'. Covers the full set of Unicode whitespace
    // codepoints that lib/Utf8::utf8UnicodeWhitespaceBytes recognises, so
    // even if a parser misses one the glyph path still degrades gracefully.
    //   U+00A0 NBSP, U+2000..U+200A (en/em/thin/hair), U+202F narrow NBSP,
    //   U+205F medium math, U+3000 ideographic.
    const bool isUnicodeWhitespace = (cp == 0x00A0) ||
                                     (cp >= 0x2000 && cp <= 0x200A) ||
                                     (cp == 0x202F) ||
                                     (cp == 0x205F) ||
                                     (cp == 0x3000);
    if (isUnicodeWhitespace) {
      const EpdGlyph* spaceGlyph = fontFamily.getGlyph(' ', style);
      if (spaceGlyph) {
        *x += spaceGlyph->advanceX;
        // U+2003 (EM SPACE) and U+3000 (IDEOGRAPHIC SPACE) are ~2x wide.
        if (cp == 0x2003 || cp == 0x3000) *x += spaceGlyph->advanceX;
        return;
      }
    }
    glyph = fontFamily.getGlyph('?', style);
  }

  // no glyph?
  if (!glyph) {
    Serial.printf("[%lu] [GFX] No glyph for codepoint %d\n", millis(), cp);
    return;
  }

  const int is2Bit = fontFamily.getData(style)->is2Bit;
  const uint32_t offset = glyph->dataOffset;
  const uint8_t width = glyph->width;
  const uint8_t height = glyph->height;
  const int left = glyph->left;

  // Bitmap lookup bypasses getStreamingFont() (no lazy resolver) for performance.
  // Font variants are already resolved during layout (word width measurement).
  const uint8_t* bitmap = nullptr;
  auto streamingIt = _streamingFonts.find(fontId);
  if (streamingIt != _streamingFonts.end()) {
    int idx = (style == EpdFontFamily::BOLD_ITALIC) ? EpdFontFamily::BOLD : style;
    StreamingEpdFont* sf = streamingIt->second[idx];
    if (!sf) sf = streamingIt->second[EpdFontFamily::REGULAR];
    if (sf) {
      bitmap = sf->getGlyphBitmap(glyph);
    }
  }
  if (!bitmap && fontFamily.getData(style)->bitmap) {
    // Fall back to standard EpdFont bitmap access
    bitmap = &fontFamily.getData(style)->bitmap[offset];
  }

  if (bitmap != nullptr) {
    const int screenHeight = getScreenHeight();
    const int screenWidth = getScreenWidth();

    for (int glyphY = 0; glyphY < height; glyphY++) {
      const int screenY = *y - glyph->top + glyphY;
      if (screenY < 0 || screenY >= screenHeight) continue;

      for (int glyphX = 0; glyphX < width; glyphX++) {
        const int screenX = *x + left + glyphX;
        if (screenX < 0 || screenX >= screenWidth) continue;

        const int pixelPosition = glyphY * width + glyphX;

        if (is2Bit) {
          const uint8_t byte = bitmap[pixelPosition / 4];
          const uint8_t bit_index = (3 - pixelPosition % 4) * 2;
          // the direct bit from the font is 0 -> white, 1 -> light gray, 2 -> dark gray, 3 -> black
          // we swap this to better match the way images and screen think about colors:
          // 0 -> black, 1 -> dark grey, 2 -> light grey, 3 -> white
          uint8_t bmpVal = 3 - ((byte >> bit_index) & 0x3);

          // Text darkness: shift gray levels toward black
          // 0=Normal: no change
          // 1=Dark: light gray(2) -> dark gray(1)
          // 2=Extra Dark: light gray(2) -> black(0), dark gray(1) -> black(0)
          // 3=Maximum: all non-white -> black(0)
          if (textDarkness_ >= 3) {
            if (bmpVal < 3) bmpVal = 0;
          } else if (textDarkness_ == 2) {
            if (bmpVal == 2) bmpVal = 0;
            else if (bmpVal == 1) bmpVal = 0;
          } else if (textDarkness_ == 1) {
            if (bmpVal == 2) bmpVal = 1;
          }

          if (renderMode == BW && bmpVal < 3) {
            // Black (also paints over the grays in BW mode)
            drawPixel(screenX, screenY, pixelState);
          } else if (renderMode == GRAYSCALE_MSB && (bmpVal == 1 || bmpVal == 2)) {
            // Light gray (also mark the MSB if it's going to be a dark gray too)
            // We have to flag pixels in reverse for the gray buffers, as 0 leave alone, 1 update
            drawPixel(screenX, screenY, false);
          } else if (renderMode == GRAYSCALE_LSB && bmpVal == 1) {
            // Dark gray
            drawPixel(screenX, screenY, false);
          }
        } else {
          const uint8_t byte = bitmap[pixelPosition / 8];
          const uint8_t bit_index = 7 - (pixelPosition % 8);

          if ((byte >> bit_index) & 1) {
            drawPixel(screenX, screenY, pixelState);
          }
        }
      }
    }
  }

  *x += glyph->advanceX;
}

void GfxRenderer::getOrientedViewableTRBL(int* outTop, int* outRight, int* outBottom, int* outLeft) const {
  switch (orientation) {
    case Portrait:
      *outTop = VIEWABLE_MARGIN_TOP;
      *outRight = VIEWABLE_MARGIN_RIGHT;
      *outBottom = VIEWABLE_MARGIN_BOTTOM;
      *outLeft = VIEWABLE_MARGIN_LEFT;
      break;
    case LandscapeClockwise:
      *outTop = VIEWABLE_MARGIN_LEFT;
      *outRight = VIEWABLE_MARGIN_TOP;
      *outBottom = VIEWABLE_MARGIN_RIGHT;
      *outLeft = VIEWABLE_MARGIN_BOTTOM;
      break;
    case PortraitInverted:
      *outTop = VIEWABLE_MARGIN_BOTTOM;
      *outRight = VIEWABLE_MARGIN_LEFT;
      *outBottom = VIEWABLE_MARGIN_TOP;
      *outLeft = VIEWABLE_MARGIN_RIGHT;
      break;
    case LandscapeCounterClockwise:
      *outTop = VIEWABLE_MARGIN_RIGHT;
      *outRight = VIEWABLE_MARGIN_BOTTOM;
      *outBottom = VIEWABLE_MARGIN_LEFT;
      *outLeft = VIEWABLE_MARGIN_TOP;
      break;
  }
}

void GfxRenderer::allocateBitmapRowBuffers() {
  // Use arena imageRowRegion when available (3400 bytes fits in 4KB region)
  if (sumi::MemoryArena::isInitialized() && sumi::MemoryArena::imageRowRegion) {
    bitmapOutputRow_ = sumi::MemoryArena::imageRowRegion;
    bitmapRowBytes_ = sumi::MemoryArena::imageRowRegion + BITMAP_OUTPUT_ROW_SIZE;
    bitmapRowsOwnMemory_ = false;
    return;
  }

  bitmapOutputRow_ = static_cast<uint8_t*>(malloc(BITMAP_OUTPUT_ROW_SIZE));
  bitmapRowBytes_ = static_cast<uint8_t*>(malloc(BITMAP_ROW_BYTES_SIZE));
  bitmapRowsOwnMemory_ = true;

  if (!bitmapOutputRow_ || !bitmapRowBytes_) {
    Serial.printf("[%lu] [GFX] !! Failed to allocate bitmap row buffers\n", millis());
    freeBitmapRowBuffers();
  }
}

void GfxRenderer::freeBitmapRowBuffers() {
  if (bitmapRowsOwnMemory_) {
    free(bitmapOutputRow_);
    free(bitmapRowBytes_);
  }
  bitmapOutputRow_ = nullptr;
  bitmapRowBytes_ = nullptr;
  bitmapRowsOwnMemory_ = false;
}

void GfxRenderer::renderExternalGlyph(const uint32_t cp, int* x, const int y, const bool pixelState) const {
  if (!_externalFont || !_externalFont->isLoaded()) {
    return;
  }

  const uint8_t* bitmap = _externalFont->getGlyph(cp);
  if (!bitmap) {
    // Glyph not found - advance by 1/3 char width as fallback
    *x += _externalFont->getCharWidth() / 3;
    return;
  }

  uint8_t minX = 0;
  uint8_t advanceX = 0;
  if (!_externalFont->getGlyphMetrics(cp, &minX, &advanceX)) {
    advanceX = _externalFont->getCharWidth();
  }

  const int w = _externalFont->getCharWidth();
  const int h = _externalFont->getCharHeight();
  const int bytesPerRow = _externalFont->getBytesPerRow();
  const int screenHeight = getScreenHeight();
  const int screenWidth = getScreenWidth();

  for (int glyphY = 0; glyphY < h; glyphY++) {
    const int screenY = y + glyphY;
    if (screenY < 0 || screenY >= screenHeight) continue;

    for (int glyphX = minX; glyphX < w; glyphX++) {
      const int screenX = *x + glyphX - minX;
      if (screenX < 0 || screenX >= screenWidth) continue;

      const int byteIdx = glyphY * bytesPerRow + (glyphX / 8);
      const int bitIdx = 7 - (glyphX % 8);
      if ((bitmap[byteIdx] >> bitIdx) & 1) {
        drawPixel(screenX, screenY, pixelState);
      }
    }
  }

  *x += advanceX;
}

int GfxRenderer::getExternalGlyphWidth(const uint32_t cp) const {
  if (!_externalFont || !_externalFont->isLoaded()) {
    return 0;
  }

  // Ensure glyph is loaded to get metrics; return 0 if not found
  // so caller falls back to builtin font width
  if (!_externalFont->getGlyph(cp)) {
    return 0;
  }

  uint8_t minX = 0;
  uint8_t advanceX = _externalFont->getCharWidth();
  if (_externalFont->getGlyphMetrics(cp, &minX, &advanceX)) {
    return advanceX;
  }

  // Fallback to full character width
  return _externalFont->getCharWidth();
}

// ============================================================================
// Thai Text Rendering
// ============================================================================

int GfxRenderer::getThaiTextWidth(const int fontId, const char* text, const EpdFontFamily::Style style) const {
  if (text == nullptr || *text == '\0') {
    return 0;
  }

  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return 0;
  }

  const auto& font = fontMap.at(fontId);
  int totalWidth = 0;

  // Build clusters and sum their widths
  auto clusters = ThaiShaper::ThaiClusterBuilder::buildClusters(text);

  for (const auto& cluster : clusters) {
    for (const auto& glyph : cluster.glyphs) {
      if (!glyph.zeroAdvance) {
        const EpdGlyph* glyphData = font.getGlyph(glyph.codepoint, style);
        if (!glyphData) {
          glyphData = font.getGlyph('?', style);
        }
        if (glyphData) {
          totalWidth += glyphData->advanceX;
        }
      }
    }
  }

  return totalWidth;
}

void GfxRenderer::drawThaiText(const int fontId, const int x, const int y, const char* text, const bool black,
                               const EpdFontFamily::Style style) const {
  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return;
  }

  const int yPos = y + getFontAscenderSize(fontId);
  int xpos = x;

  const auto font = fontMap.at(fontId);

  // Build Thai clusters from the text
  auto clusters = ThaiShaper::ThaiClusterBuilder::buildClusters(text);

  // Render each cluster
  for (const auto& cluster : clusters) {
    renderThaiCluster(font, cluster, &xpos, yPos, black, style, fontId);
  }
}

void GfxRenderer::renderThaiCluster(const EpdFontFamily& fontFamily, const ThaiShaper::ThaiCluster& cluster, int* x,
                                    const int y, const bool pixelState, const EpdFontFamily::Style style,
                                    const int fontId) const {
  const EpdFontData* fontData = fontFamily.getData(style);
  if (!fontData) {
    return;
  }

  // Scale factor for stacked marks (tone mark above vowel)
  // 26px is the reference font height used for Thai glyph offset calculations
  const int fontHeight = fontData->advanceY;
  const float yScale = fontHeight / 26.0f;

  int baseX = *x;  // Store base position for combining marks

  for (const auto& glyph : cluster.glyphs) {
    const EpdGlyph* glyphData = fontFamily.getGlyph(glyph.codepoint, style);

    if (!glyphData) {
      glyphData = fontFamily.getGlyph('?', style);
    }
    if (!glyphData) {
      continue;
    }

    const int is2Bit = fontData->is2Bit;
    const uint32_t offset = glyphData->dataOffset;
    const uint8_t width = glyphData->width;
    const uint8_t height = glyphData->height;
    const int left = glyphData->left;

    // Calculate x position for this glyph
    int glyphX;
    if (glyph.zeroAdvance) {
      // Combining mark: position relative to base consonant
      glyphX = baseX + glyph.xOffset;
    } else {
      // Normal glyph: position at current cursor
      glyphX = *x + glyph.xOffset;
    }

    // Calculate y offset - only apply scaling for stacked marks
    int yOffset = 0;
    if (glyph.yOffset < -2) {
      yOffset = static_cast<int>(glyph.yOffset * yScale);
    }
    const int glyphY = y + yOffset;

    if (fontData->bitmap == nullptr) {
      continue;
    }
    const uint8_t* bitmap = &fontData->bitmap[offset];

    const int screenHeight = getScreenHeight();
    const int screenWidth = getScreenWidth();

    for (int bitmapY = 0; bitmapY < height; bitmapY++) {
      const int screenY = glyphY - glyphData->top + bitmapY;
      if (screenY < 0 || screenY >= screenHeight) continue;

      for (int bitmapX = 0; bitmapX < width; bitmapX++) {
        const int pixelPosition = bitmapY * width + bitmapX;
        const int screenX = glyphX + left + bitmapX;
        if (screenX < 0 || screenX >= screenWidth) continue;

        if (is2Bit) {
          const uint8_t byte = bitmap[pixelPosition / 4];
          const uint8_t bit_index = (3 - pixelPosition % 4) * 2;
          uint8_t bmpVal = 3 - ((byte >> bit_index) & 0x3);

          // Text darkness: shift gray levels toward black (same as renderChar)
          if (textDarkness_ >= 3) {
            if (bmpVal < 3) bmpVal = 0;
          } else if (textDarkness_ == 2) {
            if (bmpVal == 2) bmpVal = 0;
            else if (bmpVal == 1) bmpVal = 0;
          } else if (textDarkness_ == 1) {
            if (bmpVal == 2) bmpVal = 1;
          }

          if (renderMode == BW && bmpVal < 3) {
            drawPixel(screenX, screenY, pixelState);
          } else if (renderMode == GRAYSCALE_MSB && (bmpVal == 1 || bmpVal == 2)) {
            drawPixel(screenX, screenY, false);
          } else if (renderMode == GRAYSCALE_LSB && bmpVal == 1) {
            drawPixel(screenX, screenY, false);
          }
        } else {
          const uint8_t byte = bitmap[pixelPosition / 8];
          const uint8_t bit_index = 7 - (pixelPosition % 8);

          if ((byte >> bit_index) & 1) {
            drawPixel(screenX, screenY, pixelState);
          }
        }
      }
    }

    // Track advance for non-combining glyphs
    if (!glyph.zeroAdvance) {
      baseX = *x + glyphData->advanceX;
      *x += glyphData->advanceX;
    }
  }
}

// ============================================================================
// Arabic Text Rendering
// ============================================================================

int GfxRenderer::getArabicTextWidth(const int fontId, const char* text, const EpdFontFamily::Style style) const {
  if (text == nullptr || *text == '\0') return 0;

  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return 0;
  }

  const auto& font = fontMap.at(fontId);
  int totalWidth = 0;

  auto shaped = ArabicShaper::shapeText(text);

  for (const auto cp : shaped) {
    const EpdGlyph* glyphData = font.getGlyph(cp, style);
    if (!glyphData) {
      glyphData = font.getGlyph('?', style);
    }
    if (glyphData) {
      totalWidth += glyphData->advanceX;
    }
  }

  return totalWidth;
}

void GfxRenderer::drawArabicText(const int fontId, const int x, const int y, const char* text, const bool black,
                                 const EpdFontFamily::Style style) const {
  if (fontMap.count(fontId) == 0) {
    Serial.printf("[%lu] [GFX] Font %d not found\n", millis(), fontId);
    return;
  }

  const int yPos = y + getFontAscenderSize(fontId);
  int xpos = x;

  const auto& font = fontMap.at(fontId);
  auto shaped = ArabicShaper::shapeText(text);

  // Render each shaped codepoint (already in visual LTR order)
  for (const auto cp : shaped) {
    renderChar(font, cp, &xpos, &yPos, black, style, fontId);
  }
}
