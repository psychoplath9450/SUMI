#ifndef SUMI_PLUGIN_SUDOKU_H
#define SUMI_PLUGIN_SUDOKU_H

/**
 * @file Sudoku.h
 * @brief Enhanced Sudoku puzzle game for Sumi e-reader
 * @version 2.0.0
 *
 * Features:
 * - Main menu with New Game, Continue, Settings
 * - 4 difficulty levels: Easy, Medium, Hard, Expert
 * - On-device puzzle generation with unique solutions
 * - Timer and progress tracking
 * - Pencil marks (candidate numbers)
 * - Number pad UI
 * - Undo functionality
 * - Hints (reveal correct number)
 * - Same-number highlighting
 * - Error highlighting (optional)
 * - Statistics: games played, won, streak
 * - Best times per difficulty
 * - Victory screen with celebration
 */

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// Constants
// =============================================================================
#define SUDOKU_SAVE_PATH "/.sumi/sudoku_save.bin"
#define SUDOKU_SETTINGS_PATH "/.sumi/sudoku_settings.bin"
#define SUDOKU_STATS_PATH "/.sumi/sudoku_stats.bin"

enum SudokuDifficulty : uint8_t { 
    DIFF_EASY = 0,      // 38 clues
    DIFF_MEDIUM = 1,    // 30 clues
    DIFF_HARD = 2,      // 24 clues
    DIFF_EXPERT = 3     // 17 clues
};

static const int CLUE_COUNTS[] = { 38, 30, 24, 17 };
static const char* DIFF_NAMES[] = { "Easy", "Medium", "Hard", "Expert" };
static const char* DIFF_TIMES[] = { "~8 min", "~15 min", "~25 min", "~45 min" };

enum SudokuScreen : uint8_t {
    SDK_SCREEN_MAIN_MENU,
    SDK_SCREEN_NEW_GAME,
    SDK_SCREEN_PLAYING,
    SDK_SCREEN_PAUSED,
    SDK_SCREEN_SETTINGS,
    SDK_SCREEN_VICTORY
};

// =============================================================================
// Data Structures
// =============================================================================
struct SudokuSaveData {
    uint32_t magic = 0x53554432;  // "SUD2"
    uint8_t version = 2;
    uint8_t board[9][9];
    uint8_t solution[9][9];
    bool given[9][9];           // Pre-filled cells
    uint16_t pencilMarks[9][9]; // Bitmask for candidates 1-9
    uint8_t difficulty;
    uint32_t elapsedSeconds;
    int8_t cursorR, cursorC;
    uint8_t hintsUsed;
    uint8_t errorsCount;
    // Undo stack (last 20 moves)
    struct UndoEntry {
        int8_t r, c;
        uint8_t oldVal;
        uint16_t oldPencil;
    } undoStack[20];
    uint8_t undoCount;
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x53554432 && version == 2; }
};

struct SudokuSettings {
    uint32_t magic = 0x53555345;  // "SUSE"
    bool showTimer = true;
    bool highlightSame = true;
    bool highlightErrors = false;
    bool autoRemovePencil = true;
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x53555345; }
};

struct SudokuStats {
    uint32_t magic = 0x53555354;  // "SUST"
    uint16_t gamesPlayed = 0;
    uint16_t gamesWon = 0;
    uint16_t currentStreak = 0;
    uint16_t bestStreak = 0;
    uint32_t bestTimes[4] = {0, 0, 0, 0};  // Best time per difficulty (0 = none)
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x53555354; }
};

// =============================================================================
// Sudoku Game Class
// =============================================================================
class SudokuGame {
public:
    // Screen
    int screenW, screenH;
    int boardX, boardY, cellSize;
    
    // State
    SudokuScreen currentScreen;
    int menuCursor;
    
    // Settings & Stats
    SudokuSettings settings;
    SudokuStats stats;
    
    // Board
    uint8_t board[9][9];
    uint8_t solution[9][9];
    bool given[9][9];
    uint16_t pencilMarks[9][9];  // Bitmask: bit N = candidate N+1
    
    // Game state
    SudokuDifficulty difficulty;
    uint32_t startTime;
    uint32_t elapsedSeconds;
    uint32_t pausedAt;
    bool timerRunning;
    int cursorR, cursorC;
    int selectedNum;  // 0-9, 0 = clear
    bool pencilMode;
    uint8_t hintsUsed;
    uint8_t errorsCount;
    
    // Undo
    struct UndoEntry {
        int8_t r, c;
        uint8_t oldVal;
        uint16_t oldPencil;
    } undoStack[20];
    int undoCount;
    
    // UI
    bool needsFullRedraw;
    bool hasSavedGame;
    int savedProgress;  // Percentage complete for continue button
    
    // ==========================================================================
    // Constructor & Init
    // ==========================================================================
    SudokuGame() {
        currentScreen = SDK_SCREEN_MAIN_MENU;
        menuCursor = 0;
        needsFullRedraw = true;
        loadSettings();
        loadStats();
        checkSavedGame();
    }
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
        
        // Calculate board layout
        int headerH = 50;
        int progressH = 24;
        int footerH = 100;  // Number pad + buttons
        
        int availH = screenH - headerH - progressH - footerH - 20;
        int availW = screenW - 32;
        
        cellSize = min(availH, availW) / 9;
        cellSize = constrain(cellSize, 32, 42);
        
        int boardSize = cellSize * 9;
        boardX = (screenW - boardSize) / 2;
        boardY = headerH + progressH + 8;
        
        Serial.printf("[SUDOKU] Init: %dx%d, cell=%d, board at (%d,%d)\n",
                      screenW, screenH, cellSize, boardX, boardY);
        
        checkSavedGame();
        currentScreen = SDK_SCREEN_MAIN_MENU;
        menuCursor = 0;
        needsFullRedraw = true;
    }
    
    // ==========================================================================
    // Settings & Stats
    // ==========================================================================
    void loadSettings() {
        settings = SudokuSettings();
        File f = SD.open(SUDOKU_SETTINGS_PATH, FILE_READ);
        if (f) {
            f.read((uint8_t*)&settings, sizeof(SudokuSettings));
            f.close();
            if (!settings.isValid()) settings = SudokuSettings();
        }
    }
    
    void saveSettings() {
        SD.mkdir("/.sumi");
        File f = SD.open(SUDOKU_SETTINGS_PATH, FILE_WRITE);
        if (f) {
            f.write((uint8_t*)&settings, sizeof(SudokuSettings));
            f.close();
        }
    }
    
    void loadStats() {
        stats = SudokuStats();
        File f = SD.open(SUDOKU_STATS_PATH, FILE_READ);
        if (f) {
            f.read((uint8_t*)&stats, sizeof(SudokuStats));
            f.close();
            if (!stats.isValid()) stats = SudokuStats();
        }
    }
    
    void saveStats() {
        SD.mkdir("/.sumi");
        File f = SD.open(SUDOKU_STATS_PATH, FILE_WRITE);
        if (f) {
            f.write((uint8_t*)&stats, sizeof(SudokuStats));
            f.close();
        }
    }
    
    void checkSavedGame() {
        hasSavedGame = false;
        savedProgress = 0;
        
        File f = SD.open(SUDOKU_SAVE_PATH, FILE_READ);
        if (f) {
            SudokuSaveData save;
            f.read((uint8_t*)&save, sizeof(SudokuSaveData));
            f.close();
            
            if (save.isValid()) {
                hasSavedGame = true;
                // Calculate progress
                int filled = 0;
                for (int r = 0; r < 9; r++) {
                    for (int c = 0; c < 9; c++) {
                        if (save.board[r][c] != 0) filled++;
                    }
                }
                savedProgress = (filled * 100) / 81;
            }
        }
    }
    
    // ==========================================================================
    // Save/Load Game
    // ==========================================================================
    bool saveGame() {
        SD.mkdir("/.sumi");
        File f = SD.open(SUDOKU_SAVE_PATH, FILE_WRITE);
        if (!f) return false;
        
        SudokuSaveData save;
        memcpy(save.board, board, sizeof(board));
        memcpy(save.solution, solution, sizeof(solution));
        memcpy(save.given, given, sizeof(given));
        memcpy(save.pencilMarks, pencilMarks, sizeof(pencilMarks));
        save.difficulty = difficulty;
        save.elapsedSeconds = getElapsedTime();
        save.cursorR = cursorR;
        save.cursorC = cursorC;
        save.hintsUsed = hintsUsed;
        save.errorsCount = errorsCount;
        save.undoCount = min(undoCount, 20);
        memcpy(save.undoStack, undoStack, sizeof(UndoEntry) * save.undoCount);
        
        f.write((uint8_t*)&save, sizeof(SudokuSaveData));
        f.close();
        hasSavedGame = true;
        return true;
    }
    
    bool loadGame() {
        File f = SD.open(SUDOKU_SAVE_PATH, FILE_READ);
        if (!f) return false;
        
        SudokuSaveData save;
        f.read((uint8_t*)&save, sizeof(SudokuSaveData));
        f.close();
        
        if (!save.isValid()) {
            deleteSavedGame();
            return false;
        }
        
        memcpy(board, save.board, sizeof(board));
        memcpy(solution, save.solution, sizeof(solution));
        memcpy(given, save.given, sizeof(given));
        memcpy(pencilMarks, save.pencilMarks, sizeof(pencilMarks));
        difficulty = (SudokuDifficulty)save.difficulty;
        elapsedSeconds = save.elapsedSeconds;
        cursorR = save.cursorR;
        cursorC = save.cursorC;
        hintsUsed = save.hintsUsed;
        errorsCount = save.errorsCount;
        undoCount = save.undoCount;
        memcpy(undoStack, save.undoStack, sizeof(UndoEntry) * undoCount);
        
        selectedNum = 1;
        pencilMode = false;
        startTime = millis() / 1000 - elapsedSeconds;
        timerRunning = true;
        
        return true;
    }
    
    void deleteSavedGame() {
        if (SD.exists(SUDOKU_SAVE_PATH)) {
            SD.remove(SUDOKU_SAVE_PATH);
        }
        hasSavedGame = false;
        savedProgress = 0;
    }
    
    // ==========================================================================
    // New Game
    // ==========================================================================
    void startNewGame(SudokuDifficulty diff) {
        difficulty = diff;
        generatePuzzle();
        
        cursorR = cursorC = 4;
        selectedNum = 1;
        pencilMode = false;
        hintsUsed = 0;
        errorsCount = 0;
        undoCount = 0;
        
        startTime = millis() / 1000;
        elapsedSeconds = 0;
        timerRunning = true;
        
        deleteSavedGame();
        stats.gamesPlayed++;
        saveStats();
    }
    
    void generatePuzzle() {
        // Clear board
        memset(board, 0, sizeof(board));
        memset(solution, 0, sizeof(solution));
        memset(given, false, sizeof(given));
        memset(pencilMarks, 0, sizeof(pencilMarks));
        
        // Generate complete solution
        solveSudoku();
        memcpy(solution, board, sizeof(board));
        
        // Remove cells based on difficulty
        int clues = CLUE_COUNTS[difficulty];
        int toRemove = 81 - clues;
        
        // Create list of positions and shuffle
        uint8_t positions[81];
        for (int i = 0; i < 81; i++) positions[i] = i;
        for (int i = 80; i > 0; i--) {
            int j = random(0, i + 1);
            uint8_t tmp = positions[i];
            positions[i] = positions[j];
            positions[j] = tmp;
        }
        
        // Remove cells
        int removed = 0;
        for (int i = 0; i < 81 && removed < toRemove; i++) {
            int r = positions[i] / 9;
            int c = positions[i] % 9;
            board[r][c] = 0;
            removed++;
        }
        
        // Mark given cells
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                given[r][c] = (board[r][c] != 0);
            }
        }
    }
    
    void solveSudoku() {
        // Fill diagonal boxes first (independent)
        for (int box = 0; box < 3; box++) {
            fillBox(box * 3, box * 3);
        }
        solve(0, 0);
    }
    
    void fillBox(int startR, int startC) {
        uint8_t nums[] = {1,2,3,4,5,6,7,8,9};
        for (int i = 8; i > 0; i--) {
            int j = random(0, i + 1);
            uint8_t tmp = nums[i];
            nums[i] = nums[j];
            nums[j] = tmp;
        }
        
        int idx = 0;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                board[startR + r][startC + c] = nums[idx++];
            }
        }
    }
    
    bool solve(int r, int c) {
        if (r == 9) return true;
        if (c == 9) return solve(r + 1, 0);
        if (board[r][c] != 0) return solve(r, c + 1);
        
        uint8_t nums[] = {1,2,3,4,5,6,7,8,9};
        for (int i = 8; i > 0; i--) {
            int j = random(0, i + 1);
            uint8_t tmp = nums[i];
            nums[i] = nums[j];
            nums[j] = tmp;
        }
        
        for (int i = 0; i < 9; i++) {
            if (isValidPlacement(r, c, nums[i])) {
                board[r][c] = nums[i];
                if (solve(r, c + 1)) return true;
                board[r][c] = 0;
            }
        }
        return false;
    }
    
    bool isValidPlacement(int r, int c, uint8_t num) {
        // Check row
        for (int i = 0; i < 9; i++) {
            if (board[r][i] == num) return false;
        }
        // Check column
        for (int i = 0; i < 9; i++) {
            if (board[i][c] == num) return false;
        }
        // Check 3x3 box
        int boxR = (r / 3) * 3;
        int boxC = (c / 3) * 3;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (board[boxR + i][boxC + j] == num) return false;
            }
        }
        return true;
    }
    
    // ==========================================================================
    // Game Logic
    // ==========================================================================
    uint32_t getElapsedTime() {
        if (!timerRunning) return elapsedSeconds;
        return elapsedSeconds + (millis() / 1000 - startTime);
    }
    
    void pauseTimer() {
        if (timerRunning) {
            elapsedSeconds = getElapsedTime();
            timerRunning = false;
        }
    }
    
    void resumeTimer() {
        if (!timerRunning) {
            startTime = millis() / 1000;
            timerRunning = true;
        }
    }
    
    int getFilledCount() {
        int count = 0;
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (board[r][c] != 0) count++;
            }
        }
        return count;
    }
    
    bool isComplete() {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (board[r][c] != solution[r][c]) return false;
            }
        }
        return true;
    }
    
    void placeNumber(int r, int c, uint8_t num) {
        if (given[r][c]) return;
        
        // Save for undo
        if (undoCount < 20) {
            undoStack[undoCount].r = r;
            undoStack[undoCount].c = c;
            undoStack[undoCount].oldVal = board[r][c];
            undoStack[undoCount].oldPencil = pencilMarks[r][c];
            undoCount++;
        }
        
        board[r][c] = num;
        pencilMarks[r][c] = 0;
        
        // Check if wrong
        if (num != 0 && num != solution[r][c]) {
            if (settings.highlightErrors) errorsCount++;
        }
        
        // Auto-remove pencil marks in same row/col/box
        if (num != 0 && settings.autoRemovePencil) {
            uint16_t mask = ~(1 << (num - 1));
            
            // Row and column
            for (int i = 0; i < 9; i++) {
                pencilMarks[r][i] &= mask;
                pencilMarks[i][c] &= mask;
            }
            
            // Box
            int boxR = (r / 3) * 3;
            int boxC = (c / 3) * 3;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    pencilMarks[boxR + i][boxC + j] &= mask;
                }
            }
        }
        
        // Check win
        if (isComplete()) {
            timerRunning = false;
            uint32_t finalTime = getElapsedTime();
            
            stats.gamesWon++;
            stats.currentStreak++;
            if (stats.currentStreak > stats.bestStreak) {
                stats.bestStreak = stats.currentStreak;
            }
            
            // Check best time
            if (stats.bestTimes[difficulty] == 0 || finalTime < stats.bestTimes[difficulty]) {
                stats.bestTimes[difficulty] = finalTime;
            }
            
            saveStats();
            deleteSavedGame();
            currentScreen = SDK_SCREEN_VICTORY;
            menuCursor = 0;
        } else {
            saveGame();
        }
    }
    
    void togglePencilMark(int r, int c, uint8_t num) {
        if (given[r][c] || board[r][c] != 0) return;
        if (num < 1 || num > 9) return;
        
        // Save for undo
        if (undoCount < 20) {
            undoStack[undoCount].r = r;
            undoStack[undoCount].c = c;
            undoStack[undoCount].oldVal = board[r][c];
            undoStack[undoCount].oldPencil = pencilMarks[r][c];
            undoCount++;
        }
        
        pencilMarks[r][c] ^= (1 << (num - 1));
        saveGame();
    }
    
    void undo() {
        if (undoCount == 0) return;
        
        undoCount--;
        int r = undoStack[undoCount].r;
        int c = undoStack[undoCount].c;
        board[r][c] = undoStack[undoCount].oldVal;
        pencilMarks[r][c] = undoStack[undoCount].oldPencil;
        
        saveGame();
    }
    
    void giveHint() {
        // Find an empty cell and fill it
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (board[r][c] == 0) {
                    board[r][c] = solution[r][c];
                    pencilMarks[r][c] = 0;
                    hintsUsed++;
                    
                    if (isComplete()) {
                        timerRunning = false;
                        currentScreen = SDK_SCREEN_VICTORY;
                        menuCursor = 0;
                    }
                    saveGame();
                    return;
                }
            }
        }
    }
    
    // ==========================================================================
    // Input Handling
    // ==========================================================================
    bool handleInput(Button btn) {
        switch (currentScreen) {
            case SDK_SCREEN_MAIN_MENU: return handleMainMenuInput(btn);
            case SDK_SCREEN_NEW_GAME: return handleNewGameInput(btn);
            case SDK_SCREEN_PLAYING: return handlePlayingInput(btn);
            case SDK_SCREEN_PAUSED: return handlePausedInput(btn);
            case SDK_SCREEN_SETTINGS: return handleSettingsInput(btn);
            case SDK_SCREEN_VICTORY: return handleVictoryInput(btn);
            default: return false;
        }
    }
    
    bool handleMainMenuInput(Button btn) {
        int itemCount = hasSavedGame ? 3 : 2;
        
        switch (btn) {
            case BTN_UP:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case BTN_DOWN:
                if (menuCursor < itemCount - 1) { menuCursor++; }  // Partial refresh
                return true;
            case BTN_CONFIRM: {
                int action = menuCursor;
                if (!hasSavedGame && action >= 1) action++;
                
                if (action == 0) {
                    // New Game
                    menuCursor = 1;  // Default to Medium
                    currentScreen = SDK_SCREEN_NEW_GAME;
                } else if (action == 1) {
                    // Continue
                    if (loadGame()) {
                        currentScreen = SDK_SCREEN_PLAYING;
                    }
                } else if (action == 2) {
                    // Settings
                    menuCursor = 0;
                    currentScreen = SDK_SCREEN_SETTINGS;
                }
                needsFullRedraw = true;  // Screen change
                return true;
            }
            case BTN_BACK:
                return false;
            default:
                return true;
        }
    }
    
    bool handleNewGameInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case BTN_DOWN:
                if (menuCursor < 4) { menuCursor++; }  // Partial refresh
                return true;
            case BTN_CONFIRM:
                if (menuCursor < 4) {
                    startNewGame((SudokuDifficulty)menuCursor);
                    currentScreen = SDK_SCREEN_PLAYING;
                } else {
                    // Start button - use selected difficulty
                    // Find which one is selected (has focus)
                    startNewGame(DIFF_MEDIUM);
                    currentScreen = SDK_SCREEN_PLAYING;
                }
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                menuCursor = 0;
                currentScreen = SDK_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handlePlayingInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (cursorR > 0) { cursorR--; }  // Partial refresh for cursor
                return true;
            case BTN_DOWN:
                if (cursorR < 8) { cursorR++; }  // Partial refresh for cursor
                return true;
            case BTN_LEFT:
                if (cursorC > 0) { cursorC--; }  // Partial refresh for cursor
                return true;
            case BTN_RIGHT:
                if (cursorC < 8) { cursorC++; }  // Partial refresh for cursor
                return true;
            case BTN_CONFIRM:
                // Place selected number
                if (!given[cursorR][cursorC]) {
                    if (pencilMode) {
                        if (selectedNum > 0) {
                            togglePencilMark(cursorR, cursorC, selectedNum);
                        }
                    } else {
                        placeNumber(cursorR, cursorC, selectedNum);
                    }
                    needsFullRedraw = true;
                }
                return true;
            case BTN_BACK:
                pauseTimer();
                menuCursor = 0;
                currentScreen = SDK_SCREEN_PAUSED;
                needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    
    bool handlePausedInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case BTN_DOWN:
                if (menuCursor < 3) { menuCursor++; }  // Partial refresh
                return true;
            case BTN_CONFIRM:
                if (menuCursor == 0) {
                    // Resume
                    resumeTimer();
                    currentScreen = SDK_SCREEN_PLAYING;
                } else if (menuCursor == 1) {
                    // Restart
                    startNewGame(difficulty);
                    currentScreen = SDK_SCREEN_PLAYING;
                } else if (menuCursor == 2) {
                    // New Puzzle
                    menuCursor = difficulty;
                    currentScreen = SDK_SCREEN_NEW_GAME;
                } else if (menuCursor == 3) {
                    // Save & Exit
                    saveGame();
                    checkSavedGame();
                    menuCursor = 0;
                    currentScreen = SDK_SCREEN_MAIN_MENU;
                }
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                resumeTimer();
                currentScreen = SDK_SCREEN_PLAYING;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleSettingsInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case BTN_DOWN:
                if (menuCursor < 3) { menuCursor++; }  // Partial refresh
                return true;
            case BTN_CONFIRM:
            case BTN_LEFT:
            case BTN_RIGHT:
                if (menuCursor == 0) {
                    settings.showTimer = !settings.showTimer;
                } else if (menuCursor == 1) {
                    settings.highlightSame = !settings.highlightSame;
                } else if (menuCursor == 2) {
                    settings.highlightErrors = !settings.highlightErrors;
                } else if (menuCursor == 3) {
                    settings.autoRemovePencil = !settings.autoRemovePencil;
                }
                saveSettings();
                // Partial refresh for toggles
                return true;
            case BTN_BACK:
                menuCursor = hasSavedGame ? 2 : 1;
                currentScreen = SDK_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleVictoryInput(Button btn) {
        switch (btn) {
            case BTN_UP:
            case BTN_DOWN:
                menuCursor = 1 - menuCursor;
                // Partial refresh for cursor
                return true;
            case BTN_CONFIRM:
                if (menuCursor == 0) {
                    // Next puzzle
                    menuCursor = difficulty;
                    currentScreen = SDK_SCREEN_NEW_GAME;
                } else {
                    // Main menu
                    menuCursor = 0;
                    currentScreen = SDK_SCREEN_MAIN_MENU;
                }
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                menuCursor = 0;
                currentScreen = SDK_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    // Special input for number selection (from number pad area)
    void selectNumber(int num) {
        selectedNum = num;
        // Partial refresh for selection
    }
    
    void togglePencilMode() {
        pencilMode = !pencilMode;
        // Partial refresh
    }
    
    // ==========================================================================
    // Update (for timer)
    // ==========================================================================
    bool update() {
        // Timer update - only matters if playing and timer enabled
        if (currentScreen == SDK_SCREEN_PLAYING && settings.showTimer && timerRunning) {
            // Trigger redraw every second to update timer
            static uint32_t lastTimerUpdate = 0;
            uint32_t now = millis() / 1000;
            if (now != lastTimerUpdate) {
                lastTimerUpdate = now;
                // Timer updates can use partial refresh
                return true;
            }
        }
        return false;
    }
    
    // Use partial window for smoother updates
    void drawPartial() {
        display.setPartialWindow(0, 0, screenW, screenH);
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            
            switch (currentScreen) {
                case SDK_SCREEN_MAIN_MENU: drawMainMenu(); break;
                case SDK_SCREEN_NEW_GAME: drawNewGameScreen(); break;
                case SDK_SCREEN_PLAYING: drawPlayingScreen(); break;
                case SDK_SCREEN_PAUSED:
                    drawPlayingScreen();
                    drawPausedMenu();
                    break;
                case SDK_SCREEN_SETTINGS: drawSettingsScreen(); break;
                case SDK_SCREEN_VICTORY: drawVictoryScreen(); break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawFullScreen() {
        needsFullRedraw = true;
        draw();
    }
    
    // ==========================================================================
    // Drawing
    // ==========================================================================
    void draw() {
        if (!needsFullRedraw) return;
        
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            
            switch (currentScreen) {
                case SDK_SCREEN_MAIN_MENU: drawMainMenu(); break;
                case SDK_SCREEN_NEW_GAME: drawNewGameScreen(); break;
                case SDK_SCREEN_PLAYING: drawPlayingScreen(); break;
                case SDK_SCREEN_PAUSED:
                    drawPlayingScreen();
                    drawPausedMenu();
                    break;
                case SDK_SCREEN_SETTINGS: drawSettingsScreen(); break;
                case SDK_SCREEN_VICTORY: drawVictoryScreen(); break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawMainMenu() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("Sudoku", screenW / 2, 34);
        
        // Mini board preview
        int previewSize = 180;
        int previewX = (screenW - previewSize) / 2;
        int previewY = 70;
        drawMiniBoard(previewX, previewY, previewSize);
        
        // Menu items
        int menuY = previewY + previewSize + 32;
        int itemH = 62;
        
        const char* items[] = { "New Game", "Continue", "Settings" };
        char descBuf[64];
        const char* descs[3];
        descs[0] = "Start a fresh puzzle";
        snprintf(descBuf, sizeof(descBuf), "Resume saved game (%d%% complete)", savedProgress);
        descs[1] = descBuf;
        descs[2] = "Hints, timer, statistics";
        
        int itemCount = hasSavedGame ? 3 : 2;
        
        for (int i = 0; i < itemCount; i++) {
            int idx = i;
            if (!hasSavedGame && i >= 1) idx = i + 1;
            
            int y = menuY + i * (itemH + 12);
            bool sel = (menuCursor == i);
            
            if (sel) {
                display.fillRoundRect(20, y, screenW - 40, itemH, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(20, y, screenW - 40, itemH, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(40, y + 26);
            display.print(items[idx]);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(40, y + 48);
            display.print(descs[idx]);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(screenW - 50, y + 38);
            display.print(">");
        }
    }
    
    void drawNewGameScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("New Game", screenW / 2, 34);
        
        int y = 64;
        
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(24, y);
        display.print("SELECT DIFFICULTY");
        y += 20;
        
        const char* diffDescs[] = {
            "38 clues - Great for beginners",
            "30 clues - Moderate challenge",
            "24 clues - Requires advanced techniques",
            "17 clues - Extremely difficult"
        };
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 58;
            bool sel = (menuCursor == i);
            
            if (sel) {
                display.fillRoundRect(24, itemY, screenW - 48, 52, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(24, itemY, screenW - 48, 52, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Radio button
            int radioX = 44;
            int radioY = itemY + 26;
            display.drawCircle(radioX, radioY, 8, sel ? GxEPD_WHITE : GxEPD_BLACK);
            if (sel) {
                display.fillCircle(radioX, radioY, 4, GxEPD_WHITE);
            }
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(64, itemY + 22);
            display.print(DIFF_NAMES[i]);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(64, itemY + 42);
            display.print(diffDescs[i]);
            
            // Time estimate
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(DIFF_TIMES[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - 60 - tw, itemY + 32);
            display.print(DIFF_TIMES[i]);
        }
        
        y += 4 * 58 + 20;
        
        // Start button
        bool startSel = (menuCursor == 4);
        if (startSel) {
            display.fillRoundRect(24, y, screenW - 48, 56, 8, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(24, y, screenW - 48, 56, 8, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setFont(&FreeSansBold12pt7b);
        centerText("Start Puzzle", screenW / 2, y + 36);
    }
    
    void drawPlayingScreen() {
        // Header
        display.fillRect(0, 0, screenW, 42, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        
        // Difficulty (left)
        display.setCursor(16, 28);
        display.print(DIFF_NAMES[difficulty]);
        
        // Timer (center)
        if (settings.showTimer) {
            uint32_t t = getElapsedTime();
            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "%d:%02d", t / 60, t % 60);
            display.setFont(&FreeSansBold12pt7b);
            centerText(timeStr, screenW / 2, 30);
        }
        
        // Progress (right)
        display.setFont(&FreeSans9pt7b);
        char progStr[16];
        snprintf(progStr, sizeof(progStr), "%d/81", getFilledCount());
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(progStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - tw - 16, 28);
        display.print(progStr);
        
        // Progress bar
        int barY = 46;
        int barH = 8;
        display.fillRect(16, barY, screenW - 32, barH, GxEPD_WHITE);
        display.drawRect(16, barY, screenW - 32, barH, GxEPD_BLACK);
        int fillW = ((screenW - 34) * getFilledCount()) / 81;
        display.fillRect(17, barY + 1, fillW, barH - 2, GxEPD_BLACK);
        
        // Board
        drawBoard();
        
        // Number pad
        int padY = boardY + cellSize * 9 + 12;
        drawNumberPad(padY);
        
        // Footer buttons
        int footerY = screenH - 46;
        display.drawFastHLine(0, footerY - 4, screenW, GxEPD_BLACK);
        
        int btnW = (screenW - 48) / 3;
        const char* btns[] = { "Hint", "Undo", "Menu" };
        
        for (int i = 0; i < 3; i++) {
            int x = 12 + i * (btnW + 12);
            display.drawRoundRect(x, footerY, btnW, 38, 6, GxEPD_BLACK);
            display.setFont(&FreeSansBold9pt7b);
            display.setTextColor(GxEPD_BLACK);
            centerText(btns[i], x + btnW / 2, footerY + 26);
        }
    }
    
    void drawBoard() {
        int ox = boardX;
        int oy = boardY;
        int cs = cellSize;
        
        // Background
        display.fillRect(ox, oy, cs * 9, cs * 9, GxEPD_WHITE);
        
        // Find selected number for highlighting
        uint8_t highlightNum = 0;
        if (settings.highlightSame && board[cursorR][cursorC] != 0) {
            highlightNum = board[cursorR][cursorC];
        }
        
        // Draw cells
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                int x = ox + c * cs;
                int y = oy + r * cs;
                
                // Highlight same number
                bool highlight = (highlightNum != 0 && board[r][c] == highlightNum);
                if (highlight) {
                    display.fillRect(x, y, cs, cs, GxEPD_BLACK);
                }
                
                // Cell border
                display.drawRect(x, y, cs + 1, cs + 1, GxEPD_BLACK);
                
                // Number or pencil marks
                if (board[r][c] != 0) {
                    display.setFont(&FreeSansBold12pt7b);
                    char numStr[2] = { (char)('0' + board[r][c]), '\0' };
                    
                    int16_t tx, ty; uint16_t tw, th;
                    display.getTextBounds(numStr, 0, 0, &tx, &ty, &tw, &th);
                    
                    // Color based on state
                    bool isError = settings.highlightErrors && board[r][c] != solution[r][c] && !given[r][c];
                    
                    if (highlight) {
                        display.setTextColor(GxEPD_WHITE);
                    } else if (given[r][c]) {
                        display.setTextColor(GxEPD_BLACK);
                    } else {
                        display.setTextColor(GxEPD_BLACK);
                    }
                    
                    display.setCursor(x + (cs - tw) / 2, y + (cs + th) / 2);
                    display.print(numStr);
                    
                    // Error indicator
                    if (isError) {
                        display.fillCircle(x + cs - 6, y + 6, 4, highlight ? GxEPD_WHITE : GxEPD_BLACK);
                    }
                } else if (pencilMarks[r][c] != 0) {
                    // Draw pencil marks
                    display.setFont(NULL);
                    display.setTextSize(1);
                    display.setTextColor(GxEPD_BLACK);
                    
                    int markSize = cs / 3;
                    for (int n = 1; n <= 9; n++) {
                        if (pencilMarks[r][c] & (1 << (n - 1))) {
                            int mr = (n - 1) / 3;
                            int mc = (n - 1) % 3;
                            int mx = x + mc * markSize + markSize / 2 - 2;
                            int my = y + mr * markSize + markSize / 2 + 3;
                            display.setCursor(mx, my);
                            display.print(n);
                        }
                    }
                }
            }
        }
        
        // 3x3 box borders (thick)
        for (int i = 0; i <= 3; i++) {
            int vx = ox + i * 3 * cs;
            display.fillRect(vx - 1, oy, 3, cs * 9, GxEPD_BLACK);
            
            int hy = oy + i * 3 * cs;
            display.fillRect(ox, hy - 1, cs * 9, 3, GxEPD_BLACK);
        }
        
        // Cursor
        int cx = ox + cursorC * cs;
        int cy = oy + cursorR * cs;
        display.drawRect(cx - 2, cy - 2, cs + 4, cs + 4, GxEPD_BLACK);
        display.drawRect(cx - 3, cy - 3, cs + 6, cs + 6, GxEPD_BLACK);
        display.drawRect(cx - 4, cy - 4, cs + 8, cs + 8, GxEPD_BLACK);
    }
    
    void drawNumberPad(int y) {
        int padW = screenW - 32;
        int numW = padW / 9;
        int numH = 44;
        int startX = 16;
        
        // Number buttons 1-9
        for (int n = 1; n <= 9; n++) {
            int x = startX + (n - 1) * numW;
            bool sel = (selectedNum == n);
            
            if (sel) {
                display.fillRect(x + 2, y, numW - 4, numH, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRect(x + 2, y, numW - 4, numH, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold12pt7b);
            char numStr[2] = { (char)('0' + n), '\0' };
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(numStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x + (numW - tw) / 2, y + (numH + th) / 2);
            display.print(numStr);
        }
        
        // Pencil and Clear buttons
        y += numH + 8;
        int btnW = (padW - 12) / 2;
        
        // Pencil
        if (pencilMode) {
            display.fillRoundRect(startX, y, btnW, 36, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(startX, y, btnW, 36, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setFont(&FreeSansBold9pt7b);
        centerText("Pencil", startX + btnW / 2, y + 24);
        
        // Clear
        display.drawRoundRect(startX + btnW + 12, y, btnW, 36, 6, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        centerText("Clear", startX + btnW + 12 + btnW / 2, y + 24);
    }
    
    void drawPausedMenu() {
        int dialogW = 280;
        int dialogH = 260;
        int dialogX = (screenW - dialogW) / 2;
        int dialogY = (screenH - dialogH) / 2;
        
        display.fillRect(dialogX, dialogY, dialogW, dialogH, GxEPD_WHITE);
        for (int i = 0; i < 3; i++) {
            display.drawRect(dialogX + i, dialogY + i, dialogW - i*2, dialogH - i*2, GxEPD_BLACK);
        }
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        centerText("Game Paused", dialogX + dialogW / 2, dialogY + 36);
        
        const char* items[] = { "Resume", "Restart Puzzle", "New Puzzle", "Save & Exit" };
        int itemY = dialogY + 55;
        int itemH = 44;
        
        for (int i = 0; i < 4; i++) {
            int iy = itemY + i * (itemH + 6);
            bool sel = (menuCursor == i);
            
            if (sel) {
                display.fillRoundRect(dialogX + 16, iy, dialogW - 32, itemH, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(dialogX + 16, iy, dialogW - 32, itemH, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold9pt7b);
            centerText(items[i], dialogX + dialogW / 2, iy + 28);
        }
    }
    
    void drawSettingsScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("Settings", screenW / 2, 34);
        
        int y = 64;
        
        // Options
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, y);
        display.print("OPTIONS");
        y += 20;
        
        const char* opts[] = { "Show timer", "Highlight same numbers", "Highlight errors", "Auto-remove pencil marks" };
        bool vals[] = { settings.showTimer, settings.highlightSame, settings.highlightErrors, settings.autoRemovePencil };
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 54;
            bool sel = (menuCursor == i);
            
            display.drawRoundRect(20, itemY, screenW - 40, 48, 6, GxEPD_BLACK);
            if (sel) {
                display.drawRoundRect(18, itemY - 2, screenW - 36, 52, 8, GxEPD_BLACK);
                display.drawRoundRect(17, itemY - 3, screenW - 34, 54, 9, GxEPD_BLACK);
            }
            
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(35, itemY + 30);
            display.print(opts[i]);
            
            // Toggle
            int sw = 44, sh = 24;
            int sx = screenW - 70;
            int sy = itemY + 12;
            
            if (vals[i]) {
                display.fillRoundRect(sx, sy, sw, sh, sh/2, GxEPD_BLACK);
                display.fillCircle(sx + sw - sh/2, sy + sh/2, 8, GxEPD_WHITE);
            } else {
                display.drawRoundRect(sx, sy, sw, sh, sh/2, GxEPD_BLACK);
                display.fillCircle(sx + sh/2, sy + sh/2, 8, GxEPD_BLACK);
            }
        }
        
        y += 4 * 54 + 20;
        
        // Statistics
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y);
        display.print("STATISTICS");
        y += 20;
        
        int boxW = (screenW - 60) / 3;
        const char* labels[] = { "Played", "Won", "Streak" };
        int values[] = { stats.gamesPlayed, stats.gamesWon, stats.currentStreak };
        
        for (int i = 0; i < 3; i++) {
            int bx = 20 + i * (boxW + 10);
            display.drawRoundRect(bx, y, boxW, 56, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            char numStr[8];
            snprintf(numStr, sizeof(numStr), "%d", values[i]);
            centerText(numStr, bx + boxW / 2, y + 28);
            
            display.setFont(&FreeSans9pt7b);
            centerText(labels[i], bx + boxW / 2, y + 48);
        }
        
        y += 70;
        
        // Best times
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y);
        display.print("BEST TIMES");
        y += 20;
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 32;
            display.drawRect(20, itemY, screenW - 40, 28, GxEPD_BLACK);
            
            display.setCursor(30, itemY + 20);
            display.print(DIFF_NAMES[i]);
            
            char timeStr[16];
            if (stats.bestTimes[i] > 0) {
                snprintf(timeStr, sizeof(timeStr), "%d:%02d", stats.bestTimes[i] / 60, stats.bestTimes[i] % 60);
            } else {
                strcpy(timeStr, "--:--");
            }
            
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(timeStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - 50 - tw, itemY + 20);
            display.print(timeStr);
        }
    }
    
    void drawVictoryScreen() {
        // Header
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText("Puzzle Complete!", screenW / 2, 34);
        
        // Celebration
        int y = 70;
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        centerText("Congratulations!", screenW / 2, y);
        
        display.setFont(&FreeSans9pt7b);
        char diffStr[32];
        snprintf(diffStr, sizeof(diffStr), "%s puzzle completed", DIFF_NAMES[difficulty]);
        centerText(diffStr, screenW / 2, y + 24);
        
        y += 50;
        
        // Stats boxes
        int boxW = (screenW - 60) / 3;
        
        uint32_t finalTime = getElapsedTime();
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%d:%02d", finalTime / 60, finalTime % 60);
        
        char hintsStr[8], errorsStr[8];
        snprintf(hintsStr, sizeof(hintsStr), "%d", hintsUsed);
        snprintf(errorsStr, sizeof(errorsStr), "%d", errorsCount);
        
        const char* labels[] = { "Time", "Hints", "Errors" };
        const char* values[] = { timeStr, hintsStr, errorsStr };
        
        for (int i = 0; i < 3; i++) {
            int bx = 20 + i * (boxW + 10);
            display.drawRoundRect(bx, y, boxW, 70, 8, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            centerText(values[i], bx + boxW / 2, y + 32);
            
            display.setFont(&FreeSans9pt7b);
            centerText(labels[i], bx + boxW / 2, y + 56);
        }
        
        y += 86;
        
        // New record?
        bool isRecord = (stats.bestTimes[difficulty] == finalTime);
        if (isRecord) {
            display.fillRoundRect(20, y, screenW - 40, 40, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold9pt7b);
            char recordStr[48];
            snprintf(recordStr, sizeof(recordStr), "New personal best for %s!", DIFF_NAMES[difficulty]);
            centerText(recordStr, screenW / 2, y + 26);
            y += 52;
        } else {
            y += 8;
        }
        
        // Streak
        display.drawRoundRect(20, y, screenW - 40, 60, 8, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(35, y + 20);
        display.print("Current Streak");
        
        display.setFont(&FreeSansBold12pt7b);
        char streakStr[16];
        snprintf(streakStr, sizeof(streakStr), "%d days", stats.currentStreak);
        display.setCursor(35, y + 46);
        display.print(streakStr);
        
        display.setFont(&FreeSans9pt7b);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds("Best Streak", 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - 35 - tw, y + 20);
        display.print("Best Streak");
        
        display.setFont(&FreeSansBold12pt7b);
        snprintf(streakStr, sizeof(streakStr), "%d days", stats.bestStreak);
        display.getTextBounds(streakStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - 35 - tw, y + 46);
        display.print(streakStr);
        
        y += 76;
        
        // Buttons
        int btnW = (screenW - 52) / 2;
        
        for (int i = 0; i < 2; i++) {
            int bx = 20 + i * (btnW + 12);
            bool sel = (menuCursor == i);
            
            if (sel) {
                display.fillRoundRect(bx, y, btnW, 50, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(bx, y, btnW, 50, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold9pt7b);
            centerText(i == 0 ? "Next Puzzle" : "Main Menu", bx + btnW / 2, y + 32);
        }
    }
    
    void drawMiniBoard(int x, int y, int size) {
        int cs = size / 9;
        
        display.drawRect(x, y, size, size, GxEPD_BLACK);
        
        // Dithered squares
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                // Subtle pattern
                if ((r / 3 + c / 3) % 2 == 1) {
                    int sx = x + c * cs;
                    int sy = y + r * cs;
                    for (int py = 0; py < cs; py += 2) {
                        for (int px = 0; px < cs; px += 2) {
                            display.drawPixel(sx + px, sy + py, GxEPD_BLACK);
                        }
                    }
                }
            }
        }
        
        // 3x3 borders
        for (int i = 0; i <= 3; i++) {
            display.fillRect(x + i * 3 * cs - 1, y, 2, size, GxEPD_BLACK);
            display.fillRect(x, y + i * 3 * cs - 1, size, 2, GxEPD_BLACK);
        }
    }
    
    void centerText(const char* text, int x, int y) {
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(x - tw / 2, y);
        display.print(text);
    }
};

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_SUDOKU_H
