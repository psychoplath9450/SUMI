#include "PluginListState.h"

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <cstring>

#include "../core/Core.h"
#include "../plugins/LuaPlugin.h"
#include "../ui/Elements.h"
#include "ThemeManager.h"

namespace sumi {

int PluginListState::pluginCount = 0;
PluginEntry PluginListState::plugins[PluginListState::MAX_PLUGINS] = {};

// Lua plugin static storage
char PluginListState::luaPaths_[MAX_LUA_PLUGINS][64] = {};
char PluginListState::luaNames_[MAX_LUA_PLUGINS][24] = {};
int PluginListState::luaPluginCount_ = 0;
PluginRenderer* PluginListState::luaRenderer_ = nullptr;

bool PluginListState::registerPlugin(const char* name, const char* category, PluginFactory factory, const char* savePath) {
  if (pluginCount >= MAX_PLUGINS) return false;
  plugins[pluginCount] = {name, category, factory, savePath};
  pluginCount++;
  Serial.printf("[PLUGINS] Registered: %s (%s)%s\n", name, category, savePath ? " [saveable]" : "");
  return true;
}

// Factory functions for each Lua plugin slot (up to 8)
// These are plain function pointers — no captures — using static index storage
static sumi::PluginInterface* luaFactory0() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[0]); }
static sumi::PluginInterface* luaFactory1() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[1]); }
static sumi::PluginInterface* luaFactory2() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[2]); }
static sumi::PluginInterface* luaFactory3() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[3]); }
static sumi::PluginInterface* luaFactory4() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[4]); }
static sumi::PluginInterface* luaFactory5() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[5]); }
static sumi::PluginInterface* luaFactory6() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[6]); }
static sumi::PluginInterface* luaFactory7() { return new sumi::LuaPlugin(*sumi::PluginListState::luaRenderer_, sumi::PluginListState::luaPaths_[7]); }

static sumi::PluginFactory luaFactories[sumi::PluginListState::MAX_LUA_PLUGINS] = {
  luaFactory0, luaFactory1, luaFactory2, luaFactory3,
  luaFactory4, luaFactory5, luaFactory6, luaFactory7
};

void PluginListState::scanLuaPlugins(PluginRenderer& renderer) {
  luaRenderer_ = &renderer;
  luaPluginCount_ = 0;

  FsFile dir;
  if (!SdMan.exists(PLUGINS_CUSTOM_DIR) || !dir.open(PLUGINS_CUSTOM_DIR, O_RDONLY)) {
    Serial.println("[LUA] No /custom directory found, skipping Lua scan");
    return;
  }

  Serial.println("[LUA] Scanning /custom/ for .lua plugins...");

  FsFile entry;
  char fname[64];
  while (luaPluginCount_ < MAX_LUA_PLUGINS && entry.openNext(&dir, O_RDONLY)) {
    if (entry.isDirectory()) { entry.close(); continue; }

    entry.getName(fname, sizeof(fname));
    size_t len = strlen(fname);

    // Check for .lua extension
    if (len < 5 || strcasecmp(fname + len - 4, ".lua") != 0) {
      entry.close();
      continue;
    }

    // Build full path
    snprintf(luaPaths_[luaPluginCount_], sizeof(luaPaths_[0]),
             "%s/%s", PLUGINS_CUSTOM_DIR, fname);

    // Derive display name from filename
    const char* base = fname;
    size_t nameLen = len - 4;  // strip .lua
    if (nameLen >= sizeof(luaNames_[0])) nameLen = sizeof(luaNames_[0]) - 1;
    memcpy(luaNames_[luaPluginCount_], base, nameLen);
    luaNames_[luaPluginCount_][nameLen] = '\0';

    // Replace underscores with spaces, capitalize first char
    for (size_t i = 0; i < nameLen; i++) {
      if (luaNames_[luaPluginCount_][i] == '_')
        luaNames_[luaPluginCount_][i] = ' ';
    }
    char& first = luaNames_[luaPluginCount_][0];
    if (first >= 'a' && first <= 'z') first -= 32;

    Serial.printf("[LUA] Found: %s -> \"%s\"\n",
                  luaPaths_[luaPluginCount_], luaNames_[luaPluginCount_]);

    registerPlugin(luaNames_[luaPluginCount_], "Custom",
                   luaFactories[luaPluginCount_]);

    luaPluginCount_++;
    entry.close();
  }

  dir.close();
  Serial.printf("[LUA] Registered %d Lua plugin(s)\n", luaPluginCount_);
}

PluginListState::PluginListState(GfxRenderer& renderer) : renderer_(renderer) {
  visiblePluginCount_ = 0;
}

void PluginListState::buildVisibleList(const Settings& settings) {
  visiblePluginCount_ = 0;
  for (int i = 0; i < pluginCount && visiblePluginCount_ < MAX_PLUGINS; i++) {
    if (!settings.isPluginHidden(i)) {
      visiblePlugins_[visiblePluginCount_++] = static_cast<int8_t>(i);
    }
  }
}

void PluginListState::enter(Core& core) {
  needsRender_ = true;
  goHome_ = false;
  launchPlugin_ = false;

  // Build filtered list based on visibility settings
  buildVisibleList(core.settings);

  if (selected_ >= visiblePluginCount_ && visiblePluginCount_ > 0) selected_ = visiblePluginCount_ - 1;
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
        if (visiblePluginCount_ > 0) {
          selected_ = (selected_ == 0) ? visiblePluginCount_ - 1 : selected_ - 1;
          if (selected_ < scrollOffset_) scrollOffset_ = selected_;
          // Wrap: if jumped to bottom, adjust scroll
          int vis2 = visibleCount();
          if (selected_ >= scrollOffset_ + vis2) scrollOffset_ = selected_ - vis2 + 1;
          needsRender_ = true;
        }
        break;

      case Button::Down:
        if (visiblePluginCount_ > 0) {
          selected_ = (selected_ + 1) % visiblePluginCount_;
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
        if (visiblePluginCount_ > 0 && selected_ < visiblePluginCount_) launchPlugin_ = true;
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
    // Map from visible index to actual plugin index
    int actualIdx = visiblePlugins_[selected_];
    hostState_->setPluginFactory(plugins[actualIdx].factory);
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
  // Now iterates over the filtered visiblePlugins_ array
  const int startY = 60;
  int vis = visibleCount();

  for (int i = scrollOffset_; i < visiblePluginCount_ && i < scrollOffset_ + vis; i++) {
    int actualIdx = visiblePlugins_[i];
    const int y = startY + (i - scrollOffset_) * (t.menuItemHeight + t.itemSpacing);
    ui::menuItem(renderer_, t, y, plugins[actualIdx].name, i == selected_);

    // Show "Continue" right-aligned for games with saved progress
    if (plugins[actualIdx].savePath && SdMan.exists(plugins[actualIdx].savePath)) {
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
  if (scrollOffset_ > 0) {
    int cx = W / 2;
    renderer_.drawLine(cx, startY - 6, cx - 6, startY - 1, true);
    renderer_.drawLine(cx, startY - 6, cx + 6, startY - 1, true);
  }
  if (scrollOffset_ + vis < visiblePluginCount_) {
    int cx = W / 2;
    int ay = renderer_.getScreenHeight() - 38;
    renderer_.drawLine(cx, ay, cx - 6, ay - 6, true);
    renderer_.drawLine(cx, ay, cx + 6, ay - 6, true);
  }
}

}  // namespace sumi

#endif
