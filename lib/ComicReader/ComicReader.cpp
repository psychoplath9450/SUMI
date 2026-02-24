#include "ComicReader.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstring>

bool ComicReader::open(const char* path) {
  close();

  file_ = SdMan.open(path, O_RDONLY);
  if (!file_) {
    Serial.printf("[COMIC] Failed to open: %s\n", path);
    return false;
  }

  // Read header (16 bytes)
  uint8_t header[16];
  if (file_.read(header, 16) != 16) {
    Serial.println("[COMIC] Failed to read header");
    file_.close();
    return false;
  }

  // Verify magic
  uint32_t magic;
  memcpy(&magic, header, 4);
  if (magic != MAGIC) {
    Serial.println("[COMIC] Invalid magic");
    file_.close();
    return false;
  }

  // Parse header
  uint8_t version = header[4];
  if (version != 1) {
    Serial.printf("[COMIC] Unsupported version: %d\n", version);
    file_.close();
    return false;
  }

  flags_ = header[5];
  memcpy(&pageCount_, &header[6], 2);  // Little-endian

  if (pageCount_ == 0) {
    Serial.println("[COMIC] No pages");
    file_.close();
    return false;
  }

  // Read page index (6 bytes per entry)
  index_.resize(pageCount_);
  for (int i = 0; i < pageCount_; i++) {
    uint8_t entry[6];
    if (file_.read(entry, 6) != 6) {
      Serial.printf("[COMIC] Failed to read page index entry %d\n", i);
      index_.clear();
      file_.close();
      return false;
    }
    memcpy(&index_[i].offset, entry, 4);
    memcpy(&index_[i].height, entry + 4, 2);
  }

  // Extract title from filename
  std::string pathStr(path);
  size_t lastSlash = pathStr.rfind('/');
  size_t lastDot = pathStr.rfind('.');
  if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
    title_ = pathStr.substr(lastSlash + 1, lastDot - lastSlash - 1);
  } else if (lastSlash != std::string::npos) {
    title_ = pathStr.substr(lastSlash + 1);
  } else {
    title_ = pathStr;
  }

  fileOpen_ = true;
  Serial.printf("[COMIC] Opened: %s (%d pages, %s)\n", title_.c_str(), pageCount_,
                isRTL() ? "RTL" : "LTR");
  return true;
}

void ComicReader::close() {
  if (fileOpen_) {
    file_.close();
    fileOpen_ = false;
  }
  pageCount_ = 0;
  flags_ = 0;
  index_.clear();
  title_.clear();
}

uint16_t ComicReader::pageHeight(int page) const {
  if (page < 0 || page >= pageCount_) return 0;
  return index_[page].height;
}

bool ComicReader::renderPage(int page, GfxRenderer& renderer, int scrollY) const {
  if (!fileOpen_ || page < 0 || page >= pageCount_) return false;

  const auto& entry = index_[page];
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  // Clamp scrollY
  const int maxScroll = static_cast<int>(entry.height) - screenH;
  if (scrollY < 0) scrollY = 0;
  if (maxScroll > 0 && scrollY > maxScroll) scrollY = maxScroll;

  // Calculate which rows to read
  const int startRow = scrollY;
  const int rowsToRead = std::min(static_cast<int>(entry.height) - startRow, screenH);
  if (rowsToRead <= 0) return false;

  // Seek to the start of visible data
  const uint32_t seekPos = entry.offset + static_cast<uint32_t>(startRow) * ROW_BYTES;

  // We need a non-const file handle for seeking/reading
  // SdFat's read/seek are non-const, so cast away const (file state is logically mutable)
  FsFile& f = const_cast<FsFile&>(file_);
  if (!f.seek(seekPos)) {
    Serial.printf("[COMIC] Failed to seek to page %d row %d\n", page, startRow);
    return false;
  }

  // Read and render rows in chunks to minimize SD reads
  // Each row is 200 bytes (800 pixels * 2 bits / 8)
  static constexpr int CHUNK_ROWS = 4;  // Read 4 rows at a time = 800 bytes
  uint8_t rowBuf[ROW_BYTES * CHUNK_ROWS];

  // Center horizontally if page is narrower than screen (shouldn't happen with 800px pages)
  const int xOffset = (screenW > PAGE_WIDTH) ? (screenW - PAGE_WIDTH) / 2 : 0;

  for (int row = 0; row < rowsToRead;) {
    const int chunkSize = std::min(CHUNK_ROWS, rowsToRead - row);
    const int bytesToRead = chunkSize * ROW_BYTES;

    if (f.read(rowBuf, bytesToRead) != bytesToRead) {
      Serial.printf("[COMIC] Failed to read page %d rows %d-%d\n", page, startRow + row,
                    startRow + row + chunkSize);
      return false;
    }

    // Render each row in the chunk
    for (int r = 0; r < chunkSize; r++) {
      const int screenY = row + r;
      const uint8_t* rowData = rowBuf + r * ROW_BYTES;

      for (int x = 0; x < PAGE_WIDTH; x++) {
        const int screenX = xOffset + x;
        if (screenX >= screenW) break;

        // Extract 2-bit pixel value (MSB first: bits 7-6 = first pixel)
        const uint8_t byte = rowData[x / 4];
        const uint8_t val = (byte >> (6 - (x % 4) * 2)) & 0x03;

        // 2-bit palette: 0=black, 1=dark gray, 2=light gray, 3=white
        // Same rendering logic as GfxRenderer::drawBitmap for 2-bit images
        if (renderer.getRenderMode() == GfxRenderer::BW && val < 3) {
          renderer.drawPixel(screenX, screenY);
        } else if (renderer.getRenderMode() == GfxRenderer::GRAYSCALE_MSB && (val == 1 || val == 2)) {
          renderer.drawPixel(screenX, screenY, false);
        } else if (renderer.getRenderMode() == GfxRenderer::GRAYSCALE_LSB && val == 1) {
          renderer.drawPixel(screenX, screenY, false);
        }
      }
    }

    row += chunkSize;
  }

  return true;
}
