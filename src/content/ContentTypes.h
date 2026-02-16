#pragma once

#include <cstddef>
#include <cstdint>

#include "../core/Result.h"
#include "../core/Types.h"

namespace sumi {

// Metadata shared by all content types
struct ContentMetadata {
  char title[BufferSize::Title];
  char author[BufferSize::Author];
  char coverPath[BufferSize::Path];
  char cachePath[BufferSize::Path];
  uint32_t totalPages;      // Total pages/spine items
  uint32_t currentPage;     // Current reading position
  uint8_t progressPercent;  // 0-100
  ContentType type;
  ContentHint hint;         // sumi: content classification (from dc:subject)

  ContentMetadata() { clear(); }

  void clear() {
    title[0] = '\0';
    author[0] = '\0';
    coverPath[0] = '\0';
    cachePath[0] = '\0';
    totalPages = 0;
    currentPage = 0;
    progressPercent = 0;
    type = ContentType::None;
    hint = ContentHint::Generic;
  }
};

// Table of contents entry (owns its title string to avoid dangling pointers)
struct TocEntry {
  char title[BufferSize::TocTitle];  // Owned copy of title
  uint32_t pageIndex;                // Page/spine index
  uint8_t depth;                     // Nesting level (0 = top)

  TocEntry() : pageIndex(0), depth(0) { title[0] = '\0'; }
};

// Content format detection
ContentType detectContentType(const char* path);

}  // namespace sumi
