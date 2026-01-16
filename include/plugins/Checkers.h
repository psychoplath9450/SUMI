/**
 * @file Checkers.h
 * @brief Checkers/Draughts game for Sumi e-reader
 * @version 2.1.26
 */

#ifndef SUMI_PLUGIN_CHECKERS_H
#define SUMI_PLUGIN_CHECKERS_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

enum CheckerPiece : uint8_t {
    C_EMPTY = 0,
    C_RED = 1,
    C_RED_K = 2,
    C_BLACK = 3,
    C_BLACK_K = 4
};

class CheckersGame {
public:
    CheckersGame();
    void init(int screenW, int screenH);
    void newGame();
    bool handleInput(Button btn);
    void draw();
    
private:
    CheckerPiece _board[8][8];
    GameState _state;
    bool _playerTurn;
    int _cursorR, _cursorC, _selectedR, _selectedC;
    bool _mustJump;
    int _screenW, _screenH;
    bool _landscape;
    GridLayout _grid;
    
    void reset();
    bool isPlayer(CheckerPiece p);
    bool isAI(CheckerPiece p);
    bool isKing(CheckerPiece p);
    bool handleSelect();
    bool isValidMove(int fr, int fc, int tr, int tc);
    bool makeMove(int fr, int fc, int tr, int tc);
    bool canJump(int r, int c);
    bool checkEnd();
    bool hasValidMove(int r, int c);
    void aiMove();
};

extern CheckersGame checkersGame;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_CHECKERS_H
