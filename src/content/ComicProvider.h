#pragma once

#include <ComicReader.h>

#include "../core/Result.h"
#include "ContentTypes.h"

namespace sumi {

// ComicProvider wraps the ComicReader for .comic binary files
struct ComicProvider {
  ComicReader reader;
  ContentMetadata meta;

  ComicProvider() = default;
  ~ComicProvider() = default;

  // Non-copyable
  ComicProvider(const ComicProvider&) = delete;
  ComicProvider& operator=(const ComicProvider&) = delete;

  Result<void> open(const char* path, const char* cacheDir);
  void close();

  uint32_t pageCount() const;
  uint16_t tocCount() const { return 0; }
  Result<TocEntry> getTocEntry(uint16_t /*index*/) const { return Err<TocEntry>(Error::InvalidState); }

  ComicReader& getReader() { return reader; }
  const ComicReader& getReader() const { return reader; }
};

}  // namespace sumi
