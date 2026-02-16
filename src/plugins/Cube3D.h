#pragma once

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include <EInkDisplay.h>
#include <esp_heap_caps.h>

#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

/**
 * @file Cube3D.h
 * @brief SUMI Benchmark — E-ink Refresh Rate Testing
 *
 * Tests:
 * 1. Fast Refresh (LUT) — FAST_REFRESH alternating B/W, measures page-turn speed
 * 2. Half Refresh       — HALF_REFRESH alternating B/W, measures clean refresh speed
 * 3. Pattern Fill       — FAST_REFRESH with checkerboard/stripe patterns
 * 4. Partial Window     — Small-region partial updates
 *
 * Each test runs N frames, measures min/avg/max frame time, shows real FPS.
 * The plugin handles its own display refresh during test runs for accurate timing.
 */

class Cube3DApp : public PluginInterface {
 public:
  const char* name() const override { return "Benchmark"; }
  PluginRunMode runMode() const override { return PluginRunMode::Animation; }
  bool handlesOwnRefresh() const override { return benchRunning_; }

  explicit Cube3DApp(PluginRenderer& renderer)
      : d_(renderer), screenW_(480), screenH_(800) {}

  void init(int w, int h) override {
    screenW_ = w;
    screenH_ = h;
    state_ = STATE_MENU;
    menuCursor_ = 0;
    needsFullRedraw = true;
  }

  bool handleInput(PluginButton btn) override {
    if (benchRunning_) return true;  // Ignore input during benchmark

    switch (state_) {
      case STATE_MENU:
        return handleMenuInput(btn);
      case STATE_RESULTS:
        return handleResultsInput(btn);
      default:
        return true;
    }
  }

  bool update() override {
    if (!benchRunning_) return false;
    return runNextFrame();
  }

  void draw() override {
    if (!needsFullRedraw) return;

    d_.fillScreen(GxEPD_WHITE);
    switch (state_) {
      case STATE_MENU:
        drawMenu();
        break;
      case STATE_RUNNING:
        // During benchmark, this draws + refreshes directly (handlesOwnRefresh=true)
        drawBenchFrame();
        break;
      case STATE_RESULTS:
        drawResults();
        break;
    }
    needsFullRedraw = false;
  }

  bool isRunning() const override { return running_; }

  PluginRenderer& d_;

 private:
  enum AppState : uint8_t { STATE_MENU, STATE_RUNNING, STATE_RESULTS };

  // Test definitions
  static constexpr int TEST_COUNT = 4;

  struct TestDef {
    const char* name;
    const char* desc;
    EInkDisplay::RefreshMode mode;
    int frames;
  };

  static constexpr TestDef TESTS[TEST_COUNT] = {
      {"Fast Refresh (LUT)", "Page-turn speed", EInkDisplay::FAST_REFRESH, 20},
      {"Half Refresh", "Clean refresh speed", EInkDisplay::HALF_REFRESH, 10},
      {"Pattern Fill", "Complex LUT transitions", EInkDisplay::FAST_REFRESH, 20},
      {"Partial Window", "Small region updates", EInkDisplay::FAST_REFRESH, 20},
  };

  struct TestResult {
    uint32_t totalMs;
    uint32_t minMs;
    uint32_t maxMs;
    int frames;
    bool valid;

    float fps() const {
      return (totalMs > 0 && frames > 0) ? (float)frames * 1000.0f / (float)totalMs : 0;
    }
    uint32_t avgMs() const { return (frames > 0) ? totalMs / frames : 0; }
  };

  // State
  int screenW_, screenH_;
  bool running_ = true;
  AppState state_ = STATE_MENU;
  int menuCursor_ = 0;
  bool benchRunning_ = false;
  bool runAll_ = false;  // true when running all tests sequentially

  // Active benchmark state
  int currentTest_ = -1;
  int currentFrame_ = 0;

  // Per-test frame time tracking
  uint32_t frameTotalMs_ = 0;
  uint32_t frameMinMs_ = 0xFFFFFFFF;
  uint32_t frameMaxMs_ = 0;

  // Results
  TestResult results_[TEST_COUNT] = {};

  // ==================== MENU ====================

  bool handleMenuInput(PluginButton btn) {
    switch (btn) {
      case PluginButton::Back:
        running_ = false;
        return false;
      case PluginButton::Up:
        menuCursor_ = (menuCursor_ == 0) ? TEST_COUNT : menuCursor_ - 1;
        needsFullRedraw = true;
        return true;
      case PluginButton::Down:
        menuCursor_ = (menuCursor_ + 1) % (TEST_COUNT + 1);
        needsFullRedraw = true;
        return true;
      case PluginButton::Center:
        if (menuCursor_ < TEST_COUNT) {
          startSingleTest(menuCursor_);
        } else {
          startAllTests();
        }
        return true;
      default:
        return true;
    }
  }

  void drawMenu() {
    d_.setTextColor(GxEPD_BLACK);
    d_.setFont(nullptr);

    // Title
    centerText("SUMI Benchmark", screenW_ / 2, 32);
    d_.drawFastHLine(4, 47, screenW_ - 8, GxEPD_BLACK);
    d_.drawFastHLine(4, 48, screenW_ - 8, GxEPD_BLACK);
    centerText("E-ink Refresh Rate Testing", screenW_ / 2, 70);

    // Test cards
    int cardW = screenW_ - 60, cardH = 52, startY = 100;
    for (int i = 0; i < TEST_COUNT; i++) {
      int y = startY + i * (cardH + 8);
      bool sel = (menuCursor_ == i);
      if (sel) {
        d_.fillRect(30, y, cardW, cardH, GxEPD_BLACK);
        d_.setTextColor(GxEPD_WHITE);
      } else {
        d_.drawRect(30, y, cardW, cardH, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
      }
      d_.setCursor(46, y + 22);
      d_.print(TESTS[i].name);
      d_.setCursor(46, y + 40);
      d_.print(TESTS[i].desc);

      // Show previous result if available
      if (results_[i].valid) {
        char fps[16];
        snprintf(fps, sizeof(fps), "%.1f FPS", (double)results_[i].fps());
        int16_t tx, ty;
        uint16_t tw, th;
        d_.getTextBounds(fps, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(30 + cardW - tw - 12, y + 22);
        d_.print(fps);
      }
    }

    // "Run All" option
    int allY = startY + TEST_COUNT * (cardH + 8) + 8;
    bool selAll = (menuCursor_ == TEST_COUNT);
    if (selAll) {
      d_.fillRect(30, allY, cardW, 40, GxEPD_BLACK);
      d_.setTextColor(GxEPD_WHITE);
    } else {
      d_.drawRect(30, allY, cardW, 40, GxEPD_BLACK);
      d_.setTextColor(GxEPD_BLACK);
    }
    d_.setCursor(46, allY + 26);
    d_.print("Run All Tests");

    // System info
    int infoY = allY + 64;
    d_.setTextColor(GxEPD_BLACK);
    char buf[64];
    snprintf(buf, sizeof(buf), "Free Heap: %lu B", (unsigned long)ESP.getFreeHeap());
    d_.setCursor(30, infoY);
    d_.print(buf);
    snprintf(buf, sizeof(buf), "Largest Block: %lu B",
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    d_.setCursor(30, infoY + 18);
    d_.print(buf);
    snprintf(buf, sizeof(buf), "Display: %dx%d  CPU: %dMHz", screenW_, screenH_, ESP.getCpuFreqMHz());
    d_.setCursor(30, infoY + 36);
    d_.print(buf);

    centerText("UP/DN: Select | OK: Run | BACK: Exit", screenW_ / 2, screenH_ - 20);
  }

  // ==================== BENCHMARK EXECUTION ====================

  void startSingleTest(int testIdx) {
    currentTest_ = testIdx;
    currentFrame_ = 0;
    frameTotalMs_ = 0;
    frameMinMs_ = 0xFFFFFFFF;
    frameMaxMs_ = 0;
    benchRunning_ = true;
    runAll_ = false;
    state_ = STATE_RUNNING;
    needsFullRedraw = true;
    Serial.printf("[BENCH] Starting test %d: %s\n", testIdx, TESTS[testIdx].name);
  }

  void startAllTests() {
    for (int i = 0; i < TEST_COUNT; i++) results_[i].valid = false;
    currentTest_ = 0;
    currentFrame_ = 0;
    frameTotalMs_ = 0;
    frameMinMs_ = 0xFFFFFFFF;
    frameMaxMs_ = 0;
    benchRunning_ = true;
    runAll_ = true;
    state_ = STATE_RUNNING;
    needsFullRedraw = true;
    Serial.printf("[BENCH] Starting all tests\n");
  }

  void storeCurrentResult() {
    results_[currentTest_].totalMs = frameTotalMs_;
    results_[currentTest_].minMs = (frameMinMs_ == 0xFFFFFFFF) ? 0 : frameMinMs_;
    results_[currentTest_].maxMs = frameMaxMs_;
    results_[currentTest_].frames = TESTS[currentTest_].frames;
    results_[currentTest_].valid = true;

    Serial.printf("[BENCH] Test %d done: %lu ms total, %.1f FPS\n", currentTest_,
                  (unsigned long)frameTotalMs_, (double)results_[currentTest_].fps());
  }

  bool runNextFrame() {
    if (currentTest_ < 0 || currentTest_ >= TEST_COUNT) {
      finishBenchmark();
      return false;
    }

    if (currentFrame_ >= TESTS[currentTest_].frames) {
      storeCurrentResult();

      // Advance to next test if running all
      if (runAll_ && currentTest_ < TEST_COUNT - 1) {
        currentTest_++;
        currentFrame_ = 0;
        frameTotalMs_ = 0;
        frameMinMs_ = 0xFFFFFFFF;
        frameMaxMs_ = 0;
        needsFullRedraw = true;
        return true;
      } else {
        finishBenchmark();
        return false;
      }
    }

    needsFullRedraw = true;
    return true;
  }

  void finishBenchmark() {
    benchRunning_ = false;
    state_ = STATE_RESULTS;
    needsFullRedraw = true;
    Serial.println("[BENCH] All tests complete");
  }

  void drawBenchFrame() {
    // This renders AND refreshes the display directly for accurate timing.
    // handlesOwnRefresh() returns true, so the host skips its own refresh.

    const TestDef& test = TESTS[currentTest_];
    GfxRenderer& gfx = d_.gfx();

    // Render the frame content into the framebuffer
    renderTestPattern(currentTest_, currentFrame_);

    // Show progress indicator on a white strip
    d_.fillRect(0, 0, screenW_, 20, GxEPD_WHITE);
    d_.setTextColor(GxEPD_BLACK);
    d_.setFont(nullptr);
    char progress[48];
    snprintf(progress, sizeof(progress), "%s %d/%d", test.name, currentFrame_ + 1, test.frames);
    d_.setCursor(8, 16);
    d_.print(progress);

    // Time the actual display refresh
    uint32_t refreshStart = millis();

    if (currentTest_ == 3) {
      // Partial window test — only refresh a portion of the screen
      gfx.displayWindow(40, 200, screenW_ - 80, 400);
    } else {
      gfx.displayBuffer(test.mode);
    }

    uint32_t frameTime = millis() - refreshStart;

    // Track stats
    frameTotalMs_ += frameTime;
    if (frameTime < frameMinMs_) frameMinMs_ = frameTime;
    if (frameTime > frameMaxMs_) frameMaxMs_ = frameTime;

    currentFrame_++;

    Serial.printf("[BENCH] Test %d frame %d: %lu ms\n", currentTest_, currentFrame_,
                  (unsigned long)frameTime);
  }

  void renderTestPattern(int testIdx, int frame) {
    GfxRenderer& gfx = d_.gfx();

    switch (testIdx) {
      case 0:  // Fast Refresh — alternating B/W
      case 1:  // Half Refresh — alternating B/W
        if (frame % 2 == 0) {
          gfx.clearScreen(0x00);  // Black
        } else {
          gfx.clearScreen(0xFF);  // White
        }
        break;

      case 2:  // Pattern Fill — varied patterns
        gfx.clearScreen(0xFF);
        switch (frame % 4) {
          case 0:  // Checkerboard 16px
            for (int y = 0; y < screenH_; y += 16)
              for (int x = 0; x < screenW_; x += 16)
                if (((x / 16) + (y / 16)) % 2 == 0) gfx.fillRect(x, y, 16, 16, true);
            break;
          case 1:  // Horizontal stripes
            for (int y = 0; y < screenH_; y += 8)
              gfx.drawLine(0, y, screenW_ - 1, y, true);
            break;
          case 2:  // Vertical stripes
            for (int x = 0; x < screenW_; x += 8)
              gfx.drawLine(x, 0, x, screenH_ - 1, true);
            break;
          case 3:  // Inverted checkerboard
            gfx.clearScreen(0x00);
            for (int y = 0; y < screenH_; y += 16)
              for (int x = 0; x < screenW_; x += 16)
                if (((x / 16) + (y / 16)) % 2 == 0) gfx.fillRect(x, y, 16, 16, false);
            break;
        }
        break;

      case 3:  // Partial Window — content in center region
        gfx.clearScreen(0xFF);
        {
          int bw = screenW_ - 80, bh = 400;
          int bx = 40, by = 200;
          // Draw border
          gfx.drawRect(bx, by, bw, bh, true);
          // Alternating content inside
          if (frame % 2 == 0) {
            // Text-like lines
            for (int y = by + 20; y < by + bh - 20; y += 16)
              gfx.drawLine(bx + 20, y, bx + bw - 20, y, true);
          } else {
            // Blocks
            for (int y = by + 20; y < by + bh - 40; y += 60)
              for (int x = bx + 20; x < bx + bw - 40; x += 80)
                gfx.fillRect(x, y, 60, 40, true);
          }
        }
        break;
    }
  }

  // ==================== RESULTS ====================

  bool handleResultsInput(PluginButton btn) {
    switch (btn) {
      case PluginButton::Back:
      case PluginButton::Center:
        state_ = STATE_MENU;
        needsFullRedraw = true;
        return true;
      default:
        return true;
    }
  }

  void drawResults() {
    d_.setTextColor(GxEPD_BLACK);
    d_.setFont(nullptr);

    centerText("Benchmark Results", screenW_ / 2, 32);
    d_.drawFastHLine(4, 47, screenW_ - 8, GxEPD_BLACK);
    d_.drawFastHLine(4, 48, screenW_ - 8, GxEPD_BLACK);

    int y = 80;
    char buf[80];

    for (int i = 0; i < TEST_COUNT; i++) {
      if (!results_[i].valid) continue;

      const TestResult& r = results_[i];

      // Test name header (inverted bar)
      d_.fillRect(20, y - 4, screenW_ - 40, 20, GxEPD_BLACK);
      d_.setTextColor(GxEPD_WHITE);
      d_.setCursor(28, y + 12);
      d_.print(TESTS[i].name);

      // FPS right-aligned in header
      snprintf(buf, sizeof(buf), "%.1f FPS", (double)r.fps());
      int16_t tx, ty;
      uint16_t tw, th;
      d_.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
      d_.setCursor(screenW_ - 28 - tw, y + 12);
      d_.print(buf);

      y += 26;
      d_.setTextColor(GxEPD_BLACK);

      // Details
      snprintf(buf, sizeof(buf), "%d frames in %lu ms  (avg %lu ms)", r.frames,
               (unsigned long)r.totalMs, (unsigned long)r.avgMs());
      d_.setCursor(28, y + 12);
      d_.print(buf);
      y += 18;

      snprintf(buf, sizeof(buf), "Min: %lu ms  Max: %lu ms", (unsigned long)r.minMs,
               (unsigned long)r.maxMs);
      d_.setCursor(28, y + 12);
      d_.print(buf);
      y += 24;

      // Separator
      d_.drawFastHLine(30, y, screenW_ - 60, GxEPD_BLACK);
      y += 12;
    }

    // System info at bottom
    y += 8;
    snprintf(buf, sizeof(buf), "Free Heap: %lu B  CPU: %dMHz", (unsigned long)ESP.getFreeHeap(),
             ESP.getCpuFreqMHz());
    d_.setCursor(28, y + 12);
    d_.print(buf);
    y += 18;
    snprintf(buf, sizeof(buf), "Display: %dx%d", screenW_, screenH_);
    d_.setCursor(28, y + 12);
    d_.print(buf);

    centerText("OK/BACK: Return to Menu", screenW_ / 2, screenH_ - 20);
  }

  // ==================== HELPERS ====================

  void centerText(const char* t, int x, int y) {
    int16_t tx, ty;
    uint16_t tw, th;
    d_.getTextBounds(t, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(x - tw / 2, y);
    d_.print(t);
  }
};

// Static constexpr definitions (required for ODR-use in C++14)
constexpr Cube3DApp::TestDef Cube3DApp::TESTS[];

}  // namespace sumi

#endif
