#pragma once

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include <SDCardManager.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

/**
 * @file Sudoku.h
 * @brief Enhanced Sudoku puzzle game for Sumi e-reader
 * @version 1.0.0
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
class SudokuGame : public PluginInterface {
public:

  const char* name() const override { return "Sudoku"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }
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
    SudokuDifficulty selectedDifficulty;  // For new game selection
    uint32_t startTime;
    uint32_t elapsedSeconds;
    uint32_t pausedAt;
    bool timerRunning;
    int cursorR, cursorC;
    int selectedNum;  // 0-9, 0 = clear
    bool pencilMode;
    uint8_t hintsUsed;
    uint8_t errorsCount;
    
    // Cell editing mode
    bool editingCell;         // Currently editing a cell
    int editCandidates[10];   // Valid numbers (1-9 + 0 for clear)
    int editCandidateCount;   // How many candidates
    int editCursor;           // Which candidate is highlighted
    
    // Undo
    struct UndoEntry {
        int8_t r, c;
        uint8_t oldVal;
        uint16_t oldPencil;
    } undoStack[20];
    int undoCount;
    
    // UI (needsFullRedraw inherited from PluginInterface)
    bool hasSavedGame;
    int savedProgress;  // Percentage complete for continue button
    
    // ==========================================================================
    // Constructor & Init
    // ==========================================================================
    explicit SudokuGame(PluginRenderer& renderer) : d_(renderer) {
        currentScreen = SDK_SCREEN_MAIN_MENU;
        menuCursor = 0;
        needsFullRedraw = true;
        selectedDifficulty = DIFF_MEDIUM;  // Default to Medium
        loadSettings();
        loadStats();
        checkSavedGame();
    }
    
    void init(int w, int h) override {
        // Always reset to portrait mode for Sudoku
        d_.setRotation(3);
        // orientation handled by host
        
        screenW = 480;  // Portrait dimensions
        screenH = 800;
        
        // Calculate board layout - more space without number pad
        int headerH = 58;  // Header + progress bar
        int footerH = 40;  // Controls footer
        
        int availH = screenH - headerH - footerH - 16;
        int availW = screenW - 32;
        
        cellSize = min(availH, availW) / 9;
        cellSize = constrain(cellSize, 36, 52);
        
        int boardSize = cellSize * 9;
        boardX = (screenW - boardSize) / 2;
        boardY = headerH + (availH - boardSize) / 2;
        
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
        FsFile f = SdMan.open(SUDOKU_SETTINGS_PATH, O_RDONLY);
        if (f) {
            f.read((uint8_t*)&settings, sizeof(SudokuSettings));
            f.close();
            if (!settings.isValid()) settings = SudokuSettings();
        }
    }
    
    void saveSettings() {
        SdMan.mkdir("/.sumi");
        FsFile f = SdMan.open(SUDOKU_SETTINGS_PATH, (O_WRONLY | O_CREAT | O_TRUNC));
        if (f) {
            f.write((uint8_t*)&settings, sizeof(SudokuSettings));
            f.close();
        }
    }
    
    void loadStats() {
        stats = SudokuStats();
        FsFile f = SdMan.open(SUDOKU_STATS_PATH, O_RDONLY);
        if (f) {
            f.read((uint8_t*)&stats, sizeof(SudokuStats));
            f.close();
            if (!stats.isValid()) stats = SudokuStats();
        }
    }
    
    void saveStats() {
        SdMan.mkdir("/.sumi");
        FsFile f = SdMan.open(SUDOKU_STATS_PATH, (O_WRONLY | O_CREAT | O_TRUNC));
        if (f) {
            f.write((uint8_t*)&stats, sizeof(SudokuStats));
            f.close();
        }
    }
    
    void checkSavedGame() {
        hasSavedGame = false;
        savedProgress = 0;
        
        FsFile f = SdMan.open(SUDOKU_SAVE_PATH, O_RDONLY);
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
        SdMan.mkdir("/.sumi");
        FsFile f = SdMan.open(SUDOKU_SAVE_PATH, (O_WRONLY | O_CREAT | O_TRUNC));
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
        FsFile f = SdMan.open(SUDOKU_SAVE_PATH, O_RDONLY);
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
        editingCell = false;
        editCandidateCount = 0;
        editCursor = 0;
        startTime = millis() / 1000 - elapsedSeconds;
        timerRunning = true;
        
        return true;
    }
    
    void deleteSavedGame() {
        if (SdMan.exists(SUDOKU_SAVE_PATH)) {
            SdMan.remove(SUDOKU_SAVE_PATH);
        }
        hasSavedGame = false;
        savedProgress = 0;
    }
    
    // ==========================================================================
    // New Game
    // ==========================================================================
    void startNewGame(SudokuDifficulty diff) {
        // Show generating overlay before blocking solver
        d_.fillScreen(0);  // white
        d_.setCursor(d_.width() / 2 - 70, d_.height() / 2);
        d_.print("Generating...");
        d_.displayPartial();

        difficulty = diff;
        generatePuzzle();
        
        cursorR = cursorC = 4;
        selectedNum = 1;
        pencilMode = false;
        editingCell = false;
        editCandidateCount = 0;
        editCursor = 0;
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
        
        if (c == 0) yield();  // Feed watchdog each row
        
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
    
    // Compute which numbers are valid for a given cell
    void computeCandidates(int r, int c) {
        editCandidateCount = 0;
        
        // Check which numbers 1-9 are NOT used in row, col, or box
        for (int n = 1; n <= 9; n++) {
            if (isValidPlacement(r, c, n)) {
                editCandidates[editCandidateCount++] = n;
            }
        }
        // Always add 0 (clear) at the end if cell has a value
        if (board[r][c] != 0) {
            editCandidates[editCandidateCount++] = 0;  // Clear option
        }
    }
    
    void enterEditMode() {
        if (given[cursorR][cursorC]) return;
        
        // Temporarily clear cell to compute all valid placements
        uint8_t saved = board[cursorR][cursorC];
        board[cursorR][cursorC] = 0;
        computeCandidates(cursorR, cursorC);
        board[cursorR][cursorC] = saved;
        
        if (editCandidateCount == 0) return;  // No valid numbers (shouldn't happen in valid puzzle)
        
        editingCell = true;
        editCursor = 0;
        
        // If cell already has a value, highlight that candidate
        if (saved != 0) {
            for (int i = 0; i < editCandidateCount; i++) {
                if (editCandidates[i] == saved) {
                    editCursor = i;
                    break;
                }
            }
        }
    }
    
    // ==========================================================================
    // Input Handling
    // ==========================================================================
    bool handleInput(PluginButton btn) override {
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
    
    bool handleMainMenuInput(PluginButton btn) {
        int itemCount = hasSavedGame ? 3 : 2;
        
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case PluginButton::Down:
                if (menuCursor < itemCount - 1) { menuCursor++; }  // Partial refresh
                return true;
            case PluginButton::Center: {
                int action = menuCursor;
                if (!hasSavedGame && action >= 1) action++;
                
                if (action == 0) {
                    // New Game - preselect Start button (index 4)
                    menuCursor = 4;  // Start Puzzle button
                    selectedDifficulty = DIFF_MEDIUM;  // Default to Medium
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
            case PluginButton::Back:
                return false;
            default:
                return true;
        }
    }
    
    bool handleNewGameInput(PluginButton btn) {
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }
                return true;
            case PluginButton::Down:
                if (menuCursor < 4) { menuCursor++; }
                return true;
            case PluginButton::Left:
            case PluginButton::Right:
                // Change difficulty selection when on difficulty options
                if (menuCursor < 4) {
                    selectedDifficulty = (SudokuDifficulty)menuCursor;
                }
                return true;
            case PluginButton::Center:
                if (menuCursor < 4) {
                    // Clicking on a difficulty selects it and moves to Start
                    selectedDifficulty = (SudokuDifficulty)menuCursor;
                    menuCursor = 4;  // Move to Start button
                } else {
                    // Start button - use selected difficulty
                    startNewGame(selectedDifficulty);
                    currentScreen = SDK_SCREEN_PLAYING;
                    needsFullRedraw = true;
                }
                return true;
            case PluginButton::Back:
                menuCursor = 0;
                currentScreen = SDK_SCREEN_MAIN_MENU;
                needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    
    bool handlePlayingInput(PluginButton btn) {
        if (editingCell) {
            // In cell edit mode - navigate valid numbers
            switch (btn) {
                case PluginButton::Up:
                case PluginButton::Left:
                    if (editCursor > 0) editCursor--;
                    else editCursor = editCandidateCount - 1;
                    return true;
                case PluginButton::Down:
                case PluginButton::Right:
                    if (editCursor < editCandidateCount - 1) editCursor++;
                    else editCursor = 0;
                    return true;
                case PluginButton::Center: {
                    // Place the selected number
                    int num = editCandidates[editCursor];
                    editingCell = false;
                    if (pencilMode && num > 0) {
                        togglePencilMark(cursorR, cursorC, num);
                    } else {
                        placeNumber(cursorR, cursorC, num);
                    }
                    needsFullRedraw = true;
                    return true;
                }
                case PluginButton::Back:
                    editingCell = false;
                    needsFullRedraw = true;
                    return true;
                default:
                    return true;
            }
        }
        
        // Normal grid navigation
        switch (btn) {
            case PluginButton::Up:
                if (cursorR > 0) { cursorR--; }
                return true;
            case PluginButton::Down:
                if (cursorR < 8) { cursorR++; }
                return true;
            case PluginButton::Left:
                if (cursorC > 0) { cursorC--; }
                return true;
            case PluginButton::Right:
                if (cursorC < 8) { cursorC++; }
                return true;
            case PluginButton::Center:
                // Enter edit mode for this cell
                enterEditMode();
                needsFullRedraw = true;
                return true;
            case PluginButton::Back:
                pauseTimer();
                menuCursor = 0;
                currentScreen = SDK_SCREEN_PAUSED;
                needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    
    bool handlePausedInput(PluginButton btn) {
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }
                return true;
            case PluginButton::Down:
                if (menuCursor < 3) { menuCursor++; }
                return true;
            case PluginButton::Center:
                if (menuCursor == 0) {
                    // Save & Exit
                    saveGame();
                    checkSavedGame();
                    menuCursor = 0;
                    currentScreen = SDK_SCREEN_MAIN_MENU;
                } else if (menuCursor == 1) {
                    // Resume
                    resumeTimer();
                    currentScreen = SDK_SCREEN_PLAYING;
                } else if (menuCursor == 2) {
                    // Restart Puzzle
                    startNewGame(difficulty);
                    currentScreen = SDK_SCREEN_PLAYING;
                } else if (menuCursor == 3) {
                    // New Puzzle
                    menuCursor = difficulty;
                    currentScreen = SDK_SCREEN_NEW_GAME;
                }
                needsFullRedraw = true;
                return true;
            case PluginButton::Back:
                resumeTimer();
                currentScreen = SDK_SCREEN_PLAYING;
                needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    
    bool handleSettingsInput(PluginButton btn) {
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case PluginButton::Down:
                if (menuCursor < 3) { menuCursor++; }  // Partial refresh
                return true;
            case PluginButton::Center:
            case PluginButton::Left:
            case PluginButton::Right:
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
            case PluginButton::Back:
                menuCursor = hasSavedGame ? 2 : 1;
                currentScreen = SDK_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleVictoryInput(PluginButton btn) {
        switch (btn) {
            case PluginButton::Up:
            case PluginButton::Down:
                menuCursor = 1 - menuCursor;
                // Partial refresh for cursor
                return true;
            case PluginButton::Center:
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
            case PluginButton::Back:
                menuCursor = 0;
                currentScreen = SDK_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    // ==========================================================================
    // Update (for timer)
    // ==========================================================================
    bool update() {
        // Only update timer display when actually playing with timer enabled
        // and NOT doing it every frame - only when the displayed time changes
        if (currentScreen == SDK_SCREEN_PLAYING && settings.showTimer && timerRunning) {
            static uint32_t lastDisplayedTime = 0xFFFFFFFF;
            uint32_t currentTime = getElapsedTime();
            
            // Only trigger redraw if the seconds display actually changed
            if (currentTime != lastDisplayedTime) {
                lastDisplayedTime = currentTime;
                // Don't trigger full redraw - just note that we need timer update
                // But since partial updates cause flicker, skip timer-only updates
                // Timer will update on next user interaction
                return false;
            }
        }
        return false;
    }
    
    // Use partial window for smoother updates
    void drawPartial() {
        d_.setPartialWindow(0, 0, screenW, screenH);
        d_.firstPage();
        do {
            d_.fillScreen(GxEPD_WHITE);
            d_.setTextColor(GxEPD_BLACK);
            
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
        } while (d_.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawFullScreen() {
        needsFullRedraw = true;
        draw();
    }
    
    // ==========================================================================
    // Drawing
    // ==========================================================================
    void draw() override {
        d_.setFullWindow();
        d_.firstPage();
        do {
            d_.fillScreen(GxEPD_WHITE);
            
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
        } while (d_.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawMainMenu() {
        // Header
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
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
                d_.fillRoundRect(20, y, screenW - 40, itemH, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(20, y, screenW - 40, itemH, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            d_.setCursor(40, y + 26);
            d_.print(items[idx]);
            
            d_.setFont(nullptr);
            d_.setCursor(40, y + 48);
            d_.print(descs[idx]);
            
            d_.setFont(nullptr);
            d_.setCursor(screenW - 50, y + 38);
            d_.print(">");
        }
    }
    
    void drawNewGameScreen() {
        // Header
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("New Game", screenW / 2, 34);
        
        int y = 64;
        
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(24, y);
        d_.print("SELECT DIFFICULTY");
        y += 20;
        
        const char* diffDescs[] = {
            "38 clues - Great for beginners",
            "30 clues - Moderate challenge",
            "24 clues - Requires advanced techniques",
            "17 clues - Extremely difficult"
        };
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 58;
            bool hasFocus = (menuCursor == i);
            bool isSelected = (selectedDifficulty == i);
            
            if (hasFocus) {
                d_.fillRoundRect(24, itemY, screenW - 48, 52, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(24, itemY, screenW - 48, 52, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            // Radio button - filled if this difficulty is selected
            int radioX = 44;
            int radioY = itemY + 26;
            uint16_t radioColor = hasFocus ? GxEPD_WHITE : GxEPD_BLACK;
            d_.drawCircle(radioX, radioY, 8, radioColor);
            if (isSelected) {
                d_.fillCircle(radioX, radioY, 4, radioColor);
            }
            
            d_.setFont(nullptr);
            d_.setCursor(64, itemY + 22);
            d_.print(DIFF_NAMES[i]);
            
            d_.setFont(nullptr);
            d_.setCursor(64, itemY + 42);
            d_.print(diffDescs[i]);
            
            // Time estimate
            int16_t tx, ty; uint16_t tw, th;
            d_.getTextBounds(DIFF_TIMES[i], 0, 0, &tx, &ty, &tw, &th);
            d_.setCursor(screenW - 60 - tw, itemY + 32);
            d_.print(DIFF_TIMES[i]);
        }
        
        y += 4 * 58 + 20;
        
        // Start button
        bool startSel = (menuCursor == 4);
        if (startSel) {
            d_.fillRoundRect(24, y, screenW - 48, 56, 8, GxEPD_BLACK);
            d_.setTextColor(GxEPD_WHITE);
        } else {
            d_.drawRoundRect(24, y, screenW - 48, 56, 8, GxEPD_BLACK);
            d_.setTextColor(GxEPD_BLACK);
        }
        d_.setFont(nullptr);
        centerText("Start Puzzle", screenW / 2, y + 36);
    }
    
    void drawPlayingScreen() {
        // Header
        d_.fillRect(0, 0, screenW, 42, GxEPD_WHITE);
        d_.drawFastHLine(4, 41, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 40, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        
        // Difficulty (left)
        d_.setCursor(16, 28);
        d_.print(DIFF_NAMES[difficulty]);
        
        // Timer (center)
        if (settings.showTimer) {
            uint32_t t = getElapsedTime();
            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "%d:%02d", t / 60, t % 60);
            d_.setFont(nullptr);
            centerText(timeStr, screenW / 2, 30);
        }
        
        // Progress (right)
        d_.setFont(nullptr);
        char progStr[16];
        snprintf(progStr, sizeof(progStr), "%d/81", getFilledCount());
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(progStr, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(screenW - tw - 16, 28);
        d_.print(progStr);
        
        // Progress bar
        int barY = 46;
        int barH = 8;
        d_.fillRect(16, barY, screenW - 32, barH, GxEPD_WHITE);
        d_.drawRect(16, barY, screenW - 32, barH, GxEPD_BLACK);
        int fillW = ((screenW - 34) * getFilledCount()) / 81;
        d_.fillRect(17, barY + 1, fillW, barH - 2, GxEPD_BLACK);
        
        // Board
        drawBoard();
        
        // Edit popup (overlaid on board when editing)
        if (editingCell && editCandidateCount > 0) {
            drawEditPopup();
        }
        
        // Footer with controls
        int footerY = screenH - 34;
        d_.drawFastHLine(0, footerY, screenW, GxEPD_BLACK);
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        
        if (editingCell) {
            centerText("UP/DN: Choose | OK: Place | BACK: Cancel", screenW / 2, footerY + 22);
        } else {
            char footer[64];
            snprintf(footer, sizeof(footer), "OK: Edit cell | BACK: Menu%s", pencilMode ? " | [PENCIL]" : "");
            centerText(footer, screenW / 2, footerY + 22);
        }
    }
    
    void drawEditPopup() {
        int cs = cellSize;
        int cx = boardX + cursorC * cs;
        int cy = boardY + cursorR * cs;
        
        // Popup dimensions - horizontal bar of candidate numbers
        int numW = 36;
        int numH = 40;
        int popW = editCandidateCount * numW + 8;
        int popH = numH + 8;
        
        // Position popup above the cell if possible, below otherwise
        int popX = cx + cs / 2 - popW / 2;
        int popY = cy - popH - 4;
        
        // Keep within screen bounds
        if (popX < 4) popX = 4;
        if (popX + popW > screenW - 4) popX = screenW - 4 - popW;
        if (popY < 56) {
            popY = cy + cs + 4;  // Below cell instead
        }
        if (popY + popH > screenH - 40) {
            popY = screenH - 40 - popH;
        }
        
        // Background with border
        d_.fillRect(popX - 2, popY - 2, popW + 4, popH + 4, GxEPD_WHITE);
        d_.drawRect(popX - 2, popY - 2, popW + 4, popH + 4, GxEPD_BLACK);
        d_.drawRect(popX - 1, popY - 1, popW + 2, popH + 2, GxEPD_BLACK);
        
        // Draw each candidate
        for (int i = 0; i < editCandidateCount; i++) {
            int nx = popX + 4 + i * numW;
            int ny = popY + 4;
            bool sel = (i == editCursor);
            
            if (sel) {
                d_.fillRect(nx, ny, numW - 4, numH, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRect(nx, ny, numW - 4, numH, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            char numStr[4];
            if (editCandidates[i] == 0) {
                strcpy(numStr, "X");  // Clear
            } else {
                numStr[0] = '0' + editCandidates[i];
                numStr[1] = '\0';
            }
            
            d_.setFont(nullptr);
            int16_t tx, ty; uint16_t tw, th;
            d_.getTextBounds(numStr, 0, 0, &tx, &ty, &tw, &th);
            d_.setCursor(nx + (numW - 4 - tw) / 2, ny + (numH + th) / 2);
            d_.print(numStr);
        }
    }
    
    void drawBoard() {
        int ox = boardX;
        int oy = boardY;
        int cs = cellSize;
        
        // Background
        d_.fillRect(ox, oy, cs * 9, cs * 9, GxEPD_WHITE);
        
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
                    d_.fillRect(x, y, cs, cs, GxEPD_BLACK);
                }
                
                // Cell border
                d_.drawRect(x, y, cs + 1, cs + 1, GxEPD_BLACK);
                
                // Number or pencil marks
                if (board[r][c] != 0) {
                    d_.setFont(nullptr);
                    char numStr[2] = { (char)('0' + board[r][c]), '\0' };
                    
                    int16_t tx, ty; uint16_t tw, th;
                    d_.getTextBounds(numStr, 0, 0, &tx, &ty, &tw, &th);
                    
                    // Color based on state
                    bool isError = settings.highlightErrors && board[r][c] != solution[r][c] && !given[r][c];
                    
                    if (highlight) {
                        d_.setTextColor(GxEPD_WHITE);
                    } else if (given[r][c]) {
                        d_.setTextColor(GxEPD_BLACK);
                    } else {
                        d_.setTextColor(GxEPD_BLACK);
                    }
                    
                    d_.setCursor(x + (cs - tw) / 2, y + (cs + th) / 2);
                    d_.print(numStr);
                    
                    // Error indicator
                    if (isError) {
                        d_.fillCircle(x + cs - 6, y + 6, 4, highlight ? GxEPD_WHITE : GxEPD_BLACK);
                    }
                } else if (pencilMarks[r][c] != 0) {
                    // Draw pencil marks
                    d_.setFont(NULL);
                    d_.setTextSize(1);
                    d_.setTextColor(GxEPD_BLACK);
                    
                    int markSize = cs / 3;
                    for (int n = 1; n <= 9; n++) {
                        if (pencilMarks[r][c] & (1 << (n - 1))) {
                            int mr = (n - 1) / 3;
                            int mc = (n - 1) % 3;
                            int mx = x + mc * markSize + markSize / 2 - 2;
                            int my = y + mr * markSize + markSize / 2 + 3;
                            d_.setCursor(mx, my);
                            d_.print(n);
                        }
                    }
                }
            }
        }
        
        // 3x3 box borders (thick)
        for (int i = 0; i <= 3; i++) {
            int vx = ox + i * 3 * cs;
            d_.fillRect(vx - 1, oy, 3, cs * 9, GxEPD_BLACK);
            
            int hy = oy + i * 3 * cs;
            d_.fillRect(ox, hy - 1, cs * 9, 3, GxEPD_BLACK);
        }
        
        // Cursor
        int cx = ox + cursorC * cs;
        int cy = oy + cursorR * cs;
        d_.drawRect(cx - 2, cy - 2, cs + 4, cs + 4, GxEPD_BLACK);
        d_.drawRect(cx - 3, cy - 3, cs + 6, cs + 6, GxEPD_BLACK);
        d_.drawRect(cx - 4, cy - 4, cs + 8, cs + 8, GxEPD_BLACK);
    }
    
    void drawPausedMenu() {
        int dialogW = 280;
        int dialogH = 260;
        int dialogX = (screenW - dialogW) / 2;
        int dialogY = (screenH - dialogH) / 2;
        
        d_.fillRect(dialogX, dialogY, dialogW, dialogH, GxEPD_WHITE);
        for (int i = 0; i < 3; i++) {
            d_.drawRect(dialogX + i, dialogY + i, dialogW - i*2, dialogH - i*2, GxEPD_BLACK);
        }
        
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        centerText("Game Paused", dialogX + dialogW / 2, dialogY + 36);
        
        const char* items[] = { "Save & Exit", "Resume", "Restart Puzzle", "New Puzzle" };
        int itemY = dialogY + 55;
        int itemH = 44;
        
        for (int i = 0; i < 4; i++) {
            int iy = itemY + i * (itemH + 6);
            bool sel = (menuCursor == i);
            
            if (sel) {
                d_.fillRoundRect(dialogX + 16, iy, dialogW - 32, itemH, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(dialogX + 16, iy, dialogW - 32, itemH, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            centerText(items[i], dialogX + dialogW / 2, iy + 28);
        }
    }
    
    void drawSettingsScreen() {
        // Header
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("Settings", screenW / 2, 34);
        
        int y = 64;
        
        // Options
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(20, y);
        d_.print("OPTIONS");
        y += 20;
        
        const char* opts[] = { "Show timer", "Highlight same numbers", "Highlight errors", "Auto-remove pencil marks" };
        bool vals[] = { settings.showTimer, settings.highlightSame, settings.highlightErrors, settings.autoRemovePencil };
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 54;
            bool sel = (menuCursor == i);
            
            d_.drawRoundRect(20, itemY, screenW - 40, 48, 6, GxEPD_BLACK);
            if (sel) {
                d_.drawRoundRect(18, itemY - 2, screenW - 36, 52, 8, GxEPD_BLACK);
                d_.drawRoundRect(17, itemY - 3, screenW - 34, 54, 9, GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            d_.setTextColor(GxEPD_BLACK);
            d_.setCursor(35, itemY + 30);
            d_.print(opts[i]);
            
            // Toggle
            int sw = 44, sh = 24;
            int sx = screenW - 70;
            int sy = itemY + 12;
            
            if (vals[i]) {
                d_.fillRoundRect(sx, sy, sw, sh, sh/2, GxEPD_BLACK);
                d_.fillCircle(sx + sw - sh/2, sy + sh/2, 8, GxEPD_WHITE);
            } else {
                d_.drawRoundRect(sx, sy, sw, sh, sh/2, GxEPD_BLACK);
                d_.fillCircle(sx + sh/2, sy + sh/2, 8, GxEPD_BLACK);
            }
        }
        
        y += 4 * 54 + 20;
        
        // Statistics
        d_.setFont(nullptr);
        d_.setCursor(20, y);
        d_.print("STATISTICS");
        y += 20;
        
        int boxW = (screenW - 60) / 3;
        const char* labels[] = { "Played", "Won", "Streak" };
        int values[] = { stats.gamesPlayed, stats.gamesWon, stats.currentStreak };
        
        for (int i = 0; i < 3; i++) {
            int bx = 20 + i * (boxW + 10);
            d_.drawRoundRect(bx, y, boxW, 56, 6, GxEPD_BLACK);
            
            d_.setFont(nullptr);
            char numStr[8];
            snprintf(numStr, sizeof(numStr), "%d", values[i]);
            centerText(numStr, bx + boxW / 2, y + 28);
            
            d_.setFont(nullptr);
            centerText(labels[i], bx + boxW / 2, y + 48);
        }
        
        y += 70;
        
        // Best times
        d_.setFont(nullptr);
        d_.setCursor(20, y);
        d_.print("BEST TIMES");
        y += 20;
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 32;
            d_.drawRect(20, itemY, screenW - 40, 28, GxEPD_BLACK);
            
            d_.setCursor(30, itemY + 20);
            d_.print(DIFF_NAMES[i]);
            
            char timeStr[16];
            if (stats.bestTimes[i] > 0) {
                snprintf(timeStr, sizeof(timeStr), "%d:%02d", stats.bestTimes[i] / 60, stats.bestTimes[i] % 60);
            } else {
                strcpy(timeStr, "--:--");
            }
            
            int16_t tx, ty; uint16_t tw, th;
            d_.getTextBounds(timeStr, 0, 0, &tx, &ty, &tw, &th);
            d_.setCursor(screenW - 50 - tw, itemY + 20);
            d_.print(timeStr);
        }
    }
    
    void drawVictoryScreen() {
        // Header
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("Puzzle Complete!", screenW / 2, 34);
        
        // Celebration
        int y = 70;
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        centerText("Congratulations!", screenW / 2, y);
        
        d_.setFont(nullptr);
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
            d_.drawRoundRect(bx, y, boxW, 70, 8, GxEPD_BLACK);
            
            d_.setFont(nullptr);
            centerText(values[i], bx + boxW / 2, y + 32);
            
            d_.setFont(nullptr);
            centerText(labels[i], bx + boxW / 2, y + 56);
        }
        
        y += 86;
        
        // New record?
        bool isRecord = (stats.bestTimes[difficulty] == finalTime);
        if (isRecord) {
            d_.fillRoundRect(20, y, screenW - 40, 40, 6, GxEPD_BLACK);
            d_.setTextColor(GxEPD_WHITE);
            d_.setFont(nullptr);
            char recordStr[48];
            snprintf(recordStr, sizeof(recordStr), "New personal best for %s!", DIFF_NAMES[difficulty]);
            centerText(recordStr, screenW / 2, y + 26);
            y += 52;
        } else {
            y += 8;
        }
        
        // Streak
        d_.drawRoundRect(20, y, screenW - 40, 60, 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        
        d_.setFont(nullptr);
        d_.setCursor(35, y + 20);
        d_.print("Current Streak");
        
        d_.setFont(nullptr);
        char streakStr[16];
        snprintf(streakStr, sizeof(streakStr), "%d days", stats.currentStreak);
        d_.setCursor(35, y + 46);
        d_.print(streakStr);
        
        d_.setFont(nullptr);
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds("Best Streak", 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(screenW - 35 - tw, y + 20);
        d_.print("Best Streak");
        
        d_.setFont(nullptr);
        snprintf(streakStr, sizeof(streakStr), "%d days", stats.bestStreak);
        d_.getTextBounds(streakStr, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(screenW - 35 - tw, y + 46);
        d_.print(streakStr);
        
        y += 76;
        
        // Buttons
        int btnW = (screenW - 52) / 2;
        
        for (int i = 0; i < 2; i++) {
            int bx = 20 + i * (btnW + 12);
            bool sel = (menuCursor == i);
            
            if (sel) {
                d_.fillRoundRect(bx, y, btnW, 50, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(bx, y, btnW, 50, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            centerText(i == 0 ? "Next Puzzle" : "Main Menu", bx + btnW / 2, y + 32);
        }
    }
    
    void drawMiniBoard(int x, int y, int size) {
        int cs = size / 9;
        
        d_.drawRect(x, y, size, size, GxEPD_BLACK);
        
        // Dithered squares
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                // Subtle pattern
                if ((r / 3 + c / 3) % 2 == 1) {
                    int sx = x + c * cs;
                    int sy = y + r * cs;
                    for (int py = 0; py < cs; py += 2) {
                        for (int px = 0; px < cs; px += 2) {
                            d_.drawPixel(sx + px, sy + py, GxEPD_BLACK);
                        }
                    }
                }
            }
        }
        
        // 3x3 borders
        for (int i = 0; i <= 3; i++) {
            d_.fillRect(x + i * 3 * cs - 1, y, 2, size, GxEPD_BLACK);
            d_.fillRect(x, y + i * 3 * cs - 1, size, 2, GxEPD_BLACK);
        }
    }
    
    void centerText(const char* text, int x, int y) {
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(x - tw / 2, y);
        d_.print(text);
    }
  PluginRenderer& d_;
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
