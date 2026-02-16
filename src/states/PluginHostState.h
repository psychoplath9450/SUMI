#pragma once

#include <GfxRenderer.h>

#include "../config.h"

#if FEATURE_PLUGINS

#include "../plugins/PluginInterface.h"
#include "../plugins/PluginRenderer.h"
#include "State.h"

namespace sumi {

// Factory function type for creating plugin instances
using PluginFactory = PluginInterface* (*)();

class PluginHostState : public State {
 public:
  explicit PluginHostState(GfxRenderer& renderer);
  ~PluginHostState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::PluginHost; }

  // Set which plugin to launch (called before transition)
  void setPluginFactory(PluginFactory factory) { factory_ = factory; }

 private:
  GfxRenderer& renderer_;
  PluginRenderer pluginRenderer_;
  PluginInterface* plugin_ = nullptr;
  PluginFactory factory_ = nullptr;
  bool goBack_ = false;
  bool needsRender_ = true;
  int partialCount_ = 0;
  unsigned long lastUpdateMs_ = 0;
  int backPressCount_ = 0;
  unsigned long lastBackMs_ = 0;
  bool isLandscape_ = false;  // Track if plugin switched to landscape

  // Translate SUMI button events → PluginButton
  PluginButton translateButton(Button btn) const;

  // Perform display refresh based on plugin run mode
  void refreshDisplay(Core& core);
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
