#pragma once

#include <cstdint>
#include <cstring>

namespace sumi {

struct Core;

// RecentBooks - Tracks recently opened books in order
// Stored in /.sumi/recent.bin
// Used by HomeState to show library carousel
class RecentBooks {
 public:
  static constexpr int MAX_RECENT = 10;
  static constexpr int PATH_LEN = 128;
  static constexpr int TITLE_LEN = 64;
  static constexpr int AUTHOR_LEN = 48;

  struct __attribute__((packed)) Entry {
    char path[PATH_LEN];
    char title[TITLE_LEN];
    char author[AUTHOR_LEN];
    uint32_t lastAccess;  // Reserved (no RTC; ordering is by file position)
    uint16_t progress;    // 0-100 percent
    
    bool isEmpty() const { return path[0] == '\0'; }
  };

  // Record that a book was opened (moves to front if already in list)
  static void recordOpen(Core& core, const char* path, const char* title, 
                         const char* author, uint16_t progress = 0);
  
  // Update progress for a book (doesn't change order)
  static void updateProgress(Core& core, const char* path, uint16_t progress);

  // Load all recent books (returns count, fills entries array)
  // Entries are in most-recent-first order
  static int loadAll(Core& core, Entry* entries, int maxEntries);

  // Get the most recent book (returns false if no recent books)
  static bool getMostRecent(Core& core, Entry& entry);

  // Clear all recent books
  static void clear(Core& core);

 private:
  static constexpr uint8_t VERSION = 1;
  static constexpr const char* INDEX_PATH = "/.sumi/recent.bin";
};

}  // namespace sumi
