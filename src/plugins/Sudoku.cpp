/**
 * @file Sudoku.cpp
 * @brief Sudoku puzzle game with smooth partial refresh
 * @version 2.1.23
 * 
 * Features:
 * - Partial refresh for smooth cursor movement
 * - Proper portrait button mapping
 * - Number picker overlay
 */

#include "plugins/Sudoku.h"

#if FEATURE_GAMES

SudokuGame sudokuGame;

// =============================================================================
// Constructor
// =============================================================================

SudokuGame::SudokuGame() { 
    reset(); 
}

// =============================================================================
// Public Methods
// =============================================================================

void SudokuGame::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _grid = calculateGrid(screenW, screenH, 9, 9, true, true);
    Serial.printf("[SUDOKU] Init: %dx%d cell=%d landscape=%d\n", screenW, screenH, _grid.cellSize, _landscape);
    newGame(1);
}

void SudokuGame::newGame(int difficulty) {
    reset();
    generatePuzzle(difficulty);
    _state = GAME_PLAYING;
    _cursorR = 4; _cursorC = 4;
    _prevCursorR = _cursorR; _prevCursorC = _cursorC;
    _selectedNum = 1;
    _inputMode = false;
    _needsFullRedraw = true;
    memset(_dirtyCells, 0, sizeof(_dirtyCells));
    _anyDirty = false;
}

bool SudokuGame::handleInput(Button btn) {
    // Use raw buttons directly in portrait mode - matches main menu behavior
    // UP = move up on screen, DOWN = move down, etc.
    // No remapping needed because the grid is displayed in screen coordinates
    
    if (_state == GAME_WIN) {
        if (btn == BTN_CONFIRM) { newGame(1); return true; }
        return false;
    }
    
    if (_inputMode) {
        return handleNumberInput(btn);
    }
    
    // Save previous cursor for dirty tracking
    _prevCursorR = _cursorR;
    _prevCursorC = _cursorC;
    
    switch (btn) {
        case BTN_UP:    
            if (_cursorR > 0) { 
                _cursorR--; 
                markCursorDirty(); 
            } 
            return true;
        case BTN_DOWN:  
            if (_cursorR < 8) { 
                _cursorR++; 
                markCursorDirty(); 
            } 
            return true;
        case BTN_LEFT:  
            if (_cursorC > 0) { 
                _cursorC--; 
                markCursorDirty(); 
            } 
            return true;
        case BTN_RIGHT: 
            if (_cursorC < 8) { 
                _cursorC++; 
                markCursorDirty(); 
            } 
            return true;
        case BTN_CONFIRM:
            if (!_fixed[_cursorR][_cursorC]) {
                _inputMode = true;
                _selectedNum = _board[_cursorR][_cursorC];
                if (_selectedNum == 0) _selectedNum = 1;
                _needsFullRedraw = true;  // Show number picker overlay
            }
            return true;
        case BTN_BACK:
            return false;
        default: 
            return false;
    }
}

void SudokuGame::draw() {
    if (_needsFullRedraw) {
        drawFull();
        _needsFullRedraw = false;
        memset(_dirtyCells, 0, sizeof(_dirtyCells));
        _anyDirty = false;
    } else if (_anyDirty) {
        drawPartial();
        memset(_dirtyCells, 0, sizeof(_dirtyCells));
        _anyDirty = false;
    }
}

// =============================================================================
// Private Methods
// =============================================================================

void SudokuGame::reset() {
    memset(_board, 0, sizeof(_board));
    memset(_fixed, 0, sizeof(_fixed));
    memset(_dirtyCells, 0, sizeof(_dirtyCells));
    _state = GAME_PLAYING;
    _inputMode = false;
    _anyDirty = false;
    _needsFullRedraw = true;
}

void SudokuGame::markCellDirty(int r, int c) {
    if (r >= 0 && r < 9 && c >= 0 && c < 9) {
        _dirtyCells[r][c] = true;
        _anyDirty = true;
    }
}

void SudokuGame::markCursorDirty() {
    markCellDirty(_prevCursorR, _prevCursorC);
    markCellDirty(_cursorR, _cursorC);
}

void SudokuGame::drawFull() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawContent();
    } while (display.nextPage());
}

void SudokuGame::drawPartial() {
    // Find bounding box of dirty cells
    int minR = 9, maxR = -1, minC = 9, maxC = -1;
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (_dirtyCells[r][c]) {
                if (r < minR) minR = r;
                if (r > maxR) maxR = r;
                if (c < minC) minC = c;
                if (c > maxC) maxC = c;
            }
        }
    }
    
    if (maxR < 0) return;  // Nothing dirty
    
    int cs = _grid.cellSize;
    
    // Calculate partial window with margin for cursor
    int x = _grid.offsetX + minC * cs - 4;
    int y = _grid.offsetY + minR * cs - 4;
    int w = (maxC - minC + 1) * cs + 8;
    int h = (maxR - minR + 1) * cs + 8;
    
    // Clamp to screen
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > _screenW) w = _screenW - x;
    if (y + h > _screenH) h = _screenH - y;
    
    display.setPartialWindow(x, y, w, h);
    display.firstPage();
    do {
        // Redraw affected cells
        for (int r = minR; r <= maxR; r++) {
            for (int c = minC; c <= maxC; c++) {
                drawCell(r, c);
            }
        }
        // Redraw thick borders if needed
        drawThickBorders(minR, maxR, minC, maxC);
    } while (display.nextPage());
}

void SudokuGame::drawContent() {
    PluginUI::drawHeader("Sudoku", _screenW);
    
    // Draw all cells
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            drawCell(r, c);
        }
    }
    
    // Draw thick 3x3 box borders
    drawThickBorders(0, 8, 0, 8);
    
    // Number input overlay
    if (_inputMode) {
        drawNumberPicker();
    }
    
    // Footer
    const char* status = _inputMode ? "Select number" : "Navigate";
    PluginUI::drawFooter(status, "OK:Enter", _screenW, _screenH);
    
    if (_state == GAME_WIN) {
        PluginUI::drawGameOver("Puzzle Solved!", "", _screenW, _screenH);
    }
}

void SudokuGame::drawCell(int r, int c) {
    int cs = _grid.cellSize;
    int x = _grid.offsetX + c * cs;
    int y = _grid.offsetY + r * cs;
    
    // Fill cell background (white)
    display.fillRect(x, y, cs, cs, GxEPD_WHITE);
    
    // Draw cell border
    display.drawRect(x, y, cs, cs, GxEPD_BLACK);
    
    // Draw number if present
    uint8_t val = _board[r][c];
    if (val > 0) {
        char s[2] = { (char)('0' + val), '\0' };
        display.setTextColor(GxEPD_BLACK);
        
        // Fixed numbers could be bold/different, but keeping simple
        PluginUI::drawTextCentered(s, x, y, cs, cs);
    }
    
    // Draw cursor if this is the cursor cell
    if (r == _cursorR && c == _cursorC) {
        PluginUI::drawCursor(x, y, cs, cs);
    }
}

void SudokuGame::drawThickBorders(int minR, int maxR, int minC, int maxC) {
    int cs = _grid.cellSize;
    int boxSize = cs * 3;
    
    // Draw thick 3x3 box borders
    for (int i = 0; i <= 3; i++) {
        int bx = _grid.offsetX + i * boxSize;
        int by = _grid.offsetY + i * boxSize;
        
        // Vertical lines
        if (i * 3 >= minC && i * 3 <= maxC + 1) {
            display.drawLine(bx, _grid.offsetY, bx, _grid.offsetY + 9*cs, GxEPD_BLACK);
            display.drawLine(bx+1, _grid.offsetY, bx+1, _grid.offsetY + 9*cs, GxEPD_BLACK);
        }
        
        // Horizontal lines
        if (i * 3 >= minR && i * 3 <= maxR + 1) {
            display.drawLine(_grid.offsetX, by, _grid.offsetX + 9*cs, by, GxEPD_BLACK);
            display.drawLine(_grid.offsetX, by+1, _grid.offsetX + 9*cs, by+1, GxEPD_BLACK);
        }
    }
}

bool SudokuGame::handleNumberInput(Button btn) {
    switch (btn) {
        case BTN_UP:
            _selectedNum = (_selectedNum == 9) ? 0 : _selectedNum + 1;
            _needsFullRedraw = true;
            return true;
        case BTN_DOWN:
            _selectedNum = (_selectedNum == 0) ? 9 : _selectedNum - 1;
            _needsFullRedraw = true;
            return true;
        case BTN_LEFT:
            _selectedNum = (_selectedNum == 0) ? 9 : _selectedNum - 1;
            _needsFullRedraw = true;
            return true;
        case BTN_RIGHT:
            _selectedNum = (_selectedNum >= 9) ? 0 : _selectedNum + 1;
            _needsFullRedraw = true;
            return true;
        case BTN_CONFIRM:
            _board[_cursorR][_cursorC] = _selectedNum;
            _inputMode = false;
            _needsFullRedraw = true;
            checkWin();
            return true;
        case BTN_BACK:
            _inputMode = false;
            _needsFullRedraw = true;
            return true;
        default:
            return false;
    }
}

void SudokuGame::drawNumberPicker() {
    int pw = 220;
    int ph = 50;
    int px = (_screenW - pw) / 2;
    int py = (_screenH - ph) / 2;
    
    // Background
    display.fillRect(px - 2, py - 2, pw + 4, ph + 4, GxEPD_WHITE);
    display.drawRect(px, py, pw, ph, GxEPD_BLACK);
    display.drawRect(px+1, py+1, pw-2, ph-2, GxEPD_BLACK);
    
    int numW = pw / 10;
    for (int i = 0; i <= 9; i++) {
        int nx = px + i * numW;
        if (i == _selectedNum) {
            display.fillRect(nx + 2, py + 8, numW - 4, 34, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.setTextColor(GxEPD_BLACK);
        }
        char s[2] = { i == 0 ? 'X' : (char)('0' + i), '\0' };
        display.setCursor(nx + numW/2 - 5, py + 32);
        display.print(s);
    }
    display.setTextColor(GxEPD_BLACK);
}

void SudokuGame::generatePuzzle(int difficulty) {
    solveSudoku();
    memcpy(_solution, _board, sizeof(_board));
    
    // Remove more cells for higher difficulty
    int toRemove = 35 + difficulty * 10;
    int removed = 0;
    
    while (removed < toRemove) {
        int r = random(0, 9);
        int c = random(0, 9);
        if (_board[r][c] != 0) {
            _board[r][c] = 0;
            removed++;
        }
    }
    
    // Mark remaining numbers as fixed
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            _fixed[r][c] = (_board[r][c] != 0);
        }
    }
}

void SudokuGame::solveSudoku() {
    memset(_board, 0, sizeof(_board));
    
    // Fill diagonal boxes first (they're independent)
    for (int box = 0; box < 3; box++) {
        fillBox(box * 3, box * 3);
    }
    
    solve(0, 0);
}

void SudokuGame::fillBox(int startR, int startC) {
    uint8_t nums[] = {1,2,3,4,5,6,7,8,9};
    // Shuffle
    for (int i = 8; i > 0; i--) {
        int j = random(0, i + 1);
        uint8_t tmp = nums[i];
        nums[i] = nums[j];
        nums[j] = tmp;
    }
    
    int idx = 0;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            _board[startR + r][startC + c] = nums[idx++];
        }
    }
}

bool SudokuGame::solve(int r, int c) {
    if (r == 9) return true;
    if (c == 9) return solve(r + 1, 0);
    if (_board[r][c] != 0) return solve(r, c + 1);
    
    uint8_t nums[] = {1,2,3,4,5,6,7,8,9};
    // Shuffle for variety
    for (int i = 8; i > 0; i--) {
        int j = random(0, i + 1);
        uint8_t tmp = nums[i];
        nums[i] = nums[j];
        nums[j] = tmp;
    }
    
    for (int i = 0; i < 9; i++) {
        if (isValid(r, c, nums[i])) {
            _board[r][c] = nums[i];
            if (solve(r, c + 1)) return true;
            _board[r][c] = 0;
        }
    }
    return false;
}

bool SudokuGame::isValid(int r, int c, uint8_t num) {
    // Check row
    for (int i = 0; i < 9; i++) {
        if (_board[r][i] == num) return false;
    }
    // Check column
    for (int i = 0; i < 9; i++) {
        if (_board[i][c] == num) return false;
    }
    // Check 3x3 box
    int boxR = (r / 3) * 3;
    int boxC = (c / 3) * 3;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (_board[boxR + i][boxC + j] == num) return false;
        }
    }
    return true;
}

void SudokuGame::checkWin() {
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (_board[r][c] == 0) return;
            if (_board[r][c] != _solution[r][c]) return;
        }
    }
    _state = GAME_WIN;
}

#endif // FEATURE_GAMES
