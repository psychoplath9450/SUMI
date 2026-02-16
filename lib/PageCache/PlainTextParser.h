#pragma once

#include <Epub/RenderConfig.h>
#include <ScriptDetector.h>
#include <SdFat.h>

#include <string>

#include "ContentParser.h"

class GfxRenderer;

/**
 * Content parser for plain text files (TXT, Markdown).
 * Reads text, wraps into lines, and creates Page objects.
 */
class PlainTextParser : public ContentParser {
  std::string filepath_;
  GfxRenderer& renderer_;
  RenderConfig config_;
  size_t fileSize_ = 0;
  size_t currentOffset_ = 0;
  bool hasMore_ = true;
  bool isRtl_ = false;

 public:
  PlainTextParser(std::string filepath, GfxRenderer& renderer, const RenderConfig& config);
  ~PlainTextParser() override = default;

  bool parsePages(const std::function<void(std::unique_ptr<Page>)>& onPageComplete, uint16_t maxPages = 0,
                  const AbortCallback& shouldAbort = nullptr) override;
  bool hasMoreContent() const override { return hasMore_; }
  bool canResume() const override { return currentOffset_ > 0 && hasMore_; }
  void reset() override;
};
