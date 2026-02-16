#include "test_utils.h"

#include <algorithm>
#include <cmath>

// Helper to compute expected output dimensions (mirrors the logic in JpegToBmpConverter.cpp)
// This tests the "contain" mode scaling: min(scaleToFitWidth, scaleToFitHeight)
// The image is scaled to fit entirely within the target bounds while preserving aspect ratio.
struct ScalingResult {
  int outWidth;
  int outHeight;
  bool needsScaling;
};

ScalingResult computeScaledDimensions(int srcWidth, int srcHeight, int targetWidth, int targetHeight) {
  ScalingResult result = {srcWidth, srcHeight, false};

  // Mirror the condition from JpegToBmpConverter.cpp line 298
  if (targetWidth > 0 && targetHeight > 0 && (srcWidth > targetWidth || srcHeight > targetHeight)) {
    const float scaleToFitWidth = static_cast<float>(targetWidth) / srcWidth;
    const float scaleToFitHeight = static_cast<float>(targetHeight) / srcHeight;
    // Contain mode: use smaller scale factor to ensure image fits within target
    const float scale = std::min(scaleToFitWidth, scaleToFitHeight);

    result.outWidth = static_cast<int>(srcWidth * scale);
    result.outHeight = static_cast<int>(srcHeight * scale);

    // Ensure at least 1 pixel
    if (result.outWidth < 1) result.outWidth = 1;
    if (result.outHeight < 1) result.outHeight = 1;
    result.needsScaling = true;
  }

  return result;
}

int main() {
  TestUtils::TestRunner runner("JpegToBmpConverter Scaling");

  // ============================================
  // Test contain mode scaling (min scale factor)
  // Target: 480x800 (portrait e-ink display)
  // ============================================

  // Test 1: Landscape image into portrait target (width-constrained)
  // Input: 800x400, Target: 480x800
  // scaleToFitWidth = 480/800 = 0.6
  // scaleToFitHeight = 800/400 = 2.0
  // scale = min(0.6, 2.0) = 0.6
  // Output: 800*0.6 x 400*0.6 = 480x240
  {
    ScalingResult r = computeScaledDimensions(800, 400, 480, 800);
    runner.expectTrue(r.needsScaling, "Landscape 800x400: needs scaling");
    runner.expectEq(480, r.outWidth, "Landscape 800x400: width = 480 (fits exactly)");
    runner.expectEq(240, r.outHeight, "Landscape 800x400: height = 240 (within bounds)");
  }

  // Test 2: Portrait image into portrait target (height exceeds - needs scaling)
  // Input: 400x900, Target: 480x800
  // scaleToFitWidth = 480/400 = 1.2
  // scaleToFitHeight = 800/900 = 0.889
  // scale = min(1.2, 0.889) = 0.889
  // Output: 400*0.889 x 900*0.889 = 355x800
  {
    ScalingResult r = computeScaledDimensions(400, 900, 480, 800);
    runner.expectTrue(r.needsScaling, "Portrait 400x900: needs scaling (height exceeds)");
    runner.expectEq(355, r.outWidth, "Portrait 400x900: width = 355 (within bounds)");
    runner.expectEq(800, r.outHeight, "Portrait 400x900: height = 800 (fits exactly)");
  }

  // Test 3: Square image into rectangular target (width-constrained)
  // Input: 600x600, Target: 480x800
  // scaleToFitWidth = 480/600 = 0.8
  // scaleToFitHeight = 800/600 = 1.333
  // scale = min(0.8, 1.333) = 0.8
  // Output: 600*0.8 x 600*0.8 = 480x480
  {
    ScalingResult r = computeScaledDimensions(600, 600, 480, 800);
    runner.expectTrue(r.needsScaling, "Square 600x600: needs scaling");
    runner.expectEq(480, r.outWidth, "Square 600x600: width = 480 (fits exactly)");
    runner.expectEq(480, r.outHeight, "Square 600x600: height = 480 (within bounds)");
  }

  // Test 4: Image smaller than target - no scaling needed
  // Input: 200x300, Target: 480x800
  // Neither dimension exceeds target, so no scaling
  {
    ScalingResult r = computeScaledDimensions(200, 300, 480, 800);
    runner.expectFalse(r.needsScaling, "Small 200x300: no scaling needed");
    runner.expectEq(200, r.outWidth, "Small 200x300: width preserved");
    runner.expectEq(300, r.outHeight, "Small 200x300: height preserved");
  }

  // Test 5: Very wide panoramic image (extreme width-constrained)
  // Input: 1200x200, Target: 480x800
  // scaleToFitWidth = 480/1200 = 0.4
  // scaleToFitHeight = 800/200 = 4.0
  // scale = min(0.4, 4.0) = 0.4
  // Output: 1200*0.4 x 200*0.4 = 480x80
  {
    ScalingResult r = computeScaledDimensions(1200, 200, 480, 800);
    runner.expectTrue(r.needsScaling, "Panoramic 1200x200: needs scaling");
    runner.expectEq(480, r.outWidth, "Panoramic 1200x200: width = 480 (fits exactly)");
    runner.expectEq(80, r.outHeight, "Panoramic 1200x200: height = 80 (within bounds)");
  }

  // Test 6: Very tall image (extreme height-constrained)
  // Input: 200x1600, Target: 480x800
  // scaleToFitWidth = 480/200 = 2.4
  // scaleToFitHeight = 800/1600 = 0.5
  // scale = min(2.4, 0.5) = 0.5
  // Output: 200*0.5 x 1600*0.5 = 100x800
  {
    ScalingResult r = computeScaledDimensions(200, 1600, 480, 800);
    runner.expectTrue(r.needsScaling, "Tall 200x1600: needs scaling");
    runner.expectEq(100, r.outWidth, "Tall 200x1600: width = 100 (within bounds)");
    runner.expectEq(800, r.outHeight, "Tall 200x1600: height = 800 (fits exactly)");
  }

  // ============================================
  // Edge cases
  // ============================================

  // Test 7: Image exactly at target size - no scaling
  {
    ScalingResult r = computeScaledDimensions(480, 800, 480, 800);
    runner.expectFalse(r.needsScaling, "Exact fit 480x800: no scaling needed");
    runner.expectEq(480, r.outWidth, "Exact fit 480x800: width preserved");
    runner.expectEq(800, r.outHeight, "Exact fit 480x800: height preserved");
  }

  // Test 8: One dimension at limit, other within - no scaling
  // Width exactly at target, height within
  {
    ScalingResult r = computeScaledDimensions(480, 600, 480, 800);
    runner.expectFalse(r.needsScaling, "Width at limit 480x600: no scaling needed");
    runner.expectEq(480, r.outWidth, "Width at limit 480x600: width preserved");
    runner.expectEq(600, r.outHeight, "Width at limit 480x600: height preserved");
  }

  // Test 9: Width exceeds, height exactly at target
  // Input: 600x800, Target: 480x800
  // scaleToFitWidth = 480/600 = 0.8
  // scaleToFitHeight = 800/800 = 1.0
  // scale = min(0.8, 1.0) = 0.8
  // Output: 600*0.8 x 800*0.8 = 480x640
  {
    ScalingResult r = computeScaledDimensions(600, 800, 480, 800);
    runner.expectTrue(r.needsScaling, "Width exceeds 600x800: needs scaling");
    runner.expectEq(480, r.outWidth, "Width exceeds 600x800: width = 480");
    runner.expectEq(640, r.outHeight, "Width exceeds 600x800: height = 640");
  }

  // Test 10: Height exceeds, width exactly at target
  // Input: 480x1000, Target: 480x800
  // scaleToFitWidth = 480/480 = 1.0
  // scaleToFitHeight = 800/1000 = 0.8
  // scale = min(1.0, 0.8) = 0.8
  // Output: 480*0.8 x 1000*0.8 = 384x800
  {
    ScalingResult r = computeScaledDimensions(480, 1000, 480, 800);
    runner.expectTrue(r.needsScaling, "Height exceeds 480x1000: needs scaling");
    runner.expectEq(384, r.outWidth, "Height exceeds 480x1000: width = 384");
    runner.expectEq(800, r.outHeight, "Height exceeds 480x1000: height = 800");
  }

  // Test 11: Both dimensions exceed target (both must fit)
  // Input: 960x1600, Target: 480x800
  // scaleToFitWidth = 480/960 = 0.5
  // scaleToFitHeight = 800/1600 = 0.5
  // scale = min(0.5, 0.5) = 0.5
  // Output: 960*0.5 x 1600*0.5 = 480x800
  {
    ScalingResult r = computeScaledDimensions(960, 1600, 480, 800);
    runner.expectTrue(r.needsScaling, "Both exceed 960x1600: needs scaling");
    runner.expectEq(480, r.outWidth, "Both exceed 960x1600: width = 480");
    runner.expectEq(800, r.outHeight, "Both exceed 960x1600: height = 800");
  }

  // Test 12: Very small source (minimum output is 1x1)
  // Input: 10x10, Target: 2x2, scale = 0.2
  // Output: 10*0.2 x 10*0.2 = 2x2
  {
    ScalingResult r = computeScaledDimensions(10, 10, 2, 2);
    runner.expectTrue(r.needsScaling, "Tiny 10x10 to 2x2: needs scaling");
    runner.expectEq(2, r.outWidth, "Tiny 10x10 to 2x2: width = 2");
    runner.expectEq(2, r.outHeight, "Tiny 10x10 to 2x2: height = 2");
  }

  // Test 13: Extreme downscale - ensure minimum 1 pixel
  // Input: 1000x1000, Target: 1x1
  // scale = min(1/1000, 1/1000) = 0.001
  // Output would be 1x1 (clamped from 1000*0.001 = 1)
  {
    ScalingResult r = computeScaledDimensions(1000, 1000, 1, 1);
    runner.expectTrue(r.needsScaling, "Extreme downscale 1000x1000 to 1x1: needs scaling");
    runner.expectTrue(r.outWidth >= 1, "Extreme downscale: width >= 1");
    runner.expectTrue(r.outHeight >= 1, "Extreme downscale: height >= 1");
  }

  // Test 14: Zero target dimensions - no scaling (invalid target)
  {
    ScalingResult r = computeScaledDimensions(800, 600, 0, 0);
    runner.expectFalse(r.needsScaling, "Zero target: no scaling");
    runner.expectEq(800, r.outWidth, "Zero target: width preserved");
    runner.expectEq(600, r.outHeight, "Zero target: height preserved");
  }

  // Test 15: Only width exceeds target (common case)
  // Input: 800x300, Target: 480x800
  // scaleToFitWidth = 480/800 = 0.6
  // scaleToFitHeight = 800/300 = 2.666
  // scale = min(0.6, 2.666) = 0.6
  // Output: 800*0.6 x 300*0.6 = 480x180
  {
    ScalingResult r = computeScaledDimensions(800, 300, 480, 800);
    runner.expectTrue(r.needsScaling, "Width only exceeds 800x300: needs scaling");
    runner.expectEq(480, r.outWidth, "Width only exceeds 800x300: width = 480");
    runner.expectEq(180, r.outHeight, "Width only exceeds 800x300: height = 180");
  }

  // Test 16: Only height exceeds target
  // Input: 300x1000, Target: 480x800
  // scaleToFitWidth = 480/300 = 1.6
  // scaleToFitHeight = 800/1000 = 0.8
  // scale = min(1.6, 0.8) = 0.8
  // Output: 300*0.8 x 1000*0.8 = 240x800
  {
    ScalingResult r = computeScaledDimensions(300, 1000, 480, 800);
    runner.expectTrue(r.needsScaling, "Height only exceeds 300x1000: needs scaling");
    runner.expectEq(240, r.outWidth, "Height only exceeds 300x1000: width = 240");
    runner.expectEq(800, r.outHeight, "Height only exceeds 300x1000: height = 800");
  }

  // ============================================
  // Aspect ratio preservation verification
  // ============================================

  // Test 17: Verify aspect ratio is preserved (16:9 image)
  // Input: 1920x1080, Target: 480x800
  // scaleToFitWidth = 480/1920 = 0.25
  // scaleToFitHeight = 800/1080 = 0.7407
  // scale = 0.25
  // Output: 1920*0.25 x 1080*0.25 = 480x270
  // Original ratio: 1920/1080 = 1.778
  // Output ratio: 480/270 = 1.778 ✓
  {
    ScalingResult r = computeScaledDimensions(1920, 1080, 480, 800);
    runner.expectTrue(r.needsScaling, "16:9 image: needs scaling");
    float originalRatio = 1920.0f / 1080.0f;
    float outputRatio = static_cast<float>(r.outWidth) / r.outHeight;
    runner.expectFloatEq(originalRatio, outputRatio, "16:9 image: aspect ratio preserved", 0.01f);
  }

  // Test 18: Verify aspect ratio for portrait image (9:16)
  // Input: 1080x1920, Target: 480x800
  // scaleToFitWidth = 480/1080 = 0.4444
  // scaleToFitHeight = 800/1920 = 0.4166
  // scale = 0.4166
  // Output: 1080*0.4166 x 1920*0.4166 = 450x800
  {
    ScalingResult r = computeScaledDimensions(1080, 1920, 480, 800);
    runner.expectTrue(r.needsScaling, "9:16 image: needs scaling");
    float originalRatio = 1080.0f / 1920.0f;
    float outputRatio = static_cast<float>(r.outWidth) / r.outHeight;
    runner.expectFloatEq(originalRatio, outputRatio, "9:16 image: aspect ratio preserved", 0.01f);
  }

  // ============================================
  // Contain vs Cover mode verification
  // These tests verify we're using contain (smaller scale)
  // not cover (larger scale)
  // ============================================

  // Test 19: Verify contain mode (not cover)
  // In cover mode, we'd use max() to fill the target, cropping as needed
  // In contain mode, we use min() to fit entirely, with letterboxing
  // Input: 800x400, Target: 480x800
  // Cover would use: max(0.6, 2.0) = 2.0, giving 1600x800 (overflows width!)
  // Contain uses: min(0.6, 2.0) = 0.6, giving 480x240 (fits both)
  {
    ScalingResult r = computeScaledDimensions(800, 400, 480, 800);
    runner.expectTrue(r.outWidth <= 480, "Contain mode: output width <= target width");
    runner.expectTrue(r.outHeight <= 800, "Contain mode: output height <= target height");
  }

  // Test 20: Another contain mode verification
  // Input: 400x800, Target: 480x800
  // Cover would use: max(1.2, 1.0) = 1.2, giving 480x960 (overflows height!)
  // Contain uses: min(1.2, 1.0) = 1.0, giving 400x800 (fits both)
  {
    ScalingResult r = computeScaledDimensions(400, 800, 480, 800);
    runner.expectTrue(r.outWidth <= 480, "Contain mode tall: output width <= target width");
    runner.expectTrue(r.outHeight <= 800, "Contain mode tall: output height <= target height");
  }

  return runner.allPassed() ? 0 : 1;
}
