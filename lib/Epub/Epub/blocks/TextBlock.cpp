#include "TextBlock.h"

#include <GfxRenderer.h>
#include <Serialization.h>

void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y,
                       const bool black) const {
  const int lineHeight = renderer.getLineHeight(fontId);

  for (const auto& wd : wordData) {
    renderer.drawText(fontId, wd.xPos + x, y, wd.text.c_str(), black, wd.style);

    // Draw text decorations
    if (wd.decorations != DECO_NONE) {
      const int wordWidth = renderer.getTextWidth(fontId, wd.text.c_str(), wd.style);
      if (wordWidth > 0) {
        if (wd.decorations & DECO_UNDERLINE) {
          // Draw underline 2px above the bottom of the line height
          const int underlineY = y + lineHeight - 2;
          renderer.drawLine(wd.xPos + x, underlineY, wd.xPos + x + wordWidth - 1, underlineY, black);
        }
        if (wd.decorations & DECO_STRIKETHROUGH) {
          // Draw strikethrough through the middle of the text
          const int strikeY = y + (lineHeight * 2 / 5);
          renderer.drawLine(wd.xPos + x, strikeY, wd.xPos + x + wordWidth - 1, strikeY, black);
        }
      }
    }
  }
}

bool TextBlock::serialize(FsFile& file) const {
  // Word count
  serialization::writePod(file, static_cast<uint16_t>(wordData.size()));

  // Write words, then xpos, then styles, then decorations
  for (const auto& wd : wordData) serialization::writeString(file, wd.text);
  for (const auto& wd : wordData) serialization::writePod(file, wd.xPos);
  for (const auto& wd : wordData) serialization::writePod(file, wd.style);
  for (const auto& wd : wordData) serialization::writePod(file, wd.decorations);

  // Block style
  serialization::writePod(file, style);

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile& file) {
  uint16_t wc;
  BLOCK_STYLE style;

  // Word count
  if (!serialization::readPodChecked(file, wc)) {
    return nullptr;
  }

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    Serial.printf("[%lu] [TXB] Deserialization failed: word count %u exceeds maximum\n", millis(), wc);
    return nullptr;
  }

  // Read into temporary arrays (words, then xpos, then styles, then decorations)
  std::vector<std::string> words(wc);
  std::vector<uint16_t> xpos(wc);
  std::vector<EpdFontFamily::Style> styles(wc);
  std::vector<uint8_t> decorations(wc, DECO_NONE);

  for (auto& w : words) {
    if (!serialization::readString(file, w)) {
      return nullptr;
    }
  }
  for (auto& x : xpos) {
    if (!serialization::readPodChecked(file, x)) {
      return nullptr;
    }
  }
  for (auto& s : styles) {
    if (!serialization::readPodChecked(file, s)) {
      return nullptr;
    }
  }
  for (auto& d : decorations) {
    if (!serialization::readPodChecked(file, d)) {
      return nullptr;
    }
  }

  // Block style
  if (!serialization::readPodChecked(file, style)) {
    return nullptr;
  }

  // Combine into WordData vector
  std::vector<WordData> data;
  data.reserve(wc);
  for (uint16_t i = 0; i < wc; ++i) {
    data.push_back({std::move(words[i]), xpos[i], styles[i], decorations[i]});
  }

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(data), style));
}
