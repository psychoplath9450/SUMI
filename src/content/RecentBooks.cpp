#include "RecentBooks.h"

#include <Arduino.h>
#include <SDCardManager.h>

#include "../core/Core.h"

namespace sumi {

void RecentBooks::recordOpen(Core& core, const char* path, const char* title,
                             const char* author, uint16_t progress) {
  constexpr int MAX_TRACK = MAX_RECENT;
  Entry entries[MAX_TRACK];
  int count = loadAll(core, entries, MAX_TRACK);
  
  // Check if already in list
  int existingIdx = -1;
  for (int i = 0; i < count; i++) {
    if (strcmp(entries[i].path, path) == 0) {
      existingIdx = i;
      break;
    }
  }
  
  // Create new entry
  Entry newEntry = {};
  strncpy(newEntry.path, path, PATH_LEN - 1);
  strncpy(newEntry.title, title, TITLE_LEN - 1);
  strncpy(newEntry.author, author, AUTHOR_LEN - 1);
  newEntry.lastAccess = 0;  // No RTC; ordering maintained by file position (newest first)
  newEntry.progress = progress;
  
  // Write directly to file - new entry first, then existing (skip duplicate)
  FsFile file;
  if (!SdMan.openFileForWrite("RECENT", INDEX_PATH, file)) {
    Serial.println("[RECENT] Failed to open for write");
    return;
  }
  
  // Count entries to write
  int newCount = 1;  // New entry
  for (int i = 0; i < count && newCount < MAX_TRACK; i++) {
    if (i != existingIdx) newCount++;
  }
  
  // Write header
  file.write(VERSION);
  file.write(static_cast<uint8_t>(newCount));
  
  // Write new entry first
  file.write(reinterpret_cast<const uint8_t*>(&newEntry), sizeof(Entry));
  
  // Write existing entries (skip the duplicate)
  int written = 1;
  for (int i = 0; i < count && written < MAX_TRACK; i++) {
    if (i != existingIdx) {
      file.write(reinterpret_cast<const uint8_t*>(&entries[i]), sizeof(Entry));
      written++;
    }
  }
  file.close();
  
  Serial.printf("[RECENT] Recorded: %s (%d entries total)\n", title, newCount);
}

void RecentBooks::updateProgress(Core& core, const char* path, uint16_t progress) {
  constexpr int MAX_TRACK = MAX_RECENT;
  Entry entries[MAX_TRACK];
  int count = loadAll(core, entries, MAX_TRACK);
  
  // Find and update
  bool found = false;
  for (int i = 0; i < count; i++) {
    if (strcmp(entries[i].path, path) == 0) {
      entries[i].progress = progress;
      found = true;
      break;
    }
  }
  
  if (!found) return;
  
  // Write back
  FsFile file;
  if (!SdMan.openFileForWrite("RECENT", INDEX_PATH, file)) {
    return;
  }
  
  file.write(VERSION);
  file.write(static_cast<uint8_t>(count));
  file.write(reinterpret_cast<const uint8_t*>(entries), sizeof(Entry) * count);
  file.close();
}

int RecentBooks::loadAll(Core& core, Entry* entries, int maxEntries) {
  FsFile file;
  if (!SdMan.openFileForRead("RECENT", INDEX_PATH, file)) {
    return 0;
  }
  
  uint8_t version = file.read();
  if (version != VERSION) {
    file.close();
    return 0;
  }
  
  uint8_t count = file.read();
  if (count > MAX_RECENT) count = MAX_RECENT;
  if (count > maxEntries) count = static_cast<uint8_t>(maxEntries);
  
  int bytesRead = file.read(reinterpret_cast<uint8_t*>(entries), sizeof(Entry) * count);
  file.close();
  if (bytesRead != static_cast<int>(sizeof(Entry) * count)) {
    return 0;
  }
  
  // Filter out entries whose files no longer exist
  int validCount = 0;
  for (int i = 0; i < count; i++) {
    if (!entries[i].isEmpty() && SdMan.exists(entries[i].path)) {
      if (i != validCount) {
        entries[validCount] = entries[i];
      }
      validCount++;
    }
  }
  
  return validCount;
}

bool RecentBooks::getMostRecent(Core& core, Entry& entry) {
  Entry entries[1];
  int count = loadAll(core, entries, 1);
  if (count > 0) {
    entry = entries[0];
    return true;
  }
  return false;
}

void RecentBooks::clear(Core& core) {
  SdMan.remove(INDEX_PATH);
  Serial.println("[RECENT] Cleared all recent books");
}

}  // namespace sumi
