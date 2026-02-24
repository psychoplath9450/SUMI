#include "SumiBoyEmulator.h"

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include <LittleFS.h>
#include <SDCardManager.h>
#include <EInkDisplay.h>
#include <GfxRenderer.h>
#include <esp_task_wdt.h>

// gb_controls_img.h removed — using programmatic controls overlay instead

namespace sumi {

// 4x4 ordered Bayer matrix for dithering
static const uint8_t bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}};

// GB shade -> intensity (0-16 range)
static const uint8_t shadeIntensity[4] = {0, 5, 11, 16};

// =========================================================================
// Construction / Destruction
// =========================================================================

SumiBoyEmulator::SumiBoyEmulator(PluginRenderer& renderer, const char* romPath)
    : disp_(renderer) {
  strncpy(romPath_, romPath, sizeof(romPath_) - 1);

  // Extract ROM name (filename without extension) for save paths
  const char* slash = strrchr(romPath, '/');
  const char* base = slash ? slash + 1 : romPath;
  const char* dot = strrchr(base, '.');
  size_t len = dot ? (size_t)(dot - base) : strlen(base);
  if (len >= sizeof(romName_)) len = sizeof(romName_) - 1;
  memcpy(romName_, base, len);
  romName_[len] = '\0';

  // Build save paths
  snprintf(sramPath_, sizeof(sramPath_), "/games/saves/%s.sav", romName_);
  snprintf(statePath_, sizeof(statePath_), "/games/saves/%s.state", romName_);
}

SumiBoyEmulator::~SumiBoyEmulator() { cleanup(); }

// =========================================================================
// Plugin lifecycle
// =========================================================================

void SumiBoyEmulator::init(int screenW, int screenH) {
  screenW_ = screenW;
  screenH_ = screenH;

  Serial.printf("[SumiBoy] Init: %dx%d, ROM=%s\n", screenW, screenH, romPath_);
  Serial.printf("[SumiBoy] Heap before: free=%lu, largest=%lu\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());

  // Release MemoryArena to free ~80KB for emulator
  arenaWasInit_ = MemoryArena::isInitialized();
  if (arenaWasInit_) {
    MemoryArena::release();
    Serial.printf("[SumiBoy] Arena released, heap: free=%lu, largest=%lu\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  }

  if (!allocateMemory()) {
    Serial.println("[SumiBoy] FATAL: Memory allocation failed!");
    return;
  }

  initDitherLUT();

  if (!loadROM()) {
    Serial.println("[SumiBoy] FATAL: ROM load failed!");
    freeMemory();
    return;
  }

  initEmu();

  // Try to restore save state, then SRAM
  if (!loadState()) {
    loadSRAM();
    // Warmup: run 600 frames so the game is past the boot sequence
    Serial.printf("[SumiBoy] Running warmup frames...\n");
    unsigned long t = millis();
    for (int i = 0; i < 600; i++) {
      runFrame();
      if (i % 20 == 0) delay(1);  // Feed watchdog
    }
    frameCount_ = 600;
    Serial.printf("[SumiBoy] Warmup done in %lums\n", millis() - t);
  }

  firstDraw_ = true;
  lastFrameHash_ = 0;

  Serial.printf("[SumiBoy] Ready, heap: free=%lu\n", (unsigned long)ESP.getFreeHeap());
}

void SumiBoyEmulator::cleanup() {
  // Save state before exit
  if (framebuffer_) {
    saveState();
    saveSRAM();
  }

  if (romFile_) {
    romFile_.close();
  }

  freeMemory();

  // Reclaim MemoryArena
  if (arenaWasInit_ && !MemoryArena::isInitialized()) {
    MemoryArena::init();
    Serial.printf("[SumiBoy] Arena reclaimed, heap: free=%lu\n", (unsigned long)ESP.getFreeHeap());
  }

  Serial.println("[SumiBoy] Cleanup done");
}

bool SumiBoyEmulator::allocateMemory() {
  romBank0_ = (uint8_t*)malloc(ROM_BANK_SIZE);           // 16KB
  romBankN_ = (uint8_t*)malloc(ROM_BANK_SIZE);           // 16KB
  framebuffer_ = (uint8_t*)malloc(GB_W * GB_H);          // 23KB
  vram_ = (uint8_t*)calloc(1, 0x2000);                   // 8KB
  wram_ = (uint8_t*)calloc(1, 0x2000);                   // 8KB
  oam_ = (uint8_t*)calloc(1, 0xA0);                      // 160B
  hram_ = (uint8_t*)calloc(1, 0x80);                     // 128B
  sram_ = (uint8_t*)calloc(1, 0x8000);                   // 32KB

  if (!romBank0_ || !romBankN_ || !framebuffer_ || !vram_ || !wram_ || !oam_ || !hram_ || !sram_) {
    freeMemory();
    return false;
  }

  Serial.printf("[SumiBoy] Memory allocated: ~%dKB\n",
                (ROM_BANK_SIZE * 2 + GB_W * GB_H + 0x2000 * 2 + 0xA0 + 0x80 + 0x8000) / 1024);
  return true;
}

void SumiBoyEmulator::freeMemory() {
  free(romBank0_);  romBank0_ = nullptr;
  free(romBankN_);  romBankN_ = nullptr;
  free(framebuffer_);  framebuffer_ = nullptr;
  free(vram_);  vram_ = nullptr;
  free(wram_);  wram_ = nullptr;
  free(oam_);   oam_ = nullptr;
  free(hram_);  hram_ = nullptr;
  free(sram_);  sram_ = nullptr;
}

// =========================================================================
// ROM loading
// =========================================================================

bool SumiBoyEmulator::loadROM() {
  romFile_ = SdMan.open(romPath_);
  if (!romFile_) {
    Serial.printf("[SumiBoy] Failed to open ROM: %s\n", romPath_);
    return false;
  }

  romSize_ = romFile_.size();
  Serial.printf("[SumiBoy] ROM: %s (%lu bytes, %lu banks)\n",
                romPath_, (unsigned long)romSize_, (unsigned long)(romSize_ / ROM_BANK_SIZE));

  // Read bank 0 (always mapped at 0x0000-0x3FFF)
  romFile_.seek(0);
  if (romFile_.read(romBank0_, ROM_BANK_SIZE) != ROM_BANK_SIZE) {
    Serial.println("[SumiBoy] Failed to read ROM bank 0");
    romFile_.close();
    return false;
  }

  // Read bank 1 (initial switchable bank)
  if (romSize_ > ROM_BANK_SIZE) {
    romFile_.seek(ROM_BANK_SIZE);
    romFile_.read(romBankN_, ROM_BANK_SIZE);
    cachedBankNum_ = 1;
  }

  // Read cartridge header for MBC type
  uint8_t cartType = romBank0_[0x0147];
  Serial.printf("[SumiBoy] Cart type: 0x%02X\n", cartType);
  // Currently supports ROM-only and MBC3 (covers most games)
  // MBC1 (0x01-0x03) and MBC5 (0x19-0x1E) can be added later

  // Read ROM title from header (0x0134-0x0143)
  char title[17] = {};
  memcpy(title, &romBank0_[0x0134], 16);
  title[16] = '\0';
  Serial.printf("[SumiBoy] ROM title: %s\n", title);

  return true;
}

uint8_t SumiBoyEmulator::readRomByte(uint32_t addr) {
  // Bank 0: 0x0000-0x3FFF — always in romBank0_
  if (addr < ROM_BANK_SIZE) {
    return romBank0_[addr];
  }

  // Switchable bank: compute which bank
  uint32_t bank = addr / ROM_BANK_SIZE;
  uint32_t offset = addr % ROM_BANK_SIZE;

  if (bank != cachedBankNum_) {
    // Cache miss — read from SD
    uint32_t fileOffset = bank * ROM_BANK_SIZE;
    if (fileOffset + ROM_BANK_SIZE <= romSize_) {
      romFile_.seek(fileOffset);
      romFile_.read(romBankN_, ROM_BANK_SIZE);
      cachedBankNum_ = bank;
    } else {
      return 0xFF;
    }
  }

  return romBankN_[offset];
}

// =========================================================================
// Emulator init (DMG post-boot register state)
// =========================================================================

void SumiBoyEmulator::initEmu() {
  a_ = 0x01; f_ = 0xB0;
  b_ = 0x00; c_ = 0x13;
  d_ = 0x00; e_ = 0xD8;
  h_ = 0x01; l_ = 0x4D;
  pc_ = 0x100; sp_ = 0xFFFE;

  lcdc_ = 0x91; lcdstat_ = 0;
  scy_ = 0; scx_ = 0; ly_ = 0; lyc_ = 0;
  bgp_ = 0xFC; obp0_ = 0xFF; obp1_ = 0xFF;
  wy_ = 0; wx_ = 0;

  ime_ = false; halted_ = false; haltBug_ = false; eiPending_ = false;
  ie_ = 0; iflag_ = 0;

  tima_ = 0; tma_ = 0; tac_ = 0;
  timerCounter_ = 0; divCounter_ = 0; divReg_ = 0;

  romBank_ = 1; ramBank_ = 0; ramEnabled_ = false;
  joyDpad_ = 0x0F; joyButtons_ = 0x0F; joypadSelect_ = 0;

  memset(framebuffer_, 0, GB_W * GB_H);
  memset(vram_, 0, 0x2000);
  memset(wram_, 0, 0x2000);
  memset(oam_, 0, 0xA0);
  memset(hram_, 0, 0x80);
  memset(sram_, 0, 0x8000);
  memset(soundRegs_, 0, sizeof(soundRegs_));

  // Many GB games check NR52 to see if sound hardware is on
  soundRegs_[0x16] = 0xF1;
}

// =========================================================================
// Memory bus
// =========================================================================

uint8_t SumiBoyEmulator::readByte(uint16_t addr) {
  // ROM Bank 0 (fixed)
  if (addr < 0x4000)
    return romBank0_[addr];

  // ROM Bank N (switchable)
  if (addr < 0x8000)
    return readRomByte((uint32_t)romBank_ * ROM_BANK_SIZE + (addr - 0x4000));

  // VRAM
  if (addr < 0xA000)
    return vram_[addr - 0x8000];

  // External RAM (MBC3)
  if (addr < 0xC000) {
    if (!ramEnabled_) return 0xFF;
    if (ramBank_ <= 3)
      return sram_[(uint32_t)ramBank_ * RAM_BANK_SIZE + (addr - 0xA000)];
    return 0xFF;
  }

  // WRAM
  if (addr < 0xE000)
    return wram_[addr - 0xC000];

  // Echo RAM
  if (addr < 0xFE00)
    return wram_[addr - 0xE000];

  // OAM
  if (addr < 0xFEA0)
    return oam_[addr - 0xFE00];

  // Unusable
  if (addr < 0xFF00)
    return 0xFF;

  // IO Registers
  switch (addr) {
    case 0xFF00: {
      uint8_t result = 0xCF;
      if (!(joypadSelect_ & 0x10)) result &= (joyDpad_ | 0xF0);
      if (!(joypadSelect_ & 0x20)) result &= (joyButtons_ | 0xF0);
      return result;
    }
    case 0xFF04: return divReg_;
    case 0xFF05: return tima_;
    case 0xFF06: return tma_;
    case 0xFF07: return tac_ | 0xF8;
    case 0xFF0F: return iflag_ | 0xE0;

    case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13: case 0xFF14:
    case 0xFF16: case 0xFF17: case 0xFF18: case 0xFF19:
    case 0xFF1A: case 0xFF1B: case 0xFF1C: case 0xFF1D: case 0xFF1E:
    case 0xFF20: case 0xFF21: case 0xFF22: case 0xFF23:
    case 0xFF24: case 0xFF25: case 0xFF26:
      return soundRegs_[addr - 0xFF10];

    case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
    case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
    case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
    case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
      return soundRegs_[addr - 0xFF10];

    case 0xFF40: return lcdc_;
    case 0xFF41: return lcdstat_ | 0x80 | ((ly_ == lyc_) ? 4 : 0);
    case 0xFF42: return scy_;
    case 0xFF43: return scx_;
    case 0xFF44: return ly_;
    case 0xFF45: return lyc_;
    case 0xFF47: return bgp_;
    case 0xFF48: return obp0_;
    case 0xFF49: return obp1_;
    case 0xFF4A: return wy_;
    case 0xFF4B: return wx_;
    case 0xFFFF: return ie_;
  }

  if (addr >= 0xFF80 && addr < 0xFFFF)
    return hram_[addr - 0xFF80];

  return 0xFF;
}

void SumiBoyEmulator::writeByte(uint16_t addr, uint8_t val) {
  // MBC3: RAM Enable
  if (addr < 0x2000) {
    ramEnabled_ = ((val & 0x0F) == 0x0A);
    return;
  }

  // MBC3: ROM Bank Number (7 bits)
  if (addr < 0x4000) {
    uint8_t newBank = val & 0x7F;
    if (newBank == 0) newBank = 1;
    if (newBank != romBank_) {
      romBank_ = newBank;
      // Bank switch — readRomByte will handle cache miss
    }
    return;
  }

  // MBC3: RAM Bank / RTC Select
  if (addr < 0x6000) {
    ramBank_ = val;
    return;
  }

  // MBC3: Latch Clock Data — ignore
  if (addr < 0x8000) return;

  // VRAM
  if (addr < 0xA000) { vram_[addr - 0x8000] = val; return; }

  // External RAM
  if (addr < 0xC000) {
    if (!ramEnabled_) return;
    if (ramBank_ <= 3)
      sram_[(uint32_t)ramBank_ * RAM_BANK_SIZE + (addr - 0xA000)] = val;
    return;
  }

  // WRAM
  if (addr < 0xE000) { wram_[addr - 0xC000] = val; return; }

  // Echo RAM
  if (addr < 0xFE00) { wram_[addr - 0xE000] = val; return; }

  // OAM
  if (addr < 0xFEA0) { oam_[addr - 0xFE00] = val; return; }

  // Unusable
  if (addr < 0xFF00) return;

  // IO Registers
  switch (addr) {
    case 0xFF00: joypadSelect_ = val & 0x30; return;
    case 0xFF04: divReg_ = 0; divCounter_ = 0; return;
    case 0xFF05: tima_ = val; return;
    case 0xFF06: tma_ = val; return;
    case 0xFF07: tac_ = val & 0x07; return;
    case 0xFF0F: iflag_ = val & 0x1F; return;

    case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13: case 0xFF14:
    case 0xFF16: case 0xFF17: case 0xFF18: case 0xFF19:
    case 0xFF1A: case 0xFF1B: case 0xFF1C: case 0xFF1D: case 0xFF1E:
    case 0xFF20: case 0xFF21: case 0xFF22: case 0xFF23:
    case 0xFF24: case 0xFF25: case 0xFF26:
      soundRegs_[addr - 0xFF10] = val; return;
    case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
    case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
    case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
    case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
      soundRegs_[addr - 0xFF10] = val; return;

    case 0xFF40: lcdc_ = val; return;
    case 0xFF41: lcdstat_ = (lcdstat_ & 0x07) | (val & 0x78); return;
    case 0xFF42: scy_ = val; return;
    case 0xFF43: scx_ = val; return;
    case 0xFF44: return;  // LY is read-only
    case 0xFF45: lyc_ = val; return;
    case 0xFF46:  // OAM DMA
      for (int i = 0; i < 0xA0; i++)
        oam_[i] = readByte((val << 8) | i);
      return;
    case 0xFF47: bgp_ = val; return;
    case 0xFF48: obp0_ = val; return;
    case 0xFF49: obp1_ = val; return;
    case 0xFF4A: wy_ = val; return;
    case 0xFF4B: wx_ = val; return;
    case 0xFFFF: ie_ = val; return;
  }

  if (addr >= 0xFF80 && addr < 0xFFFF)
    hram_[addr - 0xFF80] = val;
}

// =========================================================================
// Timer
// =========================================================================

void SumiBoyEmulator::updateTimer(int cycles) {
  divCounter_ += cycles;
  while (divCounter_ >= 256) {
    divCounter_ -= 256;
    divReg_++;
  }

  if (!(tac_ & 0x04)) return;

  static const uint16_t freqs[] = {1024, 16, 64, 256};
  uint16_t freq = freqs[tac_ & 0x03];

  timerCounter_ += cycles;
  while (timerCounter_ >= freq) {
    timerCounter_ -= freq;
    tima_++;
    if (tima_ == 0) {
      tima_ = tma_;
      iflag_ |= 0x04;
    }
  }
}

// =========================================================================
// CPU — Full SM83 instruction set
// =========================================================================

void SumiBoyEmulator::cpu_step() {
  if (eiPending_) {
    eiPending_ = false;
    ime_ = true;
  }

  uint8_t pending = ie_ & iflag_ & 0x1F;
  if (pending) {
    halted_ = false;
    if (ime_) {
      ime_ = false;
      sp_ -= 2;
      writeByte(sp_, pc_ & 0xFF);
      writeByte(sp_ + 1, pc_ >> 8);

      if (pending & 0x01)      { iflag_ &= ~0x01; pc_ = 0x40; }
      else if (pending & 0x02) { iflag_ &= ~0x02; pc_ = 0x48; }
      else if (pending & 0x04) { iflag_ &= ~0x04; pc_ = 0x50; }
      else if (pending & 0x08) { iflag_ &= ~0x08; pc_ = 0x58; }
      else if (pending & 0x10) { iflag_ &= ~0x10; pc_ = 0x60; }
      return;
    }
  }

  if (halted_) return;

  uint8_t op = readByte(pc_++);

  if (haltBug_) {
    pc_--;
    haltBug_ = false;
  }

  switch (op) {
    case 0x00: break;
    case 0x01: c_ = readByte(pc_++); b_ = readByte(pc_++); break;
    case 0x02: writeByte((b_ << 8) | c_, a_); break;
    case 0x03: { uint16_t bc = ((b_ << 8) | c_) + 1; b_ = bc >> 8; c_ = bc & 0xFF; } break;
    case 0x04: b_++; f_ = (f_ & 0x10) | (b_ ? 0 : 0x80) | ((b_ & 0x0F) ? 0 : 0x20); break;
    case 0x05: b_--; f_ = (f_ & 0x10) | 0x40 | (b_ ? 0 : 0x80) | ((b_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x06: b_ = readByte(pc_++); break;
    case 0x07: { uint8_t c7 = a_ >> 7; a_ = (a_ << 1) | c7; f_ = c7 ? 0x10 : 0; } break;
    case 0x08: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); writeByte(addr, sp_ & 0xFF); writeByte(addr + 1, sp_ >> 8); } break;
    case 0x09: { uint16_t hl = (h_ << 8) | l_; uint16_t bc = (b_ << 8) | c_; uint32_t r = hl + bc; f_ = (f_ & 0x80) | ((r > 0xFFFF) ? 0x10 : 0) | (((hl & 0xFFF) + (bc & 0xFFF) > 0xFFF) ? 0x20 : 0); h_ = (r >> 8) & 0xFF; l_ = r & 0xFF; } break;
    case 0x0A: a_ = readByte((b_ << 8) | c_); break;
    case 0x0B: { uint16_t bc = ((b_ << 8) | c_) - 1; b_ = bc >> 8; c_ = bc & 0xFF; } break;
    case 0x0C: c_++; f_ = (f_ & 0x10) | (c_ ? 0 : 0x80) | ((c_ & 0x0F) ? 0 : 0x20); break;
    case 0x0D: c_--; f_ = (f_ & 0x10) | 0x40 | (c_ ? 0 : 0x80) | ((c_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x0E: c_ = readByte(pc_++); break;
    case 0x0F: { uint8_t c0 = a_ & 1; a_ = (a_ >> 1) | (c0 << 7); f_ = c0 ? 0x10 : 0; } break;
    case 0x10: pc_++; break;
    case 0x11: e_ = readByte(pc_++); d_ = readByte(pc_++); break;
    case 0x12: writeByte((d_ << 8) | e_, a_); break;
    case 0x13: { uint16_t de = ((d_ << 8) | e_) + 1; d_ = de >> 8; e_ = de & 0xFF; } break;
    case 0x14: d_++; f_ = (f_ & 0x10) | (d_ ? 0 : 0x80) | ((d_ & 0x0F) ? 0 : 0x20); break;
    case 0x15: d_--; f_ = (f_ & 0x10) | 0x40 | (d_ ? 0 : 0x80) | ((d_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x16: d_ = readByte(pc_++); break;
    case 0x17: { uint8_t c7 = a_ >> 7; a_ = (a_ << 1) | ((f_ >> 4) & 1); f_ = c7 ? 0x10 : 0; } break;
    case 0x18: pc_ += (int8_t)readByte(pc_) + 1; break;
    case 0x19: { uint16_t hl = (h_ << 8) | l_; uint16_t de = (d_ << 8) | e_; uint32_t r = hl + de; f_ = (f_ & 0x80) | ((r > 0xFFFF) ? 0x10 : 0) | (((hl & 0xFFF) + (de & 0xFFF) > 0xFFF) ? 0x20 : 0); h_ = (r >> 8) & 0xFF; l_ = r & 0xFF; } break;
    case 0x1A: a_ = readByte((d_ << 8) | e_); break;
    case 0x1B: { uint16_t de = ((d_ << 8) | e_) - 1; d_ = de >> 8; e_ = de & 0xFF; } break;
    case 0x1C: e_++; f_ = (f_ & 0x10) | (e_ ? 0 : 0x80) | ((e_ & 0x0F) ? 0 : 0x20); break;
    case 0x1D: e_--; f_ = (f_ & 0x10) | 0x40 | (e_ ? 0 : 0x80) | ((e_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x1E: e_ = readByte(pc_++); break;
    case 0x1F: { uint8_t c0 = a_ & 1; a_ = (a_ >> 1) | ((f_ & 0x10) << 3); f_ = c0 ? 0x10 : 0; } break;
    case 0x20: if (!(f_ & 0x80)) pc_ += (int8_t)readByte(pc_) + 1; else pc_++; break;
    case 0x21: l_ = readByte(pc_++); h_ = readByte(pc_++); break;
    case 0x22: writeByte((h_ << 8) | l_, a_); l_++; if (!l_) h_++; break;
    case 0x23: { uint16_t hl = ((h_ << 8) | l_) + 1; h_ = hl >> 8; l_ = hl & 0xFF; } break;
    case 0x24: h_++; f_ = (f_ & 0x10) | (h_ ? 0 : 0x80) | ((h_ & 0x0F) ? 0 : 0x20); break;
    case 0x25: h_--; f_ = (f_ & 0x10) | 0x40 | (h_ ? 0 : 0x80) | ((h_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x26: h_ = readByte(pc_++); break;
    case 0x27: {
      int adj = 0;
      if ((f_ & 0x20) || (!(f_ & 0x40) && (a_ & 0x0F) > 9)) adj |= 0x06;
      if ((f_ & 0x10) || (!(f_ & 0x40) && a_ > 0x99)) { adj |= 0x60; f_ |= 0x10; }
      a_ += (f_ & 0x40) ? -adj : adj;
      f_ = (f_ & 0x50) | (a_ ? 0 : 0x80);
    } break;
    case 0x28: if (f_ & 0x80) pc_ += (int8_t)readByte(pc_) + 1; else pc_++; break;
    case 0x29: { uint16_t hl = (h_ << 8) | l_; uint32_t r = hl + hl; f_ = (f_ & 0x80) | ((r > 0xFFFF) ? 0x10 : 0) | (((hl & 0xFFF) + (hl & 0xFFF) > 0xFFF) ? 0x20 : 0); h_ = (r >> 8) & 0xFF; l_ = r & 0xFF; } break;
    case 0x2A: a_ = readByte((h_ << 8) | l_); l_++; if (!l_) h_++; break;
    case 0x2B: { uint16_t hl = ((h_ << 8) | l_) - 1; h_ = hl >> 8; l_ = hl & 0xFF; } break;
    case 0x2C: l_++; f_ = (f_ & 0x10) | (l_ ? 0 : 0x80) | ((l_ & 0x0F) ? 0 : 0x20); break;
    case 0x2D: l_--; f_ = (f_ & 0x10) | 0x40 | (l_ ? 0 : 0x80) | ((l_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x2E: l_ = readByte(pc_++); break;
    case 0x2F: a_ = ~a_; f_ |= 0x60; break;
    case 0x30: if (!(f_ & 0x10)) pc_ += (int8_t)readByte(pc_) + 1; else pc_++; break;
    case 0x31: sp_ = readByte(pc_++) | (readByte(pc_++) << 8); break;
    case 0x32: writeByte((h_ << 8) | l_, a_); l_--; if (l_ == 0xFF) h_--; break;
    case 0x33: sp_++; break;
    case 0x34: { uint16_t addr = (h_ << 8) | l_; uint8_t v = readByte(addr) + 1; writeByte(addr, v); f_ = (f_ & 0x10) | (v ? 0 : 0x80) | ((v & 0x0F) ? 0 : 0x20); } break;
    case 0x35: { uint16_t addr = (h_ << 8) | l_; uint8_t v = readByte(addr) - 1; writeByte(addr, v); f_ = (f_ & 0x10) | 0x40 | (v ? 0 : 0x80) | ((v & 0x0F) == 0x0F ? 0x20 : 0); } break;
    case 0x36: writeByte((h_ << 8) | l_, readByte(pc_++)); break;
    case 0x37: f_ = (f_ & 0x80) | 0x10; break;
    case 0x38: if (f_ & 0x10) pc_ += (int8_t)readByte(pc_) + 1; else pc_++; break;
    case 0x39: { uint16_t hl = (h_ << 8) | l_; uint32_t r = hl + sp_; f_ = (f_ & 0x80) | ((r > 0xFFFF) ? 0x10 : 0) | (((hl & 0xFFF) + (sp_ & 0xFFF) > 0xFFF) ? 0x20 : 0); h_ = (r >> 8) & 0xFF; l_ = r & 0xFF; } break;
    case 0x3A: a_ = readByte((h_ << 8) | l_); l_--; if (l_ == 0xFF) h_--; break;
    case 0x3B: sp_--; break;
    case 0x3C: a_++; f_ = (f_ & 0x10) | (a_ ? 0 : 0x80) | ((a_ & 0x0F) ? 0 : 0x20); break;
    case 0x3D: a_--; f_ = (f_ & 0x10) | 0x40 | (a_ ? 0 : 0x80) | ((a_ & 0x0F) == 0x0F ? 0x20 : 0); break;
    case 0x3E: a_ = readByte(pc_++); break;
    case 0x3F: f_ = (f_ & 0x80) | ((f_ & 0x10) ? 0 : 0x10); break;

    // LD r,r block (0x40-0x7F)
    case 0x40: break; case 0x41: b_ = c_; break; case 0x42: b_ = d_; break; case 0x43: b_ = e_; break;
    case 0x44: b_ = h_; break; case 0x45: b_ = l_; break; case 0x46: b_ = readByte((h_ << 8) | l_); break; case 0x47: b_ = a_; break;
    case 0x48: c_ = b_; break; case 0x49: break; case 0x4A: c_ = d_; break; case 0x4B: c_ = e_; break;
    case 0x4C: c_ = h_; break; case 0x4D: c_ = l_; break; case 0x4E: c_ = readByte((h_ << 8) | l_); break; case 0x4F: c_ = a_; break;
    case 0x50: d_ = b_; break; case 0x51: d_ = c_; break; case 0x52: break; case 0x53: d_ = e_; break;
    case 0x54: d_ = h_; break; case 0x55: d_ = l_; break; case 0x56: d_ = readByte((h_ << 8) | l_); break; case 0x57: d_ = a_; break;
    case 0x58: e_ = b_; break; case 0x59: e_ = c_; break; case 0x5A: e_ = d_; break; case 0x5B: break;
    case 0x5C: e_ = h_; break; case 0x5D: e_ = l_; break; case 0x5E: e_ = readByte((h_ << 8) | l_); break; case 0x5F: e_ = a_; break;
    case 0x60: h_ = b_; break; case 0x61: h_ = c_; break; case 0x62: h_ = d_; break; case 0x63: h_ = e_; break;
    case 0x64: break; case 0x65: h_ = l_; break; case 0x66: h_ = readByte((h_ << 8) | l_); break; case 0x67: h_ = a_; break;
    case 0x68: l_ = b_; break; case 0x69: l_ = c_; break; case 0x6A: l_ = d_; break; case 0x6B: l_ = e_; break;
    case 0x6C: l_ = h_; break; case 0x6D: break; case 0x6E: l_ = readByte((h_ << 8) | l_); break; case 0x6F: l_ = a_; break;
    case 0x70: writeByte((h_ << 8) | l_, b_); break; case 0x71: writeByte((h_ << 8) | l_, c_); break;
    case 0x72: writeByte((h_ << 8) | l_, d_); break; case 0x73: writeByte((h_ << 8) | l_, e_); break;
    case 0x74: writeByte((h_ << 8) | l_, h_); break; case 0x75: writeByte((h_ << 8) | l_, l_); break;
    case 0x76:  // HALT
      if (ime_) {
        halted_ = true;
      } else if (ie_ & iflag_ & 0x1F) {
        haltBug_ = true;
      } else {
        halted_ = true;
      }
      break;
    case 0x77: writeByte((h_ << 8) | l_, a_); break;
    case 0x78: a_ = b_; break; case 0x79: a_ = c_; break; case 0x7A: a_ = d_; break; case 0x7B: a_ = e_; break;
    case 0x7C: a_ = h_; break; case 0x7D: a_ = l_; break; case 0x7E: a_ = readByte((h_ << 8) | l_); break; case 0x7F: break;

    // ALU (0x80-0xBF)
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      uint16_t r = a_ + v;
      f_ = ((r & 0xFF)?0:0x80) | ((r>0xFF)?0x10:0) | (((a_&0xF)+(v&0xF)>0xF)?0x20:0);
      a_ = r;
    } break;
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      uint8_t cy = (f_>>4)&1; uint16_t r = a_+v+cy;
      f_ = ((r&0xFF)?0:0x80) | ((r>0xFF)?0x10:0) | (((a_&0xF)+(v&0xF)+cy>0xF)?0x20:0);
      a_ = r;
    } break;
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      uint8_t r = a_ - v;
      f_ = (r?0:0x80) | 0x40 | ((a_<v)?0x10:0) | (((a_&0xF)<(v&0xF))?0x20:0);
      a_ = r;
    } break;
    case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      uint8_t cy = (f_>>4)&1; uint16_t sum = v+cy;
      f_ = (((uint8_t)(a_-sum))?0:0x80) | 0x40 | ((a_<sum)?0x10:0) | (((a_&0xF)<(v&0xF)+cy)?0x20:0);
      a_ -= sum;
    } break;
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      a_ &= v; f_ = (a_?0:0x80) | 0x20;
    } break;
    case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      a_ ^= v; f_ = a_?0:0x80;
    } break;
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      a_ |= v; f_ = a_?0:0x80;
    } break;
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
      uint8_t v; switch(op&7) { case 0:v=b_;break;case 1:v=c_;break;case 2:v=d_;break;case 3:v=e_;break;case 4:v=h_;break;case 5:v=l_;break;case 6:v=readByte((h_<<8)|l_);break;default:v=a_; }
      f_ = ((a_==v)?0x80:0) | 0x40 | ((a_<v)?0x10:0) | (((a_&0xF)<(v&0xF))?0x20:0);
    } break;

    // Control flow (0xC0-0xFF)
    case 0xC0: if (!(f_ & 0x80)) { pc_ = readByte(sp_) | (readByte(sp_+1) << 8); sp_ += 2; } break;
    case 0xC1: c_ = readByte(sp_); b_ = readByte(sp_+1); sp_ += 2; break;
    case 0xC2: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (!(f_ & 0x80)) pc_ = addr; } break;
    case 0xC3: pc_ = readByte(pc_) | (readByte(pc_+1) << 8); break;
    case 0xC4: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (!(f_ & 0x80)) { sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = addr; } } break;
    case 0xC5: sp_ -= 2; writeByte(sp_, c_); writeByte(sp_+1, b_); break;
    case 0xC6: { uint8_t v = readByte(pc_++); uint16_t r = a_ + v; f_ = ((r&0xFF)?0:0x80) | ((r>0xFF)?0x10:0) | (((a_&0xF)+(v&0xF)>0xF)?0x20:0); a_ = r; } break;
    case 0xC7: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x00; break;
    case 0xC8: if (f_ & 0x80) { pc_ = readByte(sp_) | (readByte(sp_+1) << 8); sp_ += 2; } break;
    case 0xC9: pc_ = readByte(sp_) | (readByte(sp_+1) << 8); sp_ += 2; break;
    case 0xCA: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (f_ & 0x80) pc_ = addr; } break;
    case 0xCB: {
      uint8_t cb = readByte(pc_++);
      uint8_t idx = cb & 0x07;
      uint8_t val; switch(idx) { case 0:val=b_;break;case 1:val=c_;break;case 2:val=d_;break;case 3:val=e_;break;case 4:val=h_;break;case 5:val=l_;break;case 6:val=readByte((h_<<8)|l_);break;default:val=a_; }
      if (cb < 0x40) {
        uint8_t result;
        switch ((cb >> 3) & 0x07) {
          case 0: { uint8_t c7=val>>7; result=(val<<1)|c7; f_=(result?0:0x80)|(c7?0x10:0); } break;
          case 1: { uint8_t c0=val&1; result=(val>>1)|(c0<<7); f_=(result?0:0x80)|(c0?0x10:0); } break;
          case 2: { uint8_t c7=val>>7; result=(val<<1)|((f_>>4)&1); f_=(result?0:0x80)|(c7?0x10:0); } break;
          case 3: { uint8_t c0=val&1; result=(val>>1)|((f_&0x10)<<3); f_=(result?0:0x80)|(c0?0x10:0); } break;
          case 4: { uint8_t c7=val>>7; result=val<<1; f_=(result?0:0x80)|(c7?0x10:0); } break;
          case 5: { uint8_t c0=val&1; result=(val>>1)|(val&0x80); f_=(result?0:0x80)|(c0?0x10:0); } break;
          case 6: result=((val&0x0F)<<4)|((val>>4)&0x0F); f_=result?0:0x80; break;
          default: { uint8_t c0=val&1; result=val>>1; f_=(result?0:0x80)|(c0?0x10:0); } break;
        }
        switch(idx) { case 0:b_=result;break;case 1:c_=result;break;case 2:d_=result;break;case 3:e_=result;break;case 4:h_=result;break;case 5:l_=result;break;case 6:writeByte((h_<<8)|l_,result);break;default:a_=result; }
      } else if (cb < 0x80) {
        uint8_t bit = (cb >> 3) & 0x07;
        f_ = (f_ & 0x10) | 0x20 | ((val & (1 << bit)) ? 0 : 0x80);
      } else if (cb < 0xC0) {
        uint8_t bit = (cb >> 3) & 0x07;
        val &= ~(1 << bit);
        switch(idx) { case 0:b_=val;break;case 1:c_=val;break;case 2:d_=val;break;case 3:e_=val;break;case 4:h_=val;break;case 5:l_=val;break;case 6:writeByte((h_<<8)|l_,val);break;default:a_=val; }
      } else {
        uint8_t bit = (cb >> 3) & 0x07;
        val |= (1 << bit);
        switch(idx) { case 0:b_=val;break;case 1:c_=val;break;case 2:d_=val;break;case 3:e_=val;break;case 4:h_=val;break;case 5:l_=val;break;case 6:writeByte((h_<<8)|l_,val);break;default:a_=val; }
      }
    } break;
    case 0xCC: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (f_ & 0x80) { sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = addr; } } break;
    case 0xCD: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = addr; } break;
    case 0xCE: { uint8_t v = readByte(pc_++); uint8_t cy=(f_>>4)&1; uint16_t r=a_+v+cy; f_=((r&0xFF)?0:0x80)|((r>0xFF)?0x10:0)|(((a_&0xF)+(v&0xF)+cy>0xF)?0x20:0); a_=r; } break;
    case 0xCF: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x08; break;
    case 0xD0: if (!(f_ & 0x10)) { pc_ = readByte(sp_) | (readByte(sp_+1) << 8); sp_ += 2; } break;
    case 0xD1: e_ = readByte(sp_); d_ = readByte(sp_+1); sp_ += 2; break;
    case 0xD2: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (!(f_ & 0x10)) pc_ = addr; } break;
    case 0xD4: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (!(f_ & 0x10)) { sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = addr; } } break;
    case 0xD5: sp_ -= 2; writeByte(sp_, e_); writeByte(sp_+1, d_); break;
    case 0xD6: { uint8_t v = readByte(pc_++); f_ = ((a_==v)?0x80:0) | 0x40 | ((a_<v)?0x10:0) | (((a_&0xF)<(v&0xF))?0x20:0); a_ -= v; } break;
    case 0xD7: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x10; break;
    case 0xD8: if (f_ & 0x10) { pc_ = readByte(sp_) | (readByte(sp_+1) << 8); sp_ += 2; } break;
    case 0xD9: eiPending_ = true; pc_ = readByte(sp_) | (readByte(sp_+1) << 8); sp_ += 2; break;
    case 0xDA: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (f_ & 0x10) pc_ = addr; } break;
    case 0xDC: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); if (f_ & 0x10) { sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = addr; } } break;
    case 0xDE: { uint8_t v = readByte(pc_++); uint8_t cy=(f_>>4)&1; uint16_t sum=v+cy; f_=(((uint8_t)(a_-sum))?0:0x80)|0x40|((a_<sum)?0x10:0)|(((a_&0xF)<(v&0xF)+cy)?0x20:0); a_-=sum; } break;
    case 0xDF: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x18; break;
    case 0xE0: writeByte(0xFF00 + readByte(pc_++), a_); break;
    case 0xE1: l_ = readByte(sp_); h_ = readByte(sp_+1); sp_ += 2; break;
    case 0xE2: writeByte(0xFF00 + c_, a_); break;
    case 0xE5: sp_ -= 2; writeByte(sp_, l_); writeByte(sp_+1, h_); break;
    case 0xE6: { uint8_t v = readByte(pc_++); a_ &= v; f_ = (a_?0:0x80)|0x20; } break;
    case 0xE7: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x20; break;
    case 0xE8: { int8_t n = (int8_t)readByte(pc_++); f_ = (((sp_&0xFF)+(n&0xFF)>0xFF)?0x10:0)|(((sp_&0xF)+(n&0xF)>0xF)?0x20:0); sp_ += n; } break;
    case 0xE9: pc_ = (h_ << 8) | l_; break;
    case 0xEA: { uint16_t addr = readByte(pc_++) | (readByte(pc_++) << 8); writeByte(addr, a_); } break;
    case 0xEE: a_ ^= readByte(pc_++); f_ = a_?0:0x80; break;
    case 0xEF: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x28; break;
    case 0xF0: a_ = readByte(0xFF00 + readByte(pc_++)); break;
    case 0xF1: f_ = readByte(sp_) & 0xF0; a_ = readByte(sp_+1); sp_ += 2; break;
    case 0xF2: a_ = readByte(0xFF00 + c_); break;
    case 0xF3: ime_ = false; break;
    case 0xF5: sp_ -= 2; writeByte(sp_, f_); writeByte(sp_+1, a_); break;
    case 0xF6: a_ |= readByte(pc_++); f_ = a_?0:0x80; break;
    case 0xF7: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x30; break;
    case 0xF8: { int8_t n = (int8_t)readByte(pc_++); f_ = (((sp_&0xFF)+(n&0xFF)>0xFF)?0x10:0)|(((sp_&0xF)+(n&0xF)>0xF)?0x20:0); uint16_t r = sp_+n; l_ = r&0xFF; h_ = r>>8; } break;
    case 0xF9: sp_ = (h_ << 8) | l_; break;
    case 0xFA: a_ = readByte(readByte(pc_++) | (readByte(pc_++) << 8)); break;
    case 0xFB: eiPending_ = true; break;
    case 0xFE: { uint8_t v = readByte(pc_++); f_ = ((a_==v)?0x80:0) | 0x40 | ((a_<v)?0x10:0) | (((a_&0xF)<(v&0xF))?0x20:0); } break;
    case 0xFF: sp_ -= 2; writeByte(sp_, pc_ & 0xFF); writeByte(sp_+1, pc_ >> 8); pc_ = 0x38; break;
    default: break;
  }
}

// =========================================================================
// PPU — Background + Window + Sprites
// =========================================================================

void SumiBoyEmulator::renderLine(int line) {
  if (!(lcdc_ & 0x80)) return;

  uint8_t* row = &framebuffer_[line * GB_W];
  uint8_t bgPriority[GB_W];
  memset(bgPriority, 0, GB_W);

  // --- BACKGROUND ---
  if (lcdc_ & 0x01) {
    uint16_t mapBase = (lcdc_ & 0x08) ? 0x1C00 : 0x1800;
    uint16_t dataBase = (lcdc_ & 0x10) ? 0x0000 : 0x0800;
    bool signedIdx = !(lcdc_ & 0x10);
    int y = (line + scy_) & 0xFF;

    for (int x = 0; x < GB_W; x++) {
      int sx = (x + scx_) & 0xFF;
      uint8_t tileIdx = vram_[mapBase + (y / 8) * 32 + sx / 8];

      uint16_t tileAddr;
      if (signedIdx) tileAddr = dataBase + ((int8_t)tileIdx + 128) * 16;
      else tileAddr = dataBase + tileIdx * 16;

      uint8_t lo = vram_[tileAddr + (y % 8) * 2];
      uint8_t hi = vram_[tileAddr + (y % 8) * 2 + 1];
      uint8_t bit = 7 - (sx % 8);
      uint8_t colorIdx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

      bgPriority[x] = colorIdx;
      row[x] = applyPalette(bgp_, colorIdx);
    }
  } else {
    memset(row, 0, GB_W);
  }

  // --- WINDOW ---
  if ((lcdc_ & 0x20) && (lcdc_ & 0x01) && line >= wy_ && wx_ <= 166) {
    uint16_t winMap = (lcdc_ & 0x40) ? 0x1C00 : 0x1800;
    uint16_t dataBase = (lcdc_ & 0x10) ? 0x0000 : 0x0800;
    bool signedIdx = !(lcdc_ & 0x10);
    int winY = line - wy_;
    int winStartX = wx_ - 7;

    for (int x = (winStartX < 0 ? 0 : winStartX); x < GB_W; x++) {
      int winX = x - winStartX;
      uint8_t tileIdx = vram_[winMap + (winY / 8) * 32 + winX / 8];

      uint16_t tileAddr;
      if (signedIdx) tileAddr = dataBase + ((int8_t)tileIdx + 128) * 16;
      else tileAddr = dataBase + tileIdx * 16;

      uint8_t lo = vram_[tileAddr + (winY % 8) * 2];
      uint8_t hi = vram_[tileAddr + (winY % 8) * 2 + 1];
      uint8_t bit = 7 - (winX % 8);
      uint8_t colorIdx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

      bgPriority[x] = colorIdx;
      row[x] = applyPalette(bgp_, colorIdx);
    }
  }

  // --- SPRITES ---
  if (lcdc_ & 0x02) {
    int spriteHeight = (lcdc_ & 0x04) ? 16 : 8;
    int spritesDrawn = 0;

    for (int i = 0; i < 40 && spritesDrawn < 10; i++) {
      int sy = oam_[i * 4] - 16;
      int sx = oam_[i * 4 + 1] - 8;
      uint8_t tile = oam_[i * 4 + 2];
      uint8_t flags = oam_[i * 4 + 3];

      if (line < sy || line >= sy + spriteHeight) continue;
      spritesDrawn++;

      bool flipX = flags & 0x20;
      bool flipY = flags & 0x40;
      bool bgOver = flags & 0x80;
      uint8_t pal = (flags & 0x10) ? obp1_ : obp0_;

      int tileY = line - sy;
      if (flipY) tileY = spriteHeight - 1 - tileY;

      uint8_t useTile = tile;
      if (spriteHeight == 16) {
        useTile = (tile & 0xFE) + (tileY >= 8 ? 1 : 0);
        tileY &= 7;
      }

      uint16_t tileAddr = useTile * 16 + tileY * 2;
      uint8_t lo = vram_[tileAddr];
      uint8_t hi = vram_[tileAddr + 1];

      for (int px = 0; px < 8; px++) {
        int screenX = sx + px;
        if (screenX < 0 || screenX >= GB_W) continue;

        uint8_t bit = flipX ? px : (7 - px);
        uint8_t colorIdx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

        if (colorIdx == 0) continue;
        if (bgOver && bgPriority[screenX] != 0) continue;

        row[screenX] = applyPalette(pal, colorIdx);
      }
    }
  }
}

void SumiBoyEmulator::runScanline() {
  for (int i = 0; i < 114; i++) {
    cpu_step();
    updateTimer(4);
  }
}

void SumiBoyEmulator::runFrame() {
  for (ly_ = 0; ly_ < 154; ly_++) {
    if (ly_ < 144) renderLine(ly_);
    if (ly_ == 144) iflag_ |= 0x01;
    if (ly_ == lyc_ && (lcdstat_ & 0x40))
      iflag_ |= 0x02;
    runScanline();
  }
}

// =========================================================================
// Dither LUT & Display rendering
// =========================================================================

void SumiBoyEmulator::initDitherLUT() {
  for (int shade = 0; shade < 4; shade++) {
    for (int bc = 0; bc < 4; bc++) {
      uint8_t val = 0xFF;
      for (int b = 0; b < 8; b++) {
        if (shadeIntensity[shade] > bayer4x4[b & 3][bc]) {
          val &= ~(0x80 >> b);
        }
      }
      shade_byte_[shade][bc] = val;
    }
  }
}

void SumiBoyEmulator::renderToDisplay() {
  // 3x scale: 160x144 → 480x432 (portrait mode: 480x800)
  // Direct framebuffer write — bypasses drawPixel() for major speedup.
  //
  // Portrait coordinate mapping (logical → physical):
  //   physX = logicalY              → physical row in buffer
  //   physY = 479 - logicalX        → physical column (bit position)
  //
  // Physical buffer: 800 cols × 480 rows, 100 bytes/row, MSB-first.
  // E-ink convention: 0 = black, 1 = white.

  uint8_t* buf = disp_.gfx().getFrameBuffer();
  if (!buf) {
    // Fallback: use drawPixel if no direct buffer access
    GfxRenderer& gfx = disp_.gfx();
    for (int gy = 0; gy < GB_H; gy++) {
      for (int gx = 0; gx < GB_W; gx++) {
        uint8_t shade = framebuffer_[gy * GB_W + gx];
        uint8_t intensity = shadeIntensity[shade];
        int baseX = gx * 3;
        int baseY = gy * 3;
        for (int dy = 0; dy < 3; dy++) {
          int ey = baseY + dy;
          uint8_t by = ey & 3;
          for (int dx = 0; dx < 3; dx++) {
            int ex = baseX + dx;
            gfx.drawPixel(ex, ey, intensity > bayer4x4[by][ex & 3]);
          }
        }
      }
    }
    return;
  }

  // Direct buffer rendering — inline portrait coordinate transform.
  //
  // GfxRenderer portrait transform (from drawPixel):
  //   rotatedX = logicalY             (range 0..799)
  //   rotatedY = 479 - logicalX       (range 0..479)
  //   byteIndex = rotatedY * 100 + rotatedX / 8
  //   bitPos    = 7 - (rotatedX & 7)
  //
  // For game pixel at logical (gx*3+dx, gy*3+dy):
  //   rotatedX = gy*3 + dy
  //   rotatedY = 479 - (gx*3 + dx)

  static constexpr int BUF_STRIDE = 100;  // DISPLAY_WIDTH/8 = 800/8

  for (int gy = 0; gy < GB_H; gy++) {
    for (int gx = 0; gx < GB_W; gx++) {
      uint8_t shade = framebuffer_[gy * GB_W + gx];
      uint8_t intensity = shadeIntensity[shade];

      // Bayer dithering uses logical coordinates
      int logBaseX = gx * 3;
      int logBaseY = gy * 3;

      for (int dy = 0; dy < 3; dy++) {
        int rotX = logBaseY + dy;             // rotatedX = logicalY
        int bayerRow = (logBaseY + dy) & 3;

        for (int dx = 0; dx < 3; dx++) {
          int rotY = 479 - (logBaseX + dx);   // rotatedY = 479 - logicalX
          int bayerCol = (logBaseX + dx) & 3;

          int byteIdx = rotY * BUF_STRIDE + (rotX >> 3);
          uint8_t bitMask = 0x80 >> (rotX & 7);

          if (intensity > bayer4x4[bayerRow][bayerCol]) {
            buf[byteIdx] &= ~bitMask;  // Black (0)
          } else {
            buf[byteIdx] |= bitMask;   // White (1)
          }
        }
      }
    }
  }
}

void SumiBoyEmulator::drawControls() {
  // Programmatic controls overlay below the game area.
  // Area: 480 wide × 368 tall, starting at y = DISP_GAME_H (432).
  // Inspired by the DALL-E gamepad mockup — clean black/white for e-ink.
  GfxRenderer& gfx = disp_.gfx();
  int font = disp_.fontId();
  int topY = DISP_GAME_H;
  int W = screenW_;  // 480

  // ═══ Separator ═══
  gfx.drawLine(0, topY, W - 1, topY, true);
  gfx.drawLine(0, topY + 2, W - 1, topY + 2, true);

  // ═══ D-PAD (left side) ═══
  // Classic cross shape — 3 squares in a plus pattern
  int dCX = 105, dCY = topY + 120;   // Center of d-pad
  int arm = 40;                       // Arm width & height
  int thick = 3;                      // Border thickness

  // Draw the cross shape as 3 filled rects (vertical bar + horizontal bar)
  // Vertical bar
  gfx.fillRect(dCX - arm/2, dCY - arm - arm/2, arm, arm * 3, true);
  // Horizontal bar
  gfx.fillRect(dCX - arm - arm/2, dCY - arm/2, arm * 3, arm, true);

  // White inner area — makes it look like a raised cross
  int inset = thick;
  // Vertical inner
  gfx.fillRect(dCX - arm/2 + inset, dCY - arm - arm/2 + inset,
               arm - inset*2, arm * 3 - inset*2, false);
  // Horizontal inner
  gfx.fillRect(dCX - arm - arm/2 + inset, dCY - arm/2 + inset,
               arm * 3 - inset*2, arm - inset*2, false);

  // Re-draw inner dividing lines to separate the 4 directions + center
  // Horizontal dividers
  gfx.drawLine(dCX - arm/2 + inset, dCY - arm/2 + inset,
               dCX + arm/2 - inset, dCY - arm/2 + inset, true);
  gfx.drawLine(dCX - arm/2 + inset, dCY + arm/2 - inset,
               dCX + arm/2 - inset, dCY + arm/2 - inset, true);
  // Vertical dividers
  gfx.drawLine(dCX - arm/2 + inset, dCY - arm/2 + inset,
               dCX - arm/2 + inset, dCY + arm/2 - inset, true);
  gfx.drawLine(dCX + arm/2 - inset, dCY - arm/2 + inset,
               dCX + arm/2 - inset, dCY + arm/2 - inset, true);

  // Arrow triangles (black on white inner area)
  // Up arrow
  for (int i = 0; i < 10; i++)
    gfx.drawLine(dCX - i, dCY - arm - 8 + i, dCX + i, dCY - arm - 8 + i, true);
  // Down arrow
  for (int i = 0; i < 10; i++)
    gfx.drawLine(dCX - i, dCY + arm + 8 - i, dCX + i, dCY + arm + 8 - i, true);
  // Left arrow
  for (int i = 0; i < 10; i++)
    gfx.drawLine(dCX - arm - 8 + i, dCY - i, dCX - arm - 8 + i, dCY + i, true);
  // Right arrow
  for (int i = 0; i < 10; i++)
    gfx.drawLine(dCX + arm + 8 - i, dCY - i, dCX + arm + 8 - i, dCY + i, true);

  // ═══ A and B BUTTONS (right side, Game Boy diagonal layout) ═══
  int btnR = 28;  // Button radius

  // B button — lower-left of the pair (outline)
  int bCX = 310, bCY = dCY - 5;
  disp_.drawCircle(bCX, bCY, btnR, true);
  disp_.drawCircle(bCX, bCY, btnR - 1, true);
  disp_.drawCircle(bCX, bCY, btnR - 2, true);
  gfx.drawText(font, bCX - 6, bCY - 12, "B", true);

  // A button — upper-right of the pair (filled)
  int aCX = 390, aCY = dCY - 45;
  disp_.fillCircle(aCX, aCY, btnR, true);
  gfx.drawText(font, aCX - 6, aCY - 12, "A", false);

  // Key labels under buttons
  gfx.drawText(font, bCX - 20, bCY + btnR + 8, "Back", true);
  gfx.drawText(font, aCX - 10, aCY + btnR + 8, "OK", true);

  // ═══ START button (pill shape) ═══
  int sy = dCY + 70, sx = 295, sw = 150, sh = 30;
  // Pill: filled rect + half-circles on ends
  gfx.fillRect(sx + sh/2, sy, sw - sh, sh, true);
  disp_.fillCircle(sx + sh/2, sy + sh/2, sh/2, true);
  disp_.fillCircle(sx + sw - sh/2, sy + sh/2, sh/2, true);
  // "START" label centered in white
  gfx.drawText(font, sx + sw/2 - 28, sy + 4, "START", false);
  // Sub-label
  gfx.drawText(font, sx + sw/2 - 40, sy + sh + 8, "Power btn", true);

  // ═══ KEY REFERENCE (bottom section) ═══
  int refY = topY + 260;
  // Thin separator
  gfx.drawLine(20, refY - 8, W - 20, refY - 8, true);

  // Two-column layout
  int col1 = 25, col2 = 250;

  gfx.drawText(font, col1, refY,      "OK     = A button", true);
  gfx.drawText(font, col1, refY + 26, "Back   = B button", true);
  gfx.drawText(font, col1, refY + 52, "Power  = Start", true);

  gfx.drawText(font, col2, refY,      "2x OK    = Select", true);
  gfx.drawText(font, col2, refY + 26, "Hold Back  = Exit", true);
  gfx.drawText(font, col2, refY + 52, "Hold Power = Sleep", true);
}

// =========================================================================
// Plugin interface: draw / handleInput / update
// =========================================================================

void SumiBoyEmulator::draw() {
  if (!framebuffer_) return;

  // Full screen draw: clear everything, render game + controls
  disp_.gfx().clearScreen(0xFF);
  renderToDisplay();
  drawControls();

  if (firstDraw_) {
    disp_.gfx().displayBuffer(EInkDisplay::FULL_REFRESH);
    firstDraw_ = false;
  } else {
    disp_.gfx().displayBuffer(EInkDisplay::FAST_REFRESH);
  }
}

bool SumiBoyEmulator::handleInput(PluginButton btn) {
  if (!framebuffer_) return false;

  // Button mapping:
  //   D-pad         → GB D-pad
  //   Center        → GB A
  //   Back          → GB B       (triple-Back = force exit, via host)
  //   Power (tap)   → GB Start   (consumed; long-press Power = sleep, via host)
  //   Center+Back   → GB Select  (double-tap Center within 400ms)

  switch (btn) {
    case PluginButton::Right:
      pendingDpad_ &= ~0x01; pendingInput_ = true; return true;
    case PluginButton::Left:
      pendingDpad_ &= ~0x02; pendingInput_ = true; return true;
    case PluginButton::Up:
      pendingDpad_ &= ~0x04; pendingInput_ = true; return true;
    case PluginButton::Down:
      pendingDpad_ &= ~0x08; pendingInput_ = true; return true;

    case PluginButton::Center: {
      unsigned long now = millis();
      if (now - lastCenterMs_ < DOUBLE_TAP_MS) {
        // Double-tap Center = Select
        pendingButtons_ &= ~0x04;
        lastCenterMs_ = 0;
      } else {
        // Single tap = A
        pendingButtons_ &= ~0x01;
        lastCenterMs_ = now;
      }
      pendingInput_ = true;
      return true;
    }

    case PluginButton::Back:
      // GB B button — host still counts for triple-back exit
      pendingButtons_ &= ~0x02;
      pendingInput_ = true;
      return true;

    case PluginButton::Power:
      // GB Start — open in-game menu, save, etc.
      // Consumed here so host doesn't go to sleep.
      // Long-press Power still triggers sleep (host intercepts before us).
      pendingButtons_ &= ~0x08;
      pendingInput_ = true;
      return true;

    default: return false;
  }
}

bool SumiBoyEmulator::handleRelease(PluginButton btn) {
  if (!framebuffer_) return false;

  // Release the corresponding GB button — mirrors handleInput() mapping
  switch (btn) {
    case PluginButton::Right:  pendingDpad_ |= 0x01; return true;
    case PluginButton::Left:   pendingDpad_ |= 0x02; return true;
    case PluginButton::Up:     pendingDpad_ |= 0x04; return true;
    case PluginButton::Down:   pendingDpad_ |= 0x08; return true;
    case PluginButton::Center:
      // Release both A and Select (whichever was active)
      pendingButtons_ |= 0x05;  // bits 0 (A) and 2 (Select)
      return true;
    case PluginButton::Back:
      pendingButtons_ |= 0x02;  // B
      return true;
    case PluginButton::Power:
      pendingButtons_ |= 0x08;  // Start
      return true;
    default: return false;
  }
}

bool SumiBoyEmulator::update() {
  if (!framebuffer_) return false;

  // Adaptive frame count
  int framesToRun = (consecutiveSkips_ >= 5) ? FRAMES_PER_REFRESH_IDLE : FRAMES_PER_REFRESH;

  // Apply held button state (set by handleInput, cleared by handleRelease).
  // Buttons stay held across update ticks — just like the old standalone emulator
  // polled physical buttons every frame.
  joyDpad_ = pendingDpad_;
  joyButtons_ = pendingButtons_;

  if (pendingInput_) {
    iflag_ |= 0x10;  // Joypad interrupt
    buttonPressedSinceSave_ = true;
    pendingInput_ = false;
  }

  // Run N game frames
  for (int i = 0; i < framesToRun; i++) {
    runFrame();
    frameCount_++;
  }

  // Auto-save
  saveCounter_++;
  if (saveCounter_ >= AUTOSAVE_INTERVAL && buttonPressedSinceSave_) {
    saveState();
    saveCounter_ = 0;
    buttonPressedSinceSave_ = false;
  } else if (saveCounter_ >= AUTOSAVE_INTERVAL) {
    saveCounter_ = 0;
  }

  sramCounter_++;
  if (sramCounter_ >= SRAM_SAVE_INTERVAL && buttonPressedSinceSave_) {
    saveSRAM();
    sramCounter_ = 0;
  } else if (sramCounter_ >= SRAM_SAVE_INTERVAL) {
    sramCounter_ = 0;
  }

  // Dirty detection — FNV-1a hash
  uint32_t hash = 2166136261u;
  const uint8_t* fb = framebuffer_;
  for (int i = 0; i < GB_W * GB_H; i += 4) {
    hash ^= *(uint32_t*)(fb + i);
    hash *= 16777619u;
  }

  if (hash != lastFrameHash_) {
    consecutiveSkips_ = 0;
    // Skip rendering if display is still refreshing from previous frame
    // (matches old standalone emulator behavior — avoids blocking on waitForRefresh)
    if (!disp_.gfx().isRefreshing()) {
      lastFrameHash_ = hash;
      if (firstDraw_) {
        // First frame: clear screen, draw game + controls, full refresh
        disp_.gfx().clearScreen(0xFF);
        renderToDisplay();
        drawControls();
        disp_.gfx().displayBuffer(EInkDisplay::FULL_REFRESH);
        firstDraw_ = false;
      } else {
        // Subsequent frames: only update game area (controls persist)
        renderToDisplay();
        disp_.gfx().displayBuffer(EInkDisplay::FAST_REFRESH);
      }
    }
    return true;
  } else {
    consecutiveSkips_++;
    return false;
  }
}

// =========================================================================
// Save / Load
// =========================================================================

#define SAVE_MAGIC 0x53554D49  // "SUMI"

struct SaveHeader {
  uint32_t magic;
  uint16_t pc, sp;
  uint8_t a, f, b, c, d, e, h, l;
  uint8_t ime, halted, haltBug, eiPending;
  uint8_t ie, iflag;
  uint8_t lcdc, lcdstat, scy, scx, ly, lyc;
  uint8_t bgp, obp0, obp1, wy, wx;
  uint8_t tima, tma, tac, divReg;
  uint16_t timerCounter, divCounter;
  uint8_t romBank, ramBank, ramEnabled;
  uint8_t joypadSelect;
  uint8_t soundRegs[0x30];
  uint32_t frameCount;
};

bool SumiBoyEmulator::saveState() {
  // Ensure /games/saves/ directory exists
  LittleFS.mkdir("/games");
  LittleFS.mkdir("/games/saves");

  File f = LittleFS.open(statePath_, FILE_WRITE);
  if (!f) { Serial.printf("[SumiBoy] Save failed: %s\n", statePath_); return false; }

  unsigned long t = millis();

  SaveHeader hdr = {};
  hdr.magic = SAVE_MAGIC;
  hdr.pc = pc_; hdr.sp = sp_;
  hdr.a = a_; hdr.f = f_; hdr.b = b_; hdr.c = c_; hdr.d = d_; hdr.e = e_; hdr.h = h_; hdr.l = l_;
  hdr.ime = ime_; hdr.halted = halted_; hdr.haltBug = haltBug_; hdr.eiPending = eiPending_;
  hdr.ie = ie_; hdr.iflag = iflag_;
  hdr.lcdc = lcdc_; hdr.lcdstat = lcdstat_; hdr.scy = scy_; hdr.scx = scx_;
  hdr.ly = ly_; hdr.lyc = lyc_; hdr.bgp = bgp_; hdr.obp0 = obp0_; hdr.obp1 = obp1_;
  hdr.wy = wy_; hdr.wx = wx_;
  hdr.tima = tima_; hdr.tma = tma_; hdr.tac = tac_; hdr.divReg = divReg_;
  hdr.timerCounter = timerCounter_; hdr.divCounter = divCounter_;
  hdr.romBank = romBank_; hdr.ramBank = ramBank_; hdr.ramEnabled = ramEnabled_;
  hdr.joypadSelect = joypadSelect_;
  memcpy(hdr.soundRegs, soundRegs_, sizeof(soundRegs_));
  hdr.frameCount = frameCount_;

  f.write((uint8_t*)&hdr, sizeof(hdr));
  f.write(vram_, 0x2000);
  f.write(wram_, 0x2000);
  f.write(oam_, 0xA0);
  f.write(hram_, 0x80);
  f.write(sram_, 0x8000);
  f.close();

  Serial.printf("[SumiBoy] State saved in %lums (F%d PC=%04X)\n", millis() - t, frameCount_, pc_);
  return true;
}

bool SumiBoyEmulator::loadState() {
  if (!LittleFS.exists(statePath_)) return false;

  File f = LittleFS.open(statePath_, FILE_READ);
  if (!f) return false;

  SaveHeader hdr;
  if (f.read((uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr)) { f.close(); return false; }
  if (hdr.magic != SAVE_MAGIC) { f.close(); return false; }

  pc_ = hdr.pc; sp_ = hdr.sp;
  a_ = hdr.a; f_ = hdr.f; b_ = hdr.b; c_ = hdr.c; d_ = hdr.d; e_ = hdr.e; h_ = hdr.h; l_ = hdr.l;
  ime_ = hdr.ime; halted_ = hdr.halted; haltBug_ = hdr.haltBug; eiPending_ = hdr.eiPending;
  ie_ = hdr.ie; iflag_ = hdr.iflag;
  lcdc_ = hdr.lcdc; lcdstat_ = hdr.lcdstat; scy_ = hdr.scy; scx_ = hdr.scx;
  ly_ = hdr.ly; lyc_ = hdr.lyc; bgp_ = hdr.bgp; obp0_ = hdr.obp0; obp1_ = hdr.obp1;
  wy_ = hdr.wy; wx_ = hdr.wx;
  tima_ = hdr.tima; tma_ = hdr.tma; tac_ = hdr.tac; divReg_ = hdr.divReg;
  timerCounter_ = hdr.timerCounter; divCounter_ = hdr.divCounter;
  romBank_ = hdr.romBank; ramBank_ = hdr.ramBank; ramEnabled_ = hdr.ramEnabled;
  joypadSelect_ = hdr.joypadSelect;
  memcpy(soundRegs_, hdr.soundRegs, sizeof(soundRegs_));
  frameCount_ = hdr.frameCount;

  f.read(vram_, 0x2000);
  f.read(wram_, 0x2000);
  f.read(oam_, 0xA0);
  f.read(hram_, 0x80);
  f.read(sram_, 0x8000);
  f.close();

  joyDpad_ = 0x0F; joyButtons_ = 0x0F;

  // Rebuild framebuffer
  for (int i = 0; i < 2; i++) runFrame();

  Serial.printf("[SumiBoy] State loaded (F%d PC=%04X Bank=%d)\n", frameCount_, pc_, romBank_);
  return true;
}

bool SumiBoyEmulator::saveSRAM() {
  LittleFS.mkdir("/games");
  LittleFS.mkdir("/games/saves");

  File f = LittleFS.open(sramPath_, FILE_WRITE);
  if (!f) return false;
  unsigned long t = millis();
  f.write(sram_, 0x8000);
  f.close();
  Serial.printf("[SumiBoy] SRAM saved in %lums\n", millis() - t);
  return true;
}

bool SumiBoyEmulator::loadSRAM() {
  if (!LittleFS.exists(sramPath_)) return false;
  File f = LittleFS.open(sramPath_, FILE_READ);
  if (!f) return false;
  if (f.size() != 0x8000) { f.close(); return false; }
  f.read(sram_, 0x8000);
  f.close();
  Serial.println("[SumiBoy] SRAM loaded");
  return true;
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS && FEATURE_GAMES
