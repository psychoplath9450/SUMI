/**
 * @file Sudoku.h
 * @brief Sudoku puzzle game for Sumi e-reader
 * @version 2.1.23
 * 
 * Features:
 * - Partial refresh for smooth cursor movement (no black flash)
 * - Proper button handling for portrait mode
 * - Number picker overlay
 */

#ifndef SUMI_PLUGIN_SUDOKU_H
#define SUMI_PLUGIN_SUDOKU_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class SudokuGame {
public:
    SudokuGame();
    void init(int screenW, int screenH);
    void newGame(int difficulty);
    bool handleInput(Button btn);
    void draw();
    
private:
    uint8_t _board[9][9];
    uint8_t _solution[9][9];
    bool _fixed[9][9];
    GameState _state;
    int _cursorR, _cursorC;
    int _prevCursorR, _prevCursorC;  // For dirty tracking
    int _selectedNum;
    bool _inputMode;
    int _screenW, _screenH;
    bool _landscape;
    GridLayout _grid;
    
    // Partial refresh support
    bool _dirtyCells[9][9];
    bool _anyDirty;
    bool _needsFullRedraw;
    
    void reset();
    bool handleNumberInput(Button btn);
    void drawNumberPicker();
    void generatePuzzle(int difficulty);
    void solveSudoku();
    void fillBox(int startR, int startC);
    bool solve(int r, int c);
    bool isValid(int r, int c, uint8_t num);
    void checkWin();
    
    // Drawing helpers
    void drawFull();
    void drawPartial();
    void drawContent();
    void drawCell(int r, int c);
    void drawThickBorders(int minR, int maxR, int minC, int maxC);
    void markCellDirty(int r, int c);
    void markCursorDirty();
};

extern SudokuGame sudokuGame;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_SUDOKU_H
