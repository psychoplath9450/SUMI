#pragma once

#include <SdFat.h>

#include <cstdint>

#include "BitmapHelpers.h"

enum class BmpReaderError : uint8_t {
  Ok = 0,
  FileInvalid,
  SeekStartFailed,
  NotBMP,
  DIBTooSmall,
  BadPlanes,
  UnsupportedBpp,
  UnsupportedCompression,
  BadDimensions,
  ImageTooLarge,
  PaletteTooLarge,
  SeekPixelDataFailed,
  BufferTooSmall,
  OomRowBuffer,
  ShortReadRow,
};

class Bitmap {
 public:
  static const char* errorToString(BmpReaderError) { return "stub"; }

  explicit Bitmap(FsFile&, bool = false) {}
  ~Bitmap() = default;
  BmpReaderError parseHeaders() { return BmpReaderError::FileInvalid; }
  BmpReaderError readRow(uint8_t*, uint8_t*, int) const { return BmpReaderError::FileInvalid; }
  BmpReaderError rewindToData() const { return BmpReaderError::FileInvalid; }
  int getWidth() const { return 0; }
  int getHeight() const { return 0; }
  bool isTopDown() const { return false; }
  bool hasGreyscale() const { return false; }
  int getRowBytes() const { return 0; }
};
