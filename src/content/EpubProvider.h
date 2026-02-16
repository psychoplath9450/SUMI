#pragma once

#include <Epub.h>

#include <memory>

#include "../core/Result.h"
#include "ContentTypes.h"

namespace sumi {

// EpubProvider wraps the Epub parser
// Uses shared_ptr because Section class requires it for rendering
struct EpubProvider {
  std::shared_ptr<Epub> epub;
  ContentMetadata meta;

  EpubProvider() = default;
  ~EpubProvider() = default;

  // Non-copyable
  EpubProvider(const EpubProvider&) = delete;
  EpubProvider& operator=(const EpubProvider&) = delete;

  // Movable
  EpubProvider(EpubProvider&&) = default;
  EpubProvider& operator=(EpubProvider&&) = default;

  Result<void> open(const char* path, const char* cacheDir);
  void close();

  uint32_t pageCount() const;
  uint16_t tocCount() const;
  Result<TocEntry> getTocEntry(uint16_t index) const;

  // Get spine/section for rendering
  Epub* getEpub() { return epub.get(); }
  const Epub* getEpub() const { return epub.get(); }
  std::shared_ptr<Epub> getEpubShared() { return epub; }
};

}  // namespace sumi
