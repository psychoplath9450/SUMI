#pragma once

#include <SdFat.h>

#include <memory>
#include <string>

#include "Block.h"

class GfxRenderer;

class ImageBlock final : public Block {
  std::string cachedBmpPath;
  uint16_t width;
  uint16_t height;

 public:
  explicit ImageBlock(std::string path, const uint16_t w, const uint16_t h)
      : cachedBmpPath(std::move(path)), width(w), height(h) {}
  ~ImageBlock() override = default;

  BlockType getType() override { return IMAGE_BLOCK; }
  bool isEmpty() override { return cachedBmpPath.empty(); }
  void layout(GfxRenderer& renderer) override {}

  uint16_t getWidth() const { return width; }
  uint16_t getHeight() const { return height; }
  const std::string& getCachedBmpPath() const { return cachedBmpPath; }

  void render(GfxRenderer& renderer, int fontId, int x, int y) const;
  bool serialize(FsFile& file) const;
  static std::unique_ptr<ImageBlock> deserialize(FsFile& file);
};
