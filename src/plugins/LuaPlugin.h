#pragma once

/**
 * @file LuaPlugin.h
 * @brief PluginInterface implementation that runs a Lua script from SD card
 *
 * LuaPlugin hosts a lua_State, loads a .lua file, binds all drawing functions,
 * and forwards init/draw/handleInput/update to Lua functions.
 *
 * Memory strategy:
 *  - MemoryArena is released before creating the Lua VM (frees 80KB)
 *  - Custom allocator caps total Lua memory at LUA_MEM_LIMIT
 *  - Instruction count hook prevents infinite loops
 *  - On cleanup, Lua VM is destroyed and MemoryArena reclaimed
 */

#include "../config.h"

#if FEATURE_PLUGINS

#include <cstdint>

#include "PluginInterface.h"
#include "PluginRenderer.h"

struct lua_State;
struct lua_Debug;

namespace sumi {

class LuaPlugin : public PluginInterface {
 public:
  // Maximum memory the Lua VM may allocate (bytes)
  static constexpr size_t LUA_MEM_LIMIT = 40 * 1024;

  // Maximum script file size (bytes)
  static constexpr size_t MAX_SCRIPT_SIZE = 16 * 1024;

  // Instruction count limit per call (prevents infinite loops)
  static constexpr int INSTRUCTION_LIMIT = 100000;

  LuaPlugin(PluginRenderer& renderer, const char* scriptPath);
  ~LuaPlugin() override;

  // PluginInterface
  void init(int screenW, int screenH) override;
  void cleanup() override;
  void draw() override;
  bool handleInput(PluginButton btn) override;
  bool update() override;
  const char* name() const override { return name_; }
  PluginRunMode runMode() const override;

 private:
  PluginRenderer& renderer_;
  lua_State* L_ = nullptr;
  char scriptPath_[64];
  char name_[24];
  bool hasUpdate_ = false;  // Does the script define update()?
  bool hasError_ = false;   // Script failed to load or crashed
  char errorMsg_[80];       // Last error message for display
  int screenW_ = 0;
  int screenH_ = 0;

  // Custom allocator tracking
  size_t memUsed_ = 0;

  // Load and execute the script file
  bool loadScript();

  // Call a Lua function by name, returns false if error
  bool callLuaFunc(const char* funcName, int nargs = 0, int nresults = 0);

  // Push button name string for the given button
  static const char* buttonName(PluginButton btn);

  // Show error on screen
  void showError();

  // Custom allocator for memory limiting
  static void* luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize);

  // Instruction count hook for infinite loop prevention
  static void luaHook(lua_State* L, lua_Debug* ar);
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
