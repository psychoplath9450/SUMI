#include "StateMachine.h"

#include <Arduino.h>

#include "Core.h"

namespace sumi {

void StateMachine::init(Core& core, StateId initialState) {
  // Exit current state if one exists (e.g., when triggering sleep from any state)
  if (current_) {
    current_->exit(core);
  }

  currentId_ = initialState;
  current_ = getState(initialState);

  if (current_) {
    Serial.printf("[SM] Initial state: %d\n", static_cast<int>(initialState));
    current_->enter(core);
  } else {
    Serial.printf("[SM] ERROR: No state registered for id %d\n", static_cast<int>(initialState));
  }
}

void StateMachine::update(Core& core) {
  if (!current_) {
    return;
  }

  StateTransition trans = current_->update(core);

  if (trans.next != currentId_) {
    transition(trans.next, core, trans.immediate);
  }

  current_->render(core);
}

void StateMachine::registerState(State* state) {
  if (!state) return;

  if (stateCount_ >= MAX_STATES) {
    Serial.println("[SM] ERROR: Too many states registered");
    return;
  }

  states_[stateCount_++] = state;
  Serial.printf("[SM] Registered state: %d\n", static_cast<int>(state->id()));
}

State* StateMachine::getState(StateId id) {
  for (size_t i = 0; i < stateCount_; ++i) {
    if (states_[i] && states_[i]->id() == id) {
      return states_[i];
    }
  }
  return nullptr;
}

void StateMachine::transition(StateId next, Core& core, bool immediate) {
  if (inTransition_) {
    Serial.printf("[SM] WARNING: Re-entrant transition to %d blocked\n", static_cast<int>(next));
    return;
  }

  State* nextState = getState(next);

  if (!nextState) {
    Serial.printf("[SM] ERROR: No state for id %d\n", static_cast<int>(next));
    return;
  }

  Serial.printf("[SM] Transition: %d -> %d%s\n", static_cast<int>(currentId_), static_cast<int>(next),
                immediate ? " (immediate)" : "");

  inTransition_ = true;

  if (current_) {
    current_->exit(core);
  }

  currentId_ = next;
  current_ = nextState;
  current_->enter(core);

  inTransition_ = false;
}

}  // namespace sumi
