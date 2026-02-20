#pragma once

#include <cstddef>
#include <cstdint>

namespace sumi {

/**
 * Flash-based thumbnail cache for instant home screen loading.
 *
 * Stores pre-dithered 1-bit cover thumbnails in LittleFS. Thumbnails are
 * generated once when a book is first opened and loaded instantly when
 * displaying the home screen carousel.
 *
 * Storage: ~2.7KB per thumbnail × 3 books = ~8KB total in LittleFS.
 */
class ThumbnailCache {
 public:
  // Thumbnail dimensions match HomeState::COVER_CACHE_WIDTH/HEIGHT
  static constexpr int WIDTH = 120;
  static constexpr int HEIGHT = 180;
  static constexpr int BYTES_PER_ROW = (WIDTH + 7) / 8;  // 15 bytes (120 bits)
  static constexpr size_t DATA_SIZE = BYTES_PER_ROW * HEIGHT;  // 2,700 bytes

  // Cache directory in LittleFS
  static constexpr const char* CACHE_DIR = "/thumbs";

  /**
   * Store a 1-bit thumbnail in flash.
   * @param bookHash Hash of book path (used as filename)
   * @param data 1-bit bitmap data (WIDTH × HEIGHT, row-major, MSB first)
   * @return true if successfully stored
   */
  static bool store(uint32_t bookHash, const uint8_t* data);

  /**
   * Load a thumbnail from flash.
   * @param bookHash Hash of book path
   * @param buffer Output buffer (must be at least DATA_SIZE bytes)
   * @return true if found and loaded
   */
  static bool load(uint32_t bookHash, uint8_t* buffer);

  /**
   * Check if a thumbnail exists in cache.
   */
  static bool exists(uint32_t bookHash);

  /**
   * Remove a thumbnail from cache.
   */
  static bool remove(uint32_t bookHash);

  /**
   * Remove all cached thumbnails.
   */
  static void clear();

  /**
   * Initialize cache directory (creates if needed).
   * Called automatically by store() if needed.
   */
  static void ensureDirectory();

 private:
  static void getPath(uint32_t bookHash, char* pathOut, size_t maxLen);
};

}  // namespace sumi
