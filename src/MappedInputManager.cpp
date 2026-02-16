#include "MappedInputManager.h"

#include "core/SumiSettings.h"

decltype(InputManager::BTN_BACK) MappedInputManager::mapButton(const Button button) const {
  const auto frontLayout = settings_ ? static_cast<sumi::Settings::FrontButtonLayout>(settings_->frontButtonLayout)
                                     : sumi::Settings::FrontBCLR;
  const auto sideLayout = settings_ ? static_cast<sumi::Settings::SideButtonLayout>(settings_->sideButtonLayout)
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

bool MappedInputManager::wasPressed(const Button button) const { return inputManager.wasPressed(mapButton(button)); }

bool MappedInputManager::wasReleased(const Button button) const { return inputManager.wasReleased(mapButton(button)); }

bool MappedInputManager::isPressed(const Button button) const { return inputManager.isPressed(mapButton(button)); }

bool MappedInputManager::wasAnyPressed() const { return inputManager.wasAnyPressed(); }

bool MappedInputManager::wasAnyReleased() const { return inputManager.wasAnyReleased(); }

unsigned long MappedInputManager::getHeldTime() const { return inputManager.getHeldTime(); }
