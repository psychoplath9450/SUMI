#include "test_utils.h"

#include <cstdint>
#include <cstring>

// Inline DeviceSettingsView to avoid firmware/graphics dependencies
struct DeviceSettingsView {
  static constexpr const char* const SLEEP_TIMEOUT_VALUES[] = {"5 min", "10 min", "15 min", "30 min", "Never"};
  static constexpr const char* const SLEEP_SCREEN_VALUES[] = {"Dark", "Light", "Custom", "Cover"};
  static constexpr const char* const STARTUP_VALUES[] = {"Last Document", "Home"};
  static constexpr const char* const SHORT_PWR_VALUES[] = {"Ignore", "Sleep", "Page Turn"};
  static constexpr const char* const PAGES_REFRESH_VALUES[] = {"1", "5", "10", "15", "30"};
  static constexpr const char* const TOGGLE_VALUES[] = {"OFF", "ON"};
  static constexpr const char* const FRONT_BUTTON_VALUES[] = {"B/C/L/R", "L/R/B/C"};
  static constexpr const char* const SIDE_BUTTON_VALUES[] = {"Prev/Next", "Next/Prev"};

  struct SettingDef {
    const char* label;
    const char* const* values;
    uint8_t valueCount;
  };

  static constexpr int SETTING_COUNT = 8;
  static const SettingDef DEFS[SETTING_COUNT];

  uint8_t values[SETTING_COUNT] = {0};
  int8_t selected = 0;

  void cycleValue(int delta) {
    const auto& def = DEFS[selected];
    values[selected] = static_cast<uint8_t>((values[selected] + def.valueCount + delta) % def.valueCount);
  }

  const char* getCurrentValueStr(int index) const {
    const auto& def = DEFS[index];
    if (def.valueCount == 0 || values[index] >= def.valueCount) {
      return def.valueCount > 0 ? def.values[0] : "???";
    }
    return def.values[values[index]];
  }
};

constexpr const char* const DeviceSettingsView::SLEEP_TIMEOUT_VALUES[];
constexpr const char* const DeviceSettingsView::SLEEP_SCREEN_VALUES[];
constexpr const char* const DeviceSettingsView::STARTUP_VALUES[];
constexpr const char* const DeviceSettingsView::SHORT_PWR_VALUES[];
constexpr const char* const DeviceSettingsView::PAGES_REFRESH_VALUES[];
constexpr const char* const DeviceSettingsView::TOGGLE_VALUES[];
constexpr const char* const DeviceSettingsView::FRONT_BUTTON_VALUES[];
constexpr const char* const DeviceSettingsView::SIDE_BUTTON_VALUES[];

const DeviceSettingsView::SettingDef DeviceSettingsView::DEFS[SETTING_COUNT] = {
    {"Auto Sleep Timeout", SLEEP_TIMEOUT_VALUES, 5}, {"Sleep Screen", SLEEP_SCREEN_VALUES, 4},
    {"Startup Behavior", STARTUP_VALUES, 2},         {"Short Power Button", SHORT_PWR_VALUES, 3},
    {"Pages Per Refresh", PAGES_REFRESH_VALUES, 5},  {"Sunlight Fading Fix", TOGGLE_VALUES, 2},
    {"Front Buttons", FRONT_BUTTON_VALUES, 2},       {"Side Buttons", SIDE_BUTTON_VALUES, 2},
};

int main() {
  TestUtils::TestRunner runner("SettingsViewTest");

  // SETTING_COUNT is 8
  runner.expectEq(8, DeviceSettingsView::SETTING_COUNT, "SETTING_COUNT is 8");

  // DEFS entries for Front Buttons (index 6)
  runner.expectTrue(strcmp(DeviceSettingsView::DEFS[6].label, "Front Buttons") == 0, "DEFS[6] label is Front Buttons");
  runner.expectEq(uint8_t(2), DeviceSettingsView::DEFS[6].valueCount, "DEFS[6] has 2 values");
  runner.expectTrue(strcmp(DeviceSettingsView::DEFS[6].values[0], "B/C/L/R") == 0, "DEFS[6] value 0 is B/C/L/R");
  runner.expectTrue(strcmp(DeviceSettingsView::DEFS[6].values[1], "L/R/B/C") == 0, "DEFS[6] value 1 is L/R/B/C");

  // DEFS entries for Side Buttons (index 7)
  runner.expectTrue(strcmp(DeviceSettingsView::DEFS[7].label, "Side Buttons") == 0, "DEFS[7] label is Side Buttons");
  runner.expectEq(uint8_t(2), DeviceSettingsView::DEFS[7].valueCount, "DEFS[7] has 2 values");
  runner.expectTrue(strcmp(DeviceSettingsView::DEFS[7].values[0], "Prev/Next") == 0, "DEFS[7] value 0 is Prev/Next");
  runner.expectTrue(strcmp(DeviceSettingsView::DEFS[7].values[1], "Next/Prev") == 0, "DEFS[7] value 1 is Next/Prev");

  // Value cycling for Front Buttons
  DeviceSettingsView view;
  view.selected = 6;
  runner.expectTrue(strcmp(view.getCurrentValueStr(6), "B/C/L/R") == 0, "Front Buttons default is B/C/L/R");
  view.cycleValue(1);
  runner.expectTrue(strcmp(view.getCurrentValueStr(6), "L/R/B/C") == 0, "Front Buttons cycles to L/R/B/C");
  view.cycleValue(1);
  runner.expectTrue(strcmp(view.getCurrentValueStr(6), "B/C/L/R") == 0, "Front Buttons wraps back to B/C/L/R");

  // Value cycling for Side Buttons
  view.selected = 7;
  runner.expectTrue(strcmp(view.getCurrentValueStr(7), "Prev/Next") == 0, "Side Buttons default is Prev/Next");
  view.cycleValue(1);
  runner.expectTrue(strcmp(view.getCurrentValueStr(7), "Next/Prev") == 0, "Side Buttons cycles to Next/Prev");
  view.cycleValue(-1);
  runner.expectTrue(strcmp(view.getCurrentValueStr(7), "Prev/Next") == 0, "Side Buttons cycles back with delta -1");

  // Bounds check: out-of-range value falls back to first
  view.values[6] = 5;  // Invalid
  runner.expectTrue(strcmp(view.getCurrentValueStr(6), "B/C/L/R") == 0, "Out-of-range value falls back to first");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
