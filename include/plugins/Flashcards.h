/**
 * @file Flashcards.h
 * @brief Flashcard app with multi-format support and partial refresh
 * @version 1.3.0
 * 
 * Supported formats:
 * - TXT: Alternating lines (question\nanswer\n) or Q;A per line
 * - CSV: question,answer (Anki, Quizlet export)
 * - TSV: term<tab>definition (Quizlet export)
 * - JSON: Multiple field name support:
 *   - front/back
 *   - question/answer  
 *   - term/definition
 *   - word/meaning
 *   - kanji/reading (for Japanese)
 */

#ifndef SUMI_PLUGIN_FLASHCARDS_H
#define SUMI_PLUGIN_FLASHCARDS_H

#include "config.h"
#if FEATURE_FLASHCARDS

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class FlashcardsApp {
public:
    static const int MAX_DECKS = 20;
    static const int MAX_CARDS = 50;   // Reduced from 100 - plenty for a session
    static const int MAX_TEXT = 80;    // Reduced from 120 - sufficient for most cards
    
    enum Mode { 
        MODE_DECKS,      // Deck selection
        MODE_QUESTION,   // Show question
        MODE_ANSWER,     // Show answer
        MODE_DONE,       // Session complete
        MODE_SETTINGS    // Settings menu
    };
    
    enum DeckFormat { FMT_UNKNOWN, FMT_TXT, FMT_CSV, FMT_TSV, FMT_JSON };
    
    enum FontSizeSetting { FONT_SMALL = 0, FONT_MEDIUM = 1, FONT_LARGE = 2 };
    
    struct FlashcardSettings {
        uint32_t magic = 0x464C5348;  // "FLSH"
        FontSizeSetting fontSize = FONT_MEDIUM;
        bool shuffleOnLoad = true;
        bool showProgressBar = true;
        bool showStats = true;
        uint8_t reserved[10] = {0};
        
        void load();
        void save();
    };
    
    struct Card {
        char front[MAX_TEXT];
        char back[MAX_TEXT];
        bool seen;
    };
    
    struct DeckInfo {
        char name[28];
        DeckFormat format;
    };
    
    FlashcardsApp();
    ~FlashcardsApp();
    
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();
    
private:
    DeckInfo _decks[MAX_DECKS];
    int _deckCount;
    
    Card* _cards;
    int _cardCount;
    int _cardIdx;
    int _correct;
    int _incorrect;
    
    Mode _mode;
    int _cursor;
    int _scroll;
    int _screenW, _screenH;
    bool _landscape;
    int _itemH, _itemsPerPage;
    
    // Partial refresh tracking
    bool _needsFullRedraw;
    Mode _lastMode;
    
    // Settings
    FlashcardSettings _settings;
    int _settingsCursor;
    
    // Format detection
    DeckFormat detectFormat(const char* filename);
    bool isSupportedFormat(const char* filename);
    
    // Deck operations
    void scanDecks();
    void loadDeck();
    void closeDeck();
    void shuffleCards();
    
    // Format loaders
    bool loadTxtDeck(const char* path);
    bool loadCsvDeck(const char* path, char delimiter);
    bool loadJsonDeck(const char* path);
    
    // CSV/TSV helpers
    bool isHeaderRow(const char* line);
    bool parseCsvLine(const char* line, char delim, char* front, char* back);
    
    // JSON helpers
    void parseJsonCards(const char* json);
    bool extractJsonString(const char* start, const char* end, const char* key, char* value, int maxLen);
    
    // Utility
    void trimString(char* str);
    void nextCard();
    
    // Drawing
    void drawDeckList();
    void drawCard(bool showAnswer);
    void drawWrappedText(const char* text, int x, int y, int maxWidth, int maxLines);
    void drawCenteredText(const char* text, int x, int y, int width, int height);
    void drawWrappedTextCentered(const char* text, int x, int y, int maxWidth, int maxLines);
    void drawDone();
    void drawSettings();
    void drawProgressBar(int x, int y, int w, int h, int current, int total);
    
    // Image support for ASL and other visual flashcards
    bool isImagePath(const char* text);
    bool drawFlashcardImage(const char* path, int x, int y, int maxW, int maxH);
    
    // Font helper
    const GFXfont* getCardFont();
};


#endif // FEATURE_FLASHCARDS
#endif // SUMI_PLUGIN_FLASHCARDS_H
