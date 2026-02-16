#include "test_utils.h"

#include <EInkDisplay.h>
#include <array>
#include <map>

// Forward declarations to match GfxRenderer.h
class ExternalFont;
class StreamingEpdFont;

// Minimal GfxRenderer with orientation, drawPixel, and begin() for testing
// coordinate rotation and framebuffer caching from commit 51a880f
class GfxRenderer {
 public:
  enum Orientation {
    Portrait,
    LandscapeClockwise,
    PortraitInverted,
    LandscapeCounterClockwise
  };

  explicit GfxRenderer(EInkDisplay& display) : einkDisplay(display), orientation(Portrait) {}

  void begin() { frameBuffer = einkDisplay.getFrameBuffer(); }

  void setOrientation(const Orientation o) { orientation = o; }
  Orientation getOrientation() const { return orientation; }

  uint8_t* getFrameBuffer() const { return frameBuffer; }

  void drawPixel(int x, int y, bool state = true) const {
    int rotatedX = 0;
    int rotatedY = 0;
    rotateCoordinates(orientation, x, y, &rotatedX, &rotatedY);

    if (rotatedX < 0 || rotatedX >= EInkDisplay::DISPLAY_WIDTH || rotatedY < 0 ||
        rotatedY >= EInkDisplay::DISPLAY_HEIGHT) {
      return;
    }

    const uint16_t byteIndex = rotatedY * EInkDisplay::DISPLAY_WIDTH_BYTES + (rotatedX / 8);
    const uint8_t bitPosition = 7 - (rotatedX % 8);

    if (state) {
      frameBuffer[byteIndex] &= ~(1 << bitPosition);
    } else {
      frameBuffer[byteIndex] |= 1 << bitPosition;
    }
  }

  void clearScreen(uint8_t color = 0xFF) const { einkDisplay.clearScreen(color); }

 private:
  EInkDisplay& einkDisplay;
  Orientation orientation;
  uint8_t* frameBuffer = nullptr;

  static inline void rotateCoordinates(Orientation orientation, int x, int y, int* rotatedX, int* rotatedY) {
    switch (orientation) {
      case Portrait:
        *rotatedX = y;
        *rotatedY = EInkDisplay::DISPLAY_HEIGHT - 1 - x;
        break;
      case LandscapeClockwise:
        *rotatedX = EInkDisplay::DISPLAY_WIDTH - 1 - x;
        *rotatedY = EInkDisplay::DISPLAY_HEIGHT - 1 - y;
        break;
      case PortraitInverted:
        *rotatedX = EInkDisplay::DISPLAY_WIDTH - 1 - y;
        *rotatedY = x;
        break;
      case LandscapeCounterClockwise:
        *rotatedX = x;
        *rotatedY = y;
        break;
    }
  }
};

// Helper: check if a specific physical pixel is set (black) in the framebuffer
static bool isPixelSet(const uint8_t* fb, int physX, int physY) {
  const uint16_t byteIndex = physY * EInkDisplay::DISPLAY_WIDTH_BYTES + (physX / 8);
  const uint8_t bitPosition = 7 - (physX % 8);
  // Bit cleared = pixel set (black), bit set = pixel clear (white)
  return (fb[byteIndex] & (1 << bitPosition)) == 0;
}

// Helper: check if entire framebuffer is uniform 0xFF (all white)
static bool isFrameBufferClear(const uint8_t* fb) {
  for (uint32_t i = 0; i < EInkDisplay::BUFFER_SIZE; i++) {
    if (fb[i] != 0xFF) return false;
  }
  return true;
}

int main() {
  TestUtils::TestRunner runner("GfxRendererPixel");

  constexpr int W = EInkDisplay::DISPLAY_WIDTH;   // 800
  constexpr int H = EInkDisplay::DISPLAY_HEIGHT;   // 480

  // --- rotateCoordinates tests ---
  // We test through drawPixel which calls rotateCoordinates internally,
  // but also verify the math directly through pixel placement.

  // Test 1: Portrait (x,y) -> physical (y, H-1-x)
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::Portrait);

    int x = 10, y = 20;
    gfx.drawPixel(x, y);

    // Expected physical: (y, H-1-x) = (20, 469)
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), 20, 469), "portrait_rotation_10_20");
  }

  // Test 2: LandscapeClockwise (x,y) -> physical (W-1-x, H-1-y)
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::LandscapeClockwise);

    int x = 10, y = 20;
    gfx.drawPixel(x, y);

    // Expected physical: (W-1-x, H-1-y) = (789, 459)
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), 789, 459), "landscape_cw_rotation_10_20");
  }

  // Test 3: PortraitInverted (x,y) -> physical (W-1-y, x)
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::PortraitInverted);

    int x = 10, y = 20;
    gfx.drawPixel(x, y);

    // Expected physical: (W-1-y, x) = (779, 10)
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), 779, 10), "portrait_inv_rotation_10_20");
  }

  // Test 4: LandscapeCounterClockwise identity (x,y) -> (x,y)
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::LandscapeCounterClockwise);

    int x = 10, y = 20;
    gfx.drawPixel(x, y);

    // Expected physical: (x, y) = (10, 20)
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), 10, 20), "landscape_ccw_identity_10_20");
  }

  // Test 5: Boundary - origin (0,0) for each orientation
  {
    // Portrait: (0,0) -> (0, H-1) = (0, 479)
    EInkDisplay d1(0, 0, 0, 0, 0, 0);
    GfxRenderer g1(d1);
    g1.begin();
    g1.setOrientation(GfxRenderer::Portrait);
    g1.drawPixel(0, 0);
    runner.expectTrue(isPixelSet(g1.getFrameBuffer(), 0, H - 1), "portrait_origin");

    // LandscapeClockwise: (0,0) -> (W-1, H-1) = (799, 479)
    EInkDisplay d2(0, 0, 0, 0, 0, 0);
    GfxRenderer g2(d2);
    g2.begin();
    g2.setOrientation(GfxRenderer::LandscapeClockwise);
    g2.drawPixel(0, 0);
    runner.expectTrue(isPixelSet(g2.getFrameBuffer(), W - 1, H - 1), "landscape_cw_origin");

    // PortraitInverted: (0,0) -> (W-1, 0) = (799, 0)
    EInkDisplay d3(0, 0, 0, 0, 0, 0);
    GfxRenderer g3(d3);
    g3.begin();
    g3.setOrientation(GfxRenderer::PortraitInverted);
    g3.drawPixel(0, 0);
    runner.expectTrue(isPixelSet(g3.getFrameBuffer(), W - 1, 0), "portrait_inv_origin");

    // LandscapeCounterClockwise: (0,0) -> (0, 0)
    EInkDisplay d4(0, 0, 0, 0, 0, 0);
    GfxRenderer g4(d4);
    g4.begin();
    g4.setOrientation(GfxRenderer::LandscapeCounterClockwise);
    g4.drawPixel(0, 0);
    runner.expectTrue(isPixelSet(g4.getFrameBuffer(), 0, 0), "landscape_ccw_origin");
  }

  // Test 6: begin() caches framebuffer pointer
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);

    runner.expectTrue(gfx.getFrameBuffer() == nullptr, "framebuffer_null_before_begin");

    gfx.begin();
    runner.expectTrue(gfx.getFrameBuffer() != nullptr, "framebuffer_cached_after_begin");
    runner.expectTrue(gfx.getFrameBuffer() == display.getFrameBuffer(), "framebuffer_matches_display");
  }

  // Test 7: drawPixel in Portrait sets correct bit
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::Portrait);

    // Pixel at logical (100, 200) -> physical (200, 379)
    gfx.drawPixel(100, 200);

    int physX = 200, physY = H - 1 - 100;  // 379
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), physX, physY), "drawPixel_portrait_bit_set");

    // Adjacent pixels should still be white
    runner.expectFalse(isPixelSet(gfx.getFrameBuffer(), physX + 1, physY), "drawPixel_portrait_adjacent_clear");
  }

  // Test 8: drawPixel in LandscapeCounterClockwise - identity mapping
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::LandscapeCounterClockwise);

    gfx.drawPixel(50, 30);
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), 50, 30), "drawPixel_landscape_ccw_identity");
  }

  // Test 9: drawPixel out-of-bounds - no crash, framebuffer unchanged
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::LandscapeCounterClockwise);

    // All these should be out of bounds and not crash
    gfx.drawPixel(-1, 0);
    gfx.drawPixel(0, -1);
    gfx.drawPixel(W, 0);
    gfx.drawPixel(0, H);
    gfx.drawPixel(W + 100, H + 100);

    runner.expectTrue(isFrameBufferClear(gfx.getFrameBuffer()), "drawPixel_oob_no_corruption");
  }

  // Test 10: clearScreen + drawPixel
  {
    EInkDisplay display(0, 0, 0, 0, 0, 0);
    GfxRenderer gfx(display);
    gfx.begin();
    gfx.setOrientation(GfxRenderer::LandscapeCounterClockwise);

    // Clear to black (0x00), then set a pixel white (state=false)
    gfx.clearScreen(0x00);
    gfx.drawPixel(5, 5, false);

    // Physical (5,5) should now have its bit SET (white)
    uint16_t byteIndex = 5 * EInkDisplay::DISPLAY_WIDTH_BYTES + (5 / 8);
    uint8_t bitPos = 7 - (5 % 8);
    bool bitIsSet = (gfx.getFrameBuffer()[byteIndex] & (1 << bitPos)) != 0;
    runner.expectTrue(bitIsSet, "clearScreen_then_drawPixel_white");

    // Clear to white, then draw black pixel
    gfx.clearScreen(0xFF);
    gfx.drawPixel(5, 5, true);
    runner.expectTrue(isPixelSet(gfx.getFrameBuffer(), 5, 5), "clearScreen_then_drawPixel_black");
  }

  return runner.allPassed() ? 0 : 1;
}
