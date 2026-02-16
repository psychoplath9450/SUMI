#pragma once

#include "../core/Result.h"
#include "State.h"

class GfxRenderer;

namespace sumi {

// ErrorState displays an error message and waits for user input
class ErrorState : public State {
 public:
  explicit ErrorState(GfxRenderer& renderer);

  void setError(Error err, const char* message = nullptr);

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Error; }

 private:
  GfxRenderer& renderer_;
  Error error_ = Error::None;
  char message_[128] = {};
  bool needsRender_ = true;
};

}  // namespace sumi
