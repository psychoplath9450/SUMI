/**
 * @file Checkers.cpp
 * @brief Checkers/Draughts game implementation
 * @version 1.3.0
 * 
 * Optimized with raw button handling
 */

#include "plugins/Checkers.h"

#if FEATURE_GAMES


CheckersGame::CheckersGame() { 
    reset(); 
}

void CheckersGame::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _grid = calculateGrid(screenW, screenH, 8, 8, true, true);
    Serial.printf("[CHECKERS] Init: %dx%d cell=%d\n", screenW, screenH, _grid.cellSize);
    newGame();
}

void CheckersGame::newGame() {
    reset();
    // Place black pieces (AI) at top
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 8; c++) {
            if ((r + c) % 2 == 1) _board[r][c] = C_BLACK;
        }
    }
    // Place red pieces (player) at bottom
    for (int r = 5; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if ((r + c) % 2 == 1) _board[r][c] = C_RED;
        }
    }
    _state = GAME_PLAYING;
    _playerTurn = true;
    _cursorR = 5; _cursorC = 1;
    _selectedR = -1; _selectedC = -1;
    _mustJump = false;
}

bool CheckersGame::handleInput(Button btn) {
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
            if (_cursorR < 7) _cursorR++;
            return true;
        case BTN_LEFT:
            if (_cursorC > 0) _cursorC--;
            return true;
        case BTN_RIGHT:
            if (_cursorC < 7) _cursorC++;
            return true;
        case BTN_CONFIRM:
            return handleSelect();
        case BTN_BACK:
            if (_selectedR >= 0) {
                _selectedR = -1; _selectedC = -1;
                return true;
            }
            return false;
        default:
            return false;
    }
}

void CheckersGame::draw() {
    PluginUI::drawHeader("Checkers", _screenW);
    
    int cs = _grid.cellSize;
    
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int x = _grid.offsetX + c * cs;
            int y = _grid.offsetY + r * cs;
            bool dark = (r + c) % 2 == 1;
            
            PluginUI::drawCheckerSquare(x, y, cs, dark);
            
            // Highlight selected piece
            if (r == _selectedR && c == _selectedC) {
                PluginUI::drawSelection(x, y, cs, cs);
            }
            
            // Draw pieces
            CheckerPiece p = _board[r][c];
            if (p != C_EMPTY) {
                int cx = x + cs / 2;
                int cy = y + cs / 2;
                int pr = cs / 2 - 4;
                
                if (p == C_RED || p == C_RED_K) {
                    // Red (player) - white filled
                    display.fillCircle(cx, cy, pr, GxEPD_WHITE);
                    display.drawCircle(cx, cy, pr, GxEPD_BLACK);
                    display.drawCircle(cx, cy, pr - 1, GxEPD_BLACK);
                    if (p == C_RED_K) {
                        display.setCursor(cx - 4, cy + 4);
                        display.setTextColor(GxEPD_BLACK);
                        display.print("K");
                    }
                } else {
                    // Black (AI) - black filled
                    display.fillCircle(cx, cy, pr, GxEPD_BLACK);
                    if (p == C_BLACK_K) {
                        display.setCursor(cx - 4, cy + 4);
                        display.setTextColor(GxEPD_WHITE);
                        display.print("K");
                    }
                }
            }
        }
    }
    
    // Draw cursor
    display.setTextColor(GxEPD_BLACK);
    int cx = _grid.offsetX + _cursorC * cs;
    int cy = _grid.offsetY + _cursorR * cs;
    PluginUI::drawCursor(cx, cy, cs, cs);
    
    // Status
    char status[32];
    if (_state == GAME_WIN) {
        snprintf(status, sizeof(status), "%s wins!", _playerTurn ? "Black" : "Red");
    } else {
        snprintf(status, sizeof(status), "%s%s", 
                 _playerTurn ? "Your turn" : "AI thinking...", 
                 _mustJump ? " (must jump!)" : "");
    }
    PluginUI::drawFooter(status, "OK:Select BACK:Cancel", _screenW, _screenH);
    
    if (_state == GAME_WIN) {
        PluginUI::drawGameOver(_playerTurn ? "You Lose!" : "You Win!", "OK to play again", _screenW, _screenH);
    }
}

void CheckersGame::reset() {
    memset(_board, 0, sizeof(_board));
    _state = GAME_PLAYING;
}

bool CheckersGame::isPlayer(CheckerPiece p) { return p == C_RED || p == C_RED_K; }
bool CheckersGame::isAI(CheckerPiece p) { return p == C_BLACK || p == C_BLACK_K; }
bool CheckersGame::isKing(CheckerPiece p) { return p == C_RED_K || p == C_BLACK_K; }

bool CheckersGame::handleSelect() {
    if (_selectedR < 0) {
        // Nothing selected - try to select player piece
        CheckerPiece p = _board[_cursorR][_cursorC];
        if (_playerTurn && isPlayer(p)) {
            _selectedR = _cursorR; _selectedC = _cursorC;
            return true;
        }
    } else {
        // Piece selected - try to move
        if (isValidMove(_selectedR, _selectedC, _cursorR, _cursorC)) {
            bool jumped = makeMove(_selectedR, _selectedC, _cursorR, _cursorC);
            
            // Check for double jump
            if (jumped && canJump(_cursorR, _cursorC)) {
                _selectedR = _cursorR; _selectedC = _cursorC;
                _mustJump = true;
            } else {
                _selectedR = -1; _selectedC = -1;
                _mustJump = false;
                
                if (checkEnd()) return true;
                
                // AI turn
                _playerTurn = false;
                aiMove();
                checkEnd();
                _playerTurn = true;
            }
            return true;
        }
        _selectedR = -1; _selectedC = -1;
    }
    return true;
}

bool CheckersGame::isValidMove(int fr, int fc, int tr, int tc) {
    if (tr < 0 || tr > 7 || tc < 0 || tc > 7) return false;
    if (_board[tr][tc] != C_EMPTY) return false;
    
    CheckerPiece p = _board[fr][fc];
    int dr = tr - fr;
    int dc = tc - fc;
    
    // Simple move (1 diagonal)
    if (abs(dr) == 1 && abs(dc) == 1) {
        if (_mustJump) return false;  // Must jump if available
        if (p == C_RED && dr > 0) return false;  // Red moves up
        if (p == C_BLACK && dr < 0) return false;  // Black moves down
        return true;
    }
    
    // Jump move (2 diagonal)
    if (abs(dr) == 2 && abs(dc) == 2) {
        int mr = fr + dr/2;
        int mc = fc + dc/2;
        CheckerPiece jumped = _board[mr][mc];
        
        if (p == C_RED && dr > 0 && !isKing(p)) return false;
        if (p == C_BLACK && dr < 0 && !isKing(p)) return false;
        
        if (isPlayer(p) && isAI(jumped)) return true;
        if (isAI(p) && isPlayer(jumped)) return true;
    }
    
    return false;
}

bool CheckersGame::makeMove(int fr, int fc, int tr, int tc) {
    CheckerPiece p = _board[fr][fc];
    bool jumped = false;
    
    // Remove jumped piece
    if (abs(tr - fr) == 2) {
        int mr = fr + (tr - fr) / 2;
        int mc = fc + (tc - fc) / 2;
        _board[mr][mc] = C_EMPTY;
        jumped = true;
    }
    
    // Move piece
    _board[tr][tc] = p;
    _board[fr][fc] = C_EMPTY;
    
    // King promotion
    if (p == C_RED && tr == 0) _board[tr][tc] = C_RED_K;
    if (p == C_BLACK && tr == 7) _board[tr][tc] = C_BLACK_K;
    
    return jumped;
}

bool CheckersGame::canJump(int r, int c) {
    int dirs[][2] = {{-2,-2},{-2,2},{2,-2},{2,2}};
    for (auto& d : dirs) {
        if (isValidMove(r, c, r + d[0], c + d[1])) return true;
    }
    return false;
}

bool CheckersGame::checkEnd() {
    int redCount = 0, blackCount = 0;
    bool redCanMove = false, blackCanMove = false;
    
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            CheckerPiece p = _board[r][c];
            if (isPlayer(p)) {
                redCount++;
                if (hasValidMove(r, c)) redCanMove = true;
            } else if (isAI(p)) {
                blackCount++;
                if (hasValidMove(r, c)) blackCanMove = true;
            }
        }
    }
    
    if (redCount == 0 || !redCanMove) {
        _state = GAME_WIN; _playerTurn = true; return true;
    }
    if (blackCount == 0 || !blackCanMove) {
        _state = GAME_WIN; _playerTurn = false; return true;
    }
    return false;
}

bool CheckersGame::hasValidMove(int r, int c) {
    int dirs[][2] = {{-1,-1},{-1,1},{1,-1},{1,1},{-2,-2},{-2,2},{2,-2},{2,2}};
    for (auto& d : dirs) {
        if (isValidMove(r, c, r + d[0], c + d[1])) return true;
    }
    return false;
}

void CheckersGame::aiMove() {
    struct Move { int fr, fc, tr, tc; bool jump; };
    Move moves[64];
    int moveCount = 0;
    
    // Find all valid moves for AI
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (!isAI(_board[r][c])) continue;
            
            int dirs[][2] = {{-1,-1},{-1,1},{1,-1},{1,1},{-2,-2},{-2,2},{2,-2},{2,2}};
            for (auto& d : dirs) {
                int tr = r + d[0];
                int tc = c + d[1];
                if (isValidMove(r, c, tr, tc) && moveCount < 64) {
                    moves[moveCount++] = {r, c, tr, tc, abs(d[0]) == 2};
                }
            }
        }
    }
    
    if (moveCount == 0) return;
    
    // Prefer jumps
    int bestIdx = 0;
    for (int i = 0; i < moveCount; i++) {
        if (moves[i].jump) { bestIdx = i; break; }
    }
    if (!moves[bestIdx].jump) bestIdx = random(0, moveCount);
    
    Move& m = moves[bestIdx];
    bool jumped = makeMove(m.fr, m.fc, m.tr, m.tc);
    
    // Chain jumps
    while (jumped && canJump(m.tr, m.tc)) {
        int dirs[][2] = {{-2,-2},{-2,2},{2,-2},{2,2}};
        for (auto& d : dirs) {
            int nr = m.tr + d[0];
            int nc = m.tc + d[1];
            if (isValidMove(m.tr, m.tc, nr, nc)) {
                jumped = makeMove(m.tr, m.tc, nr, nc);
                m.tr = nr; m.tc = nc;
                break;
            }
        }
    }
}

#endif // FEATURE_GAMES
