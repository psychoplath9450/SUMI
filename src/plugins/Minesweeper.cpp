/**
 * @file Minesweeper.cpp
 * @brief Minesweeper game implementation
 * @version 2.1.26
 * 
 * Optimized with raw button handling and partial refresh support
 */

#include "plugins/Minesweeper.h"

#if FEATURE_GAMES

MinesweeperGame minesweeperGame;

MinesweeperGame::MinesweeperGame() { 
    reset(); 
}

void MinesweeperGame::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _grid = calculateGrid(screenW, screenH, COLS, ROWS, true, true);
    Serial.printf("[MINES] Init: %dx%d cell=%d\n", screenW, screenH, _grid.cellSize);
    newGame();
}

void MinesweeperGame::newGame() {
    reset();
    placeMines();
    calculateNumbers();
    _state = GAME_PLAYING;
    _cursorR = ROWS / 2;
    _cursorC = COLS / 2;
    _flagMode = false;
    _firstClick = true;
    _flagCount = 0;
}

bool MinesweeperGame::handleInput(Button btn) {
    // Use raw buttons - no remapping
    
    if (_state == GAME_WIN || _state == GAME_OVER) {
        if (btn == BTN_CONFIRM) { newGame(); return true; }
        if (btn == BTN_BACK) return false;
        return true;
    }
    
    switch (btn) {
        case BTN_UP:
            if (_cursorR > 0) _cursorR--;
            return true;
        case BTN_DOWN:
            if (_cursorR < ROWS-1) _cursorR++;
            return true;
        case BTN_LEFT:
            if (_cursorC > 0) _cursorC--;
            return true;
        case BTN_RIGHT:
            if (_cursorC < COLS-1) _cursorC++;
            return true;
        case BTN_CONFIRM:
            if (_flagMode) {
                toggleFlag(_cursorR, _cursorC);
            } else {
                reveal(_cursorR, _cursorC);
            }
            return true;
        case BTN_BACK:
            _flagMode = !_flagMode;
            return true;
        default:
            return false;
    }
}

void MinesweeperGame::draw() {
    const char* title = _flagMode ? "Mines [FLAG]" : "Minesweeper";
    PluginUI::drawHeader(title, _screenW);
    
    int cs = _grid.cellSize;
    
    // Draw grid
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = _grid.offsetX + c * cs;
            int y = _grid.offsetY + r * cs;
            
            if (_revealed[r][c]) {
                display.drawRect(x, y, cs, cs, GxEPD_BLACK);
                
                if (_mines[r][c]) {
                    // Draw mine (X)
                    display.drawLine(x+4, y+4, x+cs-4, y+cs-4, GxEPD_BLACK);
                    display.drawLine(x+cs-4, y+4, x+4, y+cs-4, GxEPD_BLACK);
                    display.drawCircle(x+cs/2, y+cs/2, cs/4, GxEPD_BLACK);
                } else if (_numbers[r][c] > 0) {
                    char s[2] = { (char)('0' + _numbers[r][c]), '\0' };
                    display.setTextColor(GxEPD_BLACK);
                    PluginUI::drawTextCentered(s, x, y, cs, cs);
                }
            } else {
                display.fillRect(x, y, cs, cs, GxEPD_BLACK);
                
                if (_flags[r][c]) {
                    display.setTextColor(GxEPD_WHITE);
                    PluginUI::drawTextCentered("F", x, y, cs, cs);
                }
            }
        }
    }
    
    // Cursor highlight
    display.setTextColor(GxEPD_BLACK);
    int cx = _grid.offsetX + _cursorC * cs;
    int cy = _grid.offsetY + _cursorR * cs;
    
    if (_revealed[_cursorR][_cursorC]) {
        PluginUI::drawCursor(cx, cy, cs, cs);
    } else {
        // Invert cursor cell
        display.fillRect(cx+2, cy+2, cs-4, cs-4, GxEPD_WHITE);
        display.drawRect(cx+2, cy+2, cs-4, cs-4, GxEPD_BLACK);
        if (_flags[_cursorR][_cursorC]) {
            display.setTextColor(GxEPD_BLACK);
            PluginUI::drawTextCentered("F", cx, cy, cs, cs);
        }
    }
    
    // Status footer
    char status[32];
    snprintf(status, sizeof(status), "Flags: %d/%d", _flagCount, MINES);
    const char* hint = _flagMode ? "OK:Flag BACK:Dig" : "OK:Dig BACK:Flag";
    PluginUI::drawFooter(status, hint, _screenW, _screenH);
    
    if (_state == GAME_WIN) {
        PluginUI::drawGameOver("You Win!", "OK to play again", _screenW, _screenH);
    } else if (_state == GAME_OVER) {
        PluginUI::drawGameOver("BOOM!", "OK to play again", _screenW, _screenH);
    }
}

void MinesweeperGame::reset() {
    memset(_mines, 0, sizeof(_mines));
    memset(_revealed, 0, sizeof(_revealed));
    memset(_flags, 0, sizeof(_flags));
    memset(_numbers, 0, sizeof(_numbers));
    _state = GAME_PLAYING;
    _flagMode = false;
    _firstClick = true;
    _flagCount = 0;
}

void MinesweeperGame::placeMines() {
    int placed = 0;
    while (placed < MINES) {
        int r = random(0, ROWS);
        int c = random(0, COLS);
        if (!_mines[r][c]) {
            _mines[r][c] = true;
            placed++;
        }
    }
}

void MinesweeperGame::calculateNumbers() {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (_mines[r][c]) continue;
            
            int count = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                        if (_mines[nr][nc]) count++;
                    }
                }
            }
            _numbers[r][c] = count;
        }
    }
}

void MinesweeperGame::reveal(int r, int c) {
    if (_flags[r][c]) return;
    if (_revealed[r][c]) return;
    
    // First click protection - move mine if hit
    if (_firstClick && _mines[r][c]) {
        _mines[r][c] = false;
        for (int i = 0; i < ROWS * COLS; i++) {
            int nr = i / COLS;
            int nc = i % COLS;
            if (!_mines[nr][nc] && (nr != r || nc != c)) {
                _mines[nr][nc] = true;
                break;
            }
        }
        calculateNumbers();
    }
    _firstClick = false;
    
    _revealed[r][c] = true;
    
    if (_mines[r][c]) {
        // Game over - reveal all mines
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                if (_mines[i][j]) _revealed[i][j] = true;
            }
        }
        _state = GAME_OVER;
        return;
    }
    
    // Flood fill empty cells
    if (_numbers[r][c] == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int nr = r + dr;
                int nc = c + dc;
                if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                    if (!_revealed[nr][nc]) {
                        reveal(nr, nc);
                    }
                }
            }
        }
    }
    
    checkWin();
}

void MinesweeperGame::toggleFlag(int r, int c) {
    if (_revealed[r][c]) return;
    
    if (_flags[r][c]) {
        _flags[r][c] = false;
        _flagCount--;
    } else if (_flagCount < MINES) {
        _flags[r][c] = true;
        _flagCount++;
    }
    
    checkWin();
}

void MinesweeperGame::checkWin() {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!_mines[r][c] && !_revealed[r][c]) return;
        }
    }
    _state = GAME_WIN;
}

#endif // FEATURE_GAMES
