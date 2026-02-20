/**
 * JPEG to BMP Converter using JPEGDEC library with scaling support
 * 
 * Uses JPEGDEC by Larry Bank (bitbank2) for fast JPEG decoding with built-in
 * DCT scaling (1/2, 1/4, 1/8). Then applies Atkinson dithering for e-ink quality.
 * 
 * Key insight: JPEGDEC's decodeDither() doesn't support scaling, so we use 
 * decode() with JPEG scaling to get grayscale, then apply our own Atkinson
 * dithering for best quality at any target size.
 */

#include "JpegToBmpConverter.h"

#include <HardwareSerial.h>
#include <SdFat.h>
#include <JPEGDEC.h>
#include <BitmapHelpers.h>

#include <algorithm>
#include <cstring>

// ============================================================================
// BMP HEADER WRITERS
// ============================================================================

static inline void write16(Print& out, uint16_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
}

static inline void write32(Print& out, uint32_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
  out.write((value >> 16) & 0xFF);
  out.write((value >> 24) & 0xFF);
}

static inline void write32Signed(Print& out, int32_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
  out.write((value >> 16) & 0xFF);
  out.write((value >> 24) & 0xFF);
}

static void writeBmpHeader1bit(Print& bmpOut, int width, int height) {
  const int bytesPerRow = (width + 31) / 32 * 4;
  const int imageSize = bytesPerRow * height;
  const uint32_t fileSize = 62 + imageSize;

  bmpOut.write('B');
  bmpOut.write('M');
  write32(bmpOut, fileSize);
  write32(bmpOut, 0);
  write32(bmpOut, 62);

  write32(bmpOut, 40);
  write32Signed(bmpOut, width);
  write32Signed(bmpOut, -height);
  write16(bmpOut, 1);
  write16(bmpOut, 1);
  write32(bmpOut, 0);
  write32(bmpOut, imageSize);
  write32(bmpOut, 2835);
  write32(bmpOut, 2835);
  write32(bmpOut, 2);
  write32(bmpOut, 2);

  const uint8_t palette[8] = {
      0x00, 0x00, 0x00, 0x00,
      0xFF, 0xFF, 0xFF, 0x00
  };
  for (uint8_t b : palette) bmpOut.write(b);
}

static void writeBmpHeader2bit(Print& bmpOut, int width, int height) {
  const int bytesPerRow = (width * 2 + 31) / 32 * 4;
  const int imageSize = bytesPerRow * height;
  const uint32_t fileSize = 70 + imageSize;

  bmpOut.write('B');
  bmpOut.write('M');
  write32(bmpOut, fileSize);
  write32(bmpOut, 0);
  write32(bmpOut, 70);

  write32(bmpOut, 40);
  write32Signed(bmpOut, width);
  write32Signed(bmpOut, -height);
  write16(bmpOut, 1);
  write16(bmpOut, 2);
  write32(bmpOut, 0);
  write32(bmpOut, imageSize);
  write32(bmpOut, 2835);
  write32(bmpOut, 2835);
  write32(bmpOut, 4);
  write32(bmpOut, 4);

  const uint8_t palette[16] = {
      0x00, 0x00, 0x00, 0x00,
      0x55, 0x55, 0x55, 0x00,
      0xAA, 0xAA, 0xAA, 0x00,
      0xFF, 0xFF, 0xFF, 0x00
  };
  for (uint8_t b : palette) bmpOut.write(b);
}

// ============================================================================
// GLOBAL STATE FOR JPEGDEC CALLBACKS
// ============================================================================

static FsFile* g_jpegFile = nullptr;
static Print* g_bmpOut = nullptr;
static int g_outWidth = 0;
static int g_outHeight = 0;
static int g_bytesPerRow = 0;
static bool g_oneBit = false;
static bool g_quickMode = false;
static bool g_aborted = false;
static std::function<bool()> g_shouldAbort = nullptr;

// Ditherers for Atkinson dithering
static AtkinsonDitherer* g_ditherer = nullptr;
static Atkinson1BitDitherer* g_1bitDitherer = nullptr;

// Row buffer for BMP output (one row at a time)
static uint8_t* g_rowBuffer = nullptr;
static int g_lastWrittenRow = -1;

// Debug flag
static bool g_firstDrawCall = true;

// ============================================================================
// FILE I/O CALLBACKS
// ============================================================================

static void* jpegOpen(const char* filename, int32_t* pFileSize) {
  if (!g_jpegFile) return nullptr;
  *pFileSize = g_jpegFile->size();
  g_jpegFile->seek(0);
  return g_jpegFile;
}

static void jpegClose(void* pHandle) {
  // File managed externally
}

static int32_t jpegRead(JPEGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  FsFile* file = static_cast<FsFile*>(pFile->fHandle);
  if (!file) return 0;
  return file->read(pBuf, iLen);
}

static int32_t jpegSeek(JPEGFILE* pFile, int32_t iPosition) {
  FsFile* file = static_cast<FsFile*>(pFile->fHandle);
  if (!file) return 0;
  return file->seek(iPosition) ? iPosition : 0;
}

// ============================================================================
// DRAW CALLBACK - receives decoded RGB565 pixels from JPEGDEC
// Buffers grayscale, then applies dithering when rows are complete
// ============================================================================

// Additional global state for proper row-by-row dithering
static uint8_t* g_grayBuffer = nullptr;  // Full image grayscale buffer (width * height)
static int* g_pixelCount = nullptr;      // Track pixels received per row

static int jpegDrawCallback(JPEGDRAW* pDraw) {
  if (!g_bmpOut || !g_grayBuffer) return 0;
  
  // Check abort
  if (g_shouldAbort && g_shouldAbort()) {
    g_aborted = true;
    return 0;
  }

  if (g_firstDrawCall) {
    Serial.printf("[%lu] [JPG] First draw: block at (%d,%d) size %dx%d, bpp=%d\n",
                  millis(), pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->iBpp);
    g_firstDrawCall = false;
  }

  const int blockX = pDraw->x;
  const int blockY = pDraw->y;
  const int blockW = pDraw->iWidth;
  const int blockH = pDraw->iHeight;
  
  // JPEGDEC decode() returns RGB565 by default (16 bits per pixel)
  const uint16_t* srcRGB565 = pDraw->pPixels;
  
  // Convert RGB565 to grayscale and store in buffer
  for (int row = 0; row < blockH; row++) {
    const int y = blockY + row;
    if (y >= g_outHeight) continue;
    
    for (int col = 0; col < blockW; col++) {
      const int x = blockX + col;
      if (x >= g_outWidth) continue;
      
      // Get RGB565 pixel and convert to grayscale
      const uint16_t rgb565 = srcRGB565[row * blockW + col];
      const uint8_t r = (rgb565 >> 11) << 3;  // 5 bits -> 8 bits
      const uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;  // 6 bits -> 8 bits
      const uint8_t b = (rgb565 & 0x1F) << 3;  // 5 bits -> 8 bits
      const uint8_t gray = static_cast<uint8_t>((r * 77 + g * 150 + b * 29) >> 8);
      
      // Store grayscale value
      g_grayBuffer[y * g_outWidth + x] = gray;
      g_pixelCount[y]++;
    }
  }
  
  // Process complete rows with dithering and write output
  // Rows must be processed in order for dithering to work correctly
  while (g_lastWrittenRow + 1 < g_outHeight && 
         g_pixelCount[g_lastWrittenRow + 1] >= g_outWidth) {
    g_lastWrittenRow++;
    const int y = g_lastWrittenRow;
    
    // Get grayscale row
    const uint8_t* grayRow = g_grayBuffer + y * g_outWidth;
    
    // Clear output row buffer
    memset(g_rowBuffer, 0, g_bytesPerRow);
    
    // Apply dithering and pack output
    for (int x = 0; x < g_outWidth; x++) {
      const uint8_t gray = grayRow[x];
      
      if (g_oneBit) {
        uint8_t bit;
        if (g_quickMode) {
          bit = gray > 127 ? 1 : 0;
        } else if (g_1bitDitherer) {
          bit = g_1bitDitherer->processPixel(gray, x);
        } else {
          bit = gray > 127 ? 1 : 0;
        }
        
        const int byteIndex = x / 8;
        const int bitOffset = 7 - (x % 8);
        if (bit) {
          g_rowBuffer[byteIndex] |= (1 << bitOffset);
        }
      } else {
        uint8_t twoBit;
        if (g_quickMode) {
          if (gray < 64) twoBit = 0;
          else if (gray < 128) twoBit = 1;
          else if (gray < 192) twoBit = 2;
          else twoBit = 3;
        } else if (g_ditherer) {
          twoBit = g_ditherer->processPixel(gray, x);
        } else {
          if (gray < 64) twoBit = 0;
          else if (gray < 128) twoBit = 1;
          else if (gray < 192) twoBit = 2;
          else twoBit = 3;
        }
        
        const int byteIndex = x / 4;
        const int bitOffset = 6 - (x % 4) * 2;
        g_rowBuffer[byteIndex] = (g_rowBuffer[byteIndex] & ~(0x03 << bitOffset)) | (twoBit << bitOffset);
      }
    }
    
    // Advance ditherer to next row
    if (g_ditherer) g_ditherer->nextRow();
    if (g_1bitDitherer) g_1bitDitherer->nextRow();
    
    // Write row to output
    g_bmpOut->write(g_rowBuffer, g_bytesPerRow);
  }
  
  return 1;  // Continue decoding
}

// ============================================================================
// MAIN CONVERSION FUNCTION
// ============================================================================

bool JpegToBmpConverter::jpegFileToBmpStreamInternal(FsFile& jpegFile, Print& bmpOut, 
                                        int targetWidth, int targetHeight,
                                        bool oneBit, bool quickMode,
                                        const std::function<bool()>& shouldAbort) {
  Serial.printf("[%lu] [JPG] Converting JPEG to %s BMP (target: %dx%d)%s\n", 
                millis(), oneBit ? "1-bit" : "2-bit", targetWidth, targetHeight,
                quickMode ? " [QUICK]" : "");

  // Setup global state
  g_jpegFile = &jpegFile;
  g_bmpOut = &bmpOut;
  g_oneBit = oneBit;
  g_quickMode = quickMode;
  g_aborted = false;
  g_shouldAbort = shouldAbort;
  g_firstDrawCall = true;
  g_ditherer = nullptr;
  g_1bitDitherer = nullptr;
  g_rowBuffer = nullptr;
  g_grayBuffer = nullptr;
  g_pixelCount = nullptr;
  g_lastWrittenRow = -1;
  
  JPEGDEC jpeg;
  
  // Open JPEG using file callbacks (streaming - handles any file size)
  jpegFile.seek(0);
  if (!jpeg.open("", jpegOpen, jpegClose, jpegRead, jpegSeek, jpegDrawCallback)) {
    Serial.printf("[%lu] [JPG] Failed to open JPEG: error %d\n", millis(), jpeg.getLastError());
    g_jpegFile = nullptr;
    return false;
  }
  
  const int imgWidth = jpeg.getWidth();
  const int imgHeight = jpeg.getHeight();
  Serial.printf("[%lu] [JPG] JPEG dimensions: %dx%d\n", millis(), imgWidth, imgHeight);
  
  // Safety check
  if (imgWidth <= 0 || imgHeight <= 0 || imgWidth > 4096 || imgHeight > 4096) {
    Serial.printf("[%lu] [JPG] Invalid dimensions\n", millis());
    jpeg.close();
    g_jpegFile = nullptr;
    return false;
  }
  
  // Determine scale factor using JPEGDEC's built-in DCT scaling
  // These are powers of 2: JPEG_SCALE_HALF (1/2), QUARTER (1/4), EIGHTH (1/8)
  int scale = 0;  // JPEG_SCALE_FULL = 0
  int outWidth = imgWidth;
  int outHeight = imgHeight;
  
  if (targetWidth > 0 && targetHeight > 0) {
    // Find the smallest scale that gives us >= target size
    // This minimizes both memory usage and processing time
    
    // Check 1/8 scale
    int w8 = (imgWidth + 7) / 8;
    int h8 = (imgHeight + 7) / 8;
    
    // Check 1/4 scale
    int w4 = (imgWidth + 3) / 4;
    int h4 = (imgHeight + 3) / 4;
    
    // Check 1/2 scale  
    int w2 = (imgWidth + 1) / 2;
    int h2 = (imgHeight + 1) / 2;
    
    // Pick the best scale factor
    // We want the largest scale that still fits in target
    if (w8 <= targetWidth && h8 <= targetHeight && 
        imgWidth > targetWidth * 4 && imgHeight > targetHeight * 4) {
      scale = 3;  // JPEG_SCALE_EIGHTH
      outWidth = w8;
      outHeight = h8;
    } else if (w4 <= targetWidth && h4 <= targetHeight &&
               imgWidth > targetWidth * 2 && imgHeight > targetHeight * 2) {
      scale = 2;  // JPEG_SCALE_QUARTER
      outWidth = w4;
      outHeight = h4;
    } else if (w2 <= targetWidth && h2 <= targetHeight &&
               (imgWidth > targetWidth || imgHeight > targetHeight)) {
      scale = 1;  // JPEG_SCALE_HALF
      outWidth = w2;
      outHeight = h2;
    }
    // else: keep full scale
    
    if (scale > 0) {
      Serial.printf("[%lu] [JPG] Using scale 1/%d: %dx%d -> %dx%d (target: %dx%d)\n",
                    millis(), 1 << scale, imgWidth, imgHeight, outWidth, outHeight, 
                    targetWidth, targetHeight);
    }
  }
  
  // Set global dimensions
  g_outWidth = outWidth;
  g_outHeight = outHeight;
  
  // Calculate BMP row size
  if (oneBit) {
    g_bytesPerRow = (outWidth + 31) / 32 * 4;
  } else {
    g_bytesPerRow = (outWidth * 2 + 31) / 32 * 4;
  }
  
  // Check total buffer size needed
  // We need: grayscale buffer (width * height bytes) + pixel count (height * sizeof(int)) + one output row
  const size_t grayBufferSize = static_cast<size_t>(outWidth) * outHeight;
  const size_t pixelCountSize = static_cast<size_t>(outHeight) * sizeof(int);
  const size_t rowBufferSize = g_bytesPerRow;
  const size_t totalNeeded = grayBufferSize + pixelCountSize + rowBufferSize;
  
  // Safety check - don't try to allocate more than ~200KB
  constexpr size_t MAX_BUFFER_SIZE = 200 * 1024;
  if (totalNeeded > MAX_BUFFER_SIZE) {
    Serial.printf("[%lu] [JPG] Output too large: %zu bytes (max %zu)\n", 
                  millis(), totalNeeded, MAX_BUFFER_SIZE);
    jpeg.close();
    g_jpegFile = nullptr;
    return false;
  }
  
  // Allocate buffers
  g_grayBuffer = static_cast<uint8_t*>(malloc(grayBufferSize));
  g_pixelCount = static_cast<int*>(calloc(outHeight, sizeof(int)));
  g_rowBuffer = static_cast<uint8_t*>(malloc(rowBufferSize));
  
  if (!g_grayBuffer || !g_pixelCount || !g_rowBuffer) {
    Serial.printf("[%lu] [JPG] Failed to allocate buffers (%zu bytes)\n", millis(), totalNeeded);
    if (g_grayBuffer) free(g_grayBuffer);
    if (g_pixelCount) free(g_pixelCount);
    if (g_rowBuffer) free(g_rowBuffer);
    jpeg.close();
    g_jpegFile = nullptr;
    return false;
  }
  memset(g_grayBuffer, 0, grayBufferSize);
  
  Serial.printf("[%lu] [JPG] Allocated %zu bytes for %dx%d output\n", 
                millis(), totalNeeded, outWidth, outHeight);
  
  // Create ditherer if not in quick mode
  if (!quickMode) {
    if (oneBit) {
      g_1bitDitherer = new Atkinson1BitDitherer(outWidth);
    } else {
      g_ditherer = new AtkinsonDitherer(outWidth);
    }
  }
  
  // Write BMP header
  if (oneBit) {
    writeBmpHeader1bit(bmpOut, outWidth, outHeight);
  } else {
    writeBmpHeader2bit(bmpOut, outWidth, outHeight);
  }
  
  // Decode with scaling
  Serial.printf("[%lu] [JPG] Starting decode (scale=%d)...\n", millis(), scale);
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);  // Request RGB565 output
  const int decodeResult = jpeg.decode(0, 0, scale);
  
  // Process any remaining unwritten rows (in case decode completed but callback didn't write all)
  while (g_lastWrittenRow + 1 < g_outHeight && 
         g_pixelCount[g_lastWrittenRow + 1] >= g_outWidth) {
    g_lastWrittenRow++;
    const int y = g_lastWrittenRow;
    const uint8_t* grayRow = g_grayBuffer + y * g_outWidth;
    
    memset(g_rowBuffer, 0, g_bytesPerRow);
    for (int x = 0; x < g_outWidth; x++) {
      const uint8_t gray = grayRow[x];
      if (g_oneBit) {
        uint8_t bit = gray > 127 ? 1 : 0;
        if (!g_quickMode && g_1bitDitherer) {
          bit = g_1bitDitherer->processPixel(gray, x);
        }
        const int byteIndex = x / 8;
        const int bitOffset = 7 - (x % 8);
        if (bit) g_rowBuffer[byteIndex] |= (1 << bitOffset);
      } else {
        uint8_t twoBit;
        if (g_quickMode || !g_ditherer) {
          if (gray < 64) twoBit = 0;
          else if (gray < 128) twoBit = 1;
          else if (gray < 192) twoBit = 2;
          else twoBit = 3;
        } else {
          twoBit = g_ditherer->processPixel(gray, x);
        }
        const int byteIndex = x / 4;
        const int bitOffset = 6 - (x % 4) * 2;
        g_rowBuffer[byteIndex] = (g_rowBuffer[byteIndex] & ~(0x03 << bitOffset)) | (twoBit << bitOffset);
      }
    }
    if (g_ditherer) g_ditherer->nextRow();
    if (g_1bitDitherer) g_1bitDitherer->nextRow();
    bmpOut.write(g_rowBuffer, g_bytesPerRow);
  }
  
  // Cleanup
  if (g_ditherer) {
    delete g_ditherer;
    g_ditherer = nullptr;
  }
  if (g_1bitDitherer) {
    delete g_1bitDitherer;
    g_1bitDitherer = nullptr;
  }
  if (g_rowBuffer) {
    free(g_rowBuffer);
    g_rowBuffer = nullptr;
  }
  if (g_grayBuffer) {
    free(g_grayBuffer);
    g_grayBuffer = nullptr;
  }
  if (g_pixelCount) {
    free(g_pixelCount);
    g_pixelCount = nullptr;
  }
  
  jpeg.close();
  g_jpegFile = nullptr;
  
  if (g_aborted) {
    Serial.printf("[%lu] [JPG] Decode aborted\n", millis());
    return false;
  }
  
  if (decodeResult != 1) {
    Serial.printf("[%lu] [JPG] Decode failed: %d\n", millis(), decodeResult);
    return false;
  }
  
  Serial.printf("[%lu] [JPG] Successfully converted JPEG to %dx%d BMP\n", 
                millis(), outWidth, outHeight);
  return true;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool JpegToBmpConverter::jpegFileToBmpStream(FsFile& jpegFile, Print& bmpOut) {
  return jpegFileToBmpStreamInternal(jpegFile, bmpOut, 450, 750, false);
}

bool JpegToBmpConverter::jpegFileToBmpStreamWithSize(FsFile& jpegFile, Print& bmpOut, 
                                                     int targetMaxWidth, int targetMaxHeight,
                                                     const std::function<bool()>& shouldAbort) {
  return jpegFileToBmpStreamInternal(jpegFile, bmpOut, targetMaxWidth, targetMaxHeight, false, false, shouldAbort);
}

bool JpegToBmpConverter::jpegFileTo1BitBmpStream(FsFile& jpegFile, Print& bmpOut) {
  return jpegFileToBmpStreamInternal(jpegFile, bmpOut, 450, 750, true);
}

bool JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(FsFile& jpegFile, Print& bmpOut, 
                                                         int targetMaxWidth, int targetMaxHeight) {
  return jpegFileToBmpStreamInternal(jpegFile, bmpOut, targetMaxWidth, targetMaxHeight, true);
}

bool JpegToBmpConverter::jpegFileToBmpStreamQuick(FsFile& jpegFile, Print& bmpOut, 
                                                   int targetMaxWidth, int targetMaxHeight) {
  return jpegFileToBmpStreamInternal(jpegFile, bmpOut, targetMaxWidth, targetMaxHeight, false, true);
}
