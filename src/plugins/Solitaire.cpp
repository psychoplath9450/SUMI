/**
 * @file Solitaire.cpp
 * @brief Klondike Solitaire implementation
 * @version 2.1.26
 * 
 * Optimized with raw button handling
 */

#include "plugins/Solitaire.h"

#if FEATURE_GAMES

SolitaireGame solitaireGame;

SolitaireGame::SolitaireGame() { 
    reset(); 
}

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
    _state = GAME_PLAYING;
    _cursor = 2;  // Start on first tableau pile
    _selected = -1;
    _selectedPile = -1;
}

bool SolitaireGame::handleInput(Button btn) {
    // Use raw buttons - no remapping
    
    if (_state == GAME_WIN) {
        if (btn == BTN_CONFIRM) { newGame(); return true; }
        if (btn == BTN_BACK) return false;
        return true;
    }
    
    switch (btn) {
        case BTN_LEFT:
            if (_cursor > 0) _cursor--;
            return true;
        case BTN_RIGHT:
            if (_cursor < 8) _cursor++;
            return true;
        case BTN_UP:
            // Move from tableau to stock/foundation row
            if (_cursor >= 2) _cursor = 0;
            return true;
        case BTN_DOWN:
            // Move from stock/foundation to tableau
            if (_cursor < 2) _cursor = 2;
            return true;
        case BTN_CONFIRM:
            handleSelect();
            return true;
        case BTN_BACK:
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
    PluginUI::drawHeader("Solitaire", _screenW);
    
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
        display.drawRect(stockX - 2, startY - 2, pileSpacing * 2 - 4, _cardH + 4, GxEPD_BLACK);
    }
    
    // Foundation piles (cursor 1)
    for (int i = 0; i < 4; i++) {
        int x = startX + (3 + i) * pileSpacing;
        bool sel = (_cursor == 1 && _selectedPile == -1);
        
        if (_foundationTop[i] >= 0) {
            drawCard(_foundation[i][_foundationTop[i]], x, startY, sel && i == 0);
        } else {
            drawEmptyPile(x, startY);
            const char* suits[] = {"S", "H", "D", "C"};
            display.setCursor(x + _cardW/2 - 4, startY + _cardH/2 + 4);
            display.print(suits[i]);
        }
    }
    
    if (_cursor == 1) {
        int fx = startX + 3 * pileSpacing;
        display.drawRect(fx - 2, startY - 2, 4 * pileSpacing - 4, _cardH + 4, GxEPD_BLACK);
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
            PluginUI::drawCursor(x - 2, cursorY - 2, _cardW + 4, _cardH + 4);
        }
    }
    
    PluginUI::drawFooter("", "OK:Select/Move BACK:Cancel", _screenW, _screenH);
    
    if (_state == GAME_WIN) {
        PluginUI::drawGameOver("You Win!", "OK to play again", _screenW, _screenH);
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
    _state = GAME_PLAYING;
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
    _state = GAME_WIN;
}

void SolitaireGame::drawCard(int8_t card, int x, int y, bool highlight) {
    int rank = card % 13;
    int suit = card / 13;
    
    display.fillRect(x, y, _cardW, _cardH, GxEPD_WHITE);
    
    if (highlight) {
        display.drawRect(x, y, _cardW, _cardH, GxEPD_BLACK);
        display.drawRect(x+1, y+1, _cardW-2, _cardH-2, GxEPD_BLACK);
        display.drawRect(x+2, y+2, _cardW-4, _cardH-4, GxEPD_BLACK);
    } else {
        display.drawRect(x, y, _cardW, _cardH, GxEPD_BLACK);
    }
    
    const char* ranks[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};
    display.setCursor(x + 3, y + 12);
    display.print(ranks[rank]);
    
    const char* suits[] = {"S", "H", "D", "C"};
    display.setCursor(x + _cardW - 12, y + _cardH - 4);
    display.print(suits[suit]);
}

void SolitaireGame::drawCardBack(int x, int y) {
    display.fillRect(x, y, _cardW, _cardH, GxEPD_BLACK);
    display.drawRect(x + 3, y + 3, _cardW - 6, _cardH - 6, GxEPD_WHITE);
}

void SolitaireGame::drawEmptyPile(int x, int y) {
    display.drawRect(x, y, _cardW, _cardH, GxEPD_BLACK);
}

#endif // FEATURE_GAMES
