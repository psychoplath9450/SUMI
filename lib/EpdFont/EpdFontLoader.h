#pragma once

#include <cstdint>

#include "EpdFontData.h"

/**
 * Loader for binary .epdfont files from SD card.
 *
 * Binary file format (.epdfont):
 *   Header (16 bytes):
 *     - Magic: "EPDF" (4 bytes)
 *     - Version: uint16_t (2 bytes)
 *     - Flags: uint16_t (2 bytes, bit 0 = is2Bit)
 *     - Reserved: 8 bytes
 *
 *   Metrics (12 bytes):
 *     - advanceY: uint8_t
 *     - ascender: int16_t
 *     - descender: int16_t
 *     - intervalCount: uint32_t
 *     - glyphCount: uint32_t (inferred from intervals)
 *     - bitmapSize: uint32_t
 *
 *   Intervals: intervalCount * sizeof(EpdUnicodeInterval)
 *   Glyphs: glyphCount * sizeof(EpdGlyph)
 *   Bitmap: bitmapSize bytes
 */
class EpdFontLoader {
 public:
  static constexpr uint32_t MAGIC = 0x46445045;  // "EPDF" in little-endian
  static constexpr uint16_t VERSION = 1;

  struct LoadResult {
    bool success;
    EpdFontData* fontData;
    uint8_t* bitmap;
    EpdGlyph* glyphs;
    EpdUnicodeInterval* intervals;
    // Memory profiling - sizes in bytes
    size_t bitmapSize;
    size_t glyphsSize;
    size_t intervalsSize;

    size_t totalSize() const { return bitmapSize + glyphsSize + intervalsSize + sizeof(EpdFontData); }
  };

  /**
   * Load a font from a binary .epdfont file on SD card.
   * Allocates memory for font data, which caller must free.
   *
   * @param path Path to .epdfont file
   * @return LoadResult with success status and allocated data
   */
  static LoadResult loadFromFile(const char* path);

  /**
   * Load a font from internal flash (LittleFS).
   * Allocates memory for font data, which caller must free.
   *
   * @param path Path to .epdfont file on LittleFS
   * @return LoadResult with success status and allocated data
   */
  static LoadResult loadFromLittleFS(const char* path);

  /**
   * Free memory allocated by loadFromFile.
   */
  static void freeLoadResult(LoadResult& result);

  /**
   * Result structure for streaming font loading.
   * Contains metadata and glyph table but NOT bitmap data.
   */
  struct StreamingLoadResult {
    bool success;
    EpdFontData fontData;  // Copy of metadata (bitmap pointer is nullptr)
    EpdGlyph* glyphs;
    EpdUnicodeInterval* intervals;
    uint32_t glyphCount;
    uint32_t bitmapOffset;  // File offset where bitmap data starts
    size_t glyphsSize;
    size_t intervalsSize;
  };

  /**
   * Load font for streaming mode - loads intervals and glyph table only.
   * Bitmap data is NOT loaded; caller should stream it on demand.
   *
   * @param path Path to .epdfont file
   * @return StreamingLoadResult with allocated glyphs/intervals (caller must free)
   */
  static StreamingLoadResult loadForStreaming(const char* path);

  /**
   * Free memory allocated by loadForStreaming.
   */
  static void freeStreamingResult(StreamingLoadResult& result);

 private:
  struct FileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint8_t reserved[8];
  } __attribute__((packed));

  struct FileMetrics {
    uint8_t advanceY;
    uint8_t padding;
    int16_t ascender;
    int16_t descender;
    uint32_t intervalCount;
    uint32_t glyphCount;
    uint32_t bitmapSize;
  } __attribute__((packed));

  /**
   * Validate font metrics and check memory availability.
   * @return true if metrics are valid and memory is available
   */
  static bool validateMetricsAndMemory(const FileMetrics& metrics);
};
