#pragma once

#include <string>

class GfxRenderer;

namespace CoverHelpers {

struct CenteredRect {
  int x, y, width, height;
};

inline CenteredRect calculateCenteredRect(int, int, int, int, int, int) { return {0, 0, 0, 0}; }

inline bool renderCoverFromBmp(GfxRenderer&, const std::string&, int, int, int, int, int&, int, bool = false) {
  return false;
}

inline bool renderCoverWithFallback(GfxRenderer&, const std::string&, const std::string&, int, int, int, int, int&, int,
                                     bool = false) {
  return false;
}

inline std::string findCoverImage(const std::string&, const std::string&) { return ""; }

inline bool convertImageToBmp(const std::string&, const std::string&, const char*, bool) { return false; }

inline bool generateThumbFromCover(const std::string&, const std::string&, const char*) { return false; }

}  // namespace CoverHelpers
