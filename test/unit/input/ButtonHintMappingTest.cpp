#include "test_utils.h"

#include <cstdint>
#include <cstring>

// Inline the button hint remapping logic from ui::buttonBar()
// to test it independently without GfxRenderer/Theme dependencies.

static uint8_t frontButtonLayout_ = 0;

void setFrontButtonLayout(uint8_t layout) { frontButtonLayout_ = layout; }

struct RemappedLabels {
  const char* b1;
  const char* b2;
  const char* b3;
  const char* b4;
};

RemappedLabels remapButtonBar(const char* b1, const char* b2, const char* b3, const char* b4) {
  if (frontButtonLayout_ == 1) {
    return {b3, b4, b1, b2};
  }
  return {b1, b2, b3, b4};
}

int main() {
  TestUtils::TestRunner runner("ButtonHintMappingTest");

  // === BCLR (default) preserves order ===
  {
    setFrontButtonLayout(0);
    auto r = remapButtonBar("Back", "OK", "Prev", "Next");
    runner.expectTrue(strcmp(r.b1, "Back") == 0, "BCLR: b1 = Back");
    runner.expectTrue(strcmp(r.b2, "OK") == 0, "BCLR: b2 = OK");
    runner.expectTrue(strcmp(r.b3, "Prev") == 0, "BCLR: b3 = Prev");
    runner.expectTrue(strcmp(r.b4, "Next") == 0, "BCLR: b4 = Next");
  }

  // === LRBC remaps {back, confirm, left, right} -> {left, right, back, confirm} ===
  {
    setFrontButtonLayout(1);
    auto r = remapButtonBar("Back", "OK", "Prev", "Next");
    runner.expectTrue(strcmp(r.b1, "Prev") == 0, "LRBC: b1 = Prev");
    runner.expectTrue(strcmp(r.b2, "Next") == 0, "LRBC: b2 = Next");
    runner.expectTrue(strcmp(r.b3, "Back") == 0, "LRBC: b3 = Back");
    runner.expectTrue(strcmp(r.b4, "OK") == 0, "LRBC: b4 = OK");
  }

  // === Empty labels handled correctly ===
  {
    setFrontButtonLayout(1);
    auto r = remapButtonBar("Back", "Select", "", "");
    runner.expectTrue(strcmp(r.b1, "") == 0, "LRBC empty: b1 = empty");
    runner.expectTrue(strcmp(r.b2, "") == 0, "LRBC empty: b2 = empty");
    runner.expectTrue(strcmp(r.b3, "Back") == 0, "LRBC empty: b3 = Back");
    runner.expectTrue(strcmp(r.b4, "Select") == 0, "LRBC empty: b4 = Select");
  }

  // === Switching layout back to BCLR ===
  {
    setFrontButtonLayout(1);
    auto r1 = remapButtonBar("A", "B", "C", "D");
    runner.expectTrue(strcmp(r1.b1, "C") == 0, "Switch: LRBC b1 = C");

    setFrontButtonLayout(0);
    auto r2 = remapButtonBar("A", "B", "C", "D");
    runner.expectTrue(strcmp(r2.b1, "A") == 0, "Switch: BCLR b1 = A");
  }

  // === Unknown layout value defaults to BCLR behavior ===
  {
    setFrontButtonLayout(99);
    auto r = remapButtonBar("Back", "OK", "Prev", "Next");
    runner.expectTrue(strcmp(r.b1, "Back") == 0, "Unknown layout: b1 = Back");
    runner.expectTrue(strcmp(r.b2, "OK") == 0, "Unknown layout: b2 = OK");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
