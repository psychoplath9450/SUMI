/**
 * @file Flashcards.cpp
 * @brief Flashcard app with partial refresh and multi-format support
 * @version 1.3.0
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
#include "core/ZipReader.h"  // For freeing ZIP buffers
#include "core/SettingsManager.h"

#if FEATURE_FLASHCARDS

extern SettingsManager settingsManager;


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
    _settingsCursor = 0;
    _needsFullRedraw = true;
    closeDeck();
    
    // Settings are loaded from SettingsManager
    
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
                        if (settingsManager.flashcards.shuffle) {
                            shuffleCards();
                        }
                        _mode = MODE_QUESTION;
                        _needsFullRedraw = true;
                    }
                }
                return true;
            case BTN_LEFT:
                // Open settings
                _settingsCursor = 0;
                _mode = MODE_SETTINGS;
                _needsFullRedraw = true;
                return true;
            case BTN_BACK:
                return false;  // Exit app
            default:
                return true;
        }
    }
    else if (_mode == MODE_SETTINGS) {
        switch (btn) {
            case BTN_UP:
                if (_settingsCursor > 0) {
                    _settingsCursor--;
                    _needsFullRedraw = true;
                }
                return true;
            case BTN_DOWN:
                if (_settingsCursor < 4) {  // 5 settings items (0-4)
                    _settingsCursor++;
                    _needsFullRedraw = true;
                }
                return true;
            case BTN_CONFIRM:
            case BTN_LEFT:
            case BTN_RIGHT:
                // Toggle/cycle setting
                switch (_settingsCursor) {
                    case 0:  // Font size (0-3)
                        settingsManager.flashcards.fontSize = (settingsManager.flashcards.fontSize + 1) % 4;
                        break;
                    case 1:  // Center text
                        settingsManager.flashcards.centerText = !settingsManager.flashcards.centerText;
                        break;
                    case 2:  // Shuffle on load
                        settingsManager.flashcards.shuffle = !settingsManager.flashcards.shuffle;
                        break;
                    case 3:  // Show progress bar
                        settingsManager.flashcards.showProgressBar = !settingsManager.flashcards.showProgressBar;
                        break;
                    case 4:  // Show stats
                        settingsManager.flashcards.showStats = !settingsManager.flashcards.showStats;
                        break;
                }
                settingsManager.save();
                _needsFullRedraw = true;
                return true;
            case BTN_BACK:
                _mode = MODE_DECKS;
                _needsFullRedraw = true;
                return true;
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
    else if (_mode == MODE_SETTINGS) {
        drawSettings();
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
    
    // Free ZIP buffers to reclaim ~43KB for flashcards
    ZipReader_freeBuffers();
    
    // Log heap before allocation
    Serial.printf("[FLASH] Free heap: %d\n", ESP.getFreeHeap());
    
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
        Serial.printf("[FLASH] Loaded %d cards, free heap: %d\n", _cardCount, ESP.getFreeHeap());
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
    if (fileSize > 16000) {  // Reduced from 32KB to 16KB
        Serial.println("[FLASH] JSON too large (max 16KB)");
        f.close();
        return false;
    }
    
    char* json = (char*)malloc(fileSize + 1);
    if (!json) {
        Serial.println("[FLASH] Failed to allocate JSON buffer");
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
    
    display.setCursor(_screenW / 2 - 60, _screenH - 15);
    display.print("<Settings  OK:Study>");
}

// =============================================================================
// Image Support for Visual Flashcards (ASL, etc.)
// =============================================================================

bool FlashcardsApp::isImagePath(const char* text) {
    if (!text || strlen(text) < 5) return false;
    
    // Check if it ends with .bmp (case insensitive)
    int len = strlen(text);
    if (len < 4) return false;
    
    const char* ext = text + len - 4;
    return (strcasecmp(ext, ".bmp") == 0);
}

bool FlashcardsApp::drawFlashcardImage(const char* path, int x, int y, int maxW, int maxH) {
    // Open the BMP file
    File file = SD.open(path, FILE_READ);
    if (!file) {
        // Draw error placeholder
        display.setFont(&FreeSans9pt7b);
        display.setCursor(x + maxW/2 - 40, y + maxH/2);
        display.print("[Image not found]");
        return false;
    }
    
    // Read BMP header
    uint8_t header[54];
    if (file.read(header, 54) != 54) {
        file.close();
        display.setCursor(x + maxW/2 - 40, y + maxH/2);
        display.print("[Invalid BMP]");
        return false;
    }
    
    // Verify BMP signature
    if (header[0] != 'B' || header[1] != 'M') {
        file.close();
        return false;
    }
    
    // Extract dimensions (little-endian)
    int32_t imgW = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    int32_t imgH = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    uint16_t bitsPerPixel = header[28] | (header[29] << 8);
    uint32_t dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    
    // Handle negative height (top-down DIB)
    bool topDown = false;
    if (imgH < 0) {
        imgH = -imgH;
        topDown = true;
    }
    
    // Only support 1-bit (monochrome) for now - perfect for e-ink
    if (bitsPerPixel != 1) {
        file.close();
        display.setFont(&FreeSans9pt7b);
        display.setCursor(x + maxW/2 - 60, y + maxH/2);
        display.print("[Use 1-bit BMP]");
        return false;
    }
    
    // Calculate position to center image in area
    int drawX = x + (maxW - imgW) / 2;
    int drawY = y + (maxH - imgH) / 2;
    if (drawX < x) drawX = x;
    if (drawY < y) drawY = y;
    
    // Seek to pixel data
    file.seek(dataOffset);
    
    // BMP rows are padded to 4-byte boundaries
    int rowSize = ((imgW + 31) / 32) * 4;
    uint8_t rowBuf[100];  // Max ~800px wide
    if (rowSize > 100) rowSize = 100;
    
    // Draw the image
    for (int row = 0; row < imgH && row < maxH; row++) {
        int srcRow = topDown ? row : (imgH - 1 - row);
        file.seek(dataOffset + srcRow * rowSize);
        file.read(rowBuf, rowSize);
        
        for (int col = 0; col < imgW && col < maxW; col++) {
            int byteIdx = col / 8;
            int bitIdx = 7 - (col % 8);
            bool pixel = (rowBuf[byteIdx] >> bitIdx) & 1;
            
            // In 1-bit BMP: 1 = white, 0 = black (usually)
            if (!pixel) {
                display.drawPixel(drawX + col, drawY + row, GxEPD_BLACK);
            }
        }
    }
    
    file.close();
    return true;
}

// =============================================================================
// Drawing - Card View
// =============================================================================

void FlashcardsApp::drawCard(bool showAnswer) {
    if (!_cards || _cardIdx >= _cardCount) return;
    
    Card& card = _cards[_cardIdx];
    
    // Get settings from SettingsManager
    bool showProgressBar = settingsManager.flashcards.showProgressBar;
    bool showStats = settingsManager.flashcards.showStats;
    bool centerText = settingsManager.flashcards.centerText;
    
    // Progress bar at top (if enabled)
    if (showProgressBar) {
        drawProgressBar(CARD_MARGIN, 12, _screenW - 2*CARD_MARGIN, PROGRESS_H, 
                        _cardIdx + 1, _cardCount);
    }
    
    // Card number
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    char progress[32];
    snprintf(progress, sizeof(progress), "Card %d of %d", _cardIdx + 1, _cardCount);
    display.setCursor(CARD_MARGIN, showProgressBar ? 42 : 25);
    display.print(progress);
    
    // Divider
    int dividerY = showProgressBar ? 50 : 33;
    display.drawLine(0, dividerY, _screenW, dividerY, GxEPD_BLACK);
    
    // Calculate areas
    int footerY = _screenH - 45;
    int midDividerY = showAnswer ? (_screenH / 2 - 10) : footerY;
    
    // Question area: from dividerY to midDividerY (or footerY if no answer)
    int qAreaTop = dividerY + 5;
    int qAreaBottom = showAnswer ? midDividerY - 5 : footerY - 5;
    int qAreaHeight = qAreaBottom - qAreaTop;
    
    // Draw question label
    display.setFont(&FreeSans9pt7b);
    int labelHeight = 18;
    
    if (centerText) {
        int16_t tx, ty;
        uint16_t tw, th;
        display.getTextBounds("Question:", 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((_screenW - tw) / 2, qAreaTop + labelHeight);
    } else {
        display.setCursor(CARD_MARGIN, qAreaTop + labelHeight);
    }
    display.print("Question:");
    
    // Draw question text or image - centered in remaining area
    int qTextTop = qAreaTop + labelHeight + 10;
    int qTextHeight = qAreaBottom - qTextTop;
    
    // Check if question is an image path
    if (isImagePath(card.front)) {
        drawFlashcardImage(card.front, CARD_MARGIN, qTextTop, 
                          _screenW - 2*CARD_MARGIN, qTextHeight);
    } else {
        display.setFont(getCardFont());
        int textSize = (settingsManager.flashcards.fontSize == 3) ? 2 : 1;
        display.setTextSize(textSize);
        
        if (centerText) {
            drawCenteredText(card.front, CARD_MARGIN, qTextTop, 
                            _screenW - 2*CARD_MARGIN, qTextHeight);
        } else {
            drawWrappedText(card.front, CARD_MARGIN, qTextTop + 20, 
                           _screenW - 2*CARD_MARGIN, 4);
        }
        display.setTextSize(1);
    };
    
    if (showAnswer) {
        // Divider line
        display.drawLine(CARD_MARGIN, midDividerY, _screenW - CARD_MARGIN, midDividerY, GxEPD_BLACK);
        
        // Answer area: from midDividerY to footerY
        int aAreaTop = midDividerY + 5;
        int aAreaBottom = footerY - 5;
        int aAreaHeight = aAreaBottom - aAreaTop;
        
        // Draw answer label
        display.setFont(&FreeSans9pt7b);
        if (centerText) {
            int16_t tx, ty;
            uint16_t tw, th;
            display.getTextBounds("Answer:", 0, 0, &tx, &ty, &tw, &th);
            display.setCursor((_screenW - tw) / 2, aAreaTop + labelHeight);
        } else {
            display.setCursor(CARD_MARGIN, aAreaTop + labelHeight);
        }
        display.print("Answer:");
        
        // Draw answer text or image - centered in remaining area
        int aTextTop = aAreaTop + labelHeight + 10;
        int aTextHeight = aAreaBottom - aTextTop;
        
        // Check if answer is an image path
        if (isImagePath(card.back)) {
            drawFlashcardImage(card.back, CARD_MARGIN, aTextTop,
                              _screenW - 2*CARD_MARGIN, aTextHeight);
        } else {
            display.setFont(getCardFont());
            int textSize = (settingsManager.flashcards.fontSize == 3) ? 2 : 1;
            display.setTextSize(textSize);
            
            if (centerText) {
                drawCenteredText(card.back, CARD_MARGIN, aTextTop,
                                _screenW - 2*CARD_MARGIN, aTextHeight);
            } else {
                drawWrappedText(card.back, CARD_MARGIN, aTextTop + 20,
                               _screenW - 2*CARD_MARGIN, 4);
            }
            display.setTextSize(1);
        }
        
        // Bottom controls
        display.setFont(&FreeSans9pt7b);
        display.drawLine(0, footerY, _screenW, footerY, GxEPD_BLACK);
        
        // Stats (if enabled)
        if (showStats) {
            char stats[32];
            snprintf(stats, sizeof(stats), "%d/%d", _correct, _incorrect);
            display.setCursor(CARD_MARGIN, _screenH - 25);
            display.print(stats);
        }
        
        // Control hints
        display.setCursor(_screenW / 2 - 80, _screenH - 25);
        display.print("<Wrong  Right>");
        
        display.setCursor(_screenW - 50, _screenH - 25);
        display.print("v Skip");
    } else {
        // Footer for question mode
        display.setFont(&FreeSans9pt7b);
        display.drawLine(0, footerY, _screenW, footerY, GxEPD_BLACK);
        
        display.setCursor(CARD_MARGIN, _screenH - 25);
        display.print("<Shuffle");
        
        display.setCursor(_screenW / 2 - 40, _screenH - 25);
        display.print("OK: Reveal");
    }
}

// Draw text centered both horizontally and vertically in the given area
void FlashcardsApp::drawCenteredText(const char* text, int x, int y, int width, int height) {
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
    
    // If text fits on one line, center it
    if ((int)tw <= width) {
        int cx = x + (width - tw) / 2;
        int cy = y + (height + th) / 2;
        display.setCursor(cx, cy);
        display.print(text);
    } else {
        // Multi-line - use wrapped text but start it centered vertically
        int lineHeight = th + 8;
        int estLines = (tw / width) + 1;
        int totalTextHeight = estLines * lineHeight;
        int startY = y + (height - totalTextHeight) / 2 + th;
        if (startY < y + th) startY = y + th;
        
        // Draw wrapped but centered horizontally for each line
        drawWrappedTextCentered(text, x, startY, width, 4);
    }
}

// Draw wrapped text with each line centered
void FlashcardsApp::drawWrappedTextCentered(const char* text, int x, int y, int maxWidth, int maxLines) {
    int curY = y;
    int lineHeight = 28;
    int lineCount = 0;
    int centerX = x + maxWidth / 2;
    
    // Build lines first, then draw centered
    char line[100];
    int lineLen = 0;
    char word[50];
    int wordLen = 0;
    int lineWidth = 0;
    
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
                
                // Check if word fits on current line
                int spaceW = (lineLen > 0) ? 8 : 0;
                if (lineWidth + spaceW + (int)w > maxWidth) {
                    // Draw current line centered
                    if (lineLen > 0) {
                        line[lineLen] = '\0';
                        int16_t lx, ly;
                        uint16_t lw, lh;
                        display.getTextBounds(line, 0, 0, &lx, &ly, &lw, &lh);
                        display.setCursor(centerX - lw/2, curY);
                        display.print(line);
                        curY += lineHeight;
                        lineCount++;
                        if (lineCount >= maxLines) break;
                    }
                    // Start new line with this word
                    strcpy(line, word);
                    lineLen = wordLen;
                    lineWidth = w;
                } else {
                    // Add word to line
                    if (lineLen > 0) {
                        line[lineLen++] = ' ';
                    }
                    strcpy(line + lineLen, word);
                    lineLen += wordLen;
                    lineWidth += spaceW + w;
                }
            }
            
            if (*p == '\n') {
                // Draw current line and start new
                if (lineLen > 0) {
                    line[lineLen] = '\0';
                    int16_t lx, ly;
                    uint16_t lw, lh;
                    display.getTextBounds(line, 0, 0, &lx, &ly, &lw, &lh);
                    display.setCursor(centerX - lw/2, curY);
                    display.print(line);
                    curY += lineHeight;
                    lineCount++;
                }
                lineLen = 0;
                lineWidth = 0;
            }
            
            wordLen = 0;
        } else {
            if (wordLen < 49) word[wordLen++] = *p;
        }
        p++;
    }
    
    // Draw remaining line
    if (lineLen > 0 && lineCount < maxLines) {
        line[lineLen] = '\0';
        int16_t lx, ly;
        uint16_t lw, lh;
        display.getTextBounds(line, 0, 0, &lx, &ly, &lw, &lh);
        display.setCursor(centerX - lw/2, curY);
        display.print(line);
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

// =============================================================================
// Settings Load/Save
// =============================================================================

#define FLASHCARD_SETTINGS_PATH "/.sumi/flashcard_settings.bin"

void FlashcardsApp::FlashcardSettings::load() {
    File f = SD.open(FLASHCARD_SETTINGS_PATH, FILE_READ);
    if (f) {
        f.read((uint8_t*)this, sizeof(FlashcardSettings));
        f.close();
        if (magic != 0x464C5348) {
            *this = FlashcardSettings();  // Reset to defaults
        }
    }
}

void FlashcardsApp::FlashcardSettings::save() {
    SD.mkdir("/.sumi");
    File f = SD.open(FLASHCARD_SETTINGS_PATH, FILE_WRITE);
    if (f) {
        f.write((uint8_t*)this, sizeof(FlashcardSettings));
        f.close();
    }
}

// =============================================================================
// Font Helper
// =============================================================================

const GFXfont* FlashcardsApp::getCardFont() {
    // Font sizes: 0=Small (9pt), 1=Medium (12pt), 2=Large (12pt bold), 3=XLarge (12pt x2)
    // Note: For XLarge, we use 12pt with setTextSize(2) in the caller
    switch (settingsManager.flashcards.fontSize) {
        case 0:  return &FreeSans9pt7b;         // Small
        case 1:  return &FreeSansBold12pt7b;    // Medium
        case 2:  return &FreeSansBold12pt7b;    // Large
        case 3:  return &FreeSansBold12pt7b;    // XLarge (doubled by setTextSize)
        default: return &FreeSansBold12pt7b;
    }
}

// =============================================================================
// Drawing - Settings Screen
// =============================================================================

void FlashcardsApp::drawSettings() {
    // Header
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    const char* title = "Flashcard Settings";
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((_screenW - tw) / 2, 35);
    display.print(title);
    
    display.drawLine(0, 50, _screenW, 50, GxEPD_BLACK);
    
    // Settings items
    int startY = 70;
    int itemH = 44;
    int margin = 25;
    int valueBoxW = 110;
    
    struct SettingItem {
        const char* label;
        const char* value;
    };
    
    // Get current values from settingsManager
    const char* fontSizeNames[] = {"Small", "Medium", "Large", "Extra Large"};
    uint8_t fontSize = settingsManager.flashcards.fontSize;
    if (fontSize > 3) fontSize = 1;
    
    SettingItem items[] = {
        {"Font Size", fontSizeNames[fontSize]},
        {"Center Text", settingsManager.flashcards.centerText ? "Yes" : "No"},
        {"Shuffle on Load", settingsManager.flashcards.shuffle ? "Yes" : "No"},
        {"Show Progress Bar", settingsManager.flashcards.showProgressBar ? "Yes" : "No"},
        {"Show Stats", settingsManager.flashcards.showStats ? "Yes" : "No"}
    };
    int numItems = 5;
    
    display.setFont(&FreeSans9pt7b);
    
    for (int i = 0; i < numItems; i++) {
        int y = startY + i * itemH;
        bool selected = (_settingsCursor == i);
        
        if (selected) {
            display.fillRect(margin - 5, y - 5, _screenW - 2*margin + 10, itemH - 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.setTextColor(GxEPD_BLACK);
        }
        
        // Label
        display.setCursor(margin, y + 20);
        display.print(items[i].label);
        
        // Value box
        int valueX = _screenW - margin - valueBoxW;
        if (!selected) {
            display.drawRoundRect(valueX, y, valueBoxW, itemH - 10, 4, GxEPD_BLACK);
        }
        
        // Center value text
        display.getTextBounds(items[i].value, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(valueX + (valueBoxW - tw) / 2, y + 20);
        display.print(items[i].value);
        
        display.setTextColor(GxEPD_BLACK);
    }
    
    // Footer
    display.setFont(&FreeSans9pt7b);
    display.drawLine(0, _screenH - 45, _screenW, _screenH - 45, GxEPD_BLACK);
    display.setCursor(margin, _screenH - 20);
    display.print("OK: Change | BACK: Done");
}

#endif // FEATURE_FLASHCARDS
