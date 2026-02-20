#pragma once

#include <cstdint>

namespace sumi {

struct Core;

// LibraryIndex - Lightweight per-book progress index stored in /.sumi/library.bin
// Used by the file browser to show progress bars and content type icons.
// Updated by the reader whenever progress is saved.
//
// File format (v2):
//   Header: version(1) + count(2) = 3 bytes
//   Entries: [pathHash(4) + currentPage(2) + totalPages(2) + contentHint(1)] * N = 9N bytes
//
// 100 books = 903 bytes. One read on browser entry, one write per progress save.
class LibraryIndex {
 public:
  struct __attribute__((packed)) Entry {
    uint32_t pathHash;     // std::hash<string>(filepath)
    uint16_t currentPage;  // absolute page (flat)
    uint16_t totalPages;   // total pages in book
    uint8_t contentHint;   // ContentHint enum value from dc:subject

    int8_t progressPercent() const {
      if (totalPages == 0) return 0;
      return static_cast<int8_t>(static_cast<uint32_t>(currentPage) * 100 / totalPages);
    }
  };

  // Update or create an entry for the given book path.
  // Called from ReaderState when progress is saved.
  static bool updateEntry(Core& core, const char* bookPath, uint16_t currentPage, uint16_t totalPages,
                          uint8_t contentHint = 0);

  // Look up progress for a book by its full path.
  // Returns progressPercent (0-100), or -1 if not found.
  static int8_t getProgress(Core& core, const char* bookPath);

  // Batch load: read all entries into a caller-provided array.
  // Returns number of entries read (up to maxEntries).
  static int loadAll(Core& core, Entry* entries, int maxEntries);

  // Find a single entry by hash (low memory usage)
  static bool findByHash(Core& core, uint32_t hash, Entry& entry);

  // Compute hash for a filepath (same hash used everywhere)
  static uint32_t hashPath(const char* path);

 private:
  static constexpr uint8_t VERSION = 2;  // v2: added contentHint byte
  static constexpr int MAX_ENTRIES = 200;
  static constexpr const char* INDEX_PATH = "/.sumi/library.bin";
};

}  // namespace sumi
