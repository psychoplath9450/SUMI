#ifndef SUMI_PLUGIN_CHESSGAME_H
#define SUMI_PLUGIN_CHESSGAME_H

/**
 * @file ChessGame.h
 * @brief Full-featured Chess for Sumi e-reader with bitmap pieces
 * @version 1.3.0
 *
 * Features:
 * - 16x16 bitmap piece graphics
 * - Board coordinates (a-h, 1-8)
 * - Partial refresh (no black flash on moves)
 * - Valid move indicators (dots for moves, rings for captures)
 * - Minimax AI with alpha-beta pruning (depth 3)
 * - Full chess rules: castling, en passant, promotion
 * - Check/checkmate/stalemate detection
 * - Save/Resume game functionality
 */

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// 16x16 PIECE BITMAPS (1 = black pixel, 0 = transparent)
// White pieces drawn with outline, Black pieces drawn filled
// =============================================================================

// PAWN - Simple rounded shape
static const uint8_t BITMAP_PAWN[] PROGMEM = {
    0x00, 0x00,  // ................
    0x03, 0xC0,  // ......####......
    0x07, 0xE0,  // .....######.....
    0x07, 0xE0,  // .....######.....
    0x07, 0xE0,  // .....######.....
    0x03, 0xC0,  // ......####......
    0x01, 0x80,  // .......##.......
    0x03, 0xC0,  // ......####......
    0x03, 0xC0,  // ......####......
    0x07, 0xE0,  // .....######.....
    0x07, 0xE0,  // .....######.....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xF8,  // ...##########...
    0x1F, 0xF8,  // ...##########...
    0x00, 0x00   // ................
};

// ROOK - Castle tower with crenellations  
static const uint8_t BITMAP_ROOK[] PROGMEM = {
    0x00, 0x00,  // ................
    0x6D, 0xB6,  // .##.##.##.##.##.
    0x6D, 0xB6,  // .##.##.##.##.##.
    0x7F, 0xFE,  // .##############.
    0x3F, 0xFC,  // ..############..
    0x1F, 0xF8,  // ...##########...
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xF8,  // ...##########...
    0x3F, 0xFC,  // ..############..
    0x7F, 0xFE,  // .##############.
    0x7F, 0xFE,  // .##############.
    0x00, 0x00   // ................
};

// KNIGHT - Horse head profile
static const uint8_t BITMAP_KNIGHT[] PROGMEM = {
    0x00, 0x00,  // ................
    0x01, 0xC0,  // .......###......
    0x03, 0xE0,  // ......#####.....
    0x07, 0xF0,  // .....#######....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xE0,  // ...########.....
    0x3F, 0xC0,  // ..########......
    0x3F, 0x80,  // ..#######.......
    0x1F, 0xC0,  // ...#######......
    0x0F, 0xE0,  // ....#######.....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xF8,  // ...##########...
    0x3F, 0xFC,  // ..############..
    0x3F, 0xFC,  // ..############..
    0x00, 0x00   // ................
};

// BISHOP - Mitre/hat shape with slit
static const uint8_t BITMAP_BISHOP[] PROGMEM = {
    0x00, 0x00,  // ................
    0x01, 0x80,  // .......##.......
    0x03, 0xC0,  // ......####......
    0x07, 0xE0,  // .....######.....
    0x07, 0xE0,  // .....######.....
    0x0F, 0xF0,  // ....########....
    0x0E, 0x70,  // ....###..###....
    0x0F, 0xF0,  // ....########....
    0x07, 0xE0,  // .....######.....
    0x03, 0xC0,  // ......####......
    0x03, 0xC0,  // ......####......
    0x07, 0xE0,  // .....######.....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xF8,  // ...##########...
    0x1F, 0xF8,  // ...##########...
    0x00, 0x00   // ................
};

// QUEEN - Crown with multiple points
static const uint8_t BITMAP_QUEEN[] PROGMEM = {
    0x01, 0x80,  // .......##.......
    0x01, 0x80,  // .......##.......
    0x49, 0x92,  // .#..#..##..#..#.
    0x6D, 0xB6,  // .##.##.##.##.##.
    0x7F, 0xFE,  // .##############.
    0x3F, 0xFC,  // ..############..
    0x1F, 0xF8,  // ...##########...
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xF8,  // ...##########...
    0x3F, 0xFC,  // ..############..
    0x7F, 0xFE,  // .##############.
    0x7F, 0xFE,  // .##############.
    0x00, 0x00   // ................
};

// KING - Crown with cross on top
static const uint8_t BITMAP_KING[] PROGMEM = {
    0x01, 0x80,  // .......##.......
    0x01, 0x80,  // .......##.......
    0x07, 0xE0,  // .....######.....
    0x01, 0x80,  // .......##.......
    0x01, 0x80,  // .......##.......
    0x79, 0x9E,  // .####..##..####.
    0x7F, 0xFE,  // .##############.
    0x3F, 0xFC,  // ..############..
    0x1F, 0xF8,  // ...##########...
    0x0F, 0xF0,  // ....########....
    0x0F, 0xF0,  // ....########....
    0x1F, 0xF8,  // ...##########...
    0x3F, 0xFC,  // ..############..
    0x7F, 0xFE,  // .##############.
    0x7F, 0xFE,  // .##############.
    0x00, 0x00   // ................
};

static const uint8_t* PIECE_BITMAPS[] = {
    nullptr, BITMAP_PAWN, BITMAP_ROOK, BITMAP_KNIGHT, 
    BITMAP_BISHOP, BITMAP_QUEEN, BITMAP_KING
};

// =============================================================================
// Chess Pieces & Move
// =============================================================================
enum Piece : int8_t {
    EMPTY = 0,
    W_PAWN = 1, W_ROOK = 2, W_KNIGHT = 3, W_BISHOP = 4, W_QUEEN = 5, W_KING = 6,
    B_PAWN = -1, B_ROOK = -2, B_KNIGHT = -3, B_BISHOP = -4, B_QUEEN = -5, B_KING = -6
};

static const int PIECE_VALUES[] = { 0, 100, 500, 320, 330, 900, 20000 };

struct Move {
    int8_t fr, fc, tr, tc;
    int8_t captured;
    int8_t special;  // 0=normal, 1=OO, 2=OOO, 3=ep, 4=promo
    Move() : fr(-1), fc(-1), tr(-1), tc(-1), captured(0), special(0) {}
    Move(int a, int b, int c, int d) : fr(a), fc(b), tr(c), tc(d), captured(0), special(0) {}
    bool valid() const { return fr >= 0; }
};

// =============================================================================
// Saved Game Structure
// =============================================================================
#define CHESS_SAVE_PATH "/.sumi/chess_save.bin"

struct ChessSaveData {
    uint32_t magic = 0x43485353;  // "CHSS"
    uint8_t version = 1;
    int8_t board[8][8];
    bool whiteTurn;
    bool wCastleK, wCastleQ, bCastleK, bCastleQ;
    int8_t epCol;
    int16_t moveNum;
    Move lastMove;
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x43485353 && version == 1; }
};

// =============================================================================
// Chess Game Class
// =============================================================================
class ChessGame {
public:
    // Screen layout
    int screenW, screenH;
    int boardX, boardY, cellSize;
    bool landscape;
    
    // Board state
    int8_t board[8][8];
    bool whiteTurn;
    bool wCastleK, wCastleQ, bCastleK, bCastleQ;
    int8_t epCol;  // en passant column, -1 if none
    bool inCheck, gameOver, checkmate, stalemate;
    
    // UI state
    int curR, curC;              // cursor position
    int selR, selC;              // selected piece (-1 if none)
    bool hasSel;                 // piece is selected
    Move lastMove;
    int moveNum;
    
    // Valid moves for selected piece
    bool validMoves[8][8];
    
    // Refresh control
    int prevCurR, prevCurC;
    bool needsFullRedraw;
    bool aiThinking;
    
    // Dirty tracking for partial refresh
    bool dirtySquares[8][8];
    bool anyDirty;
    
    // Save/Resume state
    enum MenuState { MENU_NONE, MENU_RESUME_PROMPT };
    MenuState menuState;
    int menuCursor;  // 0 = Yes, 1 = No
    
    ChessGame() { menuState = MENU_NONE; menuCursor = 0; newGame(); }
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
        landscape = (w > h);
        
        // Layout: maximize board size
        int headerH = 50;
        int footerH = 30;
        int coordMargin = 18;
        
        int availH = screenH - headerH - footerH - coordMargin;
        int availW = screenW - coordMargin - 10;
        
        cellSize = min(availH, availW) / 8;
        if (cellSize < 30) cellSize = 30;
        if (cellSize > 50) cellSize = 50;
        
        int boardSize = cellSize * 8;
        boardX = coordMargin + (availW - boardSize) / 2 + 5;
        boardY = headerH + (availH - boardSize) / 2;
        
        Serial.printf("[CHESS] Cell: %d, Board at: %d,%d\n", cellSize, boardX, boardY);
        
        // Check for saved game
        if (hasSavedGame()) {
            menuState = MENU_RESUME_PROMPT;
            menuCursor = 0;
            needsFullRedraw = true;
        } else {
            newGame();
        }
    }
    
    // =========================================================================
    // Save/Load Functions
    // =========================================================================
    bool hasSavedGame() {
        return SD.exists(CHESS_SAVE_PATH);
    }
    
    bool saveGame() {
        SD.mkdir("/.sumi");
        File f = SD.open(CHESS_SAVE_PATH, FILE_WRITE);
        if (!f) {
            Serial.println("[CHESS] Failed to open save file");
            return false;
        }
        
        ChessSaveData save;
        memcpy(save.board, board, sizeof(board));
        save.whiteTurn = whiteTurn;
        save.wCastleK = wCastleK;
        save.wCastleQ = wCastleQ;
        save.bCastleK = bCastleK;
        save.bCastleQ = bCastleQ;
        save.epCol = epCol;
        save.moveNum = moveNum;
        save.lastMove = lastMove;
        memset(save.reserved, 0, sizeof(save.reserved));
        
        f.write((uint8_t*)&save, sizeof(ChessSaveData));
        f.close();
        
        Serial.printf("[CHESS] Game saved (move %d)\n", moveNum);
        return true;
    }
    
    bool loadGame() {
        File f = SD.open(CHESS_SAVE_PATH, FILE_READ);
        if (!f) {
            Serial.println("[CHESS] No save file found");
            return false;
        }
        
        ChessSaveData save;
        f.read((uint8_t*)&save, sizeof(ChessSaveData));
        f.close();
        
        if (!save.isValid()) {
            Serial.println("[CHESS] Invalid save file");
            deleteSavedGame();
            return false;
        }
        
        memcpy(board, save.board, sizeof(board));
        whiteTurn = save.whiteTurn;
        wCastleK = save.wCastleK;
        wCastleQ = save.wCastleQ;
        bCastleK = save.bCastleK;
        bCastleQ = save.bCastleQ;
        epCol = save.epCol;
        moveNum = save.moveNum;
        lastMove = save.lastMove;
        
        // Reset UI state
        curR = 6; curC = 4;
        prevCurR = curR; prevCurC = curC;
        selR = selC = -1;
        hasSel = false;
        aiThinking = false;
        inCheck = gameOver = checkmate = stalemate = false;
        
        memset(validMoves, 0, sizeof(validMoves));
        memset(dirtySquares, 0, sizeof(dirtySquares));
        anyDirty = false;
        needsFullRedraw = true;
        
        // Update game state (check detection)
        updateGameState();
        
        Serial.printf("[CHESS] Game loaded (move %d, %s to move)\n", 
                      moveNum, whiteTurn ? "white" : "black");
        return true;
    }
    
    void deleteSavedGame() {
        if (SD.exists(CHESS_SAVE_PATH)) {
            SD.remove(CHESS_SAVE_PATH);
            Serial.println("[CHESS] Save file deleted");
        }
    }
    
    void newGame() {
        memset(board, 0, sizeof(board));
        
        // Setup starting position
        board[0][0] = B_ROOK;  board[0][1] = B_KNIGHT; board[0][2] = B_BISHOP;
        board[0][3] = B_QUEEN; board[0][4] = B_KING;   board[0][5] = B_BISHOP;
        board[0][6] = B_KNIGHT; board[0][7] = B_ROOK;
        for (int c = 0; c < 8; c++) board[1][c] = B_PAWN;
        
        board[7][0] = W_ROOK;  board[7][1] = W_KNIGHT; board[7][2] = W_BISHOP;
        board[7][3] = W_QUEEN; board[7][4] = W_KING;   board[7][5] = W_BISHOP;
        board[7][6] = W_KNIGHT; board[7][7] = W_ROOK;
        for (int c = 0; c < 8; c++) board[6][c] = W_PAWN;
        
        whiteTurn = true;
        wCastleK = wCastleQ = bCastleK = bCastleQ = true;
        epCol = -1;
        inCheck = gameOver = checkmate = stalemate = false;
        
        curR = 6; curC = 4;  // Start at e2
        prevCurR = curR; prevCurC = curC;
        selR = selC = -1;
        hasSel = false;
        lastMove = Move();
        moveNum = 1;
        aiThinking = false;
        
        memset(validMoves, 0, sizeof(validMoves));
        memset(dirtySquares, 0, sizeof(dirtySquares));
        anyDirty = false;
        needsFullRedraw = true;
    }
    
    // =========================================================================
    // Input
    // =========================================================================
    bool handleInput(Button btn) {
        if (aiThinking) {
            Serial.println("[CHESS] AI thinking, ignoring input");
            return false;
        }
        
        Serial.printf("[CHESS] handleInput: btn=%d, menuState=%d, curR=%d, curC=%d\n", 
                      btn, (int)menuState, curR, curC);
        
        // Handle menu states first
        if (menuState == MENU_RESUME_PROMPT) {
            switch (btn) {
                case BTN_LEFT:
                case BTN_RIGHT:
                    menuCursor = 1 - menuCursor;  // Toggle Yes/No
                    needsFullRedraw = true;
                    return true;
                case BTN_CONFIRM:
                    if (menuCursor == 0) {
                        // Resume saved game
                        loadGame();
                        deleteSavedGame();  // Delete after loading
                    } else {
                        // Start new game
                        deleteSavedGame();
                        newGame();
                    }
                    menuState = MENU_NONE;
                    needsFullRedraw = true;
                    return true;
                case BTN_BACK:
                    // Start new game on back
                    deleteSavedGame();
                    newGame();
                    menuState = MENU_NONE;
                    needsFullRedraw = true;
                    return true;
                default:
                    return true;
            }
        }
        
        // Normal game input
        if (gameOver) {
            if (btn == BTN_CONFIRM) { 
                deleteSavedGame();  // Clear any old save
                newGame(); 
                return true; 
            }
            if (btn == BTN_BACK) return false;
            return true;
        }
        
        prevCurR = curR;
        prevCurC = curC;
        
        switch (btn) {
            case BTN_UP:    
                if (curR > 0) { curR--; markCursorDirty(); Serial.printf("[CHESS] Cursor UP to row %d\n", curR); } 
                return true;
            case BTN_DOWN:  
                if (curR < 7) { curR++; markCursorDirty(); Serial.printf("[CHESS] Cursor DOWN to row %d\n", curR); } 
                return true;
            case BTN_LEFT:  
                if (curC > 0) { curC--; markCursorDirty(); Serial.printf("[CHESS] Cursor LEFT to col %d\n", curC); } 
                return true;
            case BTN_RIGHT: 
                if (curC < 7) { curC++; markCursorDirty(); Serial.printf("[CHESS] Cursor RIGHT to col %d\n", curC); } 
                return true;
            case BTN_CONFIRM: return handleSelect();
            case BTN_BACK:
                if (hasSel) {
                    // Deselect
                    Serial.println("[CHESS] BACK pressed, deselecting");
                    markSquareDirty(selR, selC);
                    for (int r = 0; r < 8; r++)
                        for (int c = 0; c < 8; c++)
                            if (validMoves[r][c]) markSquareDirty(r, c);
                    hasSel = false;
                    selR = selC = -1;
                    memset(validMoves, 0, sizeof(validMoves));
                    return true;
                }
                // Just exit - game is auto-saved after every move
                Serial.println("[CHESS] BACK pressed, exiting (auto-saved)");
                return false;
            default: return false;
        }
    }
    
    bool handleSelect() {
        Serial.printf("[CHESS] handleSelect: hasSel=%d, cur=(%d,%d), sel=(%d,%d)\n", 
                      hasSel, curR, curC, selR, selC);
        
        if (!hasSel) {
            // Try to select piece
            int8_t piece = board[curR][curC];
            Serial.printf("[CHESS] Piece at cursor: %d, whiteTurn=%d\n", piece, whiteTurn);
            
            if (piece == EMPTY) {
                Serial.println("[CHESS] Empty square, ignoring");
                return true;
            }
            if ((piece > 0) != whiteTurn) {
                Serial.println("[CHESS] Wrong color piece, ignoring");
                return true;
            }
            
            selR = curR; selC = curC;
            hasSel = true;
            calcValidMoves(curR, curC);
            
            // Count valid moves for debug
            int validCount = 0;
            for (int r = 0; r < 8; r++)
                for (int c = 0; c < 8; c++)
                    if (validMoves[r][c]) validCount++;
            Serial.printf("[CHESS] Selected piece at (%d,%d), %d valid moves\n", selR, selC, validCount);
            
            // Mark all affected squares dirty
            markSquareDirty(selR, selC);
            for (int r = 0; r < 8; r++)
                for (int c = 0; c < 8; c++)
                    if (validMoves[r][c]) markSquareDirty(r, c);
            return true;
        } else {
            // Try to move
            Serial.printf("[CHESS] Trying to move to (%d,%d), validMoves[%d][%d]=%d\n", 
                          curR, curC, curR, curC, validMoves[curR][curC]);
            
            if (curR == selR && curC == selC) {
                // Deselect
                Serial.println("[CHESS] Deselecting (same square)");
                markSquareDirty(selR, selC);
                for (int r = 0; r < 8; r++)
                    for (int c = 0; c < 8; c++)
                        if (validMoves[r][c]) markSquareDirty(r, c);
                hasSel = false;
                selR = selC = -1;
                memset(validMoves, 0, sizeof(validMoves));
                return true;
            }
            
            if (validMoves[curR][curC]) {
                // Make move
                Serial.printf("[CHESS] Making move from (%d,%d) to (%d,%d)\n", selR, selC, curR, curC);
                Move move = makeMove(selR, selC, curR, curC);
                
                // Mark old and new positions dirty
                markSquareDirty(move.fr, move.fc);
                markSquareDirty(move.tr, move.tc);
                if (lastMove.valid()) {
                    markSquareDirty(lastMove.fr, lastMove.fc);
                    markSquareDirty(lastMove.tr, lastMove.tc);
                }
                
                // Clear selection highlights
                for (int r = 0; r < 8; r++)
                    for (int c = 0; c < 8; c++)
                        if (validMoves[r][c]) markSquareDirty(r, c);
                
                lastMove = move;
                hasSel = false;
                selR = selC = -1;
                memset(validMoves, 0, sizeof(validMoves));
                
                updateGameState();
                Serial.printf("[CHESS] Move complete, gameOver=%d, inCheck=%d\n", gameOver, inCheck);
                
                // Auto-save after player move
                if (!gameOver) {
                    saveGame();
                    aiThinking = true;
                    Serial.println("[CHESS] AI thinking...");
                } else {
                    // Game over - delete save file
                    deleteSavedGame();
                }
                needsFullRedraw = true;  // Refresh for status update
                return true;
            } else {
                // Check if clicking own piece to reselect
                int8_t piece = board[curR][curC];
                if (piece != EMPTY && (piece > 0) == whiteTurn) {
                    Serial.printf("[CHESS] Reselecting piece at (%d,%d)\n", curR, curC);
                    // Clear old selection
                    markSquareDirty(selR, selC);
                    for (int r = 0; r < 8; r++)
                        for (int c = 0; c < 8; c++)
                            if (validMoves[r][c]) markSquareDirty(r, c);
                    
                    selR = curR; selC = curC;
                    calcValidMoves(curR, curC);
                    
                    markSquareDirty(selR, selC);
                    for (int r = 0; r < 8; r++)
                        for (int c = 0; c < 8; c++)
                            if (validMoves[r][c]) markSquareDirty(r, c);
                } else {
                    Serial.println("[CHESS] Invalid move destination");
                }
                return true;
            }
        }
    }
    
    void markSquareDirty(int r, int c) {
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            dirtySquares[r][c] = true;
            anyDirty = true;
        }
    }
    
    void markCursorDirty() {
        markSquareDirty(prevCurR, prevCurC);
        markSquareDirty(curR, curC);
    }
    
    // =========================================================================
    // Update (AI)
    // =========================================================================
    bool update() {
        if (!aiThinking || gameOver) return false;
        
        Move aiMove = findBestMove();
        if (aiMove.valid()) {
            // Mark old move highlight
            if (lastMove.valid()) {
                markSquareDirty(lastMove.fr, lastMove.fc);
                markSquareDirty(lastMove.tr, lastMove.tc);
            }
            
            execMove(aiMove);
            lastMove = aiMove;
            
            markSquareDirty(aiMove.fr, aiMove.fc);
            markSquareDirty(aiMove.tr, aiMove.tc);
            
            updateGameState();
            aiThinking = false;
            
            // Auto-save after AI move (or delete if game over)
            if (gameOver) {
                deleteSavedGame();
            } else {
                saveGame();
            }
            
            needsFullRedraw = true;
            return true;
        }
        aiThinking = false;
        return false;
    }
    
    // =========================================================================
    // Drawing
    // =========================================================================
    void draw() {
        if (needsFullRedraw) {
            drawFull();
            needsFullRedraw = false;
            memset(dirtySquares, 0, sizeof(dirtySquares));
            anyDirty = false;
        } else if (anyDirty) {
            drawPartial();
            memset(dirtySquares, 0, sizeof(dirtySquares));
            anyDirty = false;
        }
    }
    
    void drawFull() {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            drawContent();
        } while (display.nextPage());
    }
    
    void drawPartial() {
        // Find bounding box of dirty squares
        int minR = 8, maxR = -1, minC = 8, maxC = -1;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (dirtySquares[r][c]) {
                    if (r < minR) minR = r;
                    if (r > maxR) maxR = r;
                    if (c < minC) minC = c;
                    if (c > maxC) maxC = c;
                }
            }
        }
        
        if (maxR < 0) return;  // Nothing dirty
        
        // Add cursor margin
        int x = boardX + minC * cellSize - 4;
        int y = boardY + minR * cellSize - 4;
        int w = (maxC - minC + 1) * cellSize + 8;
        int h = (maxR - minR + 1) * cellSize + 8;
        
        // Clamp to screen
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + w > screenW) w = screenW - x;
        if (y + h > screenH) h = screenH - y;
        
        display.setPartialWindow(x, y, w, h);
        display.firstPage();
        do {
            // Redraw only affected squares
            for (int r = minR; r <= maxR; r++) {
                for (int c = minC; c <= maxC; c++) {
                    drawSquare(r, c);
                }
            }
            // Redraw cursor
            drawCursor();
        } while (display.nextPage());
    }
    
    void drawContent() {
        // Header
        display.fillRect(0, 0, screenW, 45, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds("Chess", 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 32);
        display.print("Chess");
        
        // Column labels (a-h)
        display.setFont(NULL);
        display.setTextColor(GxEPD_BLACK);
        for (int c = 0; c < 8; c++) {
            int x = boardX + c * cellSize + cellSize / 2 - 3;
            int y = boardY + 8 * cellSize + 4;
            display.setCursor(x, y);
            display.print((char)('a' + c));
        }
        
        // Row labels (8-1)
        for (int r = 0; r < 8; r++) {
            int x = boardX - 12;
            int y = boardY + r * cellSize + cellSize / 2 + 4;
            display.setCursor(x, y);
            display.print((char)('8' - r));
        }
        
        // Board
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                drawSquare(r, c);
            }
        }
        
        // Cursor
        drawCursor();
        
        // Footer
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        int footerY = screenH - 8;
        
        if (gameOver) {
            display.setCursor(10, footerY);
            if (checkmate) {
                display.print(whiteTurn ? "Checkmate! Black wins" : "Checkmate! You win!");
            } else {
                display.print("Stalemate - Draw");
            }
        } else {
            display.setCursor(10, footerY);
            if (whiteTurn) {
                display.print("Your move");
                if (inCheck) display.print(" - CHECK!");
            } else {
                display.print("Thinking...");
            }
        }
        
        // Current square
        char coord[4] = { (char)('a' + curC), (char)('8' - curR), '\0' };
        display.setCursor(screenW - 90, footerY);
        display.printf("Move %d %s", moveNum, coord);
        
        // Draw menu dialogs on top
        if (menuState != MENU_NONE) {
            drawMenuDialog();
        }
    }
    
    void drawMenuDialog() {
        // Dialog dimensions
        int dialogW = 280;
        int dialogH = 120;
        int dialogX = (screenW - dialogW) / 2;
        int dialogY = (screenH - dialogH) / 2;
        
        // Draw dialog background with border
        display.fillRect(dialogX, dialogY, dialogW, dialogH, GxEPD_WHITE);
        display.drawRect(dialogX, dialogY, dialogW, dialogH, GxEPD_BLACK);
        display.drawRect(dialogX + 2, dialogY + 2, dialogW - 4, dialogH - 4, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Title
        const char* title = "Resume Game?";
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(dialogX + (dialogW - tw) / 2, dialogY + 35);
        display.print(title);
        
        // Subtitle
        display.setFont(&FreeSans9pt7b);
        const char* subtitle = "Found a saved game";
        display.getTextBounds(subtitle, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(dialogX + (dialogW - tw) / 2, dialogY + 55);
        display.print(subtitle);
        
        // Buttons
        int btnW = 80;
        int btnH = 32;
        int btnY = dialogY + dialogH - 45;
        int btnSpacing = 30;
        int yesX = dialogX + (dialogW / 2) - btnW - (btnSpacing / 2);
        int noX = dialogX + (dialogW / 2) + (btnSpacing / 2);
        
        // Yes button
        if (menuCursor == 0) {
            display.fillRoundRect(yesX, btnY, btnW, btnH, 4, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(yesX, btnY, btnW, btnH, 4, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setCursor(yesX + 25, btnY + 22);
        display.print("Yes");
        
        // No button
        if (menuCursor == 1) {
            display.fillRoundRect(noX, btnY, btnW, btnH, 4, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(noX, btnY, btnW, btnH, 4, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        display.setCursor(noX + 28, btnY + 22);
        display.print("No");
        
        display.setTextColor(GxEPD_BLACK);
    }
    
    void drawSquare(int r, int c) {
        int x = boardX + c * cellSize;
        int y = boardY + r * cellSize;
        bool dark = (r + c) % 2 == 1;
        
        // Fill square
        if (dark) {
            display.fillRect(x, y, cellSize, cellSize, GxEPD_BLACK);
        } else {
            display.fillRect(x, y, cellSize, cellSize, GxEPD_WHITE);
            display.drawRect(x, y, cellSize, cellSize, GxEPD_BLACK);
        }
        
        // Last move highlight (corner brackets)
        if (lastMove.valid() && 
            ((r == lastMove.fr && c == lastMove.fc) || (r == lastMove.tr && c == lastMove.tc))) {
            uint16_t col = dark ? GxEPD_WHITE : GxEPD_BLACK;
            int m = 2, len = cellSize / 4;
            // Corners
            display.drawFastHLine(x + m, y + m, len, col);
            display.drawFastVLine(x + m, y + m, len, col);
            display.drawFastHLine(x + cellSize - m - len, y + m, len, col);
            display.drawFastVLine(x + cellSize - m - 1, y + m, len, col);
            display.drawFastHLine(x + m, y + cellSize - m - 1, len, col);
            display.drawFastVLine(x + m, y + cellSize - m - len, len, col);
            display.drawFastHLine(x + cellSize - m - len, y + cellSize - m - 1, len, col);
            display.drawFastVLine(x + cellSize - m - 1, y + cellSize - m - len, len, col);
        }
        
        // Selection highlight (thick border)
        if (hasSel && r == selR && c == selC) {
            uint16_t col = dark ? GxEPD_WHITE : GxEPD_BLACK;
            for (int i = 2; i <= 4; i++) {
                display.drawRect(x + i, y + i, cellSize - i*2, cellSize - i*2, col);
            }
        }
        
        // Valid move indicator
        if (validMoves[r][c]) {
            int cx = x + cellSize / 2;
            int cy = y + cellSize / 2;
            int dotR = max(4, cellSize / 8);
            uint16_t col = dark ? GxEPD_WHITE : GxEPD_BLACK;
            
            if (board[r][c] != EMPTY) {
                // Capture - ring
                display.drawCircle(cx, cy, dotR + 2, col);
                display.drawCircle(cx, cy, dotR + 3, col);
            } else {
                // Move - dot
                display.fillCircle(cx, cy, dotR, col);
            }
        }
        
        // Draw piece
        int8_t piece = board[r][c];
        if (piece != EMPTY) {
            drawPiece(x, y, piece, dark);
        }
    }
    
    void drawCursor() {
        int x = boardX + curC * cellSize;
        int y = boardY + curR * cellSize;
        
        // Double ring cursor (visible on both dark and light)
        display.drawRect(x - 2, y - 2, cellSize + 4, cellSize + 4, GxEPD_WHITE);
        display.drawRect(x - 1, y - 1, cellSize + 2, cellSize + 2, GxEPD_BLACK);
        display.drawRect(x, y, cellSize, cellSize, GxEPD_BLACK);
        display.drawRect(x - 3, y - 3, cellSize + 6, cellSize + 6, GxEPD_BLACK);
    }
    
    void drawPiece(int x, int y, int8_t piece, bool onDark) {
        bool isWhite = piece > 0;
        int type = abs(piece);
        if (type < 1 || type > 6) return;
        
        const uint8_t* bitmap = PIECE_BITMAPS[type];
        if (!bitmap) return;
        
        // Scale bitmap to fit cell
        int bmpSize = 16;
        int scale = (cellSize - 8) / bmpSize;
        if (scale < 1) scale = 1;
        if (scale > 3) scale = 3;
        
        int pieceW = bmpSize * scale;
        int pieceH = bmpSize * scale;
        int px = x + (cellSize - pieceW) / 2;
        int py = y + (cellSize - pieceH) / 2;
        
        if (isWhite) {
            // WHITE PIECES: solid fill with outline
            // First pass: draw black outline
            for (int by = 0; by < bmpSize; by++) {
                uint16_t row = (pgm_read_byte(&bitmap[by * 2]) << 8) | pgm_read_byte(&bitmap[by * 2 + 1]);
                for (int bx = 0; bx < bmpSize; bx++) {
                    bool bit = (row >> (15 - bx)) & 1;
                    if (bit) {
                        uint16_t color = onDark ? GxEPD_WHITE : GxEPD_BLACK;
                        for (int sy = 0; sy < scale; sy++) {
                            for (int sx = 0; sx < scale; sx++) {
                                display.drawPixel(px + bx * scale + sx, py + by * scale + sy, color);
                            }
                        }
                    }
                }
            }
            
            // Second pass: fill interior with white (on light squares)
            if (!onDark) {
                for (int by = 1; by < bmpSize - 1; by++) {
                    uint16_t row = (pgm_read_byte(&bitmap[by * 2]) << 8) | pgm_read_byte(&bitmap[by * 2 + 1]);
                    for (int bx = 1; bx < bmpSize - 1; bx++) {
                        bool bit = (row >> (15 - bx)) & 1;
                        bool above = ((pgm_read_byte(&bitmap[(by-1) * 2]) << 8) | pgm_read_byte(&bitmap[(by-1) * 2 + 1])) >> (15 - bx) & 1;
                        bool below = ((pgm_read_byte(&bitmap[(by+1) * 2]) << 8) | pgm_read_byte(&bitmap[(by+1) * 2 + 1])) >> (15 - bx) & 1;
                        bool left = (row >> (15 - bx + 1)) & 1;
                        bool right = (row >> (15 - bx - 1)) & 1;
                        
                        // Fill interior (surrounded by set bits)
                        if (bit && above && below && left && right) {
                            for (int sy = 0; sy < scale; sy++) {
                                for (int sx = 0; sx < scale; sx++) {
                                    display.drawPixel(px + bx * scale + sx, py + by * scale + sy, GxEPD_WHITE);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // BLACK PIECES: dithered/stippled pattern for "gray" appearance
            for (int by = 0; by < bmpSize; by++) {
                uint16_t row = (pgm_read_byte(&bitmap[by * 2]) << 8) | pgm_read_byte(&bitmap[by * 2 + 1]);
                for (int bx = 0; bx < bmpSize; bx++) {
                    bool bit = (row >> (15 - bx)) & 1;
                    if (bit) {
                        // Check if this is an edge pixel (for solid outline)
                        bool above = (by > 0) ? ((pgm_read_byte(&bitmap[(by-1) * 2]) << 8) | pgm_read_byte(&bitmap[(by-1) * 2 + 1])) >> (15 - bx) & 1 : false;
                        bool below = (by < bmpSize-1) ? ((pgm_read_byte(&bitmap[(by+1) * 2]) << 8) | pgm_read_byte(&bitmap[(by+1) * 2 + 1])) >> (15 - bx) & 1 : false;
                        bool left = (bx > 0) ? (row >> (15 - bx + 1)) & 1 : false;
                        bool right = (bx < bmpSize-1) ? (row >> (15 - bx - 1)) & 1 : false;
                        bool isEdge = !above || !below || !left || !right;
                        
                        for (int sy = 0; sy < scale; sy++) {
                            for (int sx = 0; sx < scale; sx++) {
                                int screenX = px + bx * scale + sx;
                                int screenY = py + by * scale + sy;
                                
                                if (isEdge) {
                                    // Solid outline - contrasting color
                                    display.drawPixel(screenX, screenY, onDark ? GxEPD_WHITE : GxEPD_BLACK);
                                } else {
                                    // Interior: checkerboard dither pattern for "gray"
                                    bool dither = ((screenX + screenY) % 2 == 0);
                                    if (onDark) {
                                        // On dark square: white/dark checkerboard
                                        display.drawPixel(screenX, screenY, dither ? GxEPD_WHITE : GxEPD_BLACK);
                                    } else {
                                        // On light square: black/white checkerboard
                                        display.drawPixel(screenX, screenY, dither ? GxEPD_BLACK : GxEPD_WHITE);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // =========================================================================
    // Move Generation
    // =========================================================================
    void calcValidMoves(int r, int c) {
        memset(validMoves, 0, sizeof(validMoves));
        
        int8_t piece = board[r][c];
        if (piece == EMPTY) return;
        
        bool isWhite = piece > 0;
        int type = abs(piece);
        
        switch (type) {
            case 1: genPawnMoves(r, c, isWhite); break;
            case 2: genSliding(r, c, isWhite, true, false); break;
            case 3: genKnightMoves(r, c, isWhite); break;
            case 4: genSliding(r, c, isWhite, false, true); break;
            case 5: genSliding(r, c, isWhite, true, true); break;
            case 6: genKingMoves(r, c, isWhite); break;
        }
        
        filterLegalMoves(r, c, isWhite);
    }
    
    void genPawnMoves(int r, int c, bool isWhite) {
        int dir = isWhite ? -1 : 1;
        int start = isWhite ? 6 : 1;
        
        // Forward
        int nr = r + dir;
        if (nr >= 0 && nr < 8 && board[nr][c] == EMPTY) {
            validMoves[nr][c] = true;
            if (r == start) {
                int nr2 = r + dir * 2;
                if (board[nr2][c] == EMPTY) validMoves[nr2][c] = true;
            }
        }
        
        // Captures
        for (int dc = -1; dc <= 1; dc += 2) {
            int nc = c + dc;
            if (nc < 0 || nc > 7 || nr < 0 || nr > 7) continue;
            int8_t target = board[nr][nc];
            if (target != EMPTY && (target > 0) != isWhite) {
                validMoves[nr][nc] = true;
            }
            // En passant
            if (nc == epCol && r == (isWhite ? 3 : 4)) {
                validMoves[nr][nc] = true;
            }
        }
    }
    
    void genKnightMoves(int r, int c, bool isWhite) {
        static const int8_t jumps[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for (int i = 0; i < 8; i++) {
            int nr = r + jumps[i][0];
            int nc = c + jumps[i][1];
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
            int8_t target = board[nr][nc];
            if (target == EMPTY || (target > 0) != isWhite) {
                validMoves[nr][nc] = true;
            }
        }
    }
    
    void genSliding(int r, int c, bool isWhite, bool rook, bool bishop) {
        static const int8_t rookDirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
        static const int8_t bishopDirs[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
        
        auto genRay = [&](int dr, int dc) {
            int nr = r + dr, nc = c + dc;
            while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                int8_t target = board[nr][nc];
                if (target == EMPTY) {
                    validMoves[nr][nc] = true;
                } else {
                    if ((target > 0) != isWhite) validMoves[nr][nc] = true;
                    break;
                }
                nr += dr; nc += dc;
            }
        };
        
        if (rook) for (int i = 0; i < 4; i++) genRay(rookDirs[i][0], rookDirs[i][1]);
        if (bishop) for (int i = 0; i < 4; i++) genRay(bishopDirs[i][0], bishopDirs[i][1]);
    }
    
    void genKingMoves(int r, int c, bool isWhite) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r + dr, nc = c + dc;
                if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
                int8_t target = board[nr][nc];
                if (target == EMPTY || (target > 0) != isWhite) {
                    validMoves[nr][nc] = true;
                }
            }
        }
        
        // Castling
        if (isWhite && r == 7 && c == 4 && !isAttacked(7, 4, false)) {
            if (wCastleK && board[7][5] == EMPTY && board[7][6] == EMPTY &&
                !isAttacked(7, 5, false) && !isAttacked(7, 6, false)) {
                validMoves[7][6] = true;
            }
            if (wCastleQ && board[7][3] == EMPTY && board[7][2] == EMPTY && board[7][1] == EMPTY &&
                !isAttacked(7, 3, false) && !isAttacked(7, 2, false)) {
                validMoves[7][2] = true;
            }
        } else if (!isWhite && r == 0 && c == 4 && !isAttacked(0, 4, true)) {
            if (bCastleK && board[0][5] == EMPTY && board[0][6] == EMPTY &&
                !isAttacked(0, 5, true) && !isAttacked(0, 6, true)) {
                validMoves[0][6] = true;
            }
            if (bCastleQ && board[0][3] == EMPTY && board[0][2] == EMPTY && board[0][1] == EMPTY &&
                !isAttacked(0, 3, true) && !isAttacked(0, 2, true)) {
                validMoves[0][2] = true;
            }
        }
    }
    
    void filterLegalMoves(int fr, int fc, bool isWhite) {
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (!validMoves[r][c]) continue;
                
                // Try move
                int8_t saved = board[r][c];
                int8_t piece = board[fr][fc];
                board[r][c] = piece;
                board[fr][fc] = EMPTY;
                
                if (kingInCheck(isWhite)) validMoves[r][c] = false;
                
                // Undo
                board[fr][fc] = piece;
                board[r][c] = saved;
            }
        }
    }
    
    // =========================================================================
    // Check Detection
    // =========================================================================
    bool kingInCheck(bool whiteKing) {
        int8_t king = whiteKing ? W_KING : B_KING;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (board[r][c] == king)
                    return isAttacked(r, c, !whiteKing);
        return false;
    }
    
    bool isAttacked(int tr, int tc, bool byWhite) {
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int8_t piece = board[r][c];
                if (piece == EMPTY || (piece > 0) != byWhite) continue;
                if (canAttack(r, c, tr, tc)) return true;
            }
        }
        return false;
    }
    
    bool canAttack(int fr, int fc, int tr, int tc) {
        int8_t piece = board[fr][fc];
        int type = abs(piece);
        int dr = tr - fr, dc = tc - fc;
        int adr = abs(dr), adc = abs(dc);
        
        switch (type) {
            case 1: return adc == 1 && dr == ((piece > 0) ? -1 : 1);
            case 2: return (dr == 0 || dc == 0) && pathClear(fr, fc, tr, tc);
            case 3: return (adr == 2 && adc == 1) || (adr == 1 && adc == 2);
            case 4: return adr == adc && pathClear(fr, fc, tr, tc);
            case 5: return ((dr == 0 || dc == 0) || adr == adc) && pathClear(fr, fc, tr, tc);
            case 6: return adr <= 1 && adc <= 1;
        }
        return false;
    }
    
    bool pathClear(int fr, int fc, int tr, int tc) {
        int dr = (tr > fr) ? 1 : (tr < fr) ? -1 : 0;
        int dc = (tc > fc) ? 1 : (tc < fc) ? -1 : 0;
        int r = fr + dr, c = fc + dc;
        while (r != tr || c != tc) {
            if (board[r][c] != EMPTY) return false;
            r += dr; c += dc;
        }
        return true;
    }
    
    // =========================================================================
    // Move Execution
    // =========================================================================
    Move makeMove(int fr, int fc, int tr, int tc) {
        Move move(fr, fc, tr, tc);
        move.captured = board[tr][tc];
        
        int8_t piece = board[fr][fc];
        int type = abs(piece);
        
        // Detect special moves
        if (type == 6 && fc == 4) {
            if (tc == 6) move.special = 1;  // O-O
            if (tc == 2) move.special = 2;  // O-O-O
        }
        if (type == 1) {
            if (tc == epCol && board[tr][tc] == EMPTY) {
                move.special = 3;  // en passant
                move.captured = board[fr][tc];
            }
            if (tr == 0 || tr == 7) move.special = 4;  // promotion
        }
        
        execMove(move);
        return move;
    }
    
    void execMove(Move& move) {
        int8_t piece = board[move.fr][move.fc];
        bool isWhite = piece > 0;
        int type = abs(piece);
        
        // Handle special moves
        if (move.special == 1) {  // O-O
            board[move.tr][5] = board[move.tr][7];
            board[move.tr][7] = EMPTY;
        } else if (move.special == 2) {  // O-O-O
            board[move.tr][3] = board[move.tr][0];
            board[move.tr][0] = EMPTY;
        } else if (move.special == 3) {  // en passant
            board[move.fr][move.tc] = EMPTY;
        }
        
        // Make move
        board[move.tr][move.tc] = piece;
        board[move.fr][move.fc] = EMPTY;
        
        // Promotion (auto-queen)
        if (move.special == 4) {
            board[move.tr][move.tc] = isWhite ? W_QUEEN : B_QUEEN;
        }
        
        // Update castling rights
        if (type == 6) {
            if (isWhite) { wCastleK = wCastleQ = false; }
            else { bCastleK = bCastleQ = false; }
        }
        if (type == 2) {
            if (move.fr == 7 && move.fc == 7) wCastleK = false;
            if (move.fr == 7 && move.fc == 0) wCastleQ = false;
            if (move.fr == 0 && move.fc == 7) bCastleK = false;
            if (move.fr == 0 && move.fc == 0) bCastleQ = false;
        }
        
        // Update en passant
        epCol = -1;
        if (type == 1 && abs(move.tr - move.fr) == 2) {
            epCol = move.fc;
        }
        
        whiteTurn = !whiteTurn;
        if (whiteTurn) moveNum++;
    }
    
    void updateGameState() {
        inCheck = kingInCheck(whiteTurn);
        
        // Check for legal moves
        bool hasLegal = false;
        for (int r = 0; r < 8 && !hasLegal; r++) {
            for (int c = 0; c < 8 && !hasLegal; c++) {
                int8_t piece = board[r][c];
                if (piece == EMPTY || (piece > 0) != whiteTurn) continue;
                
                calcValidMoves(r, c);
                for (int tr = 0; tr < 8 && !hasLegal; tr++)
                    for (int tc = 0; tc < 8; tc++)
                        if (validMoves[tr][tc]) { hasLegal = true; break; }
            }
        }
        
        memset(validMoves, 0, sizeof(validMoves));
        
        if (!hasLegal) {
            gameOver = true;
            checkmate = inCheck;
            stalemate = !inCheck;
            deleteSavedGame();  // Clear save when game ends
        }
    }
    
    // =========================================================================
    // AI - Minimax with Alpha-Beta
    // =========================================================================
    Move findBestMove() {
        Move best;
        int bestScore = -100000;
        
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (board[r][c] >= 0) continue;  // Not black
                
                bool origTurn = whiteTurn;
                whiteTurn = false;
                calcValidMoves(r, c);
                whiteTurn = origTurn;
                
                for (int tr = 0; tr < 8; tr++) {
                    for (int tc = 0; tc < 8; tc++) {
                        if (!validMoves[tr][tc]) continue;
                        
                        // Try move
                        int8_t saved = board[tr][tc];
                        int8_t moving = board[r][c];
                        board[tr][tc] = moving;
                        board[r][c] = EMPTY;
                        
                        int8_t epSaved = EMPTY;
                        if (abs(moving) == 1 && tc == epCol && saved == EMPTY) {
                            epSaved = board[r][tc];
                            board[r][tc] = EMPTY;
                        }
                        
                        int score = minimax(2, -100000, 100000, true) + random(-3, 4);
                        
                        // Undo
                        board[r][c] = moving;
                        board[tr][tc] = saved;
                        if (epSaved != EMPTY) board[r][tc] = epSaved;
                        
                        if (score > bestScore) {
                            bestScore = score;
                            best = Move(r, c, tr, tc);
                            best.captured = saved;
                        }
                    }
                }
            }
        }
        
        memset(validMoves, 0, sizeof(validMoves));
        return best;
    }
    
    int minimax(int depth, int alpha, int beta, bool maxim) {
        if (depth == 0) return evaluate();
        
        if (maxim) {
            int maxEval = -100000;
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    if (board[r][c] >= 0) continue;
                    for (int tr = 0; tr < 8; tr++) {
                        for (int tc = 0; tc < 8; tc++) {
                            if (!quickValid(r, c, tr, tc, false)) continue;
                            
                            int8_t saved = board[tr][tc];
                            board[tr][tc] = board[r][c];
                            board[r][c] = EMPTY;
                            
                            int eval = minimax(depth - 1, alpha, beta, false);
                            
                            board[r][c] = board[tr][tc];
                            board[tr][tc] = saved;
                            
                            maxEval = max(maxEval, eval);
                            alpha = max(alpha, eval);
                            if (beta <= alpha) return maxEval;
                        }
                    }
                }
            }
            return maxEval;
        } else {
            int minEval = 100000;
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    if (board[r][c] <= 0) continue;
                    for (int tr = 0; tr < 8; tr++) {
                        for (int tc = 0; tc < 8; tc++) {
                            if (!quickValid(r, c, tr, tc, true)) continue;
                            
                            int8_t saved = board[tr][tc];
                            board[tr][tc] = board[r][c];
                            board[r][c] = EMPTY;
                            
                            int eval = minimax(depth - 1, alpha, beta, true);
                            
                            board[r][c] = board[tr][tc];
                            board[tr][tc] = saved;
                            
                            minEval = min(minEval, eval);
                            beta = min(beta, eval);
                            if (beta <= alpha) return minEval;
                        }
                    }
                }
            }
            return minEval;
        }
    }
    
    bool quickValid(int fr, int fc, int tr, int tc, bool isWhite) {
        int8_t piece = board[fr][fc];
        int8_t target = board[tr][tc];
        if (target != EMPTY && (target > 0) == isWhite) return false;
        
        int type = abs(piece);
        int dr = tr - fr, dc = tc - fc;
        int adr = abs(dr), adc = abs(dc);
        
        switch (type) {
            case 1: {
                int dir = isWhite ? -1 : 1;
                if (dc == 0 && target == EMPTY) {
                    if (dr == dir) return true;
                    if (dr == dir * 2 && fr == (isWhite ? 6 : 1) && board[fr + dir][fc] == EMPTY) return true;
                }
                if (adc == 1 && dr == dir && target != EMPTY) return true;
                return false;
            }
            case 2: return (dr == 0 || dc == 0) && pathClear(fr, fc, tr, tc);
            case 3: return (adr == 2 && adc == 1) || (adr == 1 && adc == 2);
            case 4: return adr == adc && pathClear(fr, fc, tr, tc);
            case 5: return ((dr == 0 || dc == 0) || adr == adc) && pathClear(fr, fc, tr, tc);
            case 6: return adr <= 1 && adc <= 1;
        }
        return false;
    }
    
    int evaluate() {
        int score = 0;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int8_t piece = board[r][c];
                if (piece == EMPTY) continue;
                
                int val = PIECE_VALUES[abs(piece)];
                
                // Pawn advancement
                if (abs(piece) == 1) {
                    val += ((piece > 0) ? (6 - r) : (r - 1)) * 10;
                }
                
                // Center control
                if (r >= 3 && r <= 4 && c >= 3 && c <= 4) val += 15;
                else if (r >= 2 && r <= 5 && c >= 2 && c <= 5) val += 5;
                
                score += (piece > 0) ? -val : val;  // Positive = good for black
            }
        }
        return score;
    }
};



#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_CHESSGAME_H
