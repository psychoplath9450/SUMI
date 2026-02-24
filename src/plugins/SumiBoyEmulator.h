#pragma once
/**
 * @file SumiBoyEmulator.h
 * @brief Game Boy emulator plugin for SUMI — loads ROMs from SD card
 *
 * Full SM83 CPU, PPU (BG/Window/Sprites), MBC3 banking, timer.
 * ROM banks cached from SD card on demand (32KB in RAM at any time).
 * Dithered rendering to e-ink via direct framebuffer access.
 *
 * Ported from standalone sumi-emulator firmware.
 */

#include <Arduino.h>

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <SdFat.h>

#include "PluginInterface.h"
#include "PluginRenderer.h"
#include "../core/MemoryArena.h"

namespace sumi {

class SumiBoyEmulator : public PluginInterface {
 public:
  explicit SumiBoyEmulator(PluginRenderer& renderer, const char* romPath);
  ~SumiBoyEmulator() override;

  // --- PluginInterface ---
  void init(int screenW, int screenH) override;
  void cleanup() override;
  void draw() override;
  bool handleInput(PluginButton btn) override;
  bool handleRelease(PluginButton btn) override;
  bool update() override;  // Runs N frames, checks dirty, triggers refresh
  PluginRunMode runMode() const override { return PluginRunMode::WithUpdate; }
  bool wantsLandscape() const override { return false; }
  bool handlesOwnRefresh() const override { return true; }
  const char* name() const override { return "SumiBoy"; }

 private:
  PluginRenderer& disp_;

  // --- ROM loading ---
  bool loadROM();
  uint8_t readRomByte(uint32_t addr);

  // --- Emulator core ---
  void cpu_step();
  void renderLine(int line);
  void runScanline();
  void runFrame();
  uint8_t readByte(uint16_t addr);
  void writeByte(uint16_t addr, uint8_t val);
  void updateTimer(int cycles);
  void initEmu();

  // --- Display ---
  void renderToDisplay();
  void drawControls();
  void initDitherLUT();

  // --- Save/load ---
  bool saveSRAM();
  bool loadSRAM();
  bool saveState();
  bool loadState();

  // --- Helpers ---
  inline uint8_t applyPalette(uint8_t palette, uint8_t colorIdx) {
    return (palette >> (colorIdx * 2)) & 0x03;
  }

  // =====================================================================
  // ROM state
  // =====================================================================
  char romPath_[80] = {};
  char romName_[32] = {};       // Filename without extension (for save paths)
  char sramPath_[80] = {};
  char statePath_[80] = {};

  FsFile romFile_;
  uint32_t romSize_ = 0;
  uint8_t* romBank0_ = nullptr;       // 16KB fixed bank 0
  uint8_t* romBankN_ = nullptr;       // 16KB switchable bank (cached)
  uint8_t cachedBankNum_ = 0xFF;

  // =====================================================================
  // CPU registers
  // =====================================================================
  uint16_t pc_ = 0, sp_ = 0;
  uint8_t a_ = 0, f_ = 0, b_ = 0, c_ = 0, d_ = 0, e_ = 0, h_ = 0, l_ = 0;
  bool ime_ = false;
  bool halted_ = false;
  bool haltBug_ = false;
  bool eiPending_ = false;
  uint8_t ie_ = 0, iflag_ = 0;

  // =====================================================================
  // LCD registers
  // =====================================================================
  uint8_t lcdc_ = 0, lcdstat_ = 0, scy_ = 0, scx_ = 0, ly_ = 0, lyc_ = 0;
  uint8_t bgp_ = 0, obp0_ = 0, obp1_ = 0, wy_ = 0, wx_ = 0;

  // =====================================================================
  // Timer
  // =====================================================================
  uint8_t tima_ = 0, tma_ = 0, tac_ = 0;
  uint16_t timerCounter_ = 0;
  uint16_t divCounter_ = 0;
  uint8_t divReg_ = 0;

  // =====================================================================
  // Joypad
  // =====================================================================
  uint8_t joyDpad_ = 0x0F;
  uint8_t joyButtons_ = 0x0F;
  uint8_t joypadSelect_ = 0;

  // =====================================================================
  // Sound (stub)
  // =====================================================================
  uint8_t soundRegs_[0x30] = {};

  // =====================================================================
  // MBC3 state
  // =====================================================================
  uint8_t romBank_ = 1;
  uint8_t ramBank_ = 0;
  bool ramEnabled_ = false;

  // =====================================================================
  // Memory
  // =====================================================================
  static constexpr int GB_W = 160;
  static constexpr int GB_H = 144;
  static constexpr int ROM_BANK_SIZE = 0x4000;
  static constexpr int RAM_BANK_SIZE = 0x2000;

  uint8_t* framebuffer_ = nullptr;   // 160x144 shade values (0-3)
  uint8_t* vram_ = nullptr;          // 8KB
  uint8_t* wram_ = nullptr;          // 8KB
  uint8_t* oam_ = nullptr;           // 160 bytes
  uint8_t* hram_ = nullptr;          // 128 bytes
  uint8_t* sram_ = nullptr;          // 32KB (4 banks)

  // =====================================================================
  // Display / e-ink state
  // =====================================================================
  int screenW_ = 0, screenH_ = 0;
  uint32_t lastFrameHash_ = 0;
  int frameCount_ = 0;
  int consecutiveSkips_ = 0;
  bool firstDraw_ = true;
  bool arenaWasInit_ = false;        // Track if we need to reclaim arena

  // Dither LUT: shade_byte_[shade][bayer_col] = 8-bit dither pattern
  uint8_t shade_byte_[4][4] = {};

  // Auto-save
  int saveCounter_ = 0;
  int sramCounter_ = 0;
  bool buttonPressedSinceSave_ = false;

  // Button state: maps directly to GB joypad bits (active low).
  // Set by handleInput() on press, cleared by handleRelease() on release.
  // Buttons stay held across multiple update() ticks — just like real hardware.
  //   D-pad bits: Right=0, Left=1, Up=2, Down=3
  //   Button bits: A=0, B=1, Select=2, Start=3
  uint8_t pendingDpad_ = 0x0F;
  uint8_t pendingButtons_ = 0x0F;
  bool pendingInput_ = false;

  // Double-tap detection (Center x2 = Select)
  unsigned long lastCenterMs_ = 0;
  static constexpr unsigned long DOUBLE_TAP_MS = 400;

  // Frame timing
  static constexpr int FRAMES_PER_REFRESH = 10;
  static constexpr int FRAMES_PER_REFRESH_IDLE = 4;
  static constexpr int AUTOSAVE_INTERVAL = 300;
  static constexpr int SRAM_SAVE_INTERVAL = 100;

  // Display constants
  static constexpr int DISP_GAME_W = GB_W * 3;  // 480
  static constexpr int DISP_GAME_H = GB_H * 3;  // 432

  bool allocateMemory();
  void freeMemory();
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS && FEATURE_GAMES
