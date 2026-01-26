/**
 * @file Sudoku.cpp
 * @brief Sudoku puzzle game with inline number cycling
 * @version 1.3.0
 * 
 * Features:
 * - Navigate with D-pad, press OK to enter edit mode
 * - In edit mode: UP/DOWN cycles valid numbers, OK confirms, BACK cancels
 * - Only shows numbers that are valid for each cell
 * - Partial refresh for smooth cursor movement
 */

#include "plugins/Sudoku.h"

#if FEATURE_GAMES


// =============================================================================
// Constructor
// =============================================================================

SudokuGame::SudokuGame() { 
    reset(); 
    _menuState = MENU_NONE;
    _menuCursor = 0;
    _actionCount = 0;
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
    
    // Check for saved game
    if (hasSavedGame()) {
        _menuState = MENU_RESUME_PROMPT;
        _menuCursor = 0;  // Default to Yes (resume)
        _needsFullRedraw = true;
    } else {
        newGame(1);
    }
}

void SudokuGame::newGame(int difficulty) {
    reset();
    generatePuzzle(difficulty);
    _state = GAME_PLAYING;
    _cursorR = 4; _cursorC = 4;
    _prevCursorR = _cursorR; _prevCursorC = _cursorC;
    _selectedNum = 1;
    _inputMode = false;
    _savedValue = 0;
    _menuState = MENU_NONE;
    _needsFullRedraw = true;
    _actionCount = 0;
    memset(_dirtyCells, 0, sizeof(_dirtyCells));
    _anyDirty = false;
}

// Get list of valid numbers for a cell (0 = clear, plus valid 1-9)
void SudokuGame::getValidNumbers(int r, int c, uint8_t* validNums, int& count) {
    count = 0;
    
    // 0 is always valid (means clear the cell)
    validNums[count++] = 0;
    
    // Check which numbers 1-9 are valid
    for (uint8_t num = 1; num <= 9; num++) {
        // Temporarily clear cell to check validity
        uint8_t oldVal = _board[r][c];
        _board[r][c] = 0;
        
        if (isValid(r, c, num)) {
            validNums[count++] = num;
        }
        
        // Restore
        _board[r][c] = oldVal;
    }
}

// Find next valid number in cycle direction
uint8_t SudokuGame::cycleNumber(int r, int c, int direction) {
    uint8_t validNums[10];
    int count;
    getValidNumbers(r, c, validNums, count);
    
    if (count <= 1) return 0;  // Only 0 is valid (shouldn't happen in a valid puzzle)
    
    // Find current value in valid list
    uint8_t current = _board[r][c];
    int currentIdx = -1;
    for (int i = 0; i < count; i++) {
        if (validNums[i] == current) {
            currentIdx = i;
            break;
        }
    }
    
    // If current value not in valid list (shouldn't happen), start at 0
    if (currentIdx < 0) currentIdx = 0;
    
    // Cycle to next/prev
    int newIdx = currentIdx + direction;
    if (newIdx >= count) newIdx = 0;
    if (newIdx < 0) newIdx = count - 1;
    
    return validNums[newIdx];
}

bool SudokuGame::handleInput(Button btn) {
    // Handle resume prompt menu
    if (_menuState == MENU_RESUME_PROMPT) {
        switch (btn) {
            case BTN_LEFT:
            case BTN_RIGHT:
                _menuCursor = 1 - _menuCursor;
                _needsFullRedraw = true;
                return true;
            case BTN_CONFIRM:
                if (_menuCursor == 0) {
                    // Resume saved game
                    loadGame();
                } else {
                    // Start new game
                    deleteSavedGame();
                    newGame(1);
                }
                _menuState = MENU_NONE;
                _needsFullRedraw = true;
                return true;
            case BTN_BACK:
                // Treat as "No" - start new game
                deleteSavedGame();
                newGame(1);
                return true;
            default:
                return true;
        }
    }
    
    if (_state == GAME_WIN) {
        if (btn == BTN_CONFIRM) { 
            deleteSavedGame();
            newGame(1); 
            return true; 
        }
        return false;
    }
    
    // Save previous cursor for dirty tracking
    _prevCursorR = _cursorR;
    _prevCursorC = _cursorC;
    
    // INPUT MODE - cycling through numbers in selected cell
    if (_inputMode) {
        bool canEdit = !_fixed[_cursorR][_cursorC];
        
        switch (btn) {
            case BTN_UP:
                if (canEdit) {
                    // Cycle number UP
                    uint8_t newNum = cycleNumber(_cursorR, _cursorC, 1);
                    _board[_cursorR][_cursorC] = newNum;
                    markCellDirty(_cursorR, _cursorC);
                    _actionCount++;
                }
                break;
                
            case BTN_DOWN:
                if (canEdit) {
                    // Cycle number DOWN
                    uint8_t newNum = cycleNumber(_cursorR, _cursorC, -1);
                    _board[_cursorR][_cursorC] = newNum;
                    markCellDirty(_cursorR, _cursorC);
                    _actionCount++;
                }
                break;
                
            case BTN_CONFIRM:
                // Confirm number and exit input mode
                _inputMode = false;
                saveGame();
                checkWin();
                _needsFullRedraw = true;  // Redraw to show normal cursor
                return true;
                
            case BTN_BACK:
                // Cancel - restore original value and exit input mode
                _board[_cursorR][_cursorC] = _savedValue;
                _inputMode = false;
                markCellDirty(_cursorR, _cursorC);
                _needsFullRedraw = true;
                return true;
                
            case BTN_LEFT:
            case BTN_RIGHT:
                // Ignore L/R in input mode
                return true;
                
            default:
                return true;
        }
        
        // Smooth refresh every 4 number changes
        if (_actionCount >= 4) {
            _needsFullRedraw = true;
            _actionCount = 0;
        }
        return true;
    }
    
    // NAVIGATION MODE - moving cursor around grid
    switch (btn) {
        case BTN_UP:    
            if (_cursorR > 0) { 
                _cursorR--; 
                markCursorDirty(); 
                _actionCount++;
            } 
            break;
        case BTN_DOWN:  
            if (_cursorR < 8) { 
                _cursorR++; 
                markCursorDirty(); 
                _actionCount++;
            } 
            break;
        case BTN_LEFT:  
            if (_cursorC > 0) { 
                _cursorC--; 
                markCursorDirty(); 
                _actionCount++;
            } 
            break;
        case BTN_RIGHT: 
            if (_cursorC < 8) { 
                _cursorC++; 
                markCursorDirty(); 
                _actionCount++;
            } 
            break;
        case BTN_CONFIRM:
            // Enter input mode if cell is editable
            if (!_fixed[_cursorR][_cursorC]) {
                _inputMode = true;
                _savedValue = _board[_cursorR][_cursorC];  // Save in case of cancel
                _needsFullRedraw = true;  // Redraw to show input mode cursor
            }
            return true;
        case BTN_BACK:
            // Exit game
            return false;
        default: 
            return false;
    }
    
    // Smooth full-screen partial refresh every 4 movements
    if (_actionCount >= 4) {
        _needsFullRedraw = true;
        _actionCount = 0;
    }
    
    return true;
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
    _menuState = MENU_NONE;
    _menuCursor = 0;
    _anyDirty = false;
    _needsFullRedraw = true;
    _actionCount = 0;
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
    // Use partial window for smooth refresh without black flash
    display.setPartialWindow(0, 0, _screenW, _screenH);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        if (_menuState == MENU_RESUME_PROMPT) {
            // Draw menu overlay
            PluginUI::drawHeader("Sudoku", _screenW);
            drawMenu();
        } else {
            drawContent();
        }
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
    
    // Draw instructions at bottom
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    const char* hint = _inputMode ? "U/D: Number  OK: Set" : "Move  OK: Edit";
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(hint, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((_screenW - tw) / 2, _screenH - 8);
    display.print(hint);
    
    // Draw all cells
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            drawCell(r, c);
        }
    }
    
    // Draw thick 3x3 box borders
    drawThickBorders(0, 8, 0, 8);
    
    // Draw win message if applicable
    if (_state == GAME_WIN) {
        int msgW = 160;
        int msgH = 50;
        int msgX = (_screenW - msgW) / 2;
        int msgY = (_screenH - msgH) / 2;
        
        display.fillRect(msgX - 2, msgY - 2, msgW + 4, msgH + 4, GxEPD_WHITE);
        display.drawRect(msgX, msgY, msgW, msgH, GxEPD_BLACK);
        display.drawRect(msgX+1, msgY+1, msgW-2, msgH-2, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        const char* winMsg = "You Win!";
        display.getTextBounds(winMsg, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(msgX + (msgW - tw) / 2, msgY + msgH/2 + 6);
        display.print(winMsg);
    }
}

void SudokuGame::drawCell(int r, int c) {
    int cs = _grid.cellSize;
    int x = _grid.offsetX + c * cs;
    int y = _grid.offsetY + r * cs;
    
    bool isCursor = (r == _cursorR && c == _cursorC);
    bool isFixed = _fixed[r][c];
    uint8_t val = _board[r][c];
    bool isInputMode = isCursor && _inputMode;
    
    // Cell background - inverted when in input mode
    if (isInputMode) {
        display.fillRect(x, y, cs, cs, GxEPD_BLACK);
    } else {
        display.fillRect(x, y, cs, cs, GxEPD_WHITE);
    }
    
    // Thin grid line (skip if input mode - already filled black)
    if (!isInputMode) {
        display.drawRect(x, y, cs, cs, GxEPD_BLACK);
    }
    
    // Cursor highlight - thick border (only in navigation mode)
    if (isCursor && !_inputMode) {
        display.drawRect(x + 1, y + 1, cs - 2, cs - 2, GxEPD_BLACK);
        display.drawRect(x + 2, y + 2, cs - 4, cs - 4, GxEPD_BLACK);
        display.drawRect(x + 3, y + 3, cs - 6, cs - 6, GxEPD_BLACK);
    }
    
    // Draw number
    if (val > 0) {
        // Fixed numbers in bold, user numbers in regular
        if (isFixed) {
            display.setFont(&FreeSansBold12pt7b);
        } else {
            display.setFont(&FreeSans12pt7b);
        }
        
        // Invert text color in input mode
        display.setTextColor(isInputMode ? GxEPD_WHITE : GxEPD_BLACK);
        
        char s[2] = { (char)('0' + val), '\0' };
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(s, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(x + (cs - tw) / 2 - tx, y + (cs + th) / 2);
        display.print(s);
    }
}

void SudokuGame::drawThickBorders(int minR, int maxR, int minC, int maxC) {
    int cs = _grid.cellSize;
    int ox = _grid.offsetX;
    int oy = _grid.offsetY;
    int gridW = cs * 9;
    int gridH = cs * 9;
    
    // Outer border (thick)
    for (int t = 0; t < 3; t++) {
        display.drawRect(ox - t, oy - t, gridW + t*2, gridH + t*2, GxEPD_BLACK);
    }
    
    // Inner 3x3 box borders
    for (int i = 1; i < 3; i++) {
        // Vertical lines
        int vx = ox + i * 3 * cs;
        if (vx >= ox + minC * cs - 4 && vx <= ox + (maxC + 1) * cs + 4) {
            display.fillRect(vx - 1, oy, 3, gridH, GxEPD_BLACK);
        }
        // Horizontal lines
        int hy = oy + i * 3 * cs;
        if (hy >= oy + minR * cs - 4 && hy <= oy + (maxR + 1) * cs + 4) {
            display.fillRect(ox, hy - 1, gridW, 3, GxEPD_BLACK);
        }
    }
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
    _needsFullRedraw = true;
}

// =============================================================================
// Save/Load Functions
// =============================================================================

bool SudokuGame::hasSavedGame() {
    return SD.exists(SUDOKU_SAVE_PATH);
}

bool SudokuGame::saveGame() {
    SD.mkdir("/.sumi");
    File f = SD.open(SUDOKU_SAVE_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("[SUDOKU] Failed to open save file");
        return false;
    }
    
    SudokuSaveData save;
    memcpy(save.board, _board, sizeof(_board));
    memcpy(save.solution, _solution, sizeof(_solution));
    memcpy(save.fixed, _fixed, sizeof(_fixed));
    save.cursorR = _cursorR;
    save.cursorC = _cursorC;
    memset(save.reserved, 0, sizeof(save.reserved));
    
    f.write((uint8_t*)&save, sizeof(SudokuSaveData));
    f.close();
    
    return true;
}

bool SudokuGame::loadGame() {
    File f = SD.open(SUDOKU_SAVE_PATH, FILE_READ);
    if (!f) {
        Serial.println("[SUDOKU] No save file found");
        return false;
    }
    
    SudokuSaveData save;
    f.read((uint8_t*)&save, sizeof(SudokuSaveData));
    f.close();
    
    if (!save.isValid()) {
        Serial.println("[SUDOKU] Invalid save file");
        deleteSavedGame();
        return false;
    }
    
    memcpy(_board, save.board, sizeof(_board));
    memcpy(_solution, save.solution, sizeof(_solution));
    memcpy(_fixed, save.fixed, sizeof(_fixed));
    _cursorR = save.cursorR;
    _cursorC = save.cursorC;
    _prevCursorR = _cursorR;
    _prevCursorC = _cursorC;
    
    _state = GAME_PLAYING;
    _inputMode = false;
    _selectedNum = 1;
    _menuState = MENU_NONE;
    _needsFullRedraw = true;
    _actionCount = 0;
    memset(_dirtyCells, 0, sizeof(_dirtyCells));
    _anyDirty = false;
    
    Serial.println("[SUDOKU] Game loaded");
    return true;
}

void SudokuGame::deleteSavedGame() {
    if (SD.exists(SUDOKU_SAVE_PATH)) {
        SD.remove(SUDOKU_SAVE_PATH);
        Serial.println("[SUDOKU] Save file deleted");
    }
}

// =============================================================================
// Menu Drawing
// =============================================================================

void SudokuGame::drawMenu() {
    int dialogW = 220;
    int dialogH = 100;
    int dialogX = (_screenW - dialogW) / 2;
    int dialogY = (_screenH - dialogH) / 2;
    
    // Dialog background
    display.fillRect(dialogX - 2, dialogY - 2, dialogW + 4, dialogH + 4, GxEPD_WHITE);
    display.drawRect(dialogX, dialogY, dialogW, dialogH, GxEPD_BLACK);
    display.drawRect(dialogX + 1, dialogY + 1, dialogW - 2, dialogH - 2, GxEPD_BLACK);
    
    // Title
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    const char* title = "Resume Game?";
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(dialogX + (dialogW - tw) / 2, dialogY + 28);
    display.print(title);
    
    // Subtitle
    display.setFont(&FreeSans9pt7b);
    const char* subtitle = "Found a saved puzzle";
    display.getTextBounds(subtitle, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(dialogX + (dialogW - tw) / 2, dialogY + 48);
    display.print(subtitle);
    
    // Buttons
    int btnW = 70;
    int btnH = 28;
    int btnY = dialogY + dialogH - 38;
    int btnSpacing = 20;
    int yesX = dialogX + (dialogW / 2) - btnW - (btnSpacing / 2);
    int noX = dialogX + (dialogW / 2) + (btnSpacing / 2);
    
    // Yes button
    if (_menuCursor == 0) {
        display.fillRoundRect(yesX, btnY, btnW, btnH, 4, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
    } else {
        display.drawRoundRect(yesX, btnY, btnW, btnH, 4, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
    }
    display.setCursor(yesX + 20, btnY + 20);
    display.print("Yes");
    
    // No button
    if (_menuCursor == 1) {
        display.fillRoundRect(noX, btnY, btnW, btnH, 4, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
    } else {
        display.drawRoundRect(noX, btnY, btnW, btnH, 4, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
    }
    display.setCursor(noX + 24, btnY + 20);
    display.print("No");
    
    display.setTextColor(GxEPD_BLACK);
}

// Unused but kept for header compatibility
bool SudokuGame::handleNumberInput(Button btn) { return false; }
void SudokuGame::drawNumberPicker() {}

#endif // FEATURE_GAMES
