#pragma once

#include <SDCardManager.h>

#include <cstdint>
#include <string>
#include <vector>

// ComicReader - reads .comic binary files (pre-rendered 2-bit page images)
//
// File format (SCOM v1):
//   HEADER (16 bytes):
//     magic[4]     = "SCOM"
//     version[1]   = 1
//     flags[1]     = bit 0: RTL reading direction
//     pageCount[2] = little-endian uint16
//     reserved[8]  = zeros
//
//   PAGE INDEX (6 bytes x pageCount):
//     offset[4]    = uint32 LE, byte offset to page pixel data
//     height[2]    = uint16 LE, page height in pixels
//
//   PAGE DATA:
//     pixels[ROW_BYTES x height] = 2-bit packed rows, 4 pixels/byte, MSB first
//     Row width always 800px -> 200 bytes/row

class GfxRenderer;

class ComicReader {
 public:
  static constexpr uint16_t PAGE_WIDTH = 800;
  static constexpr uint16_t ROW_BYTES = PAGE_WIDTH / 4;  // 200 bytes per row (2-bit)
  static constexpr uint32_t MAGIC = 0x4D4F4353;          // "SCOM" little-endian

  enum class Flag : uint8_t {
    RTL = 0x01,
  };

  ComicReader() = default;
  ~ComicReader() { close(); }

  // Non-copyable
  ComicReader(const ComicReader&) = delete;
  ComicReader& operator=(const ComicReader&) = delete;

  bool open(const char* path);
  void close();
  bool isOpen() const { return fileOpen_; }

  uint16_t pageCount() const { return pageCount_; }
  uint16_t pageHeight(int page) const;
  bool isRTL() const { return flags_ & static_cast<uint8_t>(Flag::RTL); }

  const std::string& title() const { return title_; }

  // Render a page to the GfxRenderer framebuffer
  // scrollY: vertical scroll offset (0 = top of page)
  // renderMode: BW, GRAYSCALE_MSB, GRAYSCALE_LSB (matches GfxRenderer modes)
  // Returns true on success
  bool renderPage(int page, GfxRenderer& renderer, int scrollY) const;

 private:
  struct PageEntry {
    uint32_t offset;
    uint16_t height;
  };

  FsFile file_;
  bool fileOpen_ = false;
  uint16_t pageCount_ = 0;
  uint8_t flags_ = 0;
  std::string title_;
  std::vector<PageEntry> index_;
};
