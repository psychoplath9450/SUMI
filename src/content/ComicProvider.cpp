#include "ComicProvider.h"

#include <SDCardManager.h>

#include <cstring>
#include <functional>
#include <string>

namespace sumi {

Result<void> ComicProvider::open(const char* path, const char* cacheDir) {
  close();

  if (!reader.open(path)) {
    return ErrVoid(Error::ParseFailed);
  }

  // Populate metadata
  meta.clear();
  meta.type = ContentType::Comic;
  meta.hint = reader.isRTL() ? ContentHint::ComicRtl : ContentHint::Comic;

  const std::string& title = reader.title();
  if (!title.empty()) {
    strncpy(meta.title, title.c_str(), sizeof(meta.title) - 1);
  } else {
    const char* lastSlash = strrchr(path, '/');
    const char* filename = lastSlash ? lastSlash + 1 : path;
    strncpy(meta.title, filename, sizeof(meta.title) - 1);
  }
  meta.title[sizeof(meta.title) - 1] = '\0';

  meta.author[0] = '\0';

  // Create cache path for progress saving
  if (cacheDir && cacheDir[0] != '\0') {
    std::string pathStr(path);
    size_t hash = std::hash<std::string>{}(pathStr);
    snprintf(meta.cachePath, sizeof(meta.cachePath), "%s/comic_%zu", cacheDir, hash);
    SdMan.mkdir(meta.cachePath);
  } else {
    meta.cachePath[0] = '\0';
  }

  meta.coverPath[0] = '\0';
  meta.totalPages = reader.pageCount();
  meta.currentPage = 0;
  meta.progressPercent = 0;

  return Ok();
}

void ComicProvider::close() {
  reader.close();
  meta.clear();
}

uint32_t ComicProvider::pageCount() const { return reader.pageCount(); }

}  // namespace sumi
