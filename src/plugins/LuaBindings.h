#pragma once

/**
 * @file LuaBindings.h
 * @brief C binding functions that expose PluginRenderer + PluginHelpers to Lua scripts
 *
 * Each function is a static lua_CFunction that reads args from the Lua stack,
 * calls the corresponding PluginRenderer method, and pushes any return values.
 * The PluginRenderer pointer is stored in the Lua registry at key "sumi_renderer".
 */

#include "../config.h"

#if FEATURE_PLUGINS

#include <cstdint>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "PluginRenderer.h"
#include "PluginHelpers.h"

namespace sumi {
namespace lua_bind {

// ---------------------------------------------------------------------------
// Helper: retrieve the PluginRenderer* from the Lua registry
// ---------------------------------------------------------------------------
static inline PluginRenderer& getRenderer(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "sumi_renderer");
  auto* r = static_cast<PluginRenderer*>(lua_touserdata(L, -1));
  lua_pop(L, 1);
  return *r;
}

// Store screen dimensions in registry for quick access
static inline int getScreenW(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "sumi_screenW");
  int w = (int)lua_tointeger(L, -1);
  lua_pop(L, 1);
  return w;
}

static inline int getScreenH(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "sumi_screenH");
  int h = (int)lua_tointeger(L, -1);
  lua_pop(L, 1);
  return h;
}

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------
static int l_fillScreen(lua_State* L) {
  bool color = lua_toboolean(L, 1);
  getRenderer(L).fillScreen(color);
  return 0;
}

static int l_width(lua_State* L) {
  lua_pushinteger(L, getRenderer(L).width());
  return 1;
}

static int l_height(lua_State* L) {
  lua_pushinteger(L, getRenderer(L).height());
  return 1;
}

// ---------------------------------------------------------------------------
// Drawing Primitives
// ---------------------------------------------------------------------------
static int l_drawPixel(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  bool c = lua_toboolean(L, 3);
  getRenderer(L).drawPixel(x, y, c);
  return 0;
}

static int l_drawLine(lua_State* L) {
  int x0 = (int)luaL_checkinteger(L, 1);
  int y0 = (int)luaL_checkinteger(L, 2);
  int x1 = (int)luaL_checkinteger(L, 3);
  int y1 = (int)luaL_checkinteger(L, 4);
  bool c = lua_toboolean(L, 5);
  getRenderer(L).drawLine(x0, y0, x1, y1, c);
  return 0;
}

static int l_drawRect(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int w = (int)luaL_checkinteger(L, 3);
  int h = (int)luaL_checkinteger(L, 4);
  bool c = lua_toboolean(L, 5);
  getRenderer(L).drawRect(x, y, w, h, c);
  return 0;
}

static int l_fillRect(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int w = (int)luaL_checkinteger(L, 3);
  int h = (int)luaL_checkinteger(L, 4);
  bool c = lua_toboolean(L, 5);
  getRenderer(L).fillRect(x, y, w, h, c);
  return 0;
}

static int l_drawRoundRect(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int w = (int)luaL_checkinteger(L, 3);
  int h = (int)luaL_checkinteger(L, 4);
  int r = (int)luaL_checkinteger(L, 5);
  bool c = lua_toboolean(L, 6);
  getRenderer(L).drawRoundRect(x, y, w, h, r, c);
  return 0;
}

static int l_fillRoundRect(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int w = (int)luaL_checkinteger(L, 3);
  int h = (int)luaL_checkinteger(L, 4);
  int r = (int)luaL_checkinteger(L, 5);
  bool c = lua_toboolean(L, 6);
  getRenderer(L).fillRoundRect(x, y, w, h, r, c);
  return 0;
}

static int l_drawHLine(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int w = (int)luaL_checkinteger(L, 3);
  bool c = lua_toboolean(L, 4);
  getRenderer(L).drawFastHLine(x, y, w, c);
  return 0;
}

static int l_drawVLine(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int h = (int)luaL_checkinteger(L, 3);
  bool c = lua_toboolean(L, 4);
  getRenderer(L).drawFastVLine(x, y, h, c);
  return 0;
}

static int l_drawCircle(lua_State* L) {
  int cx = (int)luaL_checkinteger(L, 1);
  int cy = (int)luaL_checkinteger(L, 2);
  int r  = (int)luaL_checkinteger(L, 3);
  bool c = lua_toboolean(L, 4);
  getRenderer(L).drawCircle(cx, cy, r, c);
  return 0;
}

static int l_fillCircle(lua_State* L) {
  int cx = (int)luaL_checkinteger(L, 1);
  int cy = (int)luaL_checkinteger(L, 2);
  int r  = (int)luaL_checkinteger(L, 3);
  bool c = lua_toboolean(L, 4);
  getRenderer(L).fillCircle(cx, cy, r, c);
  return 0;
}

static int l_drawTriangle(lua_State* L) {
  int x0 = (int)luaL_checkinteger(L, 1);
  int y0 = (int)luaL_checkinteger(L, 2);
  int x1 = (int)luaL_checkinteger(L, 3);
  int y1 = (int)luaL_checkinteger(L, 4);
  int x2 = (int)luaL_checkinteger(L, 5);
  int y2 = (int)luaL_checkinteger(L, 6);
  bool c = lua_toboolean(L, 7);
  getRenderer(L).drawTriangle(x0, y0, x1, y1, x2, y2, c);
  return 0;
}

static int l_fillTriangle(lua_State* L) {
  int x0 = (int)luaL_checkinteger(L, 1);
  int y0 = (int)luaL_checkinteger(L, 2);
  int x1 = (int)luaL_checkinteger(L, 3);
  int y1 = (int)luaL_checkinteger(L, 4);
  int x2 = (int)luaL_checkinteger(L, 5);
  int y2 = (int)luaL_checkinteger(L, 6);
  bool c = lua_toboolean(L, 7);
  getRenderer(L).fillTriangle(x0, y0, x1, y1, x2, y2, c);
  return 0;
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
static int l_setCursor(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  getRenderer(L).setCursor(x, y);
  return 0;
}

static int l_setTextColor(lua_State* L) {
  bool c = lua_toboolean(L, 1);
  getRenderer(L).setTextColor(c);
  return 0;
}

static int l_setTextSize(lua_State* L) {
  int s = (int)luaL_checkinteger(L, 1);
  getRenderer(L).setTextSize(s);
  return 0;
}

// text(str) — print without newline
static int l_text(lua_State* L) {
  const char* s = luaL_checkstring(L, 1);
  getRenderer(L).print(s);
  return 0;
}

// textLine(str) — print with newline
static int l_textLine(lua_State* L) {
  const char* s = luaL_optstring(L, 1, "");
  getRenderer(L).println(s);
  return 0;
}

static int l_textWidth(lua_State* L) {
  const char* s = luaL_checkstring(L, 1);
  lua_pushinteger(L, getRenderer(L).getTextWidth(s));
  return 1;
}

static int l_lineHeight(lua_State* L) {
  lua_pushinteger(L, getRenderer(L).getLineHeight());
  return 1;
}

static int l_cursorX(lua_State* L) {
  lua_pushinteger(L, getRenderer(L).getCursorX());
  return 1;
}

static int l_cursorY(lua_State* L) {
  lua_pushinteger(L, getRenderer(L).getCursorY());
  return 1;
}

// ---------------------------------------------------------------------------
// UI Helpers (from PluginHelpers.h / PluginUI namespace)
// ---------------------------------------------------------------------------
static int l_drawHeader(lua_State* L) {
  const char* title = luaL_checkstring(L, 1);
  auto& r = getRenderer(L);
  PluginUI::drawHeader(r, title, getScreenW(L));
  return 0;
}

static int l_drawFooter(lua_State* L) {
  const char* left  = luaL_checkstring(L, 1);
  const char* right = luaL_optstring(L, 2, "");
  auto& r = getRenderer(L);
  PluginUI::drawFooter(r, left, right, getScreenW(L), getScreenH(L));
  return 0;
}

static int l_drawCursor(lua_State* L) {
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  int w = (int)luaL_checkinteger(L, 3);
  int h = (int)luaL_checkinteger(L, 4);
  PluginUI::drawCursor(getRenderer(L), x, y, w, h);
  return 0;
}

static int l_drawTextCentered(lua_State* L) {
  const char* txt = luaL_checkstring(L, 1);
  int x = (int)luaL_checkinteger(L, 2);
  int y = (int)luaL_checkinteger(L, 3);
  int w = (int)luaL_checkinteger(L, 4);
  int h = (int)luaL_checkinteger(L, 5);
  PluginUI::drawTextCentered(getRenderer(L), txt, x, y, w, h);
  return 0;
}

static int l_drawMenuItem(lua_State* L) {
  const char* txt = luaL_checkstring(L, 1);
  int x   = (int)luaL_checkinteger(L, 2);
  int y   = (int)luaL_checkinteger(L, 3);
  int w   = (int)luaL_checkinteger(L, 4);
  int h   = (int)luaL_checkinteger(L, 5);
  bool sel = lua_toboolean(L, 6);
  PluginUI::drawMenuItem(getRenderer(L), txt, x, y, w, h, sel);
  return 0;
}

static int l_drawDialog(lua_State* L) {
  const char* title = luaL_checkstring(L, 1);
  const char* msg   = luaL_checkstring(L, 2);
  auto& r = getRenderer(L);
  PluginUI::drawDialog(r, title, msg, getScreenW(L), getScreenH(L));
  return 0;
}

static int l_drawGameOver(lua_State* L) {
  const char* result = luaL_checkstring(L, 1);
  const char* stats  = luaL_optstring(L, 2, "");
  auto& r = getRenderer(L);
  PluginUI::drawGameOver(r, result, stats, getScreenW(L), getScreenH(L));
  return 0;
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
static int l_millis(lua_State* L) {
  lua_pushinteger(L, (lua_Integer)millis());
  return 1;
}

static int l_random(lua_State* L) {
  int nargs = lua_gettop(L);
  if (nargs == 1) {
    // random(max) → 0..max-1
    int mx = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, mx > 0 ? (random(mx)) : 0);
  } else {
    // random(min, max) → min..max
    int lo = (int)luaL_checkinteger(L, 1);
    int hi = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, random(lo, hi + 1));
  }
  return 1;
}

static int l_delay(lua_State* L) {
  int ms = (int)luaL_checkinteger(L, 1);
  if (ms > 1000) ms = 1000;  // Cap at 1 second for safety
  if (ms > 0) delay(ms);
  return 0;
}

// ---------------------------------------------------------------------------
// Registration: push all bindings as globals into a lua_State
// ---------------------------------------------------------------------------
static void registerAll(lua_State* L) {
  // Drawing primitives
  static const struct { const char* name; lua_CFunction func; } funcs[] = {
    {"fillScreen",       l_fillScreen},
    {"drawPixel",        l_drawPixel},
    {"drawLine",         l_drawLine},
    {"drawRect",         l_drawRect},
    {"fillRect",         l_fillRect},
    {"drawRoundRect",    l_drawRoundRect},
    {"fillRoundRect",    l_fillRoundRect},
    {"drawHLine",        l_drawHLine},
    {"drawVLine",        l_drawVLine},
    {"drawCircle",       l_drawCircle},
    {"fillCircle",       l_fillCircle},
    {"drawTriangle",     l_drawTriangle},
    {"fillTriangle",     l_fillTriangle},
    // Text
    {"setCursor",        l_setCursor},
    {"setTextColor",     l_setTextColor},
    {"setTextSize",      l_setTextSize},
    {"text",             l_text},
    {"textLine",         l_textLine},
    {"textWidth",        l_textWidth},
    {"lineHeight",       l_lineHeight},
    {"cursorX",          l_cursorX},
    {"cursorY",          l_cursorY},
    // UI helpers
    {"drawHeader",       l_drawHeader},
    {"drawFooter",       l_drawFooter},
    {"drawCursor",       l_drawCursor},
    {"drawTextCentered", l_drawTextCentered},
    {"drawMenuItem",     l_drawMenuItem},
    {"drawDialog",       l_drawDialog},
    {"drawGameOver",     l_drawGameOver},
    // Screen info
    {"width",            l_width},
    {"height",           l_height},
    // Utilities
    {"millis",           l_millis},
    {"random",           l_random},
    {"delay",            l_delay},
    {nullptr, nullptr}
  };

  for (int i = 0; funcs[i].name; i++) {
    lua_pushcfunction(L, funcs[i].func);
    lua_setglobal(L, funcs[i].name);
  }

  // Constants
  lua_pushboolean(L, 1);  lua_setglobal(L, "BLACK");
  lua_pushboolean(L, 0);  lua_setglobal(L, "WHITE");

  // Button name constants (strings, matching what onButton receives)
  lua_pushstring(L, "up");     lua_setglobal(L, "BTN_UP");
  lua_pushstring(L, "down");   lua_setglobal(L, "BTN_DOWN");
  lua_pushstring(L, "left");   lua_setglobal(L, "BTN_LEFT");
  lua_pushstring(L, "right");  lua_setglobal(L, "BTN_RIGHT");
  lua_pushstring(L, "center"); lua_setglobal(L, "BTN_CENTER");
  lua_pushstring(L, "back");   lua_setglobal(L, "BTN_BACK");

  // UI layout constants
  lua_pushinteger(L, PLUGIN_HEADER_H); lua_setglobal(L, "HEADER_H");
  lua_pushinteger(L, PLUGIN_FOOTER_H); lua_setglobal(L, "FOOTER_H");
  lua_pushinteger(L, PLUGIN_MARGIN);   lua_setglobal(L, "MARGIN");
}

// Remove dangerous globals from the base library
static void sandboxGlobals(lua_State* L) {
  // Remove functions that could be dangerous in embedded context
  const char* blocked[] = {
    "dofile", "loadfile", "load", "loadstring",
    "rawget", "rawset", "rawequal", "rawlen",
    "collectgarbage",
    nullptr
  };
  for (int i = 0; blocked[i]; i++) {
    lua_pushnil(L);
    lua_setglobal(L, blocked[i]);
  }
}

}  // namespace lua_bind
}  // namespace sumi

#endif  // FEATURE_PLUGINS
