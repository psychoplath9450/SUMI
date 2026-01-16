/**
 * @file Solitaire.h
 * @brief Klondike Solitaire for Sumi e-reader
 * @version 2.1.26
 */

#ifndef SUMI_PLUGIN_SOLITAIRE_H
#define SUMI_PLUGIN_SOLITAIRE_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class SolitaireGame {
public:
    SolitaireGame();
    void init(int screenW, int screenH);
    void newGame();
    bool handleInput(Button btn);
    void draw();
    
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

extern SolitaireGame solitaireGame;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_SOLITAIRE_H
