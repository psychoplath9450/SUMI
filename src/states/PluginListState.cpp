#include "PluginListState.h"

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <cstring>

#include "../core/Core.h"
#include "../ui/Elements.h"
#include "ThemeManager.h"

namespace sumi {

int PluginListState::pluginCount = 0;
PluginEntry PluginListState::plugins[PluginListState::MAX_PLUGINS] = {};

bool PluginListState::registerPlugin(const char* name, const char* category, PluginFactory factory, const char* savePath) {
  if (pluginCount >= MAX_PLUGINS) return false;
  plugins[pluginCount] = {name, category, factory, savePath};
  pluginCount++;
  Serial.printf("[PLUGINS] Registered: %s (%s)%s\n", name, category, savePath ? " [saveable]" : "");
  return true;
}

PluginListState::PluginListState(GfxRenderer& renderer) : renderer_(renderer) {}

void PluginListState::enter(Core& core) {
  needsRender_ = true;
  goHome_ = false;
  launchPlugin_ = false;
  if (selected_ >= pluginCount && pluginCount > 0) selected_ = pluginCount - 1;
  if (selected_ < 0) selected_ = 0;
}

void PluginListState::exit(Core& core) {}

int PluginListState::visibleCount() const {
  const Theme& t = THEME;
  const int startY = 60;
  const int footerH = 40;
  int available = renderer_.getScreenHeight() - startY - footerH;
  int itemTotal = t.menuItemHeight + t.itemSpacing;
  return (itemTotal > 0) ? available / itemTotal : 10;
}

StateTransition PluginListState::update(Core& core) {
  Event e;
  while (core.events.pop(e)) {
    if (e.type != EventType::ButtonPress) continue;

    switch (e.button) {
      case Button::Up:
        if (pluginCount > 0) {
          selected_ = (selected_ == 0) ? pluginCount - 1 : selected_ - 1;
          if (selected_ < scrollOffset_) scrollOffset_ = selected_;
          // Wrap: if jumped to bottom, adjust scroll
          int vis2 = visibleCount();
          if (selected_ >= scrollOffset_ + vis2) scrollOffset_ = selected_ - vis2 + 1;
          needsRender_ = true;
        }
        break;

      case Button::Down:
        if (pluginCount > 0) {
          selected_ = (selected_ + 1) % pluginCount;
          int vis = visibleCount();
          if (selected_ >= scrollOffset_ + vis) scrollOffset_ = selected_ - vis + 1;
          // Wrap: if jumped to top, reset scroll
          if (selected_ == 0) scrollOffset_ = 0;
          needsRender_ = true;
        }
        break;

      case Button::Left:
      case Button::Back:
        goHome_ = true;
        break;

      case Button::Center:
      case Button::Right:
        if (pluginCount > 0 && selected_ < pluginCount) launchPlugin_ = true;
        break;

      case Button::Power:
        return StateTransition::to(StateId::Sleep);

      default:
        break;
    }
  }

  if (goHome_) {
    goHome_ = false;
    return StateTransition::to(StateId::Settings);
  }

  if (launchPlugin_ && hostState_) {
    launchPlugin_ = false;
    hostState_->setPluginFactory(plugins[selected_].factory);
    return StateTransition::to(StateId::PluginHost);
  }

  return StateTransition::stay(StateId::PluginList);
}

void PluginListState::render(Core& core) {
  if (!needsRender_) return;
  drawList();
  renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);
  needsRender_ = false;
  core.display.markDirty();
}

void PluginListState::drawList() const {
  const Theme& t = THEME;

  renderer_.clearScreen(t.backgroundColor);

  // Header - same as Settings
  ui::title(renderer_, t, t.screenMarginTop, "Apps");

  // Items - using standard menuItem (matches Settings menu exactly)
  const int startY = 60;
  int vis = visibleCount();

  for (int i = scrollOffset_; i < pluginCount && i < scrollOffset_ + vis; i++) {
    const int y = startY + (i - scrollOffset_) * (t.menuItemHeight + t.itemSpacing);
    ui::menuItem(renderer_, t, y, plugins[i].name, i == selected_);

    // Show "Continue" right-aligned for games with saved progress
    if (plugins[i].savePath && SdMan.exists(plugins[i].savePath)) {
      const int h = t.menuItemHeight;
      const int textY = y + (h - renderer_.getLineHeight(t.smallFontId)) / 2;
      const int rightEdge = renderer_.getScreenWidth() - t.screenMarginSide - t.itemPaddingX;
      const int tw = renderer_.getTextWidth(t.smallFontId, "Continue");
      bool black = (i == selected_) ? t.selectionTextBlack : t.secondaryTextBlack;
      renderer_.drawText(t.smallFontId, rightEdge - tw, textY, "Continue", black);
    }
  }

  // Scroll indicators
  const int W = renderer_.getScreenWidth();
  const int mx = t.screenMarginSide + 10;
  if (scrollOffset_ > 0) {
    int cx = W / 2;
    renderer_.drawLine(cx, startY - 6, cx - 6, startY - 1, true);
    renderer_.drawLine(cx, startY - 6, cx + 6, startY - 1, true);
  }
  if (scrollOffset_ + vis < pluginCount) {
    int cx = W / 2;
    int ay = renderer_.getScreenHeight() - 38;
    renderer_.drawLine(cx, ay, cx - 6, ay - 6, true);
    renderer_.drawLine(cx, ay, cx + 6, ay - 6, true);
  }
}

}  // namespace sumi

#endif
