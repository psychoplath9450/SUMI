#pragma once

#include <cstdint>
#include <string>

#include "../ui/views/HomeView.h"
#include "State.h"

class GfxRenderer;

namespace sumi {

class HomeState : public State {
 public:
  explicit HomeState(GfxRenderer& renderer);
  ~HomeState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Home; }

 private:
  GfxRenderer& renderer_;
  Core* core_ = nullptr;  // Stored for theme loading
  ui::HomeView view_;

  // Cover image state
  std::string coverBmpPath_;
  bool hasCoverImage_ = false;
  bool coverLoadFailed_ = false;
  uint32_t currentBookHash_ = 0;

  void loadLastBook(Core& core);
  void loadRecentBooks(Core& core);
  void openSelectedBook(Core& core);
  void updateSelectedBook(Core& core);
  void updateBattery();
  void drawBackground(Core& core);
  void drawBackgroundFromSD(const char* themeName);
  void renderCoverToCard();
};

}  // namespace sumi
