#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sumi {

// Button identifiers
enum class Button : uint8_t {
  Up,
  Down,
  Left,
  Right,
  Center,
  Back,
  Power,
};

// Content format types
enum class ContentType : uint8_t {
  None = 0,
  Epub,
  Xtc,
  Txt,
  Markdown,
};

// Content hint from EPUB dc:subject (set by sumi.page tools)
// Used for file browser icons and auto-configuration of reader settings.
enum class ContentHint : uint8_t {
  Generic = 0,      // Unknown / third-party EPUB (no sumi: tag)
  Book,             // sumi:book — text-heavy novel, nonfiction
  BookScanned,      // sumi:book-scanned — scanned document (page images)
  Comic,            // sumi:comic — comic/graphic novel (LTR)
  ComicRtl,         // sumi:comic-rtl — manga (RTL)
  ComicWebtoon,     // sumi:comic-webtoon — manhwa (vertical strips)
  Newspaper,        // sumi:newspaper — news/periodical
  Clipped,          // sumi:clipped — web article clip
};

// Parse dc:subject string to ContentHint
inline ContentHint parseContentHint(const char* subject) {
  if (!subject || subject[0] == '\0') return ContentHint::Generic;
  if (strcmp(subject, "sumi:book") == 0) return ContentHint::Book;
  if (strcmp(subject, "sumi:book-scanned") == 0) return ContentHint::BookScanned;
  if (strcmp(subject, "sumi:comic") == 0) return ContentHint::Comic;
  if (strcmp(subject, "sumi:comic-rtl") == 0) return ContentHint::ComicRtl;
  if (strcmp(subject, "sumi:comic-webtoon") == 0) return ContentHint::ComicWebtoon;
  if (strcmp(subject, "sumi:newspaper") == 0) return ContentHint::Newspaper;
  if (strcmp(subject, "sumi:clipped") == 0) return ContentHint::Clipped;
  return ContentHint::Generic;
}

// Short label for file browser display (2-4 chars)
inline const char* contentHintLabel(ContentHint hint) {
  switch (hint) {
    case ContentHint::Book: return "BOOK";
    case ContentHint::BookScanned: return "SCAN";
    case ContentHint::Comic: return "COMC";
    case ContentHint::ComicRtl: return "MNGA";
    case ContentHint::ComicWebtoon: return "WEBT";
    case ContentHint::Newspaper: return "NEWS";
    case ContentHint::Clipped: return "CLIP";
    default: return "";
  }
}

// State identifiers
enum class StateId : uint8_t {
  Startup,
  Home,
  FileList,
  Reader,
  Settings,
  Error,
  Sleep,
  PluginList,
  PluginHost,
};

// Common buffer sizes
namespace BufferSize {
constexpr size_t Path = 256;
constexpr size_t Text = 512;
constexpr size_t Decompress = 8192;
constexpr size_t Title = 128;
constexpr size_t Author = 64;
constexpr size_t TocTitle = 64;
}  // namespace BufferSize

// Screen dimensions (X4 e-paper)
namespace Screen {
constexpr uint16_t Width = 480;
constexpr uint16_t Height = 800;
constexpr size_t BufferSize = (Width * Height) / 8;  // 1-bit display
}  // namespace Screen

}  // namespace sumi
