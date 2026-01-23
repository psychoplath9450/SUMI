/**
 * @file Sudoku.h
 * @brief Sudoku puzzle game for Sumi e-reader
 * @version 2.2.1
 * 
 * Features:
 * - Navigate with D-pad, press OK to enter edit mode
 * - In edit mode: UP/DOWN cycles valid numbers only
 * - Partial refresh for smooth cursor movement
 * - Auto-save/resume functionality
 */

#ifndef SUMI_PLUGIN_SUDOKU_H
#define SUMI_PLUGIN_SUDOKU_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// Save file path
#define SUDOKU_SAVE_PATH "/.sumi/sudoku_save.bin"

// Save data structure
struct SudokuSaveData {
    uint32_t magic = 0x5355444F;  // "SUDO"
    uint8_t board[9][9];
    uint8_t solution[9][9];
    bool fixed[9][9];
    int cursorR, cursorC;
    uint8_t reserved[32];  // For future use
    
    bool isValid() const { return magic == 0x5355444F; }
};

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
    uint8_t _savedValue;  // Original value before input mode (for cancel)
    int _screenW, _screenH;
    bool _landscape;
    GridLayout _grid;
    
    // Partial refresh support
    bool _dirtyCells[9][9];
    bool _anyDirty;
    bool _needsFullRedraw;
    int _actionCount;  // Counter for periodic smooth refresh
    
    // Menu state for resume prompt
    enum MenuState { MENU_NONE, MENU_RESUME_PROMPT };
    MenuState _menuState;
    int _menuCursor;
    
    void reset();
    bool handleNumberInput(Button btn);
    void drawNumberPicker();
    void generatePuzzle(int difficulty);
    void solveSudoku();
    void fillBox(int startR, int startC);
    bool solve(int r, int c);
    bool isValid(int r, int c, uint8_t num);
    void checkWin();
    
    // Inline number cycling (no popup menu)
    void getValidNumbers(int r, int c, uint8_t* validNums, int& count);
    uint8_t cycleNumber(int r, int c, int direction);
    
    // Save/Load functions
    bool hasSavedGame();
    bool saveGame();
    bool loadGame();
    void deleteSavedGame();
    
    // Drawing helpers
    void drawFull();
    void drawPartial();
    void drawContent();
    void drawCell(int r, int c);
    void drawThickBorders(int minR, int maxR, int minC, int maxC);
    void drawMenu();
    void markCellDirty(int r, int c);
    void markCursorDirty();
};

extern SudokuGame sudokuGame;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_SUDOKU_H
