#pragma once

#include <Txt.h>

#include <memory>

#include "../core/Result.h"
#include "ContentTypes.h"

namespace sumi {

// TxtProvider wraps the Txt handler
struct TxtProvider {
  std::unique_ptr<Txt> txt;
  ContentMetadata meta;

  TxtProvider() = default;
  ~TxtProvider() = default;

  // Non-copyable
  TxtProvider(const TxtProvider&) = delete;
  TxtProvider& operator=(const TxtProvider&) = delete;

  // Movable
  TxtProvider(TxtProvider&&) = default;
  TxtProvider& operator=(TxtProvider&&) = default;

  Result<void> open(const char* path, const char* cacheDir);
  void close();

  uint32_t pageCount() const;
  uint16_t tocCount() const { return 0; }  // TXT has no TOC
  Result<TocEntry> getTocEntry(uint16_t index) const { return Err<TocEntry>(Error::InvalidState); }

  // Direct access
  Txt* getTxt() { return txt.get(); }
  const Txt* getTxt() const { return txt.get(); }
};

}  // namespace sumi
