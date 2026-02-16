#include "test_utils.h"

#include <cstdint>

// Minimal InputManager mock
class InputManager {
 public:
  static constexpr int BTN_BACK = 0;
  static constexpr int BTN_CONFIRM = 1;
  static constexpr int BTN_LEFT = 2;
  static constexpr int BTN_RIGHT = 3;
  static constexpr int BTN_UP = 4;
  static constexpr int BTN_DOWN = 5;
  static constexpr int BTN_POWER = 6;
};

// Inline Settings enums
namespace sumi {
struct Settings {
  enum SideButtonLayout : uint8_t { PrevNext = 0, NextPrev = 1 };
  enum FrontButtonLayout : uint8_t { FrontBCLR = 0, FrontLRBC = 1 };

  uint8_t sideButtonLayout = PrevNext;
  uint8_t frontButtonLayout = FrontBCLR;
};
}  // namespace sumi

// Inline button mapping logic from MappedInputManager
enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

int mapButton(Button button, sumi::Settings* settings) {
  const auto frontLayout = settings ? static_cast<sumi::Settings::FrontButtonLayout>(settings->frontButtonLayout)
                                    : sumi::Settings::FrontBCLR;
  const auto sideLayout = settings ? static_cast<sumi::Settings::SideButtonLayout>(settings->sideButtonLayout)
                                   : sumi::Settings::PrevNext;

  switch (button) {
    case Button::Back:
      switch (frontLayout) {
        case sumi::Settings::FrontLRBC:
          return InputManager::BTN_LEFT;
        case sumi::Settings::FrontBCLR:
        default:
          return InputManager::BTN_BACK;
      }
    case Button::Confirm:
      switch (frontLayout) {
        case sumi::Settings::FrontLRBC:
          return InputManager::BTN_RIGHT;
        case sumi::Settings::FrontBCLR:
        default:
          return InputManager::BTN_CONFIRM;
      }
    case Button::Left:
      switch (frontLayout) {
        case sumi::Settings::FrontLRBC:
          return InputManager::BTN_BACK;
        case sumi::Settings::FrontBCLR:
        default:
          return InputManager::BTN_LEFT;
      }
    case Button::Right:
      switch (frontLayout) {
        case sumi::Settings::FrontLRBC:
          return InputManager::BTN_CONFIRM;
        case sumi::Settings::FrontBCLR:
        default:
          return InputManager::BTN_RIGHT;
      }
    case Button::Up:
      switch (sideLayout) {
        case sumi::Settings::NextPrev:
          return InputManager::BTN_DOWN;
        case sumi::Settings::PrevNext:
        default:
          return InputManager::BTN_UP;
      }
    case Button::Down:
      switch (sideLayout) {
        case sumi::Settings::NextPrev:
          return InputManager::BTN_UP;
        case sumi::Settings::PrevNext:
        default:
          return InputManager::BTN_DOWN;
      }
    case Button::Power:
      return InputManager::BTN_POWER;
    case Button::PageBack:
      switch (sideLayout) {
        case sumi::Settings::NextPrev:
          return InputManager::BTN_DOWN;
        case sumi::Settings::PrevNext:
        default:
          return InputManager::BTN_UP;
      }
    case Button::PageForward:
      switch (sideLayout) {
        case sumi::Settings::NextPrev:
          return InputManager::BTN_UP;
        case sumi::Settings::PrevNext:
        default:
          return InputManager::BTN_DOWN;
      }
  }
  return InputManager::BTN_BACK;
}

int main() {
  TestUtils::TestRunner runner("MappedInputManagerTest");

  // === Front button mapping: BCLR (default) ===
  {
    sumi::Settings settings;
    settings.frontButtonLayout = sumi::Settings::FrontBCLR;

    runner.expectEq(InputManager::BTN_BACK, mapButton(Button::Back, &settings), "BCLR: Back -> BTN_BACK");
    runner.expectEq(InputManager::BTN_CONFIRM, mapButton(Button::Confirm, &settings), "BCLR: Confirm -> BTN_CONFIRM");
    runner.expectEq(InputManager::BTN_LEFT, mapButton(Button::Left, &settings), "BCLR: Left -> BTN_LEFT");
    runner.expectEq(InputManager::BTN_RIGHT, mapButton(Button::Right, &settings), "BCLR: Right -> BTN_RIGHT");
  }

  // === Front button mapping: LRBC (swapped) ===
  {
    sumi::Settings settings;
    settings.frontButtonLayout = sumi::Settings::FrontLRBC;

    runner.expectEq(InputManager::BTN_LEFT, mapButton(Button::Back, &settings), "LRBC: Back -> BTN_LEFT");
    runner.expectEq(InputManager::BTN_RIGHT, mapButton(Button::Confirm, &settings), "LRBC: Confirm -> BTN_RIGHT");
    runner.expectEq(InputManager::BTN_BACK, mapButton(Button::Left, &settings), "LRBC: Left -> BTN_BACK");
    runner.expectEq(InputManager::BTN_CONFIRM, mapButton(Button::Right, &settings), "LRBC: Right -> BTN_CONFIRM");
  }

  // === Side button mapping: PrevNext (default) ===
  {
    sumi::Settings settings;
    settings.sideButtonLayout = sumi::Settings::PrevNext;

    runner.expectEq(InputManager::BTN_UP, mapButton(Button::PageBack, &settings), "PrevNext: PageBack -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageForward, &settings),
                    "PrevNext: PageForward -> BTN_DOWN");
  }

  // === Side button mapping: NextPrev (swapped) ===
  {
    sumi::Settings settings;
    settings.sideButtonLayout = sumi::Settings::NextPrev;

    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageBack, &settings), "NextPrev: PageBack -> BTN_DOWN");
    runner.expectEq(InputManager::BTN_UP, mapButton(Button::PageForward, &settings),
                    "NextPrev: PageForward -> BTN_UP");
  }

  // === Combined: LRBC front + NextPrev side ===
  {
    sumi::Settings settings;
    settings.frontButtonLayout = sumi::Settings::FrontLRBC;
    settings.sideButtonLayout = sumi::Settings::NextPrev;

    runner.expectEq(InputManager::BTN_LEFT, mapButton(Button::Back, &settings), "Combined: Back -> BTN_LEFT");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageBack, &settings),
                    "Combined: PageBack -> BTN_DOWN");
  }

  // === Up/Down remapped by sideLayout ===
  {
    sumi::Settings settings;
    settings.sideButtonLayout = sumi::Settings::PrevNext;

    runner.expectEq(InputManager::BTN_UP, mapButton(Button::Up, &settings), "PrevNext: Up -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::Down, &settings), "PrevNext: Down -> BTN_DOWN");
  }

  {
    sumi::Settings settings;
    settings.sideButtonLayout = sumi::Settings::NextPrev;

    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::Up, &settings), "NextPrev: Up -> BTN_DOWN");
    runner.expectEq(InputManager::BTN_UP, mapButton(Button::Down, &settings), "NextPrev: Down -> BTN_UP");
  }

  // === Non-remapped buttons are unaffected ===
  {
    sumi::Settings settings;
    settings.frontButtonLayout = sumi::Settings::FrontLRBC;
    settings.sideButtonLayout = sumi::Settings::NextPrev;

    runner.expectEq(InputManager::BTN_POWER, mapButton(Button::Power, &settings), "Power always -> BTN_POWER");
  }

  // === nullptr settings defaults to BCLR/PrevNext ===
  {
    runner.expectEq(InputManager::BTN_BACK, mapButton(Button::Back, nullptr), "nullptr: Back -> BTN_BACK");
    runner.expectEq(InputManager::BTN_CONFIRM, mapButton(Button::Confirm, nullptr),
                    "nullptr: Confirm -> BTN_CONFIRM");
    runner.expectEq(InputManager::BTN_UP, mapButton(Button::Up, nullptr), "nullptr: Up -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::Down, nullptr), "nullptr: Down -> BTN_DOWN");
    runner.expectEq(InputManager::BTN_UP, mapButton(Button::PageBack, nullptr), "nullptr: PageBack -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageForward, nullptr),
                    "nullptr: PageForward -> BTN_DOWN");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
