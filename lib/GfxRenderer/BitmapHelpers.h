#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

// Helper functions
uint8_t quantize(int gray, int x, int y);
uint8_t quantizeSimple(int gray);
uint8_t quantize1bit(int gray, int x, int y);
int adjustPixel(int gray);

// RGB to grayscale conversion using BT.601 coefficients via lookup tables.
// Avoids 3 multiplications per pixel on ESP32-C3 (no FPU).
// Note: Sum of max values is 254 (not 255) due to integer truncation of coefficients.
// This is expected behavior - pure white (255,255,255) maps to 254.
uint8_t rgbToGray(uint8_t r, uint8_t g, uint8_t b);

// Scale down a BMP file to create a 1-bit thumbnail.
// Uses 2x2 pixel averaging for clean downscaling with Atkinson dithering.
// Returns true on success, false on failure.
bool bmpTo1BitBmpScaled(const char* srcPath, const char* dstPath, int targetMaxWidth, int targetMaxHeight);

// 1-bit Atkinson dithering - better quality than noise dithering for thumbnails
// Error distribution pattern (same as 2-bit but quantizes to 2 levels):
//     X  1/8 1/8
// 1/8 1/8 1/8
//     1/8
class Atkinson1BitDitherer {
 public:
  static constexpr int PADDING = 16;  // Extra padding for safety
  
  explicit Atkinson1BitDitherer(int width) : width(width), allocSize(width + PADDING) {
    // Use calloc for zero-initialization
    errorRow0 = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    errorRow1 = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    errorRow2 = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    if (!errorRow0 || !errorRow1 || !errorRow2) {
      free(errorRow0);
      free(errorRow1);
      free(errorRow2);
      errorRow0 = errorRow1 = errorRow2 = nullptr;
    }
  }

  ~Atkinson1BitDitherer() {
    free(errorRow0);
    free(errorRow1);
    free(errorRow2);
  }

  Atkinson1BitDitherer(const Atkinson1BitDitherer& other) = delete;
  Atkinson1BitDitherer& operator=(const Atkinson1BitDitherer& other) = delete;

  bool isValid() const { return errorRow0 != nullptr; }

  uint8_t processPixel(int gray, int x) {
    // Apply brightness/contrast/gamma adjustments
    gray = adjustPixel(gray);

    // Add accumulated error
    int adjusted = gray + errorRow0[x + 2];
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    // Quantize to 2 levels (1-bit): 0 = black, 1 = white
    uint8_t quantized;
    int quantizedValue;
    if (adjusted < 128) {
      quantized = 0;
      quantizedValue = 0;
    } else {
      quantized = 1;
      quantizedValue = 255;
    }

    // Calculate error (only distribute 6/8 = 75%)
    int error = (adjusted - quantizedValue) >> 3;  // error/8

    // Distribute 1/8 to each of 6 neighbors
    errorRow0[x + 3] += error;  // Right
    errorRow0[x + 4] += error;  // Right+1
    errorRow1[x + 1] += error;  // Bottom-left
    errorRow1[x + 2] += error;  // Bottom
    errorRow1[x + 3] += error;  // Bottom-right
    errorRow2[x + 2] += error;  // Two rows down

    return quantized;
  }

  void nextRow() {
    int16_t* temp = errorRow0;
    errorRow0 = errorRow1;
    errorRow1 = errorRow2;
    errorRow2 = temp;
    memset(errorRow2, 0, allocSize * sizeof(int16_t));
  }

  void reset() {
    memset(errorRow0, 0, allocSize * sizeof(int16_t));
    memset(errorRow1, 0, allocSize * sizeof(int16_t));
    memset(errorRow2, 0, allocSize * sizeof(int16_t));
  }

 private:
  int width;
  int allocSize;
  int16_t* errorRow0;
  int16_t* errorRow1;
  int16_t* errorRow2;
};

// Atkinson dithering - distributes only 6/8 (75%) of error for cleaner results
// Error distribution pattern:
//     X  1/8 1/8
// 1/8 1/8 1/8
//     1/8
// Less error buildup = fewer artifacts than Floyd-Steinberg
class AtkinsonDitherer {
 public:
  static constexpr int PADDING = 16;  // Extra padding for safety (max access is x+4, so 16 is overkill)
  
  explicit AtkinsonDitherer(int width) : width(width), allocSize(width + PADDING) {
    // Use calloc for zero-initialization to prevent undefined behavior
    errorRow0 = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    errorRow1 = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    errorRow2 = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    if (!errorRow0 || !errorRow1 || !errorRow2) {
      free(errorRow0);
      free(errorRow1);
      free(errorRow2);
      errorRow0 = errorRow1 = errorRow2 = nullptr;
    }
  }

  ~AtkinsonDitherer() {
    free(errorRow0);
    free(errorRow1);
    free(errorRow2);
  }
  AtkinsonDitherer(const AtkinsonDitherer& other) = delete;
  AtkinsonDitherer& operator=(const AtkinsonDitherer& other) = delete;

  bool isValid() const { return errorRow0 != nullptr; }

  uint8_t processPixel(int gray, int x) {
    // Add accumulated error
    int adjusted = gray + errorRow0[x + 2];
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    // Quantize to 4 levels
    uint8_t quantized;
    int quantizedValue;
    if (false) {  // original thresholds
      if (adjusted < 43) {
        quantized = 0;
        quantizedValue = 0;
      } else if (adjusted < 128) {
        quantized = 1;
        quantizedValue = 85;
      } else if (adjusted < 213) {
        quantized = 2;
        quantizedValue = 170;
      } else {
        quantized = 3;
        quantizedValue = 255;
      }
    } else {  // fine-tuned to X4 eink display
      if (adjusted < 30) {
        quantized = 0;
        quantizedValue = 15;
      } else if (adjusted < 50) {
        quantized = 1;
        quantizedValue = 30;
      } else if (adjusted < 140) {
        quantized = 2;
        quantizedValue = 80;
      } else {
        quantized = 3;
        quantizedValue = 210;
      }
    }

    // Calculate error (only distribute 6/8 = 75%)
    int error = (adjusted - quantizedValue) >> 3;  // error/8

    // Distribute 1/8 to each of 6 neighbors
    errorRow0[x + 3] += error;  // Right
    errorRow0[x + 4] += error;  // Right+1
    errorRow1[x + 1] += error;  // Bottom-left
    errorRow1[x + 2] += error;  // Bottom
    errorRow1[x + 3] += error;  // Bottom-right
    errorRow2[x + 2] += error;  // Two rows down

    return quantized;
  }

  void nextRow() {
    int16_t* temp = errorRow0;
    errorRow0 = errorRow1;
    errorRow1 = errorRow2;
    errorRow2 = temp;
    memset(errorRow2, 0, allocSize * sizeof(int16_t));
  }

  void reset() {
    memset(errorRow0, 0, allocSize * sizeof(int16_t));
    memset(errorRow1, 0, allocSize * sizeof(int16_t));
    memset(errorRow2, 0, allocSize * sizeof(int16_t));
  }

 private:
  int width;
  int allocSize;
  int16_t* errorRow0;
  int16_t* errorRow1;
  int16_t* errorRow2;
};

// Floyd-Steinberg error diffusion dithering with serpentine scanning
// Serpentine scanning alternates direction each row to reduce "worm" artifacts
// Error distribution pattern (left-to-right):
//       X   7/16
// 3/16 5/16 1/16
// Error distribution pattern (right-to-left, mirrored):
// 1/16 5/16 3/16
//      7/16  X
class FloydSteinbergDitherer {
 public:
  static constexpr int PADDING = 16;  // Extra padding for safety
  
  explicit FloydSteinbergDitherer(int width) : width(width), allocSize(width + PADDING), rowCount(0) {
    // Use calloc for zero-initialization
    errorCurRow = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    errorNextRow = static_cast<int16_t*>(calloc(allocSize, sizeof(int16_t)));
    if (!errorCurRow || !errorNextRow) {
      free(errorCurRow);
      free(errorNextRow);
      errorCurRow = errorNextRow = nullptr;
    }
  }

  ~FloydSteinbergDitherer() {
    free(errorCurRow);
    free(errorNextRow);
  }

  FloydSteinbergDitherer(const FloydSteinbergDitherer& other) = delete;
  FloydSteinbergDitherer& operator=(const FloydSteinbergDitherer& other) = delete;

  bool isValid() const { return errorCurRow != nullptr; }

  // Process a single pixel and return quantized 2-bit value
  // x is the logical x position (0 to width-1), direction handled internally
  uint8_t processPixel(int gray, int x) {
    // Add accumulated error to this pixel
    int adjusted = gray + errorCurRow[x + 1];

    // Clamp to valid range
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    // Quantize to 4 levels (0, 85, 170, 255)
    uint8_t quantized;
    int quantizedValue;
    if (false) {  // original thresholds
      if (adjusted < 43) {
        quantized = 0;
        quantizedValue = 0;
      } else if (adjusted < 128) {
        quantized = 1;
        quantizedValue = 85;
      } else if (adjusted < 213) {
        quantized = 2;
        quantizedValue = 170;
      } else {
        quantized = 3;
        quantizedValue = 255;
      }
    } else {  // fine-tuned to X4 eink display
      if (adjusted < 30) {
        quantized = 0;
        quantizedValue = 15;
      } else if (adjusted < 50) {
        quantized = 1;
        quantizedValue = 30;
      } else if (adjusted < 140) {
        quantized = 2;
        quantizedValue = 80;
      } else {
        quantized = 3;
        quantizedValue = 210;
      }
    }

    // Calculate error
    int error = adjusted - quantizedValue;

    // Distribute error to neighbors (serpentine: direction-aware)
    if (!isReverseRow()) {
      // Left to right: standard distribution
      // Right: 7/16
      errorCurRow[x + 2] += (error * 7) >> 4;
      // Bottom-left: 3/16
      errorNextRow[x] += (error * 3) >> 4;
      // Bottom: 5/16
      errorNextRow[x + 1] += (error * 5) >> 4;
      // Bottom-right: 1/16
      errorNextRow[x + 2] += (error) >> 4;
    } else {
      // Right to left: mirrored distribution
      // Left: 7/16
      errorCurRow[x] += (error * 7) >> 4;
      // Bottom-right: 3/16
      errorNextRow[x + 2] += (error * 3) >> 4;
      // Bottom: 5/16
      errorNextRow[x + 1] += (error * 5) >> 4;
      // Bottom-left: 1/16
      errorNextRow[x] += (error) >> 4;
    }

    return quantized;
  }

  // Call at the end of each row to swap buffers
  void nextRow() {
    // Swap buffers
    int16_t* temp = errorCurRow;
    errorCurRow = errorNextRow;
    errorNextRow = temp;
    // Clear the next row buffer
    memset(errorNextRow, 0, allocSize * sizeof(int16_t));
    rowCount++;
  }

  // Check if current row should be processed in reverse
  bool isReverseRow() const { return (rowCount & 1) != 0; }

  // Reset for a new image or MCU block
  void reset() {
    memset(errorCurRow, 0, allocSize * sizeof(int16_t));
    memset(errorNextRow, 0, allocSize * sizeof(int16_t));
    rowCount = 0;
  }

 private:
  int width;
  int allocSize;
  int rowCount;
  int16_t* errorCurRow;
  int16_t* errorNextRow;
};
