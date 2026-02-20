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

  // Read existing file to find entry and get count
  FsFile readFile;
  auto readResult = core.storage.openRead(INDEX_PATH, readFile);
  
  int existingCount = 0;
  int targetIdx = -1;
  
  if (readResult.ok()) {
    uint8_t version;
    if (readFile.read(&version, 1) == 1 && version == VERSION) {
      uint16_t count;
      if (readFile.read(reinterpret_cast<uint8_t*>(&count), 2) == 2) {
        existingCount = count;
        // Stream through entries to find matching hash
        Entry tempEntry;
        for (int i = 0; i < existingCount && i < MAX_ENTRIES; i++) {
          if (readFile.read(reinterpret_cast<uint8_t*>(&tempEntry), sizeof(Entry)) == sizeof(Entry)) {
            if (tempEntry.pathHash == hash) {
              targetIdx = i;
              break;
            }
          }
        }
      }
    }
    readFile.close();
  }

  // Now we know if we're updating or appending
  // Reopen for read to copy entries
  readResult = core.storage.openRead(INDEX_PATH, readFile);
  
  // Open temp file for write
  FsFile writeFile;
  auto writeResult = core.storage.openWrite("/.sumi/library.tmp", writeFile);
  if (!writeResult.ok()) {
    if (readResult.ok()) readFile.close();
    Serial.println("[LIBIDX] Failed to write library.tmp");
    return false;
  }

  // Write header
  writeFile.write(&VERSION, 1);
  
  int newCount = (targetIdx >= 0) ? existingCount : 
                 (existingCount < MAX_ENTRIES) ? existingCount + 1 : existingCount;
  uint16_t cnt = static_cast<uint16_t>(newCount);
  writeFile.write(reinterpret_cast<const uint8_t*>(&cnt), 2);

  // Create the new/updated entry
  Entry newEntry;
  newEntry.pathHash = hash;
  newEntry.currentPage = currentPage;
  newEntry.totalPages = totalPages;
  newEntry.contentHint = contentHint;

  if (readResult.ok()) {
    // Skip old header
    readFile.seek(3);
    
    Entry tempEntry;
    int written = 0;
    for (int i = 0; i < existingCount && written < newCount; i++) {
      if (readFile.read(reinterpret_cast<uint8_t*>(&tempEntry), sizeof(Entry)) != sizeof(Entry)) break;
      
      if (i == targetIdx) {
        // Replace this entry
        if (contentHint == 0) newEntry.contentHint = tempEntry.contentHint;  // Preserve hint
        writeFile.write(reinterpret_cast<const uint8_t*>(&newEntry), sizeof(Entry));
      } else if (existingCount >= MAX_ENTRIES && i == 0 && targetIdx < 0) {
        // Skip oldest entry when full and adding new
        continue;
      } else {
        writeFile.write(reinterpret_cast<const uint8_t*>(&tempEntry), sizeof(Entry));
      }
      written++;
    }
    readFile.close();
    
    // Append new entry if not updating existing
    if (targetIdx < 0 && written < MAX_ENTRIES) {
      writeFile.write(reinterpret_cast<const uint8_t*>(&newEntry), sizeof(Entry));
    }
  } else {
    // No existing file, just write new entry
    writeFile.write(reinterpret_cast<const uint8_t*>(&newEntry), sizeof(Entry));
  }

  // Flush and close temp file before replacing the original.
  // SdFat rename fails if target exists, so we must remove first.
  // sync() ensures data is on disk before we remove the old file.
  writeFile.sync();
  writeFile.close();
  SdMan.remove(INDEX_PATH);
  SdMan.rename("/.sumi/library.tmp", INDEX_PATH);

  Serial.printf("[LIBIDX] Updated: hash=%u page=%u/%u (%d entries)\n", hash, currentPage, totalPages, newCount);
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

bool LibraryIndex::findByHash(Core& core, uint32_t hash, Entry& entry) {
  FsFile file;
  auto result = core.storage.openRead(INDEX_PATH, file);
  if (!result.ok()) return false;

  uint8_t version;
  if (file.read(&version, 1) != 1 || version != VERSION) {
    file.close();
    return false;
  }

  uint16_t count;
  if (file.read(reinterpret_cast<uint8_t*>(&count), 2) != 2) {
    file.close();
    return false;
  }

  Entry temp;
  for (uint16_t i = 0; i < count; i++) {
    if (file.read(reinterpret_cast<uint8_t*>(&temp), sizeof(Entry)) != sizeof(Entry)) break;
    if (temp.pathHash == hash) {
      entry = temp;
      file.close();
      return true;
    }
  }

  file.close();
  return false;
}

}  // namespace sumi
