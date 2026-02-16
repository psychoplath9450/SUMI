#include "OrderedDither.h"

void OrderedDither::fillRect(uint8_t* buffer, int bufWidth, int x, int y, int w, int h, DitherPattern pattern) {
  if (!buffer || w <= 0 || h <= 0 || x < 0 || y < 0 || bufWidth <= 0) {
    return;
  }

  const int patternIdx = static_cast<int>(pattern);
  if (patternIdx < 0 || patternIdx >= static_cast<int>(DitherPattern::Count)) {
    return;
  }

  const int bufWidthBytes = (bufWidth + 7) / 8;

  for (int py = 0; py < h; py++) {
    const int absY = y + py;
    const int patternRow = absY & 7;
    const uint8_t patternByte = kDitherPatterns[patternIdx][patternRow];

    for (int px = 0; px < w; px++) {
      const int absX = x + px;
      const int byteIdx = absY * bufWidthBytes + (absX / 8);
      const int bitIdx = 7 - (absX & 7);
      const bool pixelOn = (patternByte >> bitIdx) & 1;

      if (pixelOn) {
        buffer[byteIdx] |= (1 << bitIdx);
      } else {
        buffer[byteIdx] &= ~(1 << bitIdx);
      }
    }
  }
}
