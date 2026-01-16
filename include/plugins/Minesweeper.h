/**
 * @file Minesweeper.h
 * @brief Minesweeper game for Sumi e-reader
 * @version 2.1.26
 */

#ifndef SUMI_PLUGIN_MINESWEEPER_H
#define SUMI_PLUGIN_MINESWEEPER_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class MinesweeperGame {
public:
    static const int ROWS = 9;
    static const int COLS = 9;
    static const int MINES = 10;
    
    MinesweeperGame();
    void init(int screenW, int screenH);
    void newGame();
    bool handleInput(Button btn);
    void draw();
    
private:
    bool _mines[ROWS][COLS];
    bool _revealed[ROWS][COLS];
    bool _flags[ROWS][COLS];
    uint8_t _numbers[ROWS][COLS];
    GameState _state;
    int _cursorR, _cursorC;
    bool _flagMode;
    bool _firstClick;
    int _flagCount;
    int _screenW, _screenH;
    bool _landscape;
    GridLayout _grid;
    
    void reset();
    void placeMines();
    void calculateNumbers();
    void reveal(int r, int c);
    void toggleFlag(int r, int c);
    void checkWin();
};

extern MinesweeperGame minesweeperGame;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_MINESWEEPER_H
