#pragma once

/**
 * @file Minesweeper.h
 * @brief Minesweeper game — Minesweeper game plugin for SUMI
 */

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include <cstring>

#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"
#include "icons/minesweeper_icons.h"

namespace sumi {

class MinesweeperGame : public PluginInterface {
 public:
  static constexpr int ROWS = 9;
  static constexpr int COLS = 9;
  static constexpr int MINES = 10;

  explicit MinesweeperGame(PluginRenderer& renderer) : d_(renderer) {}

  const char* name() const override { return "Minesweeper"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }

  void init(int screenW, int screenH) override {
    screenW_ = screenW;
    screenH_ = screenH;
    landscape_ = isLandscapeMode(screenW, screenH);
    grid_ = calculateGrid(screenW, screenH, COLS, ROWS, true, true);
    Serial.printf("[MINES] Init: %dx%d cell=%d\n", screenW, screenH, grid_.cellSize);
    newGame();
  }

  void newGame() {
    reset();
    placeMines();
    calculateNumbers();
    state_ = GameState::Playing;
    cursorR_ = ROWS / 2;
    cursorC_ = COLS / 2;
    flagMode_ = false;
    firstClick_ = true;
    flagCount_ = 0;
    startTime_ = millis();
    elapsedTime_ = 0;
  }

  bool handleInput(PluginButton btn) override {
    if (state_ == GameState::Win || state_ == GameState::Over) {
      if (btn == PluginButton::Center) {
        newGame();
        return true;
      }
      if (btn == PluginButton::Back) return false;
      return true;
    }

    switch (btn) {
      case PluginButton::Up:
        if (cursorR_ > 0) cursorR_--;
        return true;
      case PluginButton::Down:
        if (cursorR_ < ROWS - 1) cursorR_++;
        return true;
      case PluginButton::Left:
        if (cursorC_ > 0) cursorC_--;
        return true;
      case PluginButton::Right:
        if (cursorC_ < COLS - 1) cursorC_++;
        return true;
      case PluginButton::Center:
        if (flagMode_) {
          toggleFlag(cursorR_, cursorC_);
        } else {
          reveal(cursorR_, cursorC_);
        }
        return true;
      case PluginButton::Back:
        flagMode_ = !flagMode_;
        return true;
      default:
        return false;
    }
  }

  void draw() override {
    const char* title = flagMode_ ? "Mines [FLAG]" : "Minesweeper";
    PluginUI::drawHeader(d_, title, screenW_);

    int cs = grid_.cellSize;

    // Draw grid
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        int x = grid_.offsetX + c * cs;
        int y = grid_.offsetY + r * cs;

        if (revealed_[r][c]) {
          d_.drawRect(x, y, cs, cs, GxEPD_BLACK);

          if (mines_[r][c]) {
            int bx = x + (cs - MINE_ICON_W) / 2;
            int by = y + (cs - MINE_ICON_H) / 2;
            d_.drawBitmap(bx, by, MINE_BOMB, MINE_ICON_W, MINE_ICON_H, GxEPD_BLACK);
          } else if (numbers_[r][c] > 0) {
            char s[2] = {(char)('0' + numbers_[r][c]), '\0'};
            d_.setTextColor(GxEPD_BLACK);
            PluginUI::drawTextCentered(d_, s, x, y, cs, cs);
          }
        } else {
          d_.fillRect(x, y, cs, cs, GxEPD_BLACK);

          if (flags_[r][c]) {
            int bx = x + (cs - MINE_ICON_W) / 2;
            int by = y + (cs - MINE_ICON_H) / 2;
            d_.drawBitmap(bx, by, MINE_FLAG, MINE_ICON_W, MINE_ICON_H, GxEPD_WHITE);
          }
        }
      }
    }

    // Cursor highlight
    d_.setTextColor(GxEPD_BLACK);
    int cx = grid_.offsetX + cursorC_ * cs;
    int cy = grid_.offsetY + cursorR_ * cs;

    if (revealed_[cursorR_][cursorC_]) {
      PluginUI::drawCursor(d_, cx, cy, cs, cs);
    } else {
      d_.fillRect(cx + 2, cy + 2, cs - 4, cs - 4, GxEPD_WHITE);
      d_.drawRect(cx + 2, cy + 2, cs - 4, cs - 4, GxEPD_BLACK);
      if (flags_[cursorR_][cursorC_]) {
        int bx = cx + (cs - MINE_ICON_W) / 2;
        int by = cy + (cs - MINE_ICON_H) / 2;
        d_.drawBitmap(bx, by, MINE_FLAG, MINE_ICON_W, MINE_ICON_H, GxEPD_BLACK);
      }
    }

    // Status footer with timer
    if (state_ == GameState::Playing) {
      elapsedTime_ = (millis() - startTime_) / 1000;
    }
    char status[48];
    snprintf(status, sizeof(status), "Flags: %d/%d  Time: %d:%02d", flagCount_, MINES, elapsedTime_ / 60,
             elapsedTime_ % 60);
    const char* hint = flagMode_ ? "OK:Flag BACK:Dig" : "OK:Dig BACK:Flag";
    PluginUI::drawFooter(d_, status, hint, screenW_, screenH_);

    if (state_ == GameState::Win) {
      PluginUI::drawGameOver(d_, "You Win!", "OK to play again", screenW_, screenH_);
    } else if (state_ == GameState::Over) {
      PluginUI::drawGameOver(d_, "BOOM!", "OK to play again", screenW_, screenH_);
    }
  }

 private:
  PluginRenderer& d_;

  bool mines_[ROWS][COLS];
  bool revealed_[ROWS][COLS];
  bool flags_[ROWS][COLS];
  uint8_t numbers_[ROWS][COLS];
  GameState state_;
  int cursorR_, cursorC_;
  bool flagMode_;
  bool firstClick_;
  int flagCount_;
  int screenW_, screenH_;
  bool landscape_;
  GridLayout grid_;
  uint32_t startTime_;
  uint16_t elapsedTime_;

  void reset() {
    memset(mines_, 0, sizeof(mines_));
    memset(revealed_, 0, sizeof(revealed_));
    memset(flags_, 0, sizeof(flags_));
    memset(numbers_, 0, sizeof(numbers_));
    state_ = GameState::Playing;
    flagMode_ = false;
    firstClick_ = true;
    flagCount_ = 0;
  }

  void placeMines() {
    int placed = 0;
    while (placed < MINES) {
      int r = random(0, ROWS);
      int c = random(0, COLS);
      if (!mines_[r][c]) {
        mines_[r][c] = true;
        placed++;
      }
    }
  }

  void calculateNumbers() {
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (mines_[r][c]) continue;
        int count = 0;
        for (int dr = -1; dr <= 1; dr++) {
          for (int dc = -1; dc <= 1; dc++) {
            int nr = r + dr;
            int nc = c + dc;
            if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
              if (mines_[nr][nc]) count++;
            }
          }
        }
        numbers_[r][c] = count;
      }
    }
  }

  void reveal(int r, int c) {
    if (flags_[r][c]) return;
    if (revealed_[r][c]) return;

    // First click protection
    if (firstClick_ && mines_[r][c]) {
      mines_[r][c] = false;
      for (int i = 0; i < ROWS * COLS; i++) {
        int nr = i / COLS;
        int nc = i % COLS;
        if (!mines_[nr][nc] && (nr != r || nc != c)) {
          mines_[nr][nc] = true;
          break;
        }
      }
      calculateNumbers();
    }
    firstClick_ = false;

    revealed_[r][c] = true;

    if (mines_[r][c]) {
      for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
          if (mines_[i][j]) revealed_[i][j] = true;
        }
      }
      state_ = GameState::Over;
      return;
    }

    // Flood fill empty cells
    if (numbers_[r][c] == 0) {
      for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
          int nr = r + dr;
          int nc = c + dc;
          if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
            if (!revealed_[nr][nc]) {
              reveal(nr, nc);
            }
          }
        }
      }
    }

    checkWin();
  }

  void toggleFlag(int r, int c) {
    if (revealed_[r][c]) return;

    if (flags_[r][c]) {
      flags_[r][c] = false;
      flagCount_--;
    } else if (flagCount_ < MINES) {
      flags_[r][c] = true;
      flagCount_++;
    }

    checkWin();
  }

  void checkWin() {
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (!mines_[r][c] && !revealed_[r][c]) return;
      }
    }
    state_ = GameState::Win;
  }
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS && FEATURE_GAMES
