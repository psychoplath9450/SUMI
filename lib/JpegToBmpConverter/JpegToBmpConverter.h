#pragma once

#include <functional>

class FsFile;
class Print;
class ZipFile;

class JpegToBmpConverter {
  // Internal implementation using JPEGDEC with scaling + Atkinson dithering
  static bool jpegFileToBmpStreamInternal(class FsFile& jpegFile, Print& bmpOut, int targetWidth, int targetHeight,
                                          bool oneBit, bool quickMode = false,
                                          const std::function<bool()>& shouldAbort = nullptr);

 public:
  static bool jpegFileToBmpStream(FsFile& jpegFile, Print& bmpOut);
  // Convert with custom target size (for thumbnails)
  static bool jpegFileToBmpStreamWithSize(FsFile& jpegFile, Print& bmpOut, int targetMaxWidth, int targetMaxHeight,
                                          const std::function<bool()>& shouldAbort = nullptr);
  // Convert to 1-bit BMP (black and white only, no grays)
  static bool jpegFileTo1BitBmpStream(FsFile& jpegFile, Print& bmpOut);
  // Convert to 1-bit BMP with custom target size (for thumbnails)
  static bool jpegFileTo1BitBmpStreamWithSize(FsFile& jpegFile, Print& bmpOut, int targetMaxWidth, int targetMaxHeight);
  // Quick preview mode: simple threshold instead of dithering (faster but lower quality)
  static bool jpegFileToBmpStreamQuick(FsFile& jpegFile, Print& bmpOut, int targetMaxWidth, int targetMaxHeight);
};
