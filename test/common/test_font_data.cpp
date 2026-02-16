#include "test_font_data.h"

#include <algorithm>
#include <cstring>

namespace TestFontData {

// Helper to append little-endian values
static void appendU8(std::string& data, uint8_t val) { data.push_back(static_cast<char>(val)); }

static void appendU16(std::string& data, uint16_t val) {
  data.push_back(static_cast<char>(val & 0xFF));
  data.push_back(static_cast<char>((val >> 8) & 0xFF));
}

static void appendU32(std::string& data, uint32_t val) {
  data.push_back(static_cast<char>(val & 0xFF));
  data.push_back(static_cast<char>((val >> 8) & 0xFF));
  data.push_back(static_cast<char>((val >> 16) & 0xFF));
  data.push_back(static_cast<char>((val >> 24) & 0xFF));
}

static void appendI16(std::string& data, int16_t val) { appendU16(data, static_cast<uint16_t>(val)); }

std::string generateFont(const std::vector<GlyphSpec>& glyphs, uint8_t advanceY, int16_t ascender, int16_t descender,
                         bool is2Bit) {
  std::string data;

  // Sort glyphs by codepoint and build intervals
  std::vector<GlyphSpec> sorted = glyphs;
  std::sort(sorted.begin(), sorted.end(),
            [](const GlyphSpec& a, const GlyphSpec& b) { return a.codepoint < b.codepoint; });

  // Build intervals (contiguous ranges)
  struct Interval {
    uint32_t first;
    uint32_t last;
    uint32_t offset;  // Index into glyph array
  };
  std::vector<Interval> intervals;
  if (!sorted.empty()) {
    uint32_t first = sorted[0].codepoint;
    uint32_t last = first;
    uint32_t offset = 0;

    for (size_t i = 1; i < sorted.size(); i++) {
      if (sorted[i].codepoint == last + 1) {
        last = sorted[i].codepoint;
      } else {
        intervals.push_back({first, last, offset});
        offset = static_cast<uint32_t>(i);
        first = sorted[i].codepoint;
        last = first;
      }
    }
    intervals.push_back({first, last, offset});
  }

  // Calculate bitmap data and offsets
  std::vector<uint32_t> bitmapOffsets;
  std::string bitmapData;
  for (const auto& g : sorted) {
    bitmapOffsets.push_back(static_cast<uint32_t>(bitmapData.size()));
    if (!g.bitmap.empty()) {
      bitmapData.append(reinterpret_cast<const char*>(g.bitmap.data()), g.bitmap.size());
    }
  }

  // Header (16 bytes)
  appendU32(data, MAGIC);
  appendU16(data, VERSION);
  appendU16(data, is2Bit ? 1 : 0);  // Flags
  for (int i = 0; i < 8; i++) {
    appendU8(data, 0);  // Reserved
  }

  // Metrics (18 bytes)
  appendU8(data, advanceY);
  appendU8(data, 0);  // Padding
  appendI16(data, ascender);
  appendI16(data, descender);
  appendU32(data, static_cast<uint32_t>(intervals.size()));
  appendU32(data, static_cast<uint32_t>(sorted.size()));
  appendU32(data, static_cast<uint32_t>(bitmapData.size()));

  // Intervals
  for (const auto& interval : intervals) {
    appendU32(data, interval.first);
    appendU32(data, interval.last);
    appendU32(data, interval.offset);
  }

  // Glyphs
  for (size_t i = 0; i < sorted.size(); i++) {
    const auto& g = sorted[i];
    appendU8(data, g.width);
    appendU8(data, g.height);
    appendU8(data, g.advanceX);
    appendU8(data, 0);  // Padding
    appendI16(data, g.left);
    appendI16(data, g.top);
    appendU16(data, static_cast<uint16_t>(g.bitmap.size()));
    appendU32(data, bitmapOffsets[i]);
  }

  // Bitmap data
  data.append(bitmapData);

  return data;
}

std::string generateBasicAsciiFont(uint8_t advanceY) {
  std::vector<GlyphSpec> glyphs;

  // Generate A-Z (codepoints 65-90)
  for (uint32_t cp = 'A'; cp <= 'Z'; cp++) {
    GlyphSpec g;
    g.codepoint = cp;
    g.width = 8;
    g.height = 12;
    g.advanceX = 10;
    g.left = 1;
    g.top = 12;
    // Generate some recognizable bitmap data (just fill with the character code)
    g.bitmap.resize(12, static_cast<uint8_t>(cp));
    glyphs.push_back(g);
  }

  // Generate a-z (codepoints 97-122)
  for (uint32_t cp = 'a'; cp <= 'z'; cp++) {
    GlyphSpec g;
    g.codepoint = cp;
    g.width = 8;
    g.height = 10;
    g.advanceX = 9;
    g.left = 0;
    g.top = 10;
    g.bitmap.resize(10, static_cast<uint8_t>(cp));
    glyphs.push_back(g);
  }

  // Add space character
  {
    GlyphSpec g;
    g.codepoint = ' ';
    g.width = 0;
    g.height = 0;
    g.advanceX = 5;
    g.left = 0;
    g.top = 0;
    glyphs.push_back(g);
  }

  // Add '?' for fallback
  {
    GlyphSpec g;
    g.codepoint = '?';
    g.width = 6;
    g.height = 12;
    g.advanceX = 8;
    g.left = 1;
    g.top = 12;
    g.bitmap.resize(9, 0x3F);
    glyphs.push_back(g);
  }

  return generateFont(glyphs, advanceY);
}

std::string generateSingleGlyphFont(uint32_t codepoint, uint8_t width, uint8_t height) {
  std::vector<GlyphSpec> glyphs;

  GlyphSpec g;
  g.codepoint = codepoint;
  g.width = width;
  g.height = height;
  g.advanceX = width + 2;
  g.left = 0;
  g.top = height;
  // Generate bitmap with unique pattern
  g.bitmap.resize(width * height / 2, static_cast<uint8_t>(codepoint & 0xFF));
  glyphs.push_back(g);

  return generateFont(glyphs);
}

std::string generateMultiIntervalFont() {
  std::vector<GlyphSpec> glyphs;

  // First interval: digits 0-9 (codepoints 48-57)
  for (uint32_t cp = '0'; cp <= '9'; cp++) {
    GlyphSpec g;
    g.codepoint = cp;
    g.width = 7;
    g.height = 12;
    g.advanceX = 9;
    g.left = 1;
    g.top = 12;
    g.bitmap.resize(11, static_cast<uint8_t>(cp));
    glyphs.push_back(g);
  }

  // Second interval: uppercase A-Z (codepoints 65-90) - gap from 58-64
  for (uint32_t cp = 'A'; cp <= 'Z'; cp++) {
    GlyphSpec g;
    g.codepoint = cp;
    g.width = 8;
    g.height = 12;
    g.advanceX = 10;
    g.left = 1;
    g.top = 12;
    g.bitmap.resize(12, static_cast<uint8_t>(cp));
    glyphs.push_back(g);
  }

  // Third interval: lowercase a-z (codepoints 97-122) - gap from 91-96
  for (uint32_t cp = 'a'; cp <= 'z'; cp++) {
    GlyphSpec g;
    g.codepoint = cp;
    g.width = 8;
    g.height = 10;
    g.advanceX = 9;
    g.left = 0;
    g.top = 10;
    g.bitmap.resize(10, static_cast<uint8_t>(cp));
    glyphs.push_back(g);
  }

  return generateFont(glyphs);
}

}  // namespace TestFontData
