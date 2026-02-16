#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace TestFontData {

// Header constants from EpdFontLoader
static constexpr uint32_t MAGIC = 0x46445045;  // "EPDF" in little-endian
static constexpr uint16_t VERSION = 1;

// Binary format sizes
static constexpr int HEADER_SIZE = 16;    // Magic(4) + Version(2) + Flags(2) + Reserved(8)
static constexpr int METRICS_SIZE = 18;   // advanceY(1) + padding(1) + ascender(2) + descender(2) +
                                          // intervalCount(4) + glyphCount(4) + bitmapSize(4)
static constexpr int GLYPH_BINARY_SIZE = 14;  // width(1) + height(1) + advanceX(1) + padding(1) +
                                              // left(2) + top(2) + dataLength(2) + dataOffset(4)
static constexpr int INTERVAL_SIZE = 12;  // first(4) + last(4) + offset(4)

struct GlyphSpec {
  uint32_t codepoint;
  uint8_t width;
  uint8_t height;
  uint8_t advanceX;
  int16_t left;
  int16_t top;
  std::vector<uint8_t> bitmap;  // Raw bitmap bytes
};

/**
 * Generate a valid .epdfont binary file with specified glyphs.
 *
 * @param glyphs Vector of glyph specifications
 * @param advanceY Line height
 * @param ascender Maximum ascent
 * @param descender Maximum descent (positive value)
 * @param is2Bit Whether font uses 2-bit pixels (default false = 4-bit)
 * @return Binary font data as string
 */
std::string generateFont(const std::vector<GlyphSpec>& glyphs, uint8_t advanceY = 20, int16_t ascender = 16,
                         int16_t descender = 4, bool is2Bit = false);

/**
 * Generate a basic ASCII font with glyphs for 'A'-'Z' and 'a'-'z'.
 * Each glyph is 8x12 pixels with minimal bitmap data.
 *
 * @param advanceY Line height (default 20)
 * @return Binary font data as string
 */
std::string generateBasicAsciiFont(uint8_t advanceY = 20);

/**
 * Generate a minimal font with a single glyph.
 * Useful for testing specific glyph handling.
 *
 * @param codepoint Unicode codepoint
 * @param width Glyph width in pixels
 * @param height Glyph height in pixels
 * @return Binary font data as string
 */
std::string generateSingleGlyphFont(uint32_t codepoint, uint8_t width = 8, uint8_t height = 12);

/**
 * Generate a font with multiple intervals (non-contiguous ranges).
 * Creates glyphs for basic Latin (A-Z, a-z) and some digits (0-9).
 *
 * @return Binary font data as string
 */
std::string generateMultiIntervalFont();

}  // namespace TestFontData
