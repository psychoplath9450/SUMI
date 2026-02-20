#include "ThumbnailCache.h"

#include <Arduino.h>
#undef FILE_READ
#undef FILE_WRITE
#include <LittleFS.h>

namespace sumi {

void ThumbnailCache::getPath(uint32_t bookHash, char* pathOut, size_t maxLen) {
  snprintf(pathOut, maxLen, "%s/%08lX.thb", CACHE_DIR, (unsigned long)bookHash);
}

void ThumbnailCache::ensureDirectory() {
  if (!LittleFS.exists(CACHE_DIR)) {
    LittleFS.mkdir(CACHE_DIR);
  }
}

bool ThumbnailCache::store(uint32_t bookHash, const uint8_t* data) {
  if (!data) return false;

  ensureDirectory();

  char path[48];
  getPath(bookHash, path, sizeof(path));

  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.printf("[%lu] [THUMB] Failed to create %s\n", millis(), path);
    return false;
  }

  size_t written = file.write(data, DATA_SIZE);
  file.close();

  if (written != DATA_SIZE) {
    Serial.printf("[%lu] [THUMB] Write failed: %zu/%zu bytes\n", millis(), written, DATA_SIZE);
    LittleFS.remove(path);
    return false;
  }

  Serial.printf("[%lu] [THUMB] Stored %s (%zu bytes)\n", millis(), path, DATA_SIZE);
  return true;
}

bool ThumbnailCache::load(uint32_t bookHash, uint8_t* buffer) {
  if (!buffer) return false;

  char path[48];
  getPath(bookHash, path, sizeof(path));

  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  size_t bytesRead = file.read(buffer, DATA_SIZE);
  file.close();

  if (bytesRead != DATA_SIZE) {
    Serial.printf("[%lu] [THUMB] Read failed: %zu/%zu bytes\n", millis(), bytesRead, DATA_SIZE);
    return false;
  }

  return true;
}

bool ThumbnailCache::exists(uint32_t bookHash) {
  char path[48];
  getPath(bookHash, path, sizeof(path));
  return LittleFS.exists(path);
}

bool ThumbnailCache::remove(uint32_t bookHash) {
  char path[48];
  getPath(bookHash, path, sizeof(path));

  if (!LittleFS.exists(path)) {
    return true;  // Already gone
  }

  return LittleFS.remove(path);
}

void ThumbnailCache::clear() {
  File dir = LittleFS.open(CACHE_DIR);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  int removed = 0;
  File entry;
  while ((entry = dir.openNextFile())) {
    String name = entry.name();
    entry.close();

    char path[64];
    snprintf(path, sizeof(path), "%s/%s", CACHE_DIR, name.c_str());
    if (LittleFS.remove(path)) {
      removed++;
    }
  }
  dir.close();

  Serial.printf("[%lu] [THUMB] Cleared cache (%d files)\n", millis(), removed);
}

}  // namespace sumi
