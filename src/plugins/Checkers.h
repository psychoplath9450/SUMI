#pragma once

/**
 * @file Checkers.h
 * @brief Checkers/Draughts game — ported from SUMI
 */

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include <cstring>

#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"
#include "icons/checkers_icons.h"

namespace sumi {

enum CheckerPiece : uint8_t { C_EMPTY = 0, C_RED = 1, C_RED_K = 2, C_BLACK = 3, C_BLACK_K = 4 };

class CheckersGame : public PluginInterface {
 public:
  explicit CheckersGame(PluginRenderer& renderer) : d_(renderer) {}

  const char* name() const override { return "Checkers"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }

  void init(int screenW, int screenH) override {
    screenW_ = screenW;
    screenH_ = screenH;
    landscape_ = isLandscapeMode(screenW, screenH);
    grid_ = calculateGrid(screenW, screenH, 8, 8, true, true);
    newGame();
  }

  void newGame() {
    memset(board_, 0, sizeof(board_));
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 8; c++)
        if ((r + c) % 2 == 1) board_[r][c] = C_BLACK;
    for (int r = 5; r < 8; r++)
      for (int c = 0; c < 8; c++)
        if ((r + c) % 2 == 1) board_[r][c] = C_RED;
    state_ = GameState::Playing;
    playerTurn_ = true;
    cursorR_ = 5;
    cursorC_ = 1;
    selectedR_ = -1;
    selectedC_ = -1;
    mustJump_ = false;
  }

  bool handleInput(PluginButton btn) override {
    if (state_ == GameState::Win || state_ == GameState::Over) {
      if (btn == PluginButton::Center) { newGame(); return true; }
      if (btn == PluginButton::Back) return false;
      return true;
    }

    switch (btn) {
      case PluginButton::Up:    if (cursorR_ > 0) cursorR_--; return true;
      case PluginButton::Down:  if (cursorR_ < 7) cursorR_++; return true;
      case PluginButton::Left:  if (cursorC_ > 0) cursorC_--; return true;
      case PluginButton::Right: if (cursorC_ < 7) cursorC_++; return true;
      case PluginButton::Center: return handleSelect();
      case PluginButton::Back:
        if (selectedR_ >= 0) { selectedR_ = -1; selectedC_ = -1; return true; }
        return false;
      default: return false;
    }
  }

  void draw() override {
    PluginUI::drawHeader(d_, "Checkers", screenW_);
    int cs = grid_.cellSize;

    for (int r = 0; r < 8; r++) {
      for (int c = 0; c < 8; c++) {
        int x = grid_.offsetX + c * cs;
        int y = grid_.offsetY + r * cs;
        bool dark = (r + c) % 2 == 1;
        PluginUI::drawCheckerSquare(d_, x, y, cs, dark);

        if (r == selectedR_ && c == selectedC_) {
          // Selection highlight
          d_.drawRect(x + 3, y + 3, cs - 6, cs - 6, dark ? GxEPD_WHITE : GxEPD_BLACK);
          d_.drawRect(x + 4, y + 4, cs - 8, cs - 8, dark ? GxEPD_WHITE : GxEPD_BLACK);
        }

        CheckerPiece p = board_[r][c];
        if (p != C_EMPTY) {
          int bx = x + (cs - CHECKER_PIECE_W) / 2;
          int by = y + (cs - CHECKER_PIECE_H) / 2;
          const uint8_t* bmp = nullptr;
          if (p == C_RED) bmp = CHECKER_WHITE;
          else if (p == C_RED_K) bmp = CHECKER_WHITE_K;
          else if (p == C_BLACK) bmp = CHECKER_BLACK;
          else if (p == C_BLACK_K) bmp = CHECKER_BLACK_K;
          if (bmp) {
            d_.fillRect(bx, by, CHECKER_PIECE_W, CHECKER_PIECE_H, GxEPD_WHITE);
            d_.drawBitmap(bx, by, bmp, CHECKER_PIECE_W, CHECKER_PIECE_H, GxEPD_BLACK);
          }
        }
      }
    }

    // Cursor - use inverted color so it's visible on both light and dark squares
    int cx = grid_.offsetX + cursorC_ * cs;
    int cy = grid_.offsetY + cursorR_ * cs;
    bool onDark = (cursorR_ + cursorC_) % 2 == 1;
    uint16_t cursorColor = onDark ? GxEPD_WHITE : GxEPD_BLACK;
    d_.drawRect(cx, cy, cs, cs, cursorColor);
    d_.drawRect(cx + 1, cy + 1, cs - 2, cs - 2, cursorColor);
    d_.drawRect(cx + 2, cy + 2, cs - 4, cs - 4, cursorColor);
    // Corner markers for extra visibility
    int m = 8;
    for (int t = 0; t < 3; t++) {
      // Top-left
      d_.drawFastHLine(cx + t, cy + t, m - t, cursorColor);
      d_.drawFastVLine(cx + t, cy + t, m - t, cursorColor);
      // Top-right
      d_.drawFastHLine(cx + cs - m, cy + t, m - t, cursorColor);
      d_.drawFastVLine(cx + cs - 1 - t, cy + t, m - t, cursorColor);
      // Bottom-left
      d_.drawFastHLine(cx + t, cy + cs - 1 - t, m - t, cursorColor);
      d_.drawFastVLine(cx + t, cy + cs - m, m - t, cursorColor);
      // Bottom-right
      d_.drawFastHLine(cx + cs - m, cy + cs - 1 - t, m - t, cursorColor);
      d_.drawFastVLine(cx + cs - 1 - t, cy + cs - m, m - t, cursorColor);
    }

    // Status
    char status[32];
    if (state_ == GameState::Win) {
      snprintf(status, sizeof(status), "%s wins!", playerTurn_ ? "Black" : "Red");
    } else {
      snprintf(status, sizeof(status), "%s%s", playerTurn_ ? "Your turn" : "AI thinking...",
               mustJump_ ? " (must jump!)" : "");
    }
    PluginUI::drawFooter(d_, status, "OK:Select BACK:Cancel", screenW_, screenH_);

    if (state_ == GameState::Win) {
      PluginUI::drawGameOver(d_, playerTurn_ ? "You Lose!" : "You Win!", "OK to play again", screenW_, screenH_);
    }
  }

 private:
  PluginRenderer& d_;
  CheckerPiece board_[8][8];
  GameState state_;
  bool playerTurn_;
  int cursorR_, cursorC_, selectedR_, selectedC_;
  bool mustJump_;
  int screenW_, screenH_;
  bool landscape_;
  GridLayout grid_;

  static bool isPlayer(CheckerPiece p) { return p == C_RED || p == C_RED_K; }
  static bool isAI(CheckerPiece p) { return p == C_BLACK || p == C_BLACK_K; }
  static bool isKing(CheckerPiece p) { return p == C_RED_K || p == C_BLACK_K; }

  bool handleSelect() {
    if (selectedR_ < 0) {
      CheckerPiece p = board_[cursorR_][cursorC_];
      if (playerTurn_ && isPlayer(p)) { selectedR_ = cursorR_; selectedC_ = cursorC_; return true; }
    } else {
      if (isValidMove(selectedR_, selectedC_, cursorR_, cursorC_)) {
        bool jumped = makeMove(selectedR_, selectedC_, cursorR_, cursorC_);
        if (jumped && canJump(cursorR_, cursorC_)) {
          selectedR_ = cursorR_; selectedC_ = cursorC_;
          mustJump_ = true;
        } else {
          selectedR_ = -1; selectedC_ = -1; mustJump_ = false;
          if (checkEnd()) return true;
          playerTurn_ = false;
          aiMove();
          checkEnd();
          playerTurn_ = true;
        }
        return true;
      }
      selectedR_ = -1; selectedC_ = -1;
    }
    return true;
  }

  bool isValidMove(int fr, int fc, int tr, int tc) {
    if (tr < 0 || tr > 7 || tc < 0 || tc > 7) return false;
    if (board_[tr][tc] != C_EMPTY) return false;
    CheckerPiece p = board_[fr][fc];
    int dr = tr - fr, dc = tc - fc;
    if (abs(dr) == 1 && abs(dc) == 1) {
      if (mustJump_) return false;
      if (p == C_RED && dr > 0) return false;
      if (p == C_BLACK && dr < 0) return false;
      return true;
    }
    if (abs(dr) == 2 && abs(dc) == 2) {
      int mr = fr + dr / 2, mc = fc + dc / 2;
      CheckerPiece j = board_[mr][mc];
      if (p == C_RED && dr > 0 && !isKing(p)) return false;
      if (p == C_BLACK && dr < 0 && !isKing(p)) return false;
      if (isPlayer(p) && isAI(j)) return true;
      if (isAI(p) && isPlayer(j)) return true;
    }
    return false;
  }

  bool makeMove(int fr, int fc, int tr, int tc) {
    CheckerPiece p = board_[fr][fc];
    bool jumped = false;
    if (abs(tr - fr) == 2) { board_[fr + (tr - fr) / 2][fc + (tc - fc) / 2] = C_EMPTY; jumped = true; }
    board_[tr][tc] = p;
    board_[fr][fc] = C_EMPTY;
    if (p == C_RED && tr == 0) board_[tr][tc] = C_RED_K;
    if (p == C_BLACK && tr == 7) board_[tr][tc] = C_BLACK_K;
    return jumped;
  }

  bool canJump(int r, int c) {
    int dirs[][2] = {{-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
    for (auto& d : dirs) if (isValidMove(r, c, r + d[0], c + d[1])) return true;
    return false;
  }

  bool checkEnd() {
    int rc = 0, bc = 0; bool rm = false, bm = false;
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
      CheckerPiece p = board_[r][c];
      if (isPlayer(p)) { rc++; if (hasValidMove(r, c)) rm = true; }
      else if (isAI(p)) { bc++; if (hasValidMove(r, c)) bm = true; }
    }
    if (rc == 0 || !rm) { state_ = GameState::Win; playerTurn_ = true; return true; }
    if (bc == 0 || !bm) { state_ = GameState::Win; playerTurn_ = false; return true; }
    return false;
  }

  bool hasValidMove(int r, int c) {
    int dirs[][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
    for (auto& d : dirs) if (isValidMove(r, c, r + d[0], c + d[1])) return true;
    return false;
  }

  void aiMove() {
    struct Move { int fr, fc, tr, tc; bool jump; };
    Move moves[64]; int mc = 0;
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
      if (!isAI(board_[r][c])) continue;
      int dirs[][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
      for (auto& dd : dirs) {
        int tr = r + dd[0], tc = c + dd[1];
        if (isValidMove(r, c, tr, tc) && mc < 64) moves[mc++] = {r, c, tr, tc, abs(dd[0]) == 2};
      }
    }
    if (mc == 0) return;
    int best = 0;
    for (int i = 0; i < mc; i++) if (moves[i].jump) { best = i; break; }
    if (!moves[best].jump) best = random(0, mc);
    Move& m = moves[best];
    bool jumped = makeMove(m.fr, m.fc, m.tr, m.tc);
    while (jumped && canJump(m.tr, m.tc)) {
      int dirs[][2] = {{-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
      for (auto& dd : dirs) {
        int nr = m.tr + dd[0], nc = m.tc + dd[1];
        if (isValidMove(m.tr, m.tc, nr, nc)) { jumped = makeMove(m.tr, m.tc, nr, nc); m.tr = nr; m.tc = nc; break; }
      }
    }
  }
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS && FEATURE_GAMES
