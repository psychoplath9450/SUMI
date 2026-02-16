#pragma once

#include <cstdint>

// Ordered dithering patterns (8x8 pixel patterns)
// From bb_epaper library by BitBank Software, Inc.
// SPDX-License-Identifier: GPL-3.0-or-later

enum class DitherPattern : uint8_t {
  None = 0,  // 100% white (no black pixels)
  D75,       // 75% white density
  D50,       // 50% density (checkerboard)
  D25Reg,    // 25% white, regular pattern
  D25Alt,    // 25% white, alternating pattern
  D12Reg,    // 12.5% white, regular pattern
  D12Alt,    // 12.5% white, alternating pattern
  Count      // Number of patterns
};

// 8x8 ordered dither patterns (8 bytes each, 1 bit per pixel)
// Each pattern is an 8-row bitmap where 1 = white, 0 = black
// Pattern repeats every 8 pixels in X and Y
// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
constexpr uint8_t kDitherPatterns[static_cast<int>(DitherPattern::Count)][8] = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},  // NONE - all white (0%)
    {0x77, 0xff, 0xdd, 0xff, 0x77, 0xff, 0xdd, 0xff},  // D75 - 75% white
    {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa},  // D50 - 50% checkerboard
    {0x55, 0x00, 0x55, 0x00, 0x55, 0x00, 0x55, 0x00},  // D25_REG - 25% regular
    {0x55, 0x00, 0xaa, 0x00, 0x55, 0x00, 0xaa, 0x00},  // D25_ALT - 25% alternating
    {0x88, 0x00, 0x88, 0x00, 0x88, 0x00, 0x88, 0x00},  // D12_REG - 12.5% regular
    {0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00},  // D12_ALT - 12.5% alternating
};
// NOLINTEND(cppcoreguidelines-avoid-c-arrays)

class OrderedDither {
 public:
  // Get pixel value at position using pattern
  // Returns true for white (pattern bit is 1), false for black
  // Precondition: pattern must be valid (not Count)
  static inline bool getPixel(DitherPattern pattern, int x, int y) {
    const int patternIdx = static_cast<int>(pattern);
    if (patternIdx < 0 || patternIdx >= static_cast<int>(DitherPattern::Count)) {
      return true;  // Default to white on invalid pattern
    }
    const int row = y & 7;        // y % 8
    const int bit = 7 - (x & 7);  // bit position in byte (MSB first)
    return (kDitherPatterns[patternIdx][row] >> bit) & 1;
  }

  // Select pattern based on grayscale value (0-255)
  // Maps gray levels to nearest density pattern
  static inline DitherPattern patternFromGray(uint8_t gray) {
    // Thresholds chosen to evenly distribute gray levels
    if (gray < 16) return DitherPattern::None;    // <6% -> 0% (all black result)
    if (gray < 48) return DitherPattern::D12Alt;  // 6-19% -> 12.5%
    if (gray < 80) return DitherPattern::D25Alt;  // 19-31% -> 25%
    if (gray < 128) return DitherPattern::D50;    // 31-50% -> 50%
    if (gray < 176) return DitherPattern::D75;    // 50-69% -> 75%
    return DitherPattern::None;                   // >69% -> use inverted logic
  }

  // Apply ordered dithering to a grayscale value
  // Returns 1 for white pixel, 0 for black pixel
  static inline uint8_t ditherPixel(uint8_t gray, int x, int y) {
    // For 1-bit output, we use a simple threshold with pattern
    // Pattern comparison: if gray > pattern threshold at (x,y), output white
    const int patternIdx = 3;  // Use D25_REG pattern for threshold matrix
    const int row = y & 7;
    const int bit = 7 - (x & 7);

    // Get threshold from pattern (0 = low threshold, 1 = high threshold)
    // Convert to 0-255 threshold: pattern bit 0 -> threshold 64, bit 1 -> threshold 192
    const bool patternBit = (kDitherPatterns[patternIdx][row] >> bit) & 1;
    const int threshold = patternBit ? 192 : 64;

    return gray >= threshold ? 1 : 0;
  }

  // Apply Bayer-style ordered dithering for better gradients
  // Uses 8x8 Bayer matrix for threshold comparison
  // Returns 1 for white, 0 for black
  static inline uint8_t ditherPixelBayer(uint8_t gray, int x, int y) {
    // 8x8 Bayer threshold matrix (normalized to 0-255)
    // This provides better gradient representation than simple patterns
    // NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
    static constexpr uint8_t kBayerMatrix[8][8] = {
        {0, 128, 32, 160, 8, 136, 40, 168},   {192, 64, 224, 96, 200, 72, 232, 104},
        {48, 176, 16, 144, 56, 184, 24, 152}, {240, 112, 208, 80, 248, 120, 216, 88},
        {12, 140, 44, 172, 4, 132, 36, 164},  {204, 76, 236, 108, 196, 68, 228, 100},
        {60, 188, 28, 156, 52, 180, 20, 148}, {252, 124, 220, 92, 244, 116, 212, 84}};
    // NOLINTEND(cppcoreguidelines-avoid-c-arrays)

    const int row = y & 7;
    const int col = x & 7;
    return gray > kBayerMatrix[row][col] ? 1 : 0;
  }

  // Fill a buffer region with a dither pattern
  // buffer: 1-bit packed buffer (MSB first)
  // bufWidth: buffer width in pixels
  // x, y, w, h: rectangle to fill
  // pattern: dither pattern to use
  // Note: This fills with PATTERN pixels (1=white in pattern), not solid
  static void fillRect(uint8_t* buffer, int bufWidth, int x, int y, int w, int h, DitherPattern pattern);
};
