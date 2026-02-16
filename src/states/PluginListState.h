#pragma once

#include <GfxRenderer.h>

#include "../config.h"

#if FEATURE_PLUGINS

#include "../plugins/PluginInterface.h"
#include "PluginHostState.h"
#include "State.h"

namespace sumi {

// Registry entry for a plugin
struct PluginEntry {
  const char* name;
  const char* category;  // "Games", "Productivity", "Tools"
  PluginFactory factory;
  const char* savePath;  // Path to save file (nullptr if no save support)
};

class PluginListState : public State {
 public:
  explicit PluginListState(GfxRenderer& renderer);
  ~PluginListState() override = default;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::PluginList; }

  // Plugin registry — populated at startup
  static constexpr int MAX_PLUGINS = 24;
  static int pluginCount;
  static PluginEntry plugins[MAX_PLUGINS];

  // Register a plugin (called from main.cpp)
  static bool registerPlugin(const char* name, const char* category, PluginFactory factory, const char* savePath = nullptr);

  // Get a reference to the host state for launching
  void setHostState(PluginHostState* host) { hostState_ = host; }

 private:
  GfxRenderer& renderer_;
  PluginHostState* hostState_ = nullptr;

  int8_t selected_ = 0;
  int8_t scrollOffset_ = 0;
  bool needsRender_ = true;
  bool goHome_ = false;
  bool launchPlugin_ = false;

  // How many items fit on screen
  int visibleCount() const;

  void drawList() const;
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
