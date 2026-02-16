#pragma once

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"
#include "icons/suit_icons.h"
#include "icons/card_back_icon.h"

namespace sumi {

/**
 * @file Solitaire.h
 * @brief Klondike Solitaire for Sumi e-reader
 * @version 1.0.0
 */


class SolitaireGame : public PluginInterface {
public:

  const char* name() const override { return "Solitaire"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }
    explicit SolitaireGame(PluginRenderer& renderer) : d_(renderer) { reset(); }
    void init(int screenW, int screenH) override;
    void newGame();
    bool handleInput(PluginButton btn) override;
    void draw() override;
    
  PluginRenderer& d_;

private:
    int8_t _stock[24];
    int _stockTop;
    int8_t _waste[24];
    int _wasteTop;
    int8_t _foundation[4][13];
    int _foundationTop[4];
    int8_t _tableau[7][20];
    int _tableauSize[7];
    int _tableauFaceDown[7];
    GameState _state;
    int _cursor;
    int _selected;
    int _selectedPile;
    int _screenW, _screenH;
    bool _landscape;
    int _cardW, _cardH, _stackGap;
    int _moves;  // Move counter
    
    void reset();
    void initDeck();
    void shuffle();
    void deal();
    void handleSelect();
    bool canPlaceOnTableau(int8_t card, int pile);
    void removeFromSource();
    void checkWin();
    void drawCard(int8_t card, int x, int y, bool highlight);
    void drawCardBack(int x, int y);
    void drawEmptyPile(int x, int y);
};


// === IMPLEMENTATION ===

/**
 * @file Solitaire.cpp
 * @brief Klondike Solitaire implementation
 * @version 1.0.0
 * 
 * Optimized with raw button handling
 */


void SolitaireGame::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    
    // Card sizes based on orientation
    _cardW = _landscape ? 50 : 40;
    _cardH = _landscape ? 70 : 56;
    _stackGap = _landscape ? 14 : 12;
    
    Serial.printf("[SOLITAIRE] Init: %dx%d card=%dx%d\n", screenW, screenH, _cardW, _cardH);
    newGame();
}

void SolitaireGame::newGame() {
    reset();
    initDeck();
    shuffle();
    deal();
    _state = GameState::Playing;
    _cursor = 2;  // Start on first tableau pile
    _selected = -1;
    _selectedPile = -1;
    _moves = 0;
}

bool SolitaireGame::handleInput(PluginButton btn) {
    // Use raw buttons - no remapping
    
    if (_state == GameState::Win) {
        if (btn == PluginButton::Center) { newGame(); return true; }
        if (btn == PluginButton::Back) return false;
        return true;
    }
    
    switch (btn) {
        case PluginButton::Left:
            if (_cursor > 0) _cursor--;
            return true;
        case PluginButton::Right:
            if (_cursor < 8) _cursor++;
            return true;
        case PluginButton::Up:
            // Move from tableau to stock/foundation row
            if (_cursor >= 2) _cursor = 0;
            return true;
        case PluginButton::Down:
            // Move from stock/foundation to tableau
            if (_cursor < 2) _cursor = 2;
            return true;
        case PluginButton::Center:
            handleSelect();
            return true;
        case PluginButton::Back:
            if (_selected >= 0) {
                _selected = -1;
                _selectedPile = -1;
                return true;
            }
            return false;
        default:
            return false;
    }
}

void SolitaireGame::draw() {
    PluginUI::drawHeader(d_, "Solitaire", _screenW);
    
    int startY = PLUGIN_HEADER_H + 8;
    int pileSpacing = _cardW + 8;
    int startX = (_screenW - 7 * pileSpacing + 8) / 2;
    
    // Stock and waste piles (cursor 0)
    int stockX = startX;
    int wasteX = startX + pileSpacing;
    
    if (_stockTop >= 0) {
        drawCardBack(stockX, startY);
    } else {
        drawEmptyPile(stockX, startY);
    }
    
    if (_wasteTop >= 0) {
        bool wasteSel = (_cursor == 0 && _selectedPile == -1) || (_selectedPile == -2);
        drawCard(_waste[_wasteTop], wasteX, startY, wasteSel);
    } else {
        drawEmptyPile(wasteX, startY);
    }
    
    // Cursor around stock/waste area
    if (_cursor == 0) {
        d_.drawRect(stockX - 2, startY - 2, pileSpacing * 2 - 4, _cardH + 4, GxEPD_BLACK);
    }
    
    // Foundation piles (cursor 1)
    for (int i = 0; i < 4; i++) {
        int x = startX + (3 + i) * pileSpacing;
        bool sel = (_cursor == 1 && _selectedPile == -1);
        
        if (_foundationTop[i] >= 0) {
            drawCard(_foundation[i][_foundationTop[i]], x, startY, sel && i == 0);
        } else {
            drawEmptyPile(x, startY);
            // Draw suit icon centered in empty foundation pile
            const uint8_t* suitBmp = SUIT_ICONS[i];
            d_.drawBitmap(x + (_cardW - SUIT_W) / 2, startY + (_cardH - SUIT_H) / 2,
                             suitBmp, SUIT_W, SUIT_H, GxEPD_BLACK);
        }
    }
    
    if (_cursor == 1) {
        int fx = startX + 3 * pileSpacing;
        d_.drawRect(fx - 2, startY - 2, 4 * pileSpacing - 4, _cardH + 4, GxEPD_BLACK);
    }
    
    // Tableau piles (cursor 2-8)
    int tableauY = startY + _cardH + 15;
    for (int pile = 0; pile < 7; pile++) {
        int x = startX + pile * pileSpacing;
        bool selected = (pile + 2 == _cursor);
        
        if (_tableauSize[pile] == 0) {
            drawEmptyPile(x, tableauY);
        } else {
            int y = tableauY;
            // Draw face-down cards
            for (int i = 0; i < _tableauFaceDown[pile]; i++) {
                drawCardBack(x, y);
                y += _stackGap;
            }
            
            // Draw face-up cards
            for (int i = _tableauFaceDown[pile]; i < _tableauSize[pile]; i++) {
                bool isSelected = selected && (i == _tableauSize[pile] - 1);
                if (_selectedPile == pile) isSelected = true;
                drawCard(_tableau[pile][i], x, y, isSelected);
                y += _stackGap;
            }
        }
        
        // Cursor on this pile
        if (selected && _selectedPile == -1) {
            int cursorY = tableauY;
            if (_tableauSize[pile] > 0) {
                cursorY += (_tableauSize[pile] - 1) * _stackGap;
            }
            PluginUI::drawCursor(d_, x - 2, cursorY - 2, _cardW + 4, _cardH + 4);
        }
    }
    
    char moveStatus[24];
    snprintf(moveStatus, sizeof(moveStatus), "Moves: %d", _moves);
    PluginUI::drawFooter(d_, moveStatus, "OK:Select/Move BACK:Cancel", _screenW, _screenH);
    
    if (_state == GameState::Win) {
        PluginUI::drawGameOver(d_, "You Win!", "OK to play again", _screenW, _screenH);
    }
}

void SolitaireGame::reset() {
    _stockTop = -1;
    _wasteTop = -1;
    for (int i = 0; i < 4; i++) _foundationTop[i] = -1;
    for (int i = 0; i < 7; i++) {
        _tableauSize[i] = 0;
        _tableauFaceDown[i] = 0;
    }
    _state = GameState::Playing;
    _cursor = 2;
    _selected = -1;
    _selectedPile = -1;
}

void SolitaireGame::initDeck() {
    for (int i = 0; i < 52; i++) {
        _stock[i < 24 ? i : 0] = i;
    }
}

void SolitaireGame::shuffle() {
    int8_t deck[52];
    for (int i = 0; i < 52; i++) deck[i] = i;
    
    // Fisher-Yates shuffle
    for (int i = 51; i > 0; i--) {
        int j = random(0, i + 1);
        int8_t tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
    
    // First 24 to stock
    for (int i = 0; i < 24; i++) {
        _stock[i] = deck[i];
    }
    _stockTop = 23;
    
    // Deal tableau
    int cardIdx = 24;
    for (int pile = 0; pile < 7; pile++) {
        for (int i = 0; i <= pile; i++) {
            _tableau[pile][i] = deck[cardIdx++];
            _tableauSize[pile]++;
        }
        _tableauFaceDown[pile] = pile;  // All but top card face down
    }
}

void SolitaireGame::deal() {
    // Already dealt in shuffle()
}

void SolitaireGame::handleSelect() {
    if (_cursor == 0) {
        // Stock/waste area
        if (_selected == -1) {
            // Draw from stock or recycle waste
            if (_stockTop >= 0) {
                _wasteTop++;
                _waste[_wasteTop] = _stock[_stockTop];
                _stockTop--;
            } else if (_wasteTop >= 0) {
                // Recycle waste to stock
                while (_wasteTop >= 0) {
                    _stockTop++;
                    _stock[_stockTop] = _waste[_wasteTop];
                    _wasteTop--;
                }
            }
        } else {
            // Try to place selected card (shouldn't happen here)
            _selected = -1;
            _selectedPile = -1;
        }
    } else if (_cursor == 1) {
        // Foundation area
        if (_selected >= 0) {
            int suit = _selected / 13;
            int rank = _selected % 13;
            
            // Can place if next in sequence
            if (_foundationTop[suit] == rank - 1) {
                _foundationTop[suit]++;
                _foundation[suit][_foundationTop[suit]] = _selected;
                removeFromSource();
                _moves++;
                checkWin();
            }
            _selected = -1;
            _selectedPile = -1;
        }
    } else {
        // Tableau piles
        int pile = _cursor - 2;
        
        if (_selected == -1) {
            // Select from this pile or waste
            if (_tableauSize[pile] > 0) {
                int topIdx = _tableauSize[pile] - 1;
                if (topIdx >= _tableauFaceDown[pile]) {
                    _selected = _tableau[pile][topIdx];
                    _selectedPile = pile;
                }
            } else if (_wasteTop >= 0) {
                // Empty pile - can select from waste
                _selected = _waste[_wasteTop];
                _selectedPile = -2;
            }
        } else {
            // Try to place selected card
            if (canPlaceOnTableau(_selected, pile)) {
                _tableauSize[pile]++;
                _tableau[pile][_tableauSize[pile] - 1] = _selected;
                removeFromSource();
                _moves++;
            }
            _selected = -1;
            _selectedPile = -1;
        }
    }
}

bool SolitaireGame::canPlaceOnTableau(int8_t card, int pile) {
    if (_tableauSize[pile] == 0) {
        // Empty pile - only King can go
        return (card % 13) == 12;
    }
    
    int8_t topCard = _tableau[pile][_tableauSize[pile] - 1];
    int topRank = topCard % 13;
    int topSuit = topCard / 13;
    int cardRank = card % 13;
    int cardSuit = card / 13;
    
    // Alternating colors
    bool topRed = (topSuit == 1 || topSuit == 2);  // Hearts or Diamonds
    bool cardRed = (cardSuit == 1 || cardSuit == 2);
    
    return (cardRank == topRank - 1) && (topRed != cardRed);
}

void SolitaireGame::removeFromSource() {
    if (_selectedPile == -2) {
        // From waste
        _wasteTop--;
    } else if (_selectedPile >= 0 && _selectedPile < 7) {
        // From tableau
        _tableauSize[_selectedPile]--;
        // Flip card if needed
        if (_tableauSize[_selectedPile] > 0 && 
            _tableauFaceDown[_selectedPile] >= _tableauSize[_selectedPile]) {
            _tableauFaceDown[_selectedPile]--;
        }
    }
}

void SolitaireGame::checkWin() {
    for (int i = 0; i < 4; i++) {
        if (_foundationTop[i] < 12) return;
    }
    _state = GameState::Win;
}

void SolitaireGame::drawCard(int8_t card, int x, int y, bool highlight) {
    int rank = card % 13;
    int suit = card / 13;
    
    d_.fillRect(x, y, _cardW, _cardH, GxEPD_WHITE);
    
    if (highlight) {
        d_.drawRect(x, y, _cardW, _cardH, GxEPD_BLACK);
        d_.drawRect(x+1, y+1, _cardW-2, _cardH-2, GxEPD_BLACK);
        d_.drawRect(x+2, y+2, _cardW-4, _cardH-4, GxEPD_BLACK);
    } else {
        d_.drawRect(x, y, _cardW, _cardH, GxEPD_BLACK);
    }
    
    const char* ranks[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};
    d_.setCursor(x + 3, y + 12);
    d_.print(ranks[rank]);
    
    // Draw suit bitmap in bottom-right area
    const uint8_t* suitBmp = SUIT_ICONS[suit];
    int sx = x + _cardW - SUIT_W - 2;
    int sy = y + _cardH - SUIT_H - 2;
    d_.drawBitmap(sx, sy, suitBmp, SUIT_W, SUIT_H, GxEPD_BLACK);
}

void SolitaireGame::drawCardBack(int x, int y) {
    d_.fillRect(x, y, _cardW, _cardH, GxEPD_BLACK);
    // Draw diamond lattice pattern centered on card
    int bx = x + (_cardW - CARD_BACK_W) / 2;
    int by = y + (_cardH - CARD_BACK_H) / 2;
    d_.drawBitmap(bx, by, CARD_BACK_PATTERN, CARD_BACK_W, CARD_BACK_H, GxEPD_WHITE);
}

void SolitaireGame::drawEmptyPile(int x, int y) {
    d_.drawRect(x, y, _cardW, _cardH, GxEPD_BLACK);
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
