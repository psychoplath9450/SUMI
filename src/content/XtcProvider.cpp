#include "XtcProvider.h"

#include <SDCardManager.h>

#include <cstring>
#include <functional>
#include <string>

namespace sumi {

Result<void> XtcProvider::open(const char* path, const char* cacheDir) {
  close();

  xtc::XtcError err = parser.open(path);
  if (err != xtc::XtcError::OK) {
    return ErrVoid(Error::ParseFailed);
  }

  // Populate metadata
  meta.clear();
  meta.type = ContentType::Xtc;

  std::string title = parser.getTitle();
  if (title.empty()) {
    // Use filename as title
    const char* lastSlash = strrchr(path, '/');
    const char* filename = lastSlash ? lastSlash + 1 : path;
    strncpy(meta.title, filename, sizeof(meta.title) - 1);
  } else {
    strncpy(meta.title, title.c_str(), sizeof(meta.title) - 1);
  }
  meta.title[sizeof(meta.title) - 1] = '\0';

  std::string author = parser.getAuthor();
  if (!author.empty()) {
    strncpy(meta.author, author.c_str(), sizeof(meta.author) - 1);
    meta.author[sizeof(meta.author) - 1] = '\0';
  } else {
    meta.author[0] = '\0';
  }

  // Create cache path for progress saving
  if (cacheDir && cacheDir[0] != '\0') {
    std::string pathStr(path);
    size_t hash = std::hash<std::string>{}(pathStr);
    snprintf(meta.cachePath, sizeof(meta.cachePath), "%s/xtc_%zu", cacheDir, hash);
    SdMan.mkdir(meta.cachePath);
  } else {
    meta.cachePath[0] = '\0';
  }

  meta.coverPath[0] = '\0';  // TODO: Extract first page as cover

  meta.totalPages = parser.getPageCount();
  meta.currentPage = 0;
  meta.progressPercent = 0;

  return Ok();
}

void XtcProvider::close() {
  parser.close();
  meta.clear();
}

uint32_t XtcProvider::pageCount() const { return parser.getPageCount(); }

uint16_t XtcProvider::tocCount() const { return parser.hasChapters() ? parser.getChapters().size() : 0; }

Result<TocEntry> XtcProvider::getTocEntry(uint16_t index) const {
  if (!parser.hasChapters() || index >= tocCount()) {
    return Err<TocEntry>(Error::InvalidState);
  }

  const auto& chapters = parser.getChapters();
  const auto& chapter = chapters[index];

  TocEntry entry;
  strncpy(entry.title, chapter.name.c_str(), sizeof(entry.title) - 1);
  entry.title[sizeof(entry.title) - 1] = '\0';
  entry.pageIndex = chapter.startPage;
  entry.depth = 0;  // XTC chapters are flat

  return Ok(entry);
}

}  // namespace sumi
