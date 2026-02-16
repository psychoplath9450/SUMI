#include "test_utils.h"

#include <cstdint>

// Extract the power button held-time tracking logic from main.cpp loop()
// into a testable struct. The real code uses static locals; this replicates
// the same state machine.
struct PowerHeldTracker {
  unsigned long heldSinceMs = 0;
  unsigned long prevCheckMs = 0;

  enum Result { None, Sleep };

  Result update(bool pressed, unsigned long now, uint16_t requiredDuration) {
    const unsigned long loopGap = now - prevCheckMs;
    prevCheckMs = now;

    if (pressed) {
      if (heldSinceMs == 0 || loopGap > 100) {
        heldSinceMs = now;
      }
      if (now - heldSinceMs > requiredDuration) {
        return Sleep;
      }
    } else {
      heldSinceMs = 0;
    }
    return None;
  }
};

int main() {
  TestUtils::TestRunner runner("PowerButtonHeldTimeTest");

  const uint16_t duration = 400;

  // === Basic: not pressed does nothing ===
  {
    PowerHeldTracker tracker;
    auto r = tracker.update(false, 100, duration);
    runner.expectTrue(r == PowerHeldTracker::None, "not pressed -> None");
    runner.expectEq(0UL, tracker.heldSinceMs, "not pressed -> heldSince stays 0");
  }

  // === Held continuously triggers sleep after duration ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 100;

    // First press at t=110
    auto r = tracker.update(true, 110, duration);
    runner.expectTrue(r == PowerHeldTracker::None, "held 0ms -> None");
    runner.expectEq(110UL, tracker.heldSinceMs, "first press sets heldSince");

    // Still held at t=130 (gap=20ms, within 100ms threshold)
    r = tracker.update(true, 130, duration);
    runner.expectTrue(r == PowerHeldTracker::None, "held 20ms -> None");
    runner.expectEq(110UL, tracker.heldSinceMs, "heldSince unchanged during continuous hold");

    // Still held at t=510 (gap=380ms - but that's > 100, so timer resets!)
    // This is the key behavior: large gaps reset the timer
    r = tracker.update(true, 510, duration);
    runner.expectTrue(r == PowerHeldTracker::None, "held after gap resets -> None");
    runner.expectEq(510UL, tracker.heldSinceMs, "gap > 100ms resets heldSince");

    // Continue holding with normal gaps: t=520, 530, ..., 920
    for (unsigned long t = 520; t <= 900; t += 10) {
      r = tracker.update(true, t, duration);
    }
    runner.expectTrue(r == PowerHeldTracker::None, "held 390ms -> None (not yet over 400)");

    // t=920: held for 410ms since reset at 510
    r = tracker.update(true, 920, duration);
    runner.expectTrue(r == PowerHeldTracker::Sleep, "held 410ms since reset -> Sleep");
  }

  // === Continuous hold without gaps triggers sleep ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 0;

    PowerHeldTracker::Result r = PowerHeldTracker::None;
    for (unsigned long t = 10; t <= 400; t += 10) {
      r = tracker.update(true, t, duration);
    }
    runner.expectTrue(r == PowerHeldTracker::None, "held exactly 390ms -> None");

    r = tracker.update(true, 420, duration);
    runner.expectTrue(r == PowerHeldTracker::Sleep, "held 410ms continuous -> Sleep");
  }

  // === Release resets timer ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 0;

    tracker.update(true, 10, duration);
    runner.expectEq(10UL, tracker.heldSinceMs, "press starts timer");

    tracker.update(true, 20, duration);
    runner.expectEq(10UL, tracker.heldSinceMs, "still held, timer unchanged");

    tracker.update(false, 30, duration);
    runner.expectEq(0UL, tracker.heldSinceMs, "release resets timer");

    // Re-press starts fresh
    tracker.update(true, 40, duration);
    runner.expectEq(40UL, tracker.heldSinceMs, "re-press starts new timer");
  }

  // === Large loop gap resets held timer (prevents false sleep during rendering) ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 100;

    // Start holding
    tracker.update(true, 110, duration);
    runner.expectEq(110UL, tracker.heldSinceMs, "start hold at 110");

    // Normal gap (10ms)
    tracker.update(true, 120, duration);
    runner.expectEq(110UL, tracker.heldSinceMs, "normal gap keeps timer");

    // Large gap (200ms) - simulates slow render
    tracker.update(true, 320, duration);
    runner.expectEq(320UL, tracker.heldSinceMs, "200ms gap resets timer");

    // Normal gaps after reset
    tracker.update(true, 330, duration);
    runner.expectEq(320UL, tracker.heldSinceMs, "normal gap after reset keeps timer");
  }

  // === Gap exactly at threshold (100ms) does NOT reset ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 100;

    tracker.update(true, 110, duration);
    runner.expectEq(110UL, tracker.heldSinceMs, "start hold");

    // Gap of exactly 100ms (not > 100, so no reset)
    tracker.update(true, 210, duration);
    runner.expectEq(110UL, tracker.heldSinceMs, "gap == 100ms does not reset");
  }

  // === Gap of 101ms resets ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 100;

    tracker.update(true, 110, duration);
    runner.expectEq(110UL, tracker.heldSinceMs, "start hold");

    // Gap of 101ms (> 100, resets)
    tracker.update(true, 211, duration);
    runner.expectEq(211UL, tracker.heldSinceMs, "gap == 101ms resets timer");
  }

  // === Short duration (PowerSleep mode, 10ms) triggers faster ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 0;

    auto r = tracker.update(true, 10, 10);
    runner.expectTrue(r == PowerHeldTracker::None, "short duration: held 0ms -> None");

    r = tracker.update(true, 21, 10);
    runner.expectTrue(r == PowerHeldTracker::Sleep, "short duration: held 11ms -> Sleep");
  }

  // === Press-release-press does not accumulate time ===
  {
    PowerHeldTracker tracker;
    tracker.prevCheckMs = 0;

    // Hold for 300ms
    for (unsigned long t = 10; t <= 300; t += 10) {
      tracker.update(true, t, duration);
    }
    runner.expectEq(10UL, tracker.heldSinceMs, "first hold period starts at 10");

    // Release
    tracker.update(false, 310, duration);
    runner.expectEq(0UL, tracker.heldSinceMs, "release clears timer");

    // Hold again for 200ms - should NOT trigger (only 200ms, not 300+200)
    PowerHeldTracker::Result r = PowerHeldTracker::None;
    for (unsigned long t = 320; t <= 520; t += 10) {
      r = tracker.update(true, t, duration);
    }
    runner.expectTrue(r == PowerHeldTracker::None, "second hold 200ms -> None (no accumulation)");

    // Continue to 730 to exceed duration from re-press at 320
    r = tracker.update(true, 730, duration);
    // Gap from 520 to 730 is 210ms > 100, so timer resets to 730
    runner.expectEq(730UL, tracker.heldSinceMs, "gap resets even during second hold");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
