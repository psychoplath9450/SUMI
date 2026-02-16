#pragma once

#include "State.h"

namespace sumi {

// StartupState handles initial boot and transitions to LegacyState
// During migration, this immediately transitions to legacy activity system
class StartupState : public State {
 public:
  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  StateId id() const override { return StateId::Startup; }

 private:
  bool initialized_ = false;
};

}  // namespace sumi
