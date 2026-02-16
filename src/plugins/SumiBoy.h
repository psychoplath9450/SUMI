#pragma once
/**
 * @file SumiBoy.h
 * @brief SumiBoy — dual boot launcher for Game Boy emulator
 *
 * Checks for emulator firmware in app1 partition (0x650000).
 * If found, shows confirmation screen and boots to it via OTA partition switch.
 * The emulator can boot back to SUMI the same way.
 */

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "../config.h"

#if FEATURE_PLUGINS

#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

class SumiBoyApp : public PluginInterface {
 public:
  explicit SumiBoyApp(PluginRenderer& renderer) : d_(renderer) {}

  const char* name() const override { return "SumiBoy"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }

  void init(int screenW, int screenH) override {
    w_ = screenW;
    h_ = screenH;
    selected_ = 0;  // 0 = Launch, 1 = Back

    // Detect emulator in app1
    emulatorPartition_ = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (emulatorPartition_) {
      uint8_t magic = 0;
      esp_partition_read(emulatorPartition_, 0, &magic, 1);
      hasEmulator_ = (magic == 0xE9);  // Valid ESP32 app image
    }

    Serial.printf("[SumiBoy] Emulator partition: %s\n",
                  hasEmulator_ ? "found" : "not installed");
  }

  void draw() override {
    d_.fillScreen(0);  // white

    if (!hasEmulator_) {
      // No emulator installed
      drawNoEmulator();
    } else {
      drawLaunchScreen();
    }

    d_.display();
  }

  bool handleInput(PluginButton btn) override {
    if (!hasEmulator_) {
      // Any button exits when no emulator
      return false;  // Let host handle → exit
    }

    switch (btn) {
      case PluginButton::Up:
      case PluginButton::Down:
        selected_ = 1 - selected_;  // Toggle between Launch/Back
        needsFullRedraw = true;
        return true;

      case PluginButton::Center:
        if (selected_ == 0) {
          // Launch emulator
          bootToEmulator();
          return true;  // Won't actually reach here (restarts)
        }
        return false;  // Back selected → exit

      case PluginButton::Back:
        return false;  // Exit plugin

      default:
        return true;
    }
  }

 private:
  PluginRenderer& d_;
  int w_ = 0, h_ = 0;
  int selected_ = 0;
  bool hasEmulator_ = false;
  const esp_partition_t* emulatorPartition_ = nullptr;

  void drawNoEmulator() {
    int cy = h_ / 2 - 40;

    d_.setCursor(w_ / 2 - 80, cy);
    d_.print("SumiBoy");
    cy += 40;

    d_.setCursor(w_ / 2 - 120, cy);
    d_.print("No emulator installed");
    cy += 30;

    d_.setCursor(w_ / 2 - 130, cy);
    d_.print("Flash emulator to app1");
    cy += 20;

    d_.setCursor(w_ / 2 - 100, cy);
    d_.print("(0x650000) to play");
  }

  void drawLaunchScreen() {
    int cy = 60;

    // Title
    d_.setCursor(w_ / 2 - 80, cy);
    d_.print("SumiBoy");
    cy += 50;

    // Description
    d_.setCursor(w_ / 2 - 140, cy);
    d_.print("Game Boy emulator ready");
    cy += 30;

    d_.setCursor(w_ / 2 - 150, cy);
    d_.print("Device will restart into");
    cy += 25;

    d_.setCursor(w_ / 2 - 100, cy);
    d_.print("emulator mode");
    cy += 50;

    // Buttons
    const int btnW = 200;
    const int btnH = 50;
    const int btnX = w_ / 2 - btnW / 2;

    // Launch button
    if (selected_ == 0) {
      d_.fillRect(btnX, cy, btnW, btnH, true);  // Black fill
      d_.setTextColor(false);  // White text
      d_.setCursor(w_ / 2 - 40, cy + 32);
      d_.print("Launch");
      d_.setTextColor(true);  // Reset to black
    } else {
      d_.drawRect(btnX, cy, btnW, btnH, true);
      d_.setCursor(w_ / 2 - 40, cy + 32);
      d_.print("Launch");
    }
    cy += btnH + 15;

    // Back button
    if (selected_ == 1) {
      d_.fillRect(btnX, cy, btnW, btnH, true);
      d_.setTextColor(false);
      d_.setCursor(w_ / 2 - 25, cy + 32);
      d_.print("Back");
      d_.setTextColor(true);
    } else {
      d_.drawRect(btnX, cy, btnW, btnH, true);
      d_.setCursor(w_ / 2 - 25, cy + 32);
      d_.print("Back");
    }
  }

  void bootToEmulator() {
    // Show transition message
    d_.fillScreen(0);
    d_.setCursor(w_ / 2 - 110, h_ / 2);
    d_.print("Launching SumiBoy...");
    d_.display();

    // Set boot partition to app1 (emulator)
    esp_err_t err = esp_ota_set_boot_partition(emulatorPartition_);
    if (err != ESP_OK) {
      Serial.printf("[SumiBoy] Failed to set boot partition: %d\n", err);
      d_.fillScreen(0);
      d_.setCursor(w_ / 2 - 80, h_ / 2);
      d_.print("Boot failed!");
      d_.display();
      delay(2000);
      needsFullRedraw = true;
      return;
    }

    Serial.println("[SumiBoy] Rebooting to emulator...");
    delay(100);
    ESP.restart();
  }
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
