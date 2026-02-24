#include "LuaPlugin.h"

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <SD.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "../core/MemoryArena.h"
#include "LuaBindings.h"
#include "PluginHelpers.h"

namespace sumi {

// ---------------------------------------------------------------------------
// Custom allocator with memory cap
// ---------------------------------------------------------------------------
void* LuaPlugin::luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  auto* self = static_cast<LuaPlugin*>(ud);

  if (nsize == 0) {
    // Free
    if (ptr) {
      self->memUsed_ -= osize;
      free(ptr);
    }
    return nullptr;
  }

  // Check memory limit
  size_t delta = nsize - (ptr ? osize : 0);
  if (self->memUsed_ + delta > LUA_MEM_LIMIT) {
    Serial.printf("[LUA] Memory limit reached (%zu + %zu > %zu)\n",
                  self->memUsed_, delta, LUA_MEM_LIMIT);
    return nullptr;  // Allocation denied
  }

  void* newPtr = realloc(ptr, nsize);
  if (newPtr) {
    self->memUsed_ += delta;
  }
  return newPtr;
}

// ---------------------------------------------------------------------------
// Instruction count hook — fires after INSTRUCTION_LIMIT ops
// ---------------------------------------------------------------------------
void LuaPlugin::luaHook(lua_State* L, lua_Debug* ar) {
  (void)ar;
  luaL_error(L, "Script exceeded instruction limit (infinite loop?)");
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
LuaPlugin::LuaPlugin(PluginRenderer& renderer, const char* scriptPath)
    : renderer_(renderer) {
  strncpy(scriptPath_, scriptPath, sizeof(scriptPath_) - 1);
  scriptPath_[sizeof(scriptPath_) - 1] = '\0';

  // Derive name from filename: "/custom/my_game.lua" → "my_game"
  const char* slash = strrchr(scriptPath_, '/');
  const char* base = slash ? slash + 1 : scriptPath_;
  const char* dot = strrchr(base, '.');
  size_t len = dot ? (size_t)(dot - base) : strlen(base);
  if (len >= sizeof(name_)) len = sizeof(name_) - 1;
  memcpy(name_, base, len);
  name_[len] = '\0';

  // Replace underscores with spaces for display
  for (size_t i = 0; i < len; i++) {
    if (name_[i] == '_') name_[i] = ' ';
  }
  // Capitalize first letter
  if (name_[0] >= 'a' && name_[0] <= 'z') {
    name_[0] -= 32;
  }

  errorMsg_[0] = '\0';
}

LuaPlugin::~LuaPlugin() {
  cleanup();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void LuaPlugin::init(int screenW, int screenH) {
  screenW_ = screenW;
  screenH_ = screenH;
  hasError_ = false;
  errorMsg_[0] = '\0';

  Serial.printf("[LUA] Init: %s (%dx%d)\n", scriptPath_, screenW, screenH);
  Serial.printf("[LUA] Heap before: free=%lu, largest=%lu\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());

  // Release MemoryArena to free 80KB for Lua VM
  if (MemoryArena::isInitialized()) {
    MemoryArena::release();
    Serial.printf("[LUA] Arena released, heap: free=%lu, largest=%lu\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  }

  // Create Lua state with custom allocator
  memUsed_ = 0;
  L_ = lua_newstate(luaAlloc, this);
  if (!L_) {
    snprintf(errorMsg_, sizeof(errorMsg_), "Failed to create Lua VM");
    hasError_ = true;
    Serial.printf("[LUA] ERROR: %s\n", errorMsg_);
    return;
  }

  // Set instruction count hook
  lua_sethook(L_, luaHook, LUA_MASKCOUNT, INSTRUCTION_LIMIT);

  // Open safe standard libraries
  luaL_openlibs(L_);

  // Sandbox: remove dangerous globals
  lua_bind::sandboxGlobals(L_);

  // Store renderer pointer in registry
  lua_pushlightuserdata(L_, &renderer_);
  lua_setfield(L_, LUA_REGISTRYINDEX, "sumi_renderer");

  // Store screen dimensions in registry
  lua_pushinteger(L_, screenW);
  lua_setfield(L_, LUA_REGISTRYINDEX, "sumi_screenW");
  lua_pushinteger(L_, screenH);
  lua_setfield(L_, LUA_REGISTRYINDEX, "sumi_screenH");

  // Register all SUMI drawing bindings
  lua_bind::registerAll(L_);

  // Set SCREEN_W and SCREEN_H constants
  lua_pushinteger(L_, screenW); lua_setglobal(L_, "SCREEN_W");
  lua_pushinteger(L_, screenH); lua_setglobal(L_, "SCREEN_H");

  Serial.printf("[LUA] VM created, mem=%zu bytes\n", memUsed_);

  // Load and execute the script
  if (!loadScript()) {
    return;  // hasError_ already set
  }

  // Check if update() is defined
  lua_getglobal(L_, "update");
  hasUpdate_ = lua_isfunction(L_, -1);
  lua_pop(L_, 1);

  // Call init(w, h)
  lua_getglobal(L_, "init");
  if (lua_isfunction(L_, -1)) {
    lua_pushinteger(L_, screenW);
    lua_pushinteger(L_, screenH);
    callLuaFunc("init", 2, 0);
  } else {
    lua_pop(L_, 1);  // init is optional
  }

  Serial.printf("[LUA] Ready, mem=%zu bytes, hasUpdate=%d\n", memUsed_, hasUpdate_);
}

void LuaPlugin::cleanup() {
  if (L_) {
    lua_close(L_);
    L_ = nullptr;
    memUsed_ = 0;
    Serial.printf("[LUA] VM closed, reclaiming arena\n");
  }

  // Reclaim MemoryArena
  if (!MemoryArena::isInitialized()) {
    MemoryArena::init();
  }

  Serial.printf("[LUA] Cleanup done, heap: free=%lu\n", (unsigned long)ESP.getFreeHeap());
}

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------
bool LuaPlugin::loadScript() {
  File f = SD.open(scriptPath_, FILE_READ);
  if (!f) {
    snprintf(errorMsg_, sizeof(errorMsg_), "Cannot open: %s", scriptPath_);
    hasError_ = true;
    Serial.printf("[LUA] ERROR: %s\n", errorMsg_);
    return false;
  }

  size_t fileSize = f.size();
  if (fileSize > MAX_SCRIPT_SIZE) {
    f.close();
    snprintf(errorMsg_, sizeof(errorMsg_), "Script too large (%zuKB > %zuKB)",
             fileSize / 1024, MAX_SCRIPT_SIZE / 1024);
    hasError_ = true;
    Serial.printf("[LUA] ERROR: %s\n", errorMsg_);
    return false;
  }

  // Read script into temporary buffer
  char* buf = static_cast<char*>(malloc(fileSize + 1));
  if (!buf) {
    f.close();
    snprintf(errorMsg_, sizeof(errorMsg_), "No memory for script (%zu bytes)", fileSize);
    hasError_ = true;
    Serial.printf("[LUA] ERROR: %s\n", errorMsg_);
    return false;
  }

  size_t bytesRead = f.read(reinterpret_cast<uint8_t*>(buf), fileSize);
  f.close();
  buf[bytesRead] = '\0';

  Serial.printf("[LUA] Loaded %zu bytes from %s\n", bytesRead, scriptPath_);

  // Execute the script (defines global functions)
  int err = luaL_dostring(L_, buf);
  free(buf);

  if (err != LUA_OK) {
    const char* msg = lua_tostring(L_, -1);
    snprintf(errorMsg_, sizeof(errorMsg_), "%.79s", msg ? msg : "Unknown error");
    lua_pop(L_, 1);
    hasError_ = true;
    Serial.printf("[LUA] Load error: %s\n", errorMsg_);
    return false;
  }

  // Verify required functions exist
  lua_getglobal(L_, "draw");
  bool hasDraw = lua_isfunction(L_, -1);
  lua_pop(L_, 1);

  lua_getglobal(L_, "onButton");
  bool hasOnButton = lua_isfunction(L_, -1);
  lua_pop(L_, 1);

  if (!hasDraw) {
    snprintf(errorMsg_, sizeof(errorMsg_), "Missing required function: draw()");
    hasError_ = true;
    Serial.printf("[LUA] ERROR: %s\n", errorMsg_);
    return false;
  }

  if (!hasOnButton) {
    snprintf(errorMsg_, sizeof(errorMsg_), "Missing required function: onButton()");
    hasError_ = true;
    Serial.printf("[LUA] ERROR: %s\n", errorMsg_);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Call a Lua function safely with error handling
// ---------------------------------------------------------------------------
bool LuaPlugin::callLuaFunc(const char* funcName, int nargs, int nresults) {
  // Reset instruction counter by re-setting the hook
  lua_sethook(L_, luaHook, LUA_MASKCOUNT, INSTRUCTION_LIMIT);

  int err = lua_pcall(L_, nargs, nresults, 0);
  if (err != LUA_OK) {
    const char* msg = lua_tostring(L_, -1);
    snprintf(errorMsg_, sizeof(errorMsg_), "%s: %.60s",
             funcName, msg ? msg : "error");
    lua_pop(L_, 1);
    hasError_ = true;
    Serial.printf("[LUA] Runtime error in %s: %s\n", funcName, errorMsg_);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void LuaPlugin::draw() {
  if (hasError_ || !L_) {
    showError();
    return;
  }

  lua_getglobal(L_, "draw");
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 1);
    return;
  }
  callLuaFunc("draw");

  if (hasError_) showError();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
const char* LuaPlugin::buttonName(PluginButton btn) {
  switch (btn) {
    case PluginButton::Up:     return "up";
    case PluginButton::Down:   return "down";
    case PluginButton::Left:   return "left";
    case PluginButton::Right:  return "right";
    case PluginButton::Center: return "center";
    case PluginButton::Back:   return "back";
    case PluginButton::Power:  return "power";
    default:                   return "none";
  }
}

bool LuaPlugin::handleInput(PluginButton btn) {
  if (hasError_ || !L_) return false;

  lua_getglobal(L_, "onButton");
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 1);
    return false;
  }

  lua_pushstring(L_, buttonName(btn));

  if (!callLuaFunc("onButton", 1, 1)) {
    return false;
  }

  bool consumed = lua_toboolean(L_, -1);
  lua_pop(L_, 1);
  needsFullRedraw = true;
  return consumed;
}

// ---------------------------------------------------------------------------
// Update (10Hz tick)
// ---------------------------------------------------------------------------
bool LuaPlugin::update() {
  if (!hasUpdate_ || hasError_ || !L_) return false;

  lua_getglobal(L_, "update");
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 1);
    return false;
  }

  if (!callLuaFunc("update", 0, 1)) {
    return false;
  }

  bool needsRedraw = lua_toboolean(L_, -1);
  lua_pop(L_, 1);
  if (needsRedraw) needsFullRedraw = true;
  return needsRedraw;
}

// ---------------------------------------------------------------------------
// Run mode
// ---------------------------------------------------------------------------
PluginRunMode LuaPlugin::runMode() const {
  return hasUpdate_ ? PluginRunMode::WithUpdate : PluginRunMode::Simple;
}

// ---------------------------------------------------------------------------
// Error display
// ---------------------------------------------------------------------------
void LuaPlugin::showError() {
  renderer_.fillScreen(false);  // WHITE
  PluginUI::drawHeader(renderer_, "Lua Error", screenW_);
  renderer_.setCursor(PLUGIN_MARGIN, PLUGIN_HEADER_H + 20);
  renderer_.setTextColor(true);  // BLACK
  renderer_.print(name_);
  renderer_.setCursor(PLUGIN_MARGIN, PLUGIN_HEADER_H + 50);
  // Word-wrap error message manually for small screen
  const char* p = errorMsg_;
  int y = PLUGIN_HEADER_H + 50;
  int maxW = screenW_ - PLUGIN_MARGIN * 2;
  while (*p && y < screenH_ - PLUGIN_FOOTER_H - 20) {
    // Find how much fits on this line
    char lineBuf[64];
    int len = 0;
    while (p[len] && len < 63) {
      lineBuf[len] = p[len];
      lineBuf[len + 1] = '\0';
      if (renderer_.getTextWidth(lineBuf) > maxW) break;
      len++;
    }
    if (len == 0) { len = 1; lineBuf[0] = *p; lineBuf[1] = '\0'; }
    renderer_.setCursor(PLUGIN_MARGIN, y);
    lineBuf[len] = '\0';
    renderer_.print(lineBuf);
    p += len;
    y += renderer_.getLineHeight() + 2;
  }
  PluginUI::drawFooter(renderer_, "Back: exit", "", screenW_, screenH_);
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
