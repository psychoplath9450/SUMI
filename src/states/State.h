#pragma once

#include "../core/Types.h"

namespace sumi {

struct Core;

struct StateTransition {
  StateId next;
  bool immediate;  // Skip enter animation if true

  static StateTransition stay(StateId current) { return {current, false}; }

  static StateTransition to(StateId next, bool immediate = false) { return {next, immediate}; }
};

class State {
 public:
  virtual ~State() = default;

  virtual void enter(Core& core) {}
  virtual void exit(Core& core) {}
  virtual StateTransition update(Core& core) = 0;
  virtual void render(Core& core) {}

  virtual StateId id() const = 0;
};

}  // namespace sumi
