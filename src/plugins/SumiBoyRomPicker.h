#pragma once
/**
 * @file SumiBoyRomPicker.h
 * @brief ROM selection UI for SumiBoy — scans /games/ for .gb files
 *
 * Shows a scrollable list of Game Boy ROMs found on the SD card.
 * On selection, creates a SumiBoyEmulator instance and transitions to it.
 */

#include <Arduino.h>
#include <SDCardManager.h>

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include "PluginInterface.h"
#include "PluginRenderer.h"
#include "SumiBoyEmulator.h"

#include <vector>

namespace sumi {

class SumiBoyRomPicker : public PluginInterface {
 public:
  explicit SumiBoyRomPicker(PluginRenderer& renderer) : d_(renderer) {}

  const char* name() const override { return "SumiBoy"; }
  PluginRunMode runMode() const override {
    return emulator_ ? PluginRunMode::WithUpdate : PluginRunMode::Simple;
  }

  void init(int screenW, int screenH) override {
    w_ = screenW;
    h_ = screenH;
    selected_ = 0;
    scrollOffset_ = 0;

    // Scan /games/ for .gb and .gbc files
    romFiles_.clear();
    auto files = SdMan.listFiles(PLUGINS_GAMES_DIR, 50);
    for (const auto& f : files) {
      String lower = f;
      lower.toLowerCase();
      if (lower.endsWith(".gb") || lower.endsWith(".gbc")) {
        romFiles_.push_back(f);
      }
    }

    Serial.printf("[SumiBoyPicker] Found %d ROM(s) in %s\n",
                  (int)romFiles_.size(), PLUGINS_GAMES_DIR);

    if (romFiles_.size() == 1) {
      // Single ROM — launch directly, skip picker UI
      launchRom(0);
    }
  }

  void draw() override {
    d_.fillScreen(0);  // White

    if (emulator_) {
      // Emulator is running — delegate draw to it
      emulator_->draw();
      return;
    }

    if (romFiles_.empty()) {
      drawNoRoms();
    } else {
      drawRomList();
    }

    d_.display();
  }

  bool handleRelease(PluginButton btn) override {
    if (emulator_) return emulator_->handleRelease(btn);
    return false;
  }

  bool handleInput(PluginButton btn) override {
    if (emulator_) {
      bool consumed = emulator_->handleInput(btn);
      needsFullRedraw = emulator_->needsFullRedraw;
      return consumed;
    }

    if (romFiles_.empty()) {
      return false;  // Any button exits
    }

    switch (btn) {
      case PluginButton::Up:
        if (selected_ > 0) {
          selected_--;
          if (selected_ < scrollOffset_) scrollOffset_ = selected_;
          needsFullRedraw = true;
        }
        return true;

      case PluginButton::Down:
        if (selected_ < (int)romFiles_.size() - 1) {
          selected_++;
          if (selected_ >= scrollOffset_ + maxVisible()) scrollOffset_ = selected_ - maxVisible() + 1;
          needsFullRedraw = true;
        }
        return true;

      case PluginButton::Center:
        launchRom(selected_);
        return true;

      case PluginButton::Back:
        if (emulator_) {
          // Exit emulator back to picker
          emulator_->cleanup();
          delete emulator_;
          emulator_ = nullptr;
          needsFullRedraw = true;
          return true;
        }
        return false;  // Exit plugin

      default:
        return true;
    }
  }

  bool update() override {
    if (emulator_) {
      return emulator_->update();
    }
    return false;
  }

  // When emulator is running, delegate to emulator's orientation preference
  bool wantsLandscape() const override {
    return emulator_ ? emulator_->wantsLandscape() : false;
  }

  bool handlesOwnRefresh() const override {
    return emulator_ != nullptr;
  }

  void cleanup() override {
    if (emulator_) {
      emulator_->cleanup();
      delete emulator_;
      emulator_ = nullptr;
    }
  }

 private:
  PluginRenderer& d_;
  int w_ = 0, h_ = 0;
  int selected_ = 0;
  int scrollOffset_ = 0;
  std::vector<String> romFiles_;
  SumiBoyEmulator* emulator_ = nullptr;

  int maxVisible() const {
    int lineH = d_.getLineHeight();
    if (lineH <= 0) lineH = 20;
    return (h_ - 80) / (lineH + 10);  // Title takes ~80px, each item ~lineH+10
  }

  void launchRom(int index) {
    if (index < 0 || index >= (int)romFiles_.size()) return;

    char path[80];
    snprintf(path, sizeof(path), "%s/%s", PLUGINS_GAMES_DIR, romFiles_[index].c_str());

    Serial.printf("[SumiBoyPicker] Launching ROM: %s\n", path);

    // Show loading message — warmup can take up to a minute for new ROMs
    d_.fillScreen(0);
    int cy = h_ / 2 - 40;
    d_.setCursor(w_ / 2 - 80, cy);
    d_.print("Loading...");
    cy += 40;
    d_.setCursor(20, cy);
    d_.print("First launch runs 600 warmup");
    cy += 25;
    d_.setCursor(20, cy);
    d_.print("frames to skip the boot");
    cy += 25;
    d_.setCursor(20, cy);
    d_.print("sequence. May take ~60 sec.");
    d_.display();

    emulator_ = new SumiBoyEmulator(d_, path);
    if (emulator_) {
      emulator_->init(w_, h_);
      needsFullRedraw = true;
    }
  }

  void drawNoRoms() {
    int cy = h_ / 2 - 60;

    d_.setCursor(w_ / 2 - 60, cy);
    d_.print("SumiBoy");
    cy += 40;

    d_.setCursor(w_ / 2 - 100, cy);
    d_.print("No ROMs found");
    cy += 35;

    d_.setCursor(20, cy);
    d_.print("Place .gb files in:");
    cy += 25;

    d_.setCursor(20, cy);
    d_.print(PLUGINS_GAMES_DIR);
    cy += 35;

    d_.setCursor(20, cy);
    d_.print("Transfer via BLE or");
    cy += 25;

    d_.setCursor(20, cy);
    d_.print("copy to SD card");
  }

  void drawRomList() {
    int cy = 30;

    // Title
    d_.setCursor(w_ / 2 - 60, cy);
    d_.print("SumiBoy");
    cy += 15;

    // Subtitle
    d_.setCursor(w_ / 2 - 80, cy);
    d_.printf("%d game%s", (int)romFiles_.size(), romFiles_.size() == 1 ? "" : "s");
    cy += 35;

    // List items
    int lineH = d_.getLineHeight();
    if (lineH <= 0) lineH = 20;
    int itemH = lineH + 10;
    int maxVis = maxVisible();

    for (int i = 0; i < maxVis && (scrollOffset_ + i) < (int)romFiles_.size(); i++) {
      int idx = scrollOffset_ + i;
      int itemY = cy + i * itemH;

      // Strip extension for display
      String display = romFiles_[idx];
      int dotPos = display.lastIndexOf('.');
      if (dotPos > 0) display = display.substring(0, dotPos);

      // Truncate long names
      if (display.length() > 28) {
        display = display.substring(0, 25) + "...";
      }

      if (idx == selected_) {
        // Selected item — inverted
        d_.fillRect(10, itemY - 2, w_ - 20, itemH, true);
        d_.setTextColor(false);  // White text
        d_.setCursor(20, itemY + lineH - 2);
        d_.print(display.c_str());
        d_.setTextColor(true);   // Reset
      } else {
        d_.setCursor(20, itemY + lineH - 2);
        d_.print(display.c_str());
      }
    }

    // Scroll indicators
    if (scrollOffset_ > 0) {
      d_.setCursor(w_ - 30, cy - 5);
      d_.print("^");
    }
    if (scrollOffset_ + maxVis < (int)romFiles_.size()) {
      d_.setCursor(w_ - 30, cy + maxVis * itemH);
      d_.print("v");
    }
  }
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS && FEATURE_GAMES
