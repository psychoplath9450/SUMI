#include "EpubProvider.h"

#include <cstring>

namespace sumi {

Result<void> EpubProvider::open(const char* path, const char* cacheDir) {
  close();

  epub = std::make_shared<Epub>(path, cacheDir);

  if (!epub->load()) {
    epub.reset();
    return ErrVoid(Error::ParseFailed);
  }

  // Populate metadata
  meta.clear();
  meta.type = ContentType::Epub;

  const std::string& title = epub->getTitle();
  strncpy(meta.title, title.c_str(), sizeof(meta.title) - 1);
  meta.title[sizeof(meta.title) - 1] = '\0';

  const std::string& author = epub->getAuthor();
  strncpy(meta.author, author.c_str(), sizeof(meta.author) - 1);
  meta.author[sizeof(meta.author) - 1] = '\0';

  const std::string& cachePath = epub->getCachePath();
  strncpy(meta.cachePath, cachePath.c_str(), sizeof(meta.cachePath) - 1);
  meta.cachePath[sizeof(meta.cachePath) - 1] = '\0';

  // Cover path
  std::string coverPath = epub->getCoverBmpPath();
  strncpy(meta.coverPath, coverPath.c_str(), sizeof(meta.coverPath) - 1);
  meta.coverPath[sizeof(meta.coverPath) - 1] = '\0';

  meta.totalPages = epub->getSpineItemsCount();
  meta.currentPage = 0;
  meta.progressPercent = 0;

  // Parse sumi: content hint from dc:subject
  const std::string& subject = epub->getSubject();
  meta.hint = parseContentHint(subject.c_str());

  return Ok();
}

void EpubProvider::close() {
  epub.reset();
  meta.clear();
}

uint32_t EpubProvider::pageCount() const { return epub ? epub->getSpineItemsCount() : 0; }

uint16_t EpubProvider::tocCount() const { return epub ? epub->getTocItemsCount() : 0; }

Result<TocEntry> EpubProvider::getTocEntry(uint16_t index) const {
  if (!epub || index >= tocCount()) {
    return Err<TocEntry>(Error::InvalidState);
  }

  auto tocItem = epub->getTocItem(index);
  TocEntry entry;
  strncpy(entry.title, tocItem.title.c_str(), sizeof(entry.title) - 1);
  entry.title[sizeof(entry.title) - 1] = '\0';
  entry.pageIndex = epub->getSpineIndexForTocIndex(index);
  entry.depth = tocItem.level;

  return Ok(entry);
}

}  // namespace sumi
