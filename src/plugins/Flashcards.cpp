/**
 * @file Flashcards.cpp
 * @brief Flashcard app with partial refresh and multi-format support
 * @version 2.1.25
 * 
 * Features:
 * - Partial refresh when flipping cards (fast!)
 * - Progress bar showing session progress
 * - Correct/Incorrect tracking with stats
 * - Multiple format support (TXT, CSV, TSV, JSON)
 * - Shuffle option
 */

#include <SD.h>
#include <new>
#include "plugins/Flashcards.h"

#if FEATURE_FLASHCARDS

FlashcardsApp flashcardsApp;

// Layout constants
static const int CARD_MARGIN = 20;
static const int CARD_TOP = 60;
static const int PROGRESS_H = 8;

// =============================================================================
// Constructor / Destructor
// =============================================================================

FlashcardsApp::FlashcardsApp() 
    : _mode(MODE_DECKS), _lastMode(MODE_DECKS), _cursor(0), _scroll(0), 
      _deckCount(0), _cards(nullptr), _cardCount(0), _cardIdx(0),
      _correct(0), _incorrect(0), _needsFullRedraw(true) {}

FlashcardsApp::~FlashcardsApp() {
    closeDeck();
}

// =============================================================================
// Public Methods
// =============================================================================

void FlashcardsApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _itemH = 52;
    _itemsPerPage = (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 20) / _itemH;
    if (_itemsPerPage < 1) _itemsPerPage = 1;
    
    _mode = MODE_DECKS;
    _lastMode = MODE_DECKS;
    _cursor = 0;
    _scroll = 0;
    _needsFullRedraw = true;
    closeDeck();
    
    scanDecks();
}

bool FlashcardsApp::handleInput(Button btn) {
    bool modeChanged = false;
    
    if (_mode == MODE_DECKS) {
        switch (btn) {
            case BTN_UP:
                if (_cursor > 0) {
                    _cursor--;
                    if (_cursor < _scroll) _scroll = _cursor;
                    _needsFullRedraw = true;
                }
                return true;
            case BTN_DOWN:
                if (_cursor < _deckCount - 1) {
                    _cursor++;
                    if (_cursor >= _scroll + _itemsPerPage) _scroll++;
                    _needsFullRedraw = true;
                }
                return true;
            case BTN_CONFIRM:
                if (_deckCount > 0) {
                    loadDeck();
                    if (_cardCount > 0) {
                        _mode = MODE_QUESTION;
                        _needsFullRedraw = true;
                    }
                }
                return true;
            case BTN_BACK:
                return false;  // Exit app
            default:
                return true;
        }
    } 
    else if (_mode == MODE_QUESTION) {
        switch (btn) {
            case BTN_CONFIRM:
            case BTN_DOWN:
                // Reveal answer - use partial refresh
                _lastMode = _mode;
                _mode = MODE_ANSWER;
                _needsFullRedraw = false;  // Partial refresh only
                return true;
            case BTN_BACK:
                closeDeck();
                _mode = MODE_DECKS;
                _needsFullRedraw = true;
                return true;
            case BTN_LEFT:
                // Shuffle cards
                shuffleCards();
                _cardIdx = 0;
                _correct = 0;
                _incorrect = 0;
                _needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    else if (_mode == MODE_ANSWER) {
        switch (btn) {
            case BTN_RIGHT:
            case BTN_CONFIRM:
                // Mark correct, next card
                _correct++;
                nextCard();
                return true;
            case BTN_LEFT:
            case BTN_UP:
                // Mark incorrect, next card
                _incorrect++;
                nextCard();
                return true;
            case BTN_DOWN:
                // Just next (no marking)
                nextCard();
                return true;
            case BTN_BACK:
                // Go back to question
                _lastMode = _mode;
                _mode = MODE_QUESTION;
                _needsFullRedraw = false;
                return true;
            default:
                return true;
        }
    }
    else if (_mode == MODE_DONE) {
        if (btn == BTN_CONFIRM || btn == BTN_BACK) {
            closeDeck();
            _mode = MODE_DECKS;
            _needsFullRedraw = true;
        }
        return true;
    }
    
    return true;
}

void FlashcardsApp::draw() {
    // NOTE: Don't do display.firstPage/nextPage here - PluginRunner handles that
    
    if (_mode == MODE_DECKS) {
        drawDeckList();
    }
    else if (_mode == MODE_QUESTION || _mode == MODE_ANSWER) {
        bool showAnswer = (_mode == MODE_ANSWER);
        drawCard(showAnswer);
    }
    else if (_mode == MODE_DONE) {
        drawDone();
    }
}

// =============================================================================
// Format Detection
// =============================================================================

FlashcardsApp::DeckFormat FlashcardsApp::detectFormat(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return FMT_UNKNOWN;
    
    if (strcasecmp(ext, ".txt") == 0) return FMT_TXT;
    if (strcasecmp(ext, ".csv") == 0) return FMT_CSV;
    if (strcasecmp(ext, ".tsv") == 0) return FMT_TSV;
    if (strcasecmp(ext, ".json") == 0) return FMT_JSON;
    
    return FMT_UNKNOWN;
}

bool FlashcardsApp::isSupportedFormat(const char* filename) {
    return detectFormat(filename) != FMT_UNKNOWN;
}

// =============================================================================
// Deck Scanning
// =============================================================================

void FlashcardsApp::scanDecks() {
    _deckCount = 0;
    File dir = SD.open("/flashcards");
    if (!dir) {
        Serial.println("[FLASH] Creating /flashcards directory");
        SD.mkdir("/flashcards");
        return;
    }
    
    while (File f = dir.openNextFile()) {
        if (_deckCount >= MAX_DECKS) break;
        const char* n = f.name();
        
        if (n[0] != '.' && isSupportedFormat(n)) {
            strncpy(_decks[_deckCount].name, n, 27);
            _decks[_deckCount].name[27] = '\0';
            _decks[_deckCount].format = detectFormat(n);
            _deckCount++;
        }
        f.close();
    }
    dir.close();
    
    Serial.printf("[FLASH] Found %d decks\n", _deckCount);
}

// =============================================================================
// Deck Loading
// =============================================================================

void FlashcardsApp::loadDeck() {
    char path[48];
    snprintf(path, sizeof(path), "/flashcards/%s", _decks[_cursor].name);
    
    Serial.printf("[FLASH] Loading: %s\n", path);
    
    closeDeck();
    
    _cards = new (std::nothrow) Card[MAX_CARDS];
    if (!_cards) {
        Serial.println("[FLASH] Out of memory");
        return;
    }
    memset(_cards, 0, sizeof(Card) * MAX_CARDS);
    
    _cardCount = 0;
    _cardIdx = 0;
    _correct = 0;
    _incorrect = 0;
    
    DeckFormat fmt = _decks[_cursor].format;
    bool success = false;
    
    switch (fmt) {
        case FMT_TXT:  success = loadTxtDeck(path); break;
        case FMT_CSV:  success = loadCsvDeck(path, ','); break;
        case FMT_TSV:  success = loadCsvDeck(path, '\t'); break;
        case FMT_JSON: success = loadJsonDeck(path); break;
        default: break;
    }
    
    if (!success || _cardCount == 0) {
        Serial.println("[FLASH] Failed to load deck");
        closeDeck();
    } else {
        Serial.printf("[FLASH] Loaded %d cards\n", _cardCount);
    }
}

void FlashcardsApp::closeDeck() {
    if (_cards) {
        delete[] _cards;
        _cards = nullptr;
    }
    _cardCount = 0;
    _cardIdx = 0;
}

void FlashcardsApp::shuffleCards() {
    if (!_cards || _cardCount < 2) return;
    
    // Fisher-Yates shuffle
    for (int i = _cardCount - 1; i > 0; i--) {
        int j = random(0, i + 1);
        Card temp = _cards[i];
        _cards[i] = _cards[j];
        _cards[j] = temp;
    }
    Serial.println("[FLASH] Cards shuffled");
}

// =============================================================================
// TXT Format - supports alternating lines or semicolon separated
// =============================================================================

bool FlashcardsApp::loadTxtDeck(const char* path) {
    File f = SD.open(path);
    if (!f) return false;
    
    char line[256];
    
    while (f.available() && _cardCount < MAX_CARDS) {
        int len = f.readBytesUntil('\n', line, 255);
        line[len] = '\0';
        trimString(line);
        
        if (strlen(line) == 0) continue;
        
        // Check for semicolon separator (Q;A format)
        char* sep = strchr(line, ';');
        if (sep) {
            *sep = '\0';
            trimString(line);
            trimString(sep + 1);
            
            if (strlen(line) > 0 && strlen(sep + 1) > 0) {
                strncpy(_cards[_cardCount].front, line, MAX_TEXT - 1);
                strncpy(_cards[_cardCount].back, sep + 1, MAX_TEXT - 1);
                _cardCount++;
            }
        } else {
            // Alternating lines format
            strncpy(_cards[_cardCount].front, line, MAX_TEXT - 1);
            
            if (f.available()) {
                len = f.readBytesUntil('\n', line, 255);
                line[len] = '\0';
                trimString(line);
                
                if (strlen(line) > 0) {
                    strncpy(_cards[_cardCount].back, line, MAX_TEXT - 1);
                    _cardCount++;
                }
            }
        }
    }
    
    f.close();
    return true;
}

// =============================================================================
// CSV/TSV Format - Anki/Quizlet compatible
// =============================================================================

bool FlashcardsApp::loadCsvDeck(const char* path, char delimiter) {
    File f = SD.open(path);
    if (!f) return false;
    
    char line[256];
    bool firstLine = true;
    
    while (f.available() && _cardCount < MAX_CARDS) {
        int len = f.readBytesUntil('\n', line, 255);
        line[len] = '\0';
        trimString(line);
        
        if (strlen(line) == 0) continue;
        
        // Skip header row
        if (firstLine) {
            firstLine = false;
            if (isHeaderRow(line)) continue;
        }
        
        char front[MAX_TEXT] = {0};
        char back[MAX_TEXT] = {0};
        
        if (parseCsvLine(line, delimiter, front, back)) {
            strncpy(_cards[_cardCount].front, front, MAX_TEXT - 1);
            strncpy(_cards[_cardCount].back, back, MAX_TEXT - 1);
            _cardCount++;
        }
    }
    
    f.close();
    return true;
}

bool FlashcardsApp::isHeaderRow(const char* line) {
    // Common header field names
    const char* headers[] = {
        "front", "back", "question", "answer", "term", "definition",
        "word", "meaning", "kanji", "reading", "vocab", "translation"
    };
    
    char lower[64];
    strncpy(lower, line, 63);
    lower[63] = '\0';
    for (char* p = lower; *p; p++) *p = tolower(*p);
    
    for (const char* h : headers) {
        if (strstr(lower, h)) return true;
    }
    return false;
}

bool FlashcardsApp::parseCsvLine(const char* line, char delim, char* front, char* back) {
    const char* p = line;
    int idx = 0;
    bool inQuote = false;
    char field[MAX_TEXT];
    int fieldLen = 0;
    int fieldNum = 0;
    
    while (*p && fieldNum < 2) {
        if (*p == '"') {
            inQuote = !inQuote;
        } else if (*p == delim && !inQuote) {
            field[fieldLen] = '\0';
            trimString(field);
            
            // Remove surrounding quotes
            if (field[0] == '"' && fieldLen > 1) {
                memmove(field, field + 1, fieldLen);
                fieldLen = strlen(field);
                if (fieldLen > 0 && field[fieldLen-1] == '"') {
                    field[fieldLen-1] = '\0';
                }
            }
            
            if (fieldNum == 0) strncpy(front, field, MAX_TEXT - 1);
            else if (fieldNum == 1) strncpy(back, field, MAX_TEXT - 1);
            
            fieldNum++;
            fieldLen = 0;
        } else {
            if (fieldLen < MAX_TEXT - 1) field[fieldLen++] = *p;
        }
        p++;
    }
    
    // Last field
    if (fieldLen > 0 && fieldNum < 2) {
        field[fieldLen] = '\0';
        trimString(field);
        
        if (field[0] == '"' && fieldLen > 1) {
            memmove(field, field + 1, fieldLen);
            fieldLen = strlen(field);
            if (fieldLen > 0 && field[fieldLen-1] == '"') {
                field[fieldLen-1] = '\0';
            }
        }
        
        if (fieldNum == 0) strncpy(front, field, MAX_TEXT - 1);
        else if (fieldNum == 1) strncpy(back, field, MAX_TEXT - 1);
        fieldNum++;
    }
    
    return strlen(front) > 0 && strlen(back) > 0;
}

// =============================================================================
// JSON Format - supports multiple field names
// =============================================================================

bool FlashcardsApp::loadJsonDeck(const char* path) {
    File f = SD.open(path);
    if (!f) return false;
    
    size_t fileSize = f.size();
    if (fileSize > 32000) {
        Serial.println("[FLASH] JSON too large");
        f.close();
        return false;
    }
    
    char* json = (char*)malloc(fileSize + 1);
    if (!json) {
        f.close();
        return false;
    }
    
    f.readBytes(json, fileSize);
    json[fileSize] = '\0';
    f.close();
    
    parseJsonCards(json);
    free(json);
    
    return _cardCount > 0;
}

void FlashcardsApp::parseJsonCards(const char* json) {
    const char* p = json;
    
    // Find array start
    while (*p && *p != '[') p++;
    if (!*p) return;
    p++;
    
    while (*p && _cardCount < MAX_CARDS) {
        // Find object start
        while (*p && *p != '{') {
            if (*p == ']') return;  // End of array
            p++;
        }
        if (!*p) return;
        
        const char* objStart = p;
        p++;
        
        // Find object end
        int braceCount = 1;
        while (*p && braceCount > 0) {
            if (*p == '{') braceCount++;
            else if (*p == '}') braceCount--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p+1)) p++;
                    p++;
                }
            }
            if (*p) p++;
        }
        const char* objEnd = p - 1;
        
        char front[MAX_TEXT] = {0};
        char back[MAX_TEXT] = {0};
        
        // Try multiple field name combinations
        // Front field names
        if (!extractJsonString(objStart, objEnd, "front", front, MAX_TEXT)) {
            if (!extractJsonString(objStart, objEnd, "question", front, MAX_TEXT)) {
                if (!extractJsonString(objStart, objEnd, "term", front, MAX_TEXT)) {
                    if (!extractJsonString(objStart, objEnd, "word", front, MAX_TEXT)) {
                        if (!extractJsonString(objStart, objEnd, "kanji", front, MAX_TEXT)) {
                            extractJsonString(objStart, objEnd, "vocab", front, MAX_TEXT);
                        }
                    }
                }
            }
        }
        
        // Back field names
        if (!extractJsonString(objStart, objEnd, "back", back, MAX_TEXT)) {
            if (!extractJsonString(objStart, objEnd, "answer", back, MAX_TEXT)) {
                if (!extractJsonString(objStart, objEnd, "definition", back, MAX_TEXT)) {
                    if (!extractJsonString(objStart, objEnd, "meaning", back, MAX_TEXT)) {
                        if (!extractJsonString(objStart, objEnd, "reading", back, MAX_TEXT)) {
                            extractJsonString(objStart, objEnd, "translation", back, MAX_TEXT);
                        }
                    }
                }
            }
        }
        
        if (strlen(front) > 0 && strlen(back) > 0) {
            strncpy(_cards[_cardCount].front, front, MAX_TEXT - 1);
            strncpy(_cards[_cardCount].back, back, MAX_TEXT - 1);
            _cardCount++;
        }
    }
}

bool FlashcardsApp::extractJsonString(const char* start, const char* end, const char* key, char* value, int maxLen) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    
    const char* keyPos = strstr(start, pattern);
    if (!keyPos || keyPos >= end) return false;
    
    const char* p = keyPos + strlen(pattern);
    while (p < end && *p != ':') p++;
    if (p >= end) return false;
    p++;
    
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    
    if (*p != '"') return false;
    p++;
    
    int len = 0;
    while (p < end && *p != '"' && len < maxLen - 1) {
        if (*p == '\\' && (p + 1) < end) {
            p++;
            switch (*p) {
                case 'n': value[len++] = ' '; break;
                case 't': value[len++] = ' '; break;
                case 'r': break;
                case '"': value[len++] = '"'; break;
                case '\\': value[len++] = '\\'; break;
                default: value[len++] = *p; break;
            }
        } else {
            value[len++] = *p;
        }
        p++;
    }
    value[len] = '\0';
    
    return len > 0;
}

// =============================================================================
// Utility
// =============================================================================

void FlashcardsApp::trimString(char* str) {
    char* start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || 
                      str[len-1] == '\r' || str[len-1] == '\n')) {
        str[--len] = '\0';
    }
}

void FlashcardsApp::nextCard() {
    _cardIdx++;
    _needsFullRedraw = true;
    
    if (_cardIdx >= _cardCount) {
        _mode = MODE_DONE;
    } else {
        _mode = MODE_QUESTION;
    }
}

// =============================================================================
// Drawing - Deck List
// =============================================================================

void FlashcardsApp::drawDeckList() {
    // Header
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(CARD_MARGIN, 35);
    display.print("Flashcards");
    
    display.setFont(&FreeSans9pt7b);
    
    // Divider
    display.drawLine(0, 48, _screenW, 48, GxEPD_BLACK);
    
    if (_deckCount == 0) {
        display.setCursor(CARD_MARGIN, _screenH / 2 - 30);
        display.print("No decks found!");
        
        display.setCursor(CARD_MARGIN, _screenH / 2);
        display.print("Add files to /flashcards/");
        
        display.setCursor(CARD_MARGIN, _screenH / 2 + 30);
        display.print("Formats: .txt .csv .tsv .json");
        return;
    }
    
    int y = 60;
    int endIdx = _scroll + _itemsPerPage;
    if (endIdx > _deckCount) endIdx = _deckCount;
    
    for (int i = _scroll; i < endIdx; i++) {
        bool sel = (i == _cursor);
        
        // Selection highlight
        if (sel) {
            display.fillRoundRect(CARD_MARGIN - 4, y - 4, 
                                  _screenW - 2*CARD_MARGIN + 8, _itemH - 4, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(CARD_MARGIN - 4, y - 4,
                                  _screenW - 2*CARD_MARGIN + 8, _itemH - 4, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        
        // Format badge
        const char* fmtStr = "";
        switch (_decks[i].format) {
            case FMT_TXT: fmtStr = "TXT"; break;
            case FMT_CSV: fmtStr = "CSV"; break;
            case FMT_TSV: fmtStr = "TSV"; break;
            case FMT_JSON: fmtStr = "JSON"; break;
            default: break;
        }
        
        // Deck name (without extension)
        char shortName[24];
        strncpy(shortName, _decks[i].name, 23);
        shortName[23] = '\0';
        char* dot = strrchr(shortName, '.');
        if (dot) *dot = '\0';
        
        // Truncate if needed
        if (strlen(shortName) > 18) {
            shortName[18] = '\0';
            strcat(shortName, "..");
        }
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(CARD_MARGIN + 8, y + 18);
        display.print(shortName);
        
        // Format indicator
        display.setFont(&FreeSans9pt7b);
        display.setCursor(CARD_MARGIN + 8, y + 36);
        display.print(fmtStr);
        
        display.setTextColor(GxEPD_BLACK);
        y += _itemH;
    }
    
    // Footer
    display.setFont(&FreeSans9pt7b);
    display.drawLine(0, _screenH - 40, _screenW, _screenH - 40, GxEPD_BLACK);
    
    char status[24];
    snprintf(status, sizeof(status), "%d/%d", _cursor + 1, _deckCount);
    display.setCursor(CARD_MARGIN, _screenH - 15);
    display.print(status);
    
    display.setCursor(_screenW - 100, _screenH - 15);
    display.print("OK: Study");
}

// =============================================================================
// Drawing - Card View
// =============================================================================

void FlashcardsApp::drawCard(bool showAnswer) {
    if (!_cards || _cardIdx >= _cardCount) return;
    
    Card& card = _cards[_cardIdx];
    
    // Progress bar at top
    drawProgressBar(CARD_MARGIN, 12, _screenW - 2*CARD_MARGIN, PROGRESS_H, 
                    _cardIdx + 1, _cardCount);
    
    // Card number
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    char progress[32];
    snprintf(progress, sizeof(progress), "Card %d of %d", _cardIdx + 1, _cardCount);
    display.setCursor(CARD_MARGIN, 42);
    display.print(progress);
    
    // Divider
    display.drawLine(0, 50, _screenW, 50, GxEPD_BLACK);
    
    // Question section
    int qY = CARD_TOP + 10;
    display.setFont(&FreeSans9pt7b);
    display.setCursor(CARD_MARGIN, qY);
    display.print("Question:");
    
    display.setFont(&FreeSansBold12pt7b);
    drawWrappedText(card.front, CARD_MARGIN, qY + 30, _screenW - 2*CARD_MARGIN, 4);
    
    if (showAnswer) {
        // Divider line
        int divY = _screenH / 2 - 10;
        display.drawLine(CARD_MARGIN, divY, _screenW - CARD_MARGIN, divY, GxEPD_BLACK);
        
        // Answer section
        int aY = divY + 20;
        display.setFont(&FreeSans9pt7b);
        display.setCursor(CARD_MARGIN, aY);
        display.print("Answer:");
        
        display.setFont(&FreeSansBold12pt7b);
        drawWrappedText(card.back, CARD_MARGIN, aY + 30, _screenW - 2*CARD_MARGIN, 4);
        
        // Bottom controls
        display.setFont(&FreeSans9pt7b);
        display.drawLine(0, _screenH - 45, _screenW, _screenH - 45, GxEPD_BLACK);
        
        // Stats
        char stats[32];
        snprintf(stats, sizeof(stats), "%d/%d", _correct, _incorrect);
        display.setCursor(CARD_MARGIN, _screenH - 25);
        display.print(stats);
        
        // Control hints
        display.setCursor(_screenW / 2 - 80, _screenH - 25);
        display.print("<Wrong  Right>");
        
        display.setCursor(_screenW - 50, _screenH - 25);
        display.print("v Skip");
    } else {
        // Footer for question mode
        display.setFont(&FreeSans9pt7b);
        display.drawLine(0, _screenH - 45, _screenW, _screenH - 45, GxEPD_BLACK);
        
        display.setCursor(CARD_MARGIN, _screenH - 25);
        display.print("<Shuffle");
        
        display.setCursor(_screenW / 2 - 40, _screenH - 25);
        display.print("OK: Reveal");
    }
}

void FlashcardsApp::drawWrappedText(const char* text, int x, int y, int maxWidth, int maxLines) {
    int curX = x;
    int curY = y;
    int lineHeight = 28;
    int lineCount = 0;
    
    char word[50];
    int wordLen = 0;
    
    const char* p = text;
    while (*p && lineCount < maxLines) {
        if (*p == ' ' || *p == '\n' || *(p + 1) == '\0') {
            if (*(p + 1) == '\0' && *p != ' ' && *p != '\n') {
                if (wordLen < 49) word[wordLen++] = *p;
            }
            word[wordLen] = '\0';
            
            if (wordLen > 0) {
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(word, 0, 0, &x1, &y1, &w, &h);
                
                if (curX + (int)w > x + maxWidth) {
                    curX = x;
                    curY += lineHeight;
                    lineCount++;
                    if (lineCount >= maxLines) break;
                }
                
                display.setCursor(curX, curY);
                display.print(word);
                curX += w + 8;
            }
            
            if (*p == '\n') {
                curX = x;
                curY += lineHeight;
                lineCount++;
            }
            
            wordLen = 0;
        } else {
            if (wordLen < 49) word[wordLen++] = *p;
        }
        p++;
    }
}

void FlashcardsApp::drawProgressBar(int x, int y, int w, int h, int current, int total) {
    // Background
    display.drawRoundRect(x, y, w, h, h/2, GxEPD_BLACK);
    
    // Fill
    if (total > 0 && current > 0) {
        int fillW = (w - 4) * current / total;
        if (fillW > 0) {
            display.fillRoundRect(x + 2, y + 2, fillW, h - 4, (h-4)/2, GxEPD_BLACK);
        }
    }
}

// =============================================================================
// Drawing - Done Screen
// =============================================================================

void FlashcardsApp::drawDone() {
    // Header
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(_screenW / 2 - 80, 50);
    display.print("Session Complete!");
    
    // Stats box
    int boxX = CARD_MARGIN;
    int boxY = 100;
    int boxW = _screenW - 2*CARD_MARGIN;
    int boxH = 200;
    
    display.drawRoundRect(boxX, boxY, boxW, boxH, 10, GxEPD_BLACK);
    
    // Total cards
    display.setFont(&FreeSansBold12pt7b);
    char total[32];
    snprintf(total, sizeof(total), "%d", _cardCount);
    display.setCursor(_screenW / 2 - 20, boxY + 50);
    display.print(total);
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(_screenW / 2 - 50, boxY + 75);
    display.print("cards reviewed");
    
    // Correct/Incorrect
    int statY = boxY + 120;
    
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(boxX + 60, statY);
    display.printf("%d", _correct);
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(boxX + 40, statY + 25);
    display.print("Correct");
    
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(_screenW - boxX - 80, statY);
    display.printf("%d", _incorrect);
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(_screenW - boxX - 100, statY + 25);
    display.print("Incorrect");
    
    // Percentage if applicable
    if (_correct + _incorrect > 0) {
        int pct = (_correct * 100) / (_correct + _incorrect);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(_screenW / 2 - 30, statY + 60);
        display.printf("%d%%", pct);
    }
    
    // Footer
    display.setFont(&FreeSans9pt7b);
    display.drawLine(0, _screenH - 45, _screenW, _screenH - 45, GxEPD_BLACK);
    display.setCursor(_screenW / 2 - 50, _screenH - 20);
    display.print("OK: Continue");
}

#endif // FEATURE_FLASHCARDS
