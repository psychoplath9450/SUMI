#include "StartupState.h"

#include <Arduino.h>

#include "../core/Core.h"

namespace sumi {

void StartupState::enter(Core& core) {
  Serial.println("[STATE] StartupState::enter");
  initialized_ = false;
}

void StartupState::exit(Core& core) { Serial.println("[STATE] StartupState::exit"); }

StateTransition StartupState::update(Core& core) {
  if (!initialized_) {
    initialized_ = true;
    // First frame - just entered
    // In future: show boot animation
    return StateTransition::stay(StateId::Startup);
  }

  // Transition to FileList (or LegacyState during migration)
  // For now, stay in startup - main.cpp will handle legacy activities
  return StateTransition::stay(StateId::Startup);
}

}  // namespace sumi
