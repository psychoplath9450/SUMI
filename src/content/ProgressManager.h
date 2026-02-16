#pragma once

#include <cstdint>

#include "ContentTypes.h"

namespace sumi {

struct Core;

// ProgressManager - Handles reading position persistence
// Stores format-specific progress data to cache directory
class ProgressManager {
 public:
  // Progress data for different content types
  struct Progress {
    int spineIndex = 0;     // EPUB: chapter index in spine
    int sectionPage = 0;    // All formats: current page within section/document
    uint32_t flatPage = 0;  // XTC: absolute page number (1-indexed internally)

    void reset() {
      spineIndex = 0;
      sectionPage = 0;
      flatPage = 0;
    }
  };

  // Save progress to cache directory
  // Returns true on success
  static bool save(Core& core, const char* cacheDir, ContentType type, const Progress& progress);

  // Load progress from cache directory
  // Returns loaded progress (or default values if no saved progress)
  static Progress load(Core& core, const char* cacheDir, ContentType type);

  // Validate progress against content bounds
  // Returns validated (possibly clamped) progress
  static Progress validate(Core& core, ContentType type, const Progress& progress);
};

}  // namespace sumi
