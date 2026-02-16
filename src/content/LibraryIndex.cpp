#include "LibraryIndex.h"

#include <Arduino.h>

#include <functional>
#include <string>

#include "../core/Core.h"

namespace sumi {

// GCC 8.4 requires out-of-class definition for ODR-used static constexpr
constexpr uint8_t LibraryIndex::VERSION;

uint32_t LibraryIndex::hashPath(const char* path) {
  // Use std::hash for consistency with Epub cache path hashing
  return static_cast<uint32_t>(std::hash<std::string>{}(std::string(path)));
}

bool LibraryIndex::updateEntry(Core& core, const char* bookPath, uint16_t currentPage, uint16_t totalPages,
                               uint8_t contentHint) {
  if (!bookPath || bookPath[0] == '\0') return false;

  const uint32_t hash = hashPath(bookPath);

  // Read existing entries
  Entry entries[MAX_ENTRIES];
  int count = loadAll(core, entries, MAX_ENTRIES);

  // Find existing entry or append
  int targetIdx = -1;
  for (int i = 0; i < count; i++) {
    if (entries[i].pathHash == hash) {
      targetIdx = i;
      break;
    }
  }

  if (targetIdx >= 0) {
    // Update existing
    entries[targetIdx].currentPage = currentPage;
    entries[targetIdx].totalPages = totalPages;
    if (contentHint != 0) entries[targetIdx].contentHint = contentHint;  // Don't overwrite with Generic
  } else if (count < MAX_ENTRIES) {
    // Append new
    entries[count].pathHash = hash;
    entries[count].currentPage = currentPage;
    entries[count].totalPages = totalPages;
    entries[count].contentHint = contentHint;
    count++;
  } else {
    // Full — overwrite oldest (index 0) and shift
    for (int i = 0; i < count - 1; i++) {
      entries[i] = entries[i + 1];
    }
    entries[count - 1].pathHash = hash;
    entries[count - 1].currentPage = currentPage;
    entries[count - 1].totalPages = totalPages;
    entries[count - 1].contentHint = contentHint;
  }

  // Write all entries back
  FsFile file;
  auto result = core.storage.openWrite(INDEX_PATH, file);
  if (!result.ok()) {
    Serial.println("[LIBIDX] Failed to write library.bin");
    return false;
  }

  file.write(&VERSION, 1);
  uint16_t cnt = static_cast<uint16_t>(count);
  file.write(reinterpret_cast<const uint8_t*>(&cnt), 2);

  for (int i = 0; i < count; i++) {
    file.write(reinterpret_cast<const uint8_t*>(&entries[i]), sizeof(Entry));
  }

  file.close();
  Serial.printf("[LIBIDX] Updated: hash=%u page=%u/%u (%d entries)\n", hash, currentPage, totalPages, count);
  return true;
}

int8_t LibraryIndex::getProgress(Core& core, const char* bookPath) {
  if (!bookPath || bookPath[0] == '\0') return -1;

  const uint32_t hash = hashPath(bookPath);

  FsFile file;
  auto result = core.storage.openRead(INDEX_PATH, file);
  if (!result.ok()) return -1;

  uint8_t version;
  if (file.read(&version, 1) != 1 || version != VERSION) {
    file.close();
    return -1;
  }

  uint16_t count;
  if (file.read(reinterpret_cast<uint8_t*>(&count), 2) != 2) {
    file.close();
    return -1;
  }

  Entry entry;
  for (uint16_t i = 0; i < count; i++) {
    if (file.read(reinterpret_cast<uint8_t*>(&entry), sizeof(Entry)) != sizeof(Entry)) break;
    if (entry.pathHash == hash) {
      file.close();
      return entry.progressPercent();
    }
  }

  file.close();
  return -1;
}

int LibraryIndex::loadAll(Core& core, Entry* entries, int maxEntries) {
  FsFile file;
  auto result = core.storage.openRead(INDEX_PATH, file);
  if (!result.ok()) return 0;

  uint8_t version;
  if (file.read(&version, 1) != 1 || version != VERSION) {
    file.close();
    return 0;
  }

  uint16_t count;
  if (file.read(reinterpret_cast<uint8_t*>(&count), 2) != 2) {
    file.close();
    return 0;
  }

  int toRead = (count < maxEntries) ? count : maxEntries;
  int actual = 0;
  for (int i = 0; i < toRead; i++) {
    if (file.read(reinterpret_cast<uint8_t*>(&entries[i]), sizeof(Entry)) == sizeof(Entry)) {
      actual++;
    } else {
      break;
    }
  }

  file.close();
  return actual;
}

}  // namespace sumi
