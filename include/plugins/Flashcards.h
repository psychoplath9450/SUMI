/**
 * @file Flashcards.h
 * @brief Enhanced Flashcard app for Sumi e-reader
 * @version 2.0.0
 *
 * Features:
 * - Main menu with quick stats
 * - Deck browser with progress tracking
 * - Enhanced study session UI
 * - Session complete with accuracy stats
 * - Statistics view with weekly chart
 * - Streak tracking
 * - Multi-format support (TXT, CSV, TSV, JSON)
 */

#ifndef SUMI_PLUGIN_FLASHCARDS_H
#define SUMI_PLUGIN_FLASHCARDS_H

#include "config.h"
#if FEATURE_FLASHCARDS

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "core/PluginHelpers.h"
#include "core/SettingsManager.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern SettingsManager settingsManager;

// =============================================================================
// Constants
// =============================================================================
#define FLASHCARDS_STATS_PATH "/.sumi/flashcards_stats.bin"
#define FLASHCARDS_DECKMETA_PATH "/.sumi/flashcards_decks.bin"

// =============================================================================
// Enums
// =============================================================================
enum FlashcardScreen : uint8_t {
    FC_SCREEN_MAIN_MENU,
    FC_SCREEN_DECK_BROWSER,
    FC_SCREEN_STUDY_QUESTION,
    FC_SCREEN_STUDY_ANSWER,
    FC_SCREEN_SESSION_COMPLETE,
    FC_SCREEN_SETTINGS,
    FC_SCREEN_STATISTICS
};

enum DeckFormat : uint8_t { 
    FMT_UNKNOWN, 
    FMT_TXT, 
    FMT_CSV, 
    FMT_TSV, 
    FMT_JSON 
};

// =============================================================================
// Data Structures
// =============================================================================
struct FlashcardStats {
    uint32_t magic;
    uint32_t totalCardsStudied;
    uint32_t totalCorrect;
    uint32_t totalIncorrect;
    uint16_t currentStreak;
    uint16_t bestStreak;
    uint32_t lastStudyDate;      // YYYYMMDD format
    uint16_t cardsToday;
    uint16_t cardsThisWeek;
    uint16_t dailyCounts[7];     // Last 7 days
    uint8_t reserved[16];
    
    FlashcardStats() : magic(0x464C5354), totalCardsStudied(0), totalCorrect(0),
                       totalIncorrect(0), currentStreak(0), bestStreak(0),
                       lastStudyDate(0), cardsToday(0), cardsThisWeek(0) {
        memset(dailyCounts, 0, sizeof(dailyCounts));
        memset(reserved, 0, sizeof(reserved));
    }
    
    bool isValid() const { return magic == 0x464C5354; }
};

struct DeckMetadata {
    char filename[32];
    uint16_t cardCount;
    uint16_t cardsStudied;
    uint16_t correctCount;
    uint32_t lastUsedDate;       // YYYYMMDD format
    uint8_t sessions;
    uint8_t reserved[7];
};

struct DeckMetadataFile {
    uint32_t magic;
    uint8_t deckCount;
    DeckMetadata decks[20];
    
    DeckMetadataFile() : magic(0x444B4D54), deckCount(0) {
        memset(decks, 0, sizeof(decks));
    }
    
    bool isValid() const { return magic == 0x444B4D54; }
};

// =============================================================================
// Flashcards App Class
// =============================================================================
class FlashcardsApp {
public:
    static const int MAX_DECKS = 20;
    static const int MAX_CARDS = 50;
    static const int MAX_TEXT = 80;
    
    struct Card {
        char front[MAX_TEXT];
        char back[MAX_TEXT];
        bool seen;
    };
    
    struct DeckInfo {
        char name[32];
        char displayName[28];
        DeckFormat format;
        int cardCount;
        int progress;            // 0-100%
        uint32_t lastUsed;       // YYYYMMDD
        bool hasImages;
    };
    
    // Screen dimensions
    int screenW, screenH;
    
    // State
    FlashcardScreen currentScreen;
    int menuCursor;
    int deckCursor;
    int settingsCursor;
    int scroll;
    bool needsFullRedraw;
    
    // Deck data
    DeckInfo decks[MAX_DECKS];
    int deckCount;
    int currentDeckIndex;
    
    // Card data
    Card* cards;
    int cardCount;
    int cardIndex;
    int sessionCorrect;
    int sessionIncorrect;
    
    // Statistics
    FlashcardStats stats;
    DeckMetadataFile deckMeta;
    
    // ==========================================================================
    // Constructor / Destructor
    // ==========================================================================
    FlashcardsApp() : cards(nullptr), cardCount(0), cardIndex(0),
                      sessionCorrect(0), sessionIncorrect(0), deckCount(0),
                      currentDeckIndex(-1), menuCursor(0), deckCursor(0),
                      settingsCursor(0), scroll(0), needsFullRedraw(true) {
        currentScreen = FC_SCREEN_MAIN_MENU;
    }
    
    ~FlashcardsApp() {
        closeDeck();
        saveStats();
        saveDeckMeta();
    }
    
    // ==========================================================================
    // Init
    // ==========================================================================
    void init(int w, int h) {
        screenW = w;
        screenH = h;
        
        loadStats();
        loadDeckMeta();
        scanDecks();
        updateTodayStats();
        
        currentScreen = FC_SCREEN_MAIN_MENU;
        menuCursor = 0;
        needsFullRedraw = true;
    }
    
    // ==========================================================================
    // Input Handling
    // ==========================================================================
    bool handleInput(Button btn) {
        // Don't force full redraw - let individual handlers decide
        
        switch (currentScreen) {
            case FC_SCREEN_MAIN_MENU: return handleMainMenuInput(btn);
            case FC_SCREEN_DECK_BROWSER: return handleDeckBrowserInput(btn);
            case FC_SCREEN_STUDY_QUESTION: return handleStudyQuestionInput(btn);
            case FC_SCREEN_STUDY_ANSWER: return handleStudyAnswerInput(btn);
            case FC_SCREEN_SESSION_COMPLETE: return handleSessionCompleteInput(btn);
            case FC_SCREEN_SETTINGS: return handleSettingsInput(btn);
            case FC_SCREEN_STATISTICS: return handleStatisticsInput(btn);
            default: return false;
        }
    }
    
    bool handleMainMenuInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (menuCursor > 0) menuCursor--;  // Partial refresh
                return true;
            case BTN_DOWN:
                if (menuCursor < 3) menuCursor++;  // Partial refresh
                return true;
            case BTN_CONFIRM:
                switch (menuCursor) {
                    case 0: // Study Decks
                        deckCursor = 0;
                        scroll = 0;
                        currentScreen = FC_SCREEN_DECK_BROWSER;
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 1: // Quick Review (continue last deck)
                        if (currentDeckIndex >= 0 && currentDeckIndex < deckCount) {
                            deckCursor = currentDeckIndex;
                            startStudySession();
                            needsFullRedraw = true;  // Screen change
                        } else if (deckCount > 0) {
                            deckCursor = 0;
                            currentScreen = FC_SCREEN_DECK_BROWSER;
                            needsFullRedraw = true;  // Screen change
                        }
                        break;
                    case 2: // Statistics
                        currentScreen = FC_SCREEN_STATISTICS;
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 3: // Settings
                        settingsCursor = 0;
                        currentScreen = FC_SCREEN_SETTINGS;
                        needsFullRedraw = true;  // Screen change
                        break;
                }
                return true;
            case BTN_BACK:
                return false;
            default:
                return true;
        }
    }
    
    bool handleDeckBrowserInput(Button btn) {
        int itemsPerPage = 6;
        
        switch (btn) {
            case BTN_UP:
                if (deckCursor > 0) {
                    deckCursor--;
                    if (deckCursor < scroll) scroll = deckCursor;
                }  // Partial refresh
                return true;
            case BTN_DOWN:
                if (deckCursor < deckCount - 1) {
                    deckCursor++;
                    if (deckCursor >= scroll + itemsPerPage) scroll++;
                }  // Partial refresh
                return true;
            case BTN_CONFIRM:
                if (deckCount > 0) {
                    startStudySession();
                    needsFullRedraw = true;  // Screen change
                }
                return true;
            case BTN_BACK:
                currentScreen = FC_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleStudyQuestionInput(Button btn) {
        switch (btn) {
            case BTN_CONFIRM:
            case BTN_DOWN:
                currentScreen = FC_SCREEN_STUDY_ANSWER;
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                // End session early
                finishSession();
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleStudyAnswerInput(Button btn) {
        switch (btn) {
            case BTN_RIGHT:
            case BTN_CONFIRM:
                // Correct
                sessionCorrect++;
                stats.totalCorrect++;
                nextCard();
                needsFullRedraw = true;  // Card change
                return true;
            case BTN_LEFT:
                // Incorrect
                sessionIncorrect++;
                stats.totalIncorrect++;
                nextCard();
                needsFullRedraw = true;  // Card change
                return true;
            case BTN_UP:
                // Back to question (review)
                currentScreen = FC_SCREEN_STUDY_QUESTION;
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                finishSession();
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleSessionCompleteInput(Button btn) {
        switch (btn) {
            case BTN_CONFIRM:
                // Study Again
                cardIndex = 0;
                sessionCorrect = 0;
                sessionIncorrect = 0;
                shuffleCards();
                currentScreen = FC_SCREEN_STUDY_QUESTION;
                needsFullRedraw = true;  // Screen change
                return true;
            case BTN_BACK:
                currentScreen = FC_SCREEN_DECK_BROWSER;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleSettingsInput(Button btn) {
        switch (btn) {
            case BTN_UP:
                if (settingsCursor > 0) settingsCursor--;  // Partial refresh
                return true;
            case BTN_DOWN:
                if (settingsCursor < 5) settingsCursor++;  // Partial refresh
                return true;
            case BTN_CONFIRM:
            case BTN_LEFT:
            case BTN_RIGHT:
                toggleSetting(settingsCursor);  // Partial refresh
                return true;
            case BTN_BACK:
                settingsManager.save();
                currentScreen = FC_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleStatisticsInput(Button btn) {
        if (btn == BTN_BACK) {
            currentScreen = FC_SCREEN_MAIN_MENU;
            needsFullRedraw = true;  // Screen change
        }
        return true;
    }
    
    void toggleSetting(int index) {
        switch (index) {
            case 0: // Font size (cycle 0-3)
                settingsManager.flashcards.fontSize = (settingsManager.flashcards.fontSize + 1) % 4;
                break;
            case 1: // Center text
                settingsManager.flashcards.centerText = !settingsManager.flashcards.centerText;
                break;
            case 2: // Shuffle
                settingsManager.flashcards.shuffle = !settingsManager.flashcards.shuffle;
                break;
            case 3: // Show progress bar
                settingsManager.flashcards.showProgressBar = !settingsManager.flashcards.showProgressBar;
                break;
            case 4: // Show stats
                settingsManager.flashcards.showStats = !settingsManager.flashcards.showStats;
                break;
            case 5: // Auto-advance (future feature)
                settingsManager.flashcards.autoFlip = !settingsManager.flashcards.autoFlip;
                break;
        }
        settingsManager.markDirty();
    }
    
    // ==========================================================================
    // Drawing
    // ==========================================================================
    void draw() {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            
            switch (currentScreen) {
                case FC_SCREEN_MAIN_MENU: drawMainMenu(); break;
                case FC_SCREEN_DECK_BROWSER: drawDeckBrowser(); break;
                case FC_SCREEN_STUDY_QUESTION: drawStudyQuestion(); break;
                case FC_SCREEN_STUDY_ANSWER: drawStudyAnswer(); break;
                case FC_SCREEN_SESSION_COMPLETE: drawSessionComplete(); break;
                case FC_SCREEN_SETTINGS: drawSettings(); break;
                case FC_SCREEN_STATISTICS: drawStatistics(); break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawPartial() {
        // Use partial window for smoother menu navigation
        display.setPartialWindow(0, 0, screenW, screenH);
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            
            switch (currentScreen) {
                case FC_SCREEN_MAIN_MENU: drawMainMenu(); break;
                case FC_SCREEN_DECK_BROWSER: drawDeckBrowser(); break;
                case FC_SCREEN_STUDY_QUESTION: drawStudyQuestion(); break;
                case FC_SCREEN_STUDY_ANSWER: drawStudyAnswer(); break;
                case FC_SCREEN_SESSION_COMPLETE: drawSessionComplete(); break;
                case FC_SCREEN_SETTINGS: drawSettings(); break;
                case FC_SCREEN_STATISTICS: drawStatistics(); break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawFullScreen() {
        needsFullRedraw = true;
        draw();
    }
    
    bool update() {
        // Flashcards doesn't need continuous updates
        return false;
    }
    
    // --------------------------------------------------------------------------
    // Main Menu
    // --------------------------------------------------------------------------
    void drawMainMenu() {
        // Header
        drawHeader("Flashcards", nullptr);
        
        int y = 60;
        
        // Quick stats card
        display.drawRoundRect(16, y, screenW - 32, 90, 8, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(28, y + 22);
        display.print("Your Progress");
        
        // Stats boxes
        int boxW = (screenW - 60) / 3;
        int boxY = y + 35;
        
        struct QuickStat { const char* label; char value[16]; };
        QuickStat quickStats[3];
        quickStats[0].label = "Today";
        snprintf(quickStats[0].value, 16, "%d", stats.cardsToday);
        quickStats[1].label = "This Week";
        snprintf(quickStats[1].value, 16, "%d", stats.cardsThisWeek);
        quickStats[2].label = "Streak";
        snprintf(quickStats[2].value, 16, "%d days", stats.currentStreak);
        
        for (int i = 0; i < 3; i++) {
            int bx = 24 + i * (boxW + 6);
            display.fillRoundRect(bx, boxY, boxW, 45, 6, GxEPD_WHITE);
            display.drawRoundRect(bx, boxY, boxW, 45, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(quickStats[i].value, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 22);
            display.print(quickStats[i].value);
            
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(quickStats[i].label, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 38);
            display.print(quickStats[i].label);
        }
        
        y += 105;
        
        // Menu items
        const char* labels[] = { "Study Decks", "Quick Review", "Statistics", "Settings" };
        char descs[4][32];
        snprintf(descs[0], 32, "%d decks available", deckCount);
        strcpy(descs[1], "Continue where you left off");
        strcpy(descs[2], "View your learning history");
        strcpy(descs[3], "Font size, shuffle, display");
        
        for (int i = 0; i < 4; i++) {
            int itemY = y + i * 62;
            bool sel = (menuCursor == i);
            
            if (sel) {
                display.fillRoundRect(16, itemY, screenW - 32, 56, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(16, itemY, screenW - 32, 56, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(32, itemY + 24);
            display.print(labels[i]);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(32, itemY + 44);
            display.print(descs[i]);
            
            // Arrow
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(screenW - 50, itemY + 34);
            display.print(">");
        }
        
        display.setTextColor(GxEPD_BLACK);
    }
    
    // --------------------------------------------------------------------------
    // Deck Browser
    // --------------------------------------------------------------------------
    void drawDeckBrowser() {
        // Header
        char subtitle[32];
        int totalCards = 0;
        for (int i = 0; i < deckCount; i++) totalCards += decks[i].cardCount;
        snprintf(subtitle, 32, "%d decks - %d cards", deckCount, totalCards);
        drawHeader("Study Decks", subtitle);
        
        int y = 56;
        int itemH = 72;
        int itemsPerPage = 6;
        
        if (deckCount == 0) {
            display.setFont(&FreeSansBold12pt7b);
            centerText("No decks found", screenW / 2, screenH / 2 - 20);
            display.setFont(&FreeSans9pt7b);
            centerText("Add .tsv or .csv files to /flashcards", screenW / 2, screenH / 2 + 10);
            return;
        }
        
        for (int i = scroll; i < deckCount && i < scroll + itemsPerPage; i++) {
            int itemY = y + (i - scroll) * itemH;
            bool sel = (deckCursor == i);
            DeckInfo& deck = decks[i];
            
            if (sel) {
                display.fillRoundRect(12, itemY, screenW - 24, itemH - 4, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(12, itemY, screenW - 24, itemH - 4, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Deck name
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(24, itemY + 22);
            display.print(deck.displayName);
            
            // Card count and last used
            display.setFont(&FreeSans9pt7b);
            char info[48];
            if (deck.lastUsed > 0) {
                snprintf(info, 48, "%d cards - Last: %s", deck.cardCount, formatDate(deck.lastUsed));
            } else {
                snprintf(info, 48, "%d cards - Never studied", deck.cardCount);
            }
            display.setCursor(24, itemY + 40);
            display.print(info);
            
            // Progress percentage
            display.setFont(&FreeSansBold9pt7b);
            char pctStr[8];
            snprintf(pctStr, 8, "%d%%", deck.progress);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(pctStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - 40 - tw, itemY + 22);
            display.print(pctStr);
            
            // Progress bar
            int barX = 24;
            int barY = itemY + 50;
            int barW = screenW - 48;
            int barH = 6;
            
            if (sel) {
                display.fillRoundRect(barX, barY, barW, barH, 3, GxEPD_WHITE);
                int fillW = (barW * deck.progress) / 100;
                if (fillW > 0) {
                    // Inverted: white bar, need to show progress somehow
                    // Draw unfilled portion as gray
                }
            } else {
                display.drawRoundRect(barX, barY, barW, barH, 3, GxEPD_BLACK);
                int fillW = (barW * deck.progress) / 100;
                if (fillW > 0) {
                    display.fillRoundRect(barX, barY, fillW, barH, 3, GxEPD_BLACK);
                }
            }
        }
        
        display.setTextColor(GxEPD_BLACK);
        
        // Footer
        drawFooter("Up/Down: Select - OK: Start - Back: Return");
    }
    
    // --------------------------------------------------------------------------
    // Study Question
    // --------------------------------------------------------------------------
    void drawStudyQuestion() {
        // Compact header with stats
        display.fillRect(0, 0, screenW, 40, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(16, 26);
        display.print(decks[currentDeckIndex].displayName);
        
        char countStr[16];
        snprintf(countStr, 16, "%d / %d", cardIndex + 1, cardCount);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(countStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 26);
        display.print(countStr);
        
        if (settingsManager.flashcards.showStats) {
            char statsStr[16];
            snprintf(statsStr, 16, "+%d -%d", sessionCorrect, sessionIncorrect);
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(statsStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - tw - 16, 26);
            display.print(statsStr);
        }
        
        display.setTextColor(GxEPD_BLACK);
        
        // Progress bar
        if (settingsManager.flashcards.showProgressBar) {
            int barY = 48;
            display.fillRect(0, 40, screenW, 20, GxEPD_WHITE);
            display.drawRect(16, barY, screenW - 32, 8, GxEPD_BLACK);
            int fillW = ((screenW - 34) * (cardIndex + 1)) / cardCount;
            display.fillRect(17, barY + 1, fillW, 6, GxEPD_BLACK);
        }
        
        int cardY = settingsManager.flashcards.showProgressBar ? 70 : 50;
        
        // Question label
        display.setFont(&FreeSans9pt7b);
        centerText("QUESTION", screenW / 2, cardY + 10);
        
        // Card
        int cardH = screenH - cardY - 100;
        display.drawRoundRect(20, cardY + 20, screenW - 40, cardH, 16, GxEPD_BLACK);
        display.drawRoundRect(22, cardY + 22, screenW - 44, cardH - 4, 14, GxEPD_BLACK);
        
        // Card content
        if (cards && cardIndex < cardCount) {
            if (isImagePath(cards[cardIndex].front)) {
                drawFlashcardImage(cards[cardIndex].front, 40, cardY + 40, screenW - 80, cardH - 60);
            } else {
                drawCardText(cards[cardIndex].front, 40, cardY + 40, screenW - 80, cardH - 60);
            }
        }
        
        // Hint
        display.setFont(&FreeSans9pt7b);
        centerText("Think of the answer, then reveal...", screenW / 2, screenH - 70);
        
        // Reveal button
        display.fillRoundRect((screenW - 180) / 2, screenH - 55, 180, 45, 8, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        centerText("Reveal Answer", screenW / 2, screenH - 26);
        display.setTextColor(GxEPD_BLACK);
    }
    
    // --------------------------------------------------------------------------
    // Study Answer
    // --------------------------------------------------------------------------
    void drawStudyAnswer() {
        // Compact header (same as question)
        display.fillRect(0, 0, screenW, 40, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(16, 26);
        display.print(decks[currentDeckIndex].displayName);
        
        char countStr[16];
        snprintf(countStr, 16, "%d / %d", cardIndex + 1, cardCount);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(countStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 26);
        display.print(countStr);
        
        if (settingsManager.flashcards.showStats) {
            char statsStr[16];
            snprintf(statsStr, 16, "+%d -%d", sessionCorrect, sessionIncorrect);
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(statsStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - tw - 16, 26);
            display.print(statsStr);
        }
        
        display.setTextColor(GxEPD_BLACK);
        
        // Progress bar
        if (settingsManager.flashcards.showProgressBar) {
            int barY = 48;
            display.fillRect(0, 40, screenW, 20, GxEPD_WHITE);
            display.drawRect(16, barY, screenW - 32, 8, GxEPD_BLACK);
            int fillW = ((screenW - 34) * (cardIndex + 1)) / cardCount;
            display.fillRect(17, barY + 1, fillW, 6, GxEPD_BLACK);
        }
        
        int y = settingsManager.flashcards.showProgressBar ? 70 : 50;
        
        // Question card (smaller)
        display.drawRoundRect(20, y, screenW - 40, 80, 12, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(32, y + 18);
        display.print("QUESTION");
        
        if (cards && cardIndex < cardCount) {
            display.setFont(getCardFont());
            if (settingsManager.flashcards.fontSize == 3) display.setTextSize(2);
            
            // Truncate question for small card
            char truncQ[40];
            strncpy(truncQ, cards[cardIndex].front, 39);
            truncQ[39] = '\0';
            
            int16_t qx, qy; uint16_t qw, qh;
            display.getTextBounds(truncQ, 0, 0, &qx, &qy, &qw, &qh);
            display.setCursor((screenW - qw) / 2, y + 55);
            display.print(truncQ);
            
            if (settingsManager.flashcards.fontSize == 3) display.setTextSize(1);
        }
        
        y += 95;
        
        // Answer card (larger)
        int answerH = screenH - y - 90;
        display.drawRoundRect(20, y, screenW - 40, answerH, 16, GxEPD_BLACK);
        display.drawRoundRect(22, y + 2, screenW - 44, answerH - 4, 14, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(32, y + 20);
        display.print("ANSWER");
        
        if (cards && cardIndex < cardCount) {
            if (isImagePath(cards[cardIndex].back)) {
                drawFlashcardImage(cards[cardIndex].back, 40, y + 35, screenW - 80, answerH - 55);
            } else {
                drawCardText(cards[cardIndex].back, 40, y + 35, screenW - 80, answerH - 55);
            }
        }
        
        // Correct/Incorrect buttons
        int btnY = screenH - 70;
        int btnW = (screenW - 48) / 2;
        
        // Incorrect button (left)
        display.drawRoundRect(16, btnY, btnW, 50, 8, GxEPD_BLACK);
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        centerText("Incorrect", 16 + btnW / 2, btnY + 32);
        
        // Correct button (right)
        display.fillRoundRect(24 + btnW, btnY, btnW, 50, 8, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        centerText("Correct", 24 + btnW + btnW / 2, btnY + 32);
        
        display.setTextColor(GxEPD_BLACK);
    }
    
    // --------------------------------------------------------------------------
    // Session Complete
    // --------------------------------------------------------------------------
    void drawSessionComplete() {
        drawHeader("Session Complete!", nullptr);
        
        int y = 60;
        
        // Celebration
        display.setFont(&FreeSansBold12pt7b);
        centerText("Great work!", screenW / 2, y + 20);
        display.setFont(&FreeSans9pt7b);
        centerText(decks[currentDeckIndex].displayName, screenW / 2, y + 45);
        
        y += 65;
        
        // Stats card
        display.drawRoundRect(16, y, screenW - 32, 160, 12, GxEPD_BLACK);
        
        // Stats boxes
        int boxW = (screenW - 60) / 3;
        int boxY = y + 16;
        
        struct SessionStat { const char* label; int value; bool highlight; };
        SessionStat sessionStats[3] = {
            { "Cards", cardCount, false },
            { "Correct", sessionCorrect, false },
            { "Incorrect", sessionIncorrect, false }
        };
        
        for (int i = 0; i < 3; i++) {
            int bx = 24 + i * (boxW + 6);
            display.fillRoundRect(bx, boxY, boxW, 55, 6, GxEPD_WHITE);
            display.drawRoundRect(bx, boxY, boxW, 55, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            char valStr[8];
            snprintf(valStr, 8, "%d", sessionStats[i].value);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(valStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 28);
            display.print(valStr);
            
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(sessionStats[i].label, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 46);
            display.print(sessionStats[i].label);
        }
        
        // Accuracy
        int accuracy = (sessionCorrect + sessionIncorrect > 0) 
            ? (sessionCorrect * 100) / (sessionCorrect + sessionIncorrect) : 0;
        
        int accY = y + 85;
        display.fillRoundRect(24, accY, screenW - 48, 60, 8, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(2);
        char accStr[8];
        snprintf(accStr, 8, "%d%%", accuracy);
        centerText(accStr, screenW / 2, accY + 35);
        display.setTextSize(1);
        display.setFont(&FreeSans9pt7b);
        centerText("Accuracy", screenW / 2, accY + 52);
        display.setTextColor(GxEPD_BLACK);
        
        y += 175;
        
        // Streak card
        display.drawRoundRect(16, y, screenW - 32, 60, 8, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(28, y + 18);
        display.print("Study Streak");
        
        display.setFont(&FreeSansBold12pt7b);
        char streakStr[16];
        snprintf(streakStr, 16, "%d days", stats.currentStreak);
        display.setCursor(28, y + 44);
        display.print(streakStr);
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(screenW / 2 + 20, y + 18);
        display.print("Cards Today");
        
        display.setFont(&FreeSansBold12pt7b);
        char todayStr[16];
        snprintf(todayStr, 16, "%d", stats.cardsToday);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(todayStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - 28 - tw, y + 44);
        display.print(todayStr);
        
        y += 75;
        
        // Action buttons
        int btnW = (screenW - 48) / 2;
        
        display.fillRoundRect(16, y, btnW, 50, 8, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        centerText("Study Again", 16 + btnW / 2, y + 32);
        
        display.setTextColor(GxEPD_BLACK);
        display.drawRoundRect(24 + btnW, y, btnW, 50, 8, GxEPD_BLACK);
        centerText("Back to Decks", 24 + btnW + btnW / 2, y + 32);
    }
    
    // --------------------------------------------------------------------------
    // Settings
    // --------------------------------------------------------------------------
    void drawSettings() {
        drawHeader("Flashcard Settings", nullptr);
        
        int y = 60;
        
        // Display section
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y);
        display.print("DISPLAY");
        y += 20;
        
        const char* labels[] = { "Font Size", "Center Text", "Shuffle Cards", 
                                  "Show Progress Bar", "Show Stats", "Auto-advance" };
        bool toggles[] = { false, settingsManager.flashcards.centerText,
                          settingsManager.flashcards.shuffle,
                          settingsManager.flashcards.showProgressBar,
                          settingsManager.flashcards.showStats,
                          settingsManager.flashcards.autoFlip };
        
        const char* fontNames[] = { "Small", "Medium", "Large", "XLarge" };
        
        for (int i = 0; i < 6; i++) {
            int itemY = y + i * 52;
            bool sel = (settingsCursor == i);
            
            if (i == 2) {
                // Study section header
                display.setFont(&FreeSans9pt7b);
                display.setCursor(20, itemY - 8);
                display.print("STUDY");
                itemY += 12;
            }
            
            if (sel) {
                display.drawRoundRect(14, itemY - 2, screenW - 28, 48, 6, GxEPD_BLACK);
                display.drawRoundRect(15, itemY - 1, screenW - 30, 46, 5, GxEPD_BLACK);
            }
            
            display.drawRoundRect(16, itemY, screenW - 32, 44, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(28, itemY + 28);
            display.print(labels[i]);
            
            if (i == 0) {
                // Font size cycle
                display.setFont(&FreeSansBold9pt7b);
                char sizeStr[24];
                snprintf(sizeStr, 24, "%s <>", fontNames[settingsManager.flashcards.fontSize % 4]);
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(sizeStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(screenW - 40 - tw, itemY + 28);
                display.print(sizeStr);
            } else {
                // Toggle switch
                drawToggle(screenW - 70, itemY + 10, toggles[i]);
            }
        }
        
        // Lifetime stats
        y = screenH - 110;
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y);
        display.print("LIFETIME STATS");
        y += 16;
        
        int boxW = (screenW - 50) / 3;
        struct LifeStat { const char* label; char value[16]; };
        LifeStat lifeStats[3];
        lifeStats[0].label = "Cards";
        snprintf(lifeStats[0].value, 16, "%lu", (unsigned long)stats.totalCardsStudied);
        lifeStats[1].label = "Accuracy";
        int acc = (stats.totalCorrect + stats.totalIncorrect > 0)
            ? (stats.totalCorrect * 100) / (stats.totalCorrect + stats.totalIncorrect) : 0;
        snprintf(lifeStats[1].value, 16, "%d%%", acc);
        lifeStats[2].label = "Best Streak";
        snprintf(lifeStats[2].value, 16, "%d days", stats.bestStreak);
        
        for (int i = 0; i < 3; i++) {
            int bx = 20 + i * (boxW + 5);
            display.drawRoundRect(bx, y, boxW, 50, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold9pt7b);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(lifeStats[i].value, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, y + 22);
            display.print(lifeStats[i].value);
            
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(lifeStats[i].label, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, y + 40);
            display.print(lifeStats[i].label);
        }
        
        drawFooter("Up/Down: Select - OK: Change - Back: Save");
    }
    
    // --------------------------------------------------------------------------
    // Statistics
    // --------------------------------------------------------------------------
    void drawStatistics() {
        drawHeader("Study Statistics", nullptr);
        
        int y = 60;
        
        // Weekly chart
        display.drawRoundRect(16, y, screenW - 32, 160, 12, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(28, y + 22);
        display.print("This Week");
        
        // Bar chart
        int chartX = 30;
        int chartY = y + 40;
        int chartH = 80;
        int barW = 40;
        int gap = (screenW - 60 - 7 * barW) / 6;
        
        // Find max for scaling
        int maxCards = 1;
        for (int i = 0; i < 7; i++) {
            if (stats.dailyCounts[i] > maxCards) maxCards = stats.dailyCounts[i];
        }
        
        const char* days[] = { "M", "T", "W", "T", "F", "S", "S" };
        
        for (int i = 0; i < 7; i++) {
            int bx = chartX + i * (barW + gap);
            int barH = (stats.dailyCounts[i] * chartH) / maxCards;
            if (barH < 2 && stats.dailyCounts[i] > 0) barH = 2;
            
            // Bar
            if (barH > 0) {
                display.fillRoundRect(bx, chartY + chartH - barH, barW, barH, 4, 
                                      (i == 6) ? GxEPD_BLACK : GxEPD_BLACK);
            }
            
            // Day label
            display.setFont(&FreeSans9pt7b);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(days[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (barW - tw) / 2, chartY + chartH + 16);
            display.print(days[i]);
            
            // Count
            char cnt[8];
            snprintf(cnt, 8, "%d", stats.dailyCounts[i]);
            display.getTextBounds(cnt, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (barW - tw) / 2, chartY + chartH + 30);
            display.print(cnt);
        }
        
        // Summary
        int sumY = y + 135;
        display.drawFastHLine(28, sumY, screenW - 56, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        char totalStr[32], avgStr[32];
        snprintf(totalStr, 32, "Total: %d cards", stats.cardsThisWeek);
        int avg = stats.cardsThisWeek / 7;
        snprintf(avgStr, 32, "Daily avg: %d", avg);
        
        display.setCursor(28, sumY + 18);
        display.print(totalStr);
        
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(avgStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(screenW - 28 - tw, sumY + 18);
        display.print(avgStr);
        
        y += 175;
        
        // Per-deck performance
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y);
        display.print("DECK PERFORMANCE");
        y += 16;
        
        int shown = 0;
        for (int i = 0; i < deckCount && shown < 4; i++) {
            DeckMetadata* dm = findDeckMeta(decks[i].name);
            if (!dm || dm->sessions == 0) continue;
            
            int itemY = y + shown * 50;
            display.drawRoundRect(16, itemY, screenW - 32, 44, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(28, itemY + 18);
            display.print(decks[i].displayName);
            
            display.setFont(&FreeSans9pt7b);
            char sessStr[24];
            snprintf(sessStr, 24, "%d sessions", dm->sessions);
            display.setCursor(28, itemY + 36);
            display.print(sessStr);
            
            // Accuracy
            int acc = (dm->cardsStudied > 0) ? (dm->correctCount * 100) / dm->cardsStudied : 0;
            display.setFont(&FreeSansBold9pt7b);
            char accStr[8];
            snprintf(accStr, 8, "%d%%", acc);
            display.getTextBounds(accStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - 40 - tw, itemY + 28);
            display.print(accStr);
            
            shown++;
        }
    }
    
    // ==========================================================================
    // Helper Drawing Functions
    // ==========================================================================
    void drawHeader(const char* title, const char* subtitle) {
        display.fillRect(0, 0, screenW, subtitle ? 48 : 40, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        centerText(title, screenW / 2, subtitle ? 26 : 28);
        if (subtitle) {
            display.setFont(&FreeSans9pt7b);
            centerText(subtitle, screenW / 2, 42);
        }
        display.setTextColor(GxEPD_BLACK);
    }
    
    void drawFooter(const char* text) {
        display.drawFastHLine(0, screenH - 36, screenW, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        centerText(text, screenW / 2, screenH - 12);
    }
    
    void drawToggle(int x, int y, bool enabled) {
        int sw = 44, sh = 24;
        if (enabled) {
            display.fillRoundRect(x, y, sw, sh, sh / 2, GxEPD_BLACK);
            display.fillCircle(x + sw - sh / 2, y + sh / 2, 8, GxEPD_WHITE);
        } else {
            display.drawRoundRect(x, y, sw, sh, sh / 2, GxEPD_BLACK);
            display.fillCircle(x + sh / 2, y + sh / 2, 8, GxEPD_BLACK);
        }
    }
    
    void centerText(const char* text, int x, int y) {
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(x - tw / 2, y);
        display.print(text);
    }
    
    void drawCardText(const char* text, int x, int y, int maxW, int maxH) {
        // Check if text has pronunciation in parentheses
        const char* parenStart = strchr(text, '(');
        
        if (parenStart && parenStart != text) {
            // Split into main text and pronunciation
            char mainText[200];
            char pronunciation[200];
            
            int mainLen = parenStart - text;
            if (mainLen > 199) mainLen = 199;
            strncpy(mainText, text, mainLen);
            mainText[mainLen] = '\0';
            
            // Trim trailing space from main text
            while (mainLen > 0 && mainText[mainLen - 1] == ' ') {
                mainText[--mainLen] = '\0';
            }
            
            // Copy pronunciation (without outer parentheses)
            const char* parenEnd = strrchr(text, ')');
            if (parenEnd && parenEnd > parenStart) {
                int pronLen = parenEnd - parenStart - 1;
                if (pronLen > 199) pronLen = 199;
                strncpy(pronunciation, parenStart + 1, pronLen);
                pronunciation[pronLen] = '\0';
            } else {
                strncpy(pronunciation, parenStart + 1, 199);
                pronunciation[199] = '\0';
            }
            
            // Use same font for both - answer font
            display.setFont(getCardFont());
            if (settingsManager.flashcards.fontSize == 3) display.setTextSize(2);
            
            // Calculate line height
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds("M", 0, 0, &tx, &ty, &tw, &th);
            int lineH = (th > 20) ? 45 : 28;
            
            // Measure main text height
            int mainLines = countWrappedLines(mainText, maxW);
            int mainTextH = mainLines * lineH;
            
            // Calculate total height needed (main + gap + pronunciation)
            int pronLines = countWrappedLines(pronunciation, maxW);
            int pronTextH = pronLines * lineH;
            int gap = 15;  // Small gap between answer and pronunciation
            int totalH = mainTextH + gap + pronTextH;
            
            // Center everything vertically
            int startY = y + (maxH - totalH) / 2;
            if (startY < y) startY = y;
            
            // Draw main text (answer)
            drawTextAtY(mainText, x, startY, maxW, mainTextH, lineH, true);
            
            // Draw pronunciation right below with same font
            int pronY = startY + mainTextH + gap;
            drawTextAtY(pronunciation, x, pronY, maxW, pronTextH, lineH, true);
            
            if (settingsManager.flashcards.fontSize == 3) display.setTextSize(1);
            
        } else {
            // No pronunciation - just draw the text with wrapping
            display.setFont(getCardFont());
            if (settingsManager.flashcards.fontSize == 3) display.setTextSize(2);
            
            drawSimpleWrappedText(text, x, y, maxW, maxH, true);
            
            if (settingsManager.flashcards.fontSize == 3) display.setTextSize(1);
        }
    }
    
    // Count how many lines wrapped text will take
    int countWrappedLines(const char* text, int maxW) {
        int lineCount = 0;
        int curLineW = 0;
        
        const char* p = text;
        while (*p) {
            // Find next word
            char word[50];
            int wordLen = 0;
            while (*p && *p != ' ' && *p != '\n' && wordLen < 49) {
                word[wordLen++] = *p++;
            }
            word[wordLen] = '\0';
            
            if (wordLen > 0) {
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(word, 0, 0, &tx, &ty, &tw, &th);
                int wordW = tw;
                int spaceW = (curLineW > 0) ? 8 : 0;
                
                if (curLineW > 0 && curLineW + spaceW + wordW > maxW) {
                    lineCount++;
                    curLineW = wordW;
                } else {
                    curLineW += spaceW + wordW;
                }
            }
            
            if (*p == '\n') {
                lineCount++;
                curLineW = 0;
                p++;
            } else if (*p == ' ') {
                p++;
            }
        }
        
        if (curLineW > 0) lineCount++;
        return max(1, lineCount);
    }
    
    // Draw text at specific Y position
    void drawTextAtY(const char* text, int x, int y, int maxW, int areaH, int lineH, bool center) {
        char lines[8][80];
        int lineWidths[8];
        int lineCount = 0;
        
        char curLine[80];
        int curLineLen = 0;
        int curLineW = 0;
        
        const char* p = text;
        while (*p && lineCount < 8) {
            char word[50];
            int wordLen = 0;
            while (*p && *p != ' ' && *p != '\n' && wordLen < 49) {
                word[wordLen++] = *p++;
            }
            word[wordLen] = '\0';
            
            if (wordLen > 0) {
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(word, 0, 0, &tx, &ty, &tw, &th);
                int wordW = tw;
                int spaceW = (curLineLen > 0) ? 8 : 0;
                
                if (curLineLen > 0 && curLineW + spaceW + wordW > maxW) {
                    curLine[curLineLen] = '\0';
                    strncpy(lines[lineCount], curLine, 79);
                    lines[lineCount][79] = '\0';
                    lineWidths[lineCount] = curLineW;
                    lineCount++;
                    curLineLen = 0;
                    curLineW = 0;
                    if (lineCount >= 8) break;
                }
                
                if (curLineLen > 0) {
                    curLine[curLineLen++] = ' ';
                    curLineW += 8;
                }
                for (int i = 0; i < wordLen && curLineLen < 78; i++) {
                    curLine[curLineLen++] = word[i];
                }
                curLineW += wordW;
            }
            
            if (*p == '\n') {
                curLine[curLineLen] = '\0';
                strncpy(lines[lineCount], curLine, 79);
                lines[lineCount][79] = '\0';
                lineWidths[lineCount] = curLineW;
                lineCount++;
                curLineLen = 0;
                curLineW = 0;
                p++;
            } else if (*p == ' ') {
                p++;
            }
        }
        
        if (curLineLen > 0 && lineCount < 8) {
            curLine[curLineLen] = '\0';
            strncpy(lines[lineCount], curLine, 79);
            lines[lineCount][79] = '\0';
            lineWidths[lineCount] = curLineW;
            lineCount++;
        }
        
        // Draw lines
        for (int i = 0; i < lineCount; i++) {
            int lineX = x;
            if (center) {
                lineX = x + (maxW - lineWidths[i]) / 2;
                if (lineX < x) lineX = x;
            }
            display.setCursor(lineX, y + i * lineH + lineH - 5);
            display.print(lines[i]);
        }
    }
    
    // Simple wrapped text drawing - always wraps and centers
    void drawSimpleWrappedText(const char* text, int x, int y, int maxW, int maxH, bool center) {
        // Parse text into wrapped lines
        char lines[8][80];
        int lineWidths[8];
        int lineCount = 0;
        
        int lineH = 28;  // Line height
        // Check if textSize is 2 by measuring
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds("M", 0, 0, &tx, &ty, &tw, &th);
        if (th > 20) lineH = 45;  // Larger line height for bigger text
        
        char curLine[80];
        int curLineLen = 0;
        int curLineW = 0;
        
        const char* p = text;
        while (*p && lineCount < 8) {
            // Find next word
            char word[50];
            int wordLen = 0;
            while (*p && *p != ' ' && *p != '\n' && wordLen < 49) {
                word[wordLen++] = *p++;
            }
            word[wordLen] = '\0';
            
            if (wordLen > 0) {
                display.getTextBounds(word, 0, 0, &tx, &ty, &tw, &th);
                int wordW = tw;
                int spaceW = (curLineLen > 0) ? 8 : 0;
                
                // Check if word fits on current line
                if (curLineLen > 0 && curLineW + spaceW + wordW > maxW) {
                    // Save current line and start new one
                    curLine[curLineLen] = '\0';
                    strncpy(lines[lineCount], curLine, 79);
                    lines[lineCount][79] = '\0';
                    lineWidths[lineCount] = curLineW;
                    lineCount++;
                    curLineLen = 0;
                    curLineW = 0;
                    
                    if (lineCount >= 8) break;
                }
                
                // Add word to current line
                if (curLineLen > 0) {
                    curLine[curLineLen++] = ' ';
                    curLineW += 8;
                }
                for (int i = 0; i < wordLen && curLineLen < 78; i++) {
                    curLine[curLineLen++] = word[i];
                }
                curLineW += wordW;
            }
            
            // Handle newlines and spaces
            if (*p == '\n') {
                curLine[curLineLen] = '\0';
                strncpy(lines[lineCount], curLine, 79);
                lines[lineCount][79] = '\0';
                lineWidths[lineCount] = curLineW;
                lineCount++;
                curLineLen = 0;
                curLineW = 0;
                p++;
            } else if (*p == ' ') {
                p++;
            }
        }
        
        // Save last line
        if (curLineLen > 0 && lineCount < 8) {
            curLine[curLineLen] = '\0';
            strncpy(lines[lineCount], curLine, 79);
            lines[lineCount][79] = '\0';
            lineWidths[lineCount] = curLineW;
            lineCount++;
        }
        
        // Calculate starting Y to center vertically
        int totalH = lineCount * lineH;
        int startY = y + (maxH - totalH) / 2;
        if (startY < y) startY = y;
        
        // Draw each line
        for (int i = 0; i < lineCount; i++) {
            int lineX = x;
            if (center) {
                // Center this line horizontally
                lineX = x + (maxW - lineWidths[i]) / 2;
                if (lineX < x) lineX = x;  // Don't go past left edge
            }
            
            display.setCursor(lineX, startY + i * lineH + lineH - 5);
            display.print(lines[i]);
        }
    }
    
    void drawWrappedText(const char* text, int x, int y, int maxWidth, int maxLines) {
        int curX = x;
        int curY = y;
        int lineHeight = (settingsManager.flashcards.fontSize == 3) ? 50 : 28;
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
                    int16_t x1, y1; uint16_t w, h;
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
    
    const GFXfont* getCardFont() {
        switch (settingsManager.flashcards.fontSize) {
            case 0: return &FreeSans9pt7b;
            case 1: return &FreeSansBold12pt7b;
            case 2: return &FreeSansBold12pt7b;
            case 3: return &FreeSansBold12pt7b;
            default: return &FreeSansBold12pt7b;
        }
    }
    
    // ==========================================================================
    // Deck Operations
    // ==========================================================================
    void scanDecks() {
        deckCount = 0;
        
        File dir = SD.open("/flashcards");
        if (!dir) {
            SD.mkdir("/flashcards");
            return;
        }
        
        while (deckCount < MAX_DECKS) {
            File entry = dir.openNextFile();
            if (!entry) break;
            
            const char* name = entry.name();
            if (entry.isDirectory() || name[0] == '.') {
                entry.close();
                continue;
            }
            
            DeckFormat fmt = detectFormat(name);
            if (fmt == FMT_UNKNOWN) {
                entry.close();
                continue;
            }
            
            // Store filename
            strncpy(decks[deckCount].name, name, 31);
            decks[deckCount].name[31] = '\0';
            
            // Create display name (remove extension)
            strncpy(decks[deckCount].displayName, name, 27);
            decks[deckCount].displayName[27] = '\0';
            char* dot = strrchr(decks[deckCount].displayName, '.');
            if (dot) *dot = '\0';
            
            // Replace underscores/hyphens with spaces
            for (char* c = decks[deckCount].displayName; *c; c++) {
                if (*c == '_' || *c == '-') *c = ' ';
            }
            
            decks[deckCount].format = fmt;
            
            // Count cards (quick scan)
            decks[deckCount].cardCount = countCardsInFile(entry, fmt);
            
            // Check for images
            decks[deckCount].hasImages = (strstr(name, "asl") != nullptr);
            
            // Load metadata
            DeckMetadata* dm = findDeckMeta(decks[deckCount].name);
            if (dm) {
                decks[deckCount].lastUsed = dm->lastUsedDate;
                // Progress = accuracy (correct / studied)
                decks[deckCount].progress = (dm->cardsStudied > 0)
                    ? (dm->correctCount * 100) / dm->cardsStudied : 0;
                if (decks[deckCount].progress > 100) decks[deckCount].progress = 100;
            } else {
                decks[deckCount].lastUsed = 0;
                decks[deckCount].progress = 0;
            }
            
            entry.close();
            deckCount++;
        }
        
        dir.close();
    }
    
    int countCardsInFile(File& f, DeckFormat fmt) {
        int count = 0;
        f.seek(0);
        
        while (f.available() && count < 200) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;
            
            if (fmt == FMT_TXT) {
                count++;  // Each non-empty line is half a card
            } else if (fmt == FMT_CSV || fmt == FMT_TSV) {
                if (line.indexOf(fmt == FMT_CSV ? ',' : '\t') > 0) {
                    count++;
                }
            }
        }
        
        if (fmt == FMT_TXT) count /= 2;
        
        f.seek(0);
        return count;
    }
    
    void startStudySession() {
        currentDeckIndex = deckCursor;
        loadDeck();
        
        if (cardCount > 0) {
            if (settingsManager.flashcards.shuffle) {
                shuffleCards();
            }
            cardIndex = 0;
            sessionCorrect = 0;
            sessionIncorrect = 0;
            currentScreen = FC_SCREEN_STUDY_QUESTION;
        }
    }
    
    void loadDeck() {
        closeDeck();
        
        if (deckCursor < 0 || deckCursor >= deckCount) return;
        
        char path[64];
        snprintf(path, sizeof(path), "/flashcards/%s", decks[deckCursor].name);
        
        DeckFormat fmt = decks[deckCursor].format;
        
        cards = new Card[MAX_CARDS];
        if (!cards) return;
        
        memset(cards, 0, sizeof(Card) * MAX_CARDS);
        cardCount = 0;
        
        bool success = false;
        if (fmt == FMT_TXT) {
            success = loadTxtDeck(path);
        } else if (fmt == FMT_CSV) {
            success = loadCsvDeck(path, ',');
        } else if (fmt == FMT_TSV) {
            success = loadCsvDeck(path, '\t');
        } else if (fmt == FMT_JSON) {
            success = loadJsonDeck(path);
        }
        
        if (!success || cardCount == 0) {
            closeDeck();
        }
    }
    
    void closeDeck() {
        if (cards) {
            delete[] cards;
            cards = nullptr;
        }
        cardCount = 0;
        cardIndex = 0;
    }
    
    void shuffleCards() {
        if (!cards || cardCount < 2) return;
        
        for (int i = cardCount - 1; i > 0; i--) {
            int j = random(i + 1);
            Card temp = cards[i];
            cards[i] = cards[j];
            cards[j] = temp;
        }
    }
    
    void nextCard() {
        cardIndex++;
        stats.totalCardsStudied++;
        stats.cardsToday++;
        
        if (cardIndex >= cardCount) {
            finishSession();
        } else {
            currentScreen = FC_SCREEN_STUDY_QUESTION;
        }
    }
    
    void finishSession() {
        // Update deck metadata
        DeckMetadata* dm = findOrCreateDeckMeta(decks[currentDeckIndex].name);
        if (dm) {
            // Track cards actually studied this session (not loaded)
            int cardsThisSession = sessionCorrect + sessionIncorrect;
            dm->cardsStudied = min((int)dm->cardsStudied + cardsThisSession, 
                                   (int)decks[currentDeckIndex].cardCount);
            dm->correctCount += sessionCorrect;
            dm->lastUsedDate = getCurrentDate();
            dm->sessions++;
            dm->cardCount = decks[currentDeckIndex].cardCount;  // Store actual deck size
        }
        
        // Update stats
        updateStreak();
        saveStats();
        saveDeckMeta();
        
        // Update deck progress display - now shows accuracy
        decks[currentDeckIndex].lastUsed = getCurrentDate();
        if (dm && dm->cardsStudied > 0) {
            // Progress = accuracy (correct answers / total attempts)
            int totalAttempts = dm->cardsStudied;
            decks[currentDeckIndex].progress = (dm->correctCount * 100) / totalAttempts;
            if (decks[currentDeckIndex].progress > 100) 
                decks[currentDeckIndex].progress = 100;
        }
        
        currentScreen = FC_SCREEN_SESSION_COMPLETE;
    }
    
    // ==========================================================================
    // Format Detection & Loading
    // ==========================================================================
    DeckFormat detectFormat(const char* filename) {
        const char* ext = strrchr(filename, '.');
        if (!ext) return FMT_UNKNOWN;
        
        if (strcasecmp(ext, ".txt") == 0) return FMT_TXT;
        if (strcasecmp(ext, ".csv") == 0) return FMT_CSV;
        if (strcasecmp(ext, ".tsv") == 0) return FMT_TSV;
        if (strcasecmp(ext, ".json") == 0) return FMT_JSON;
        
        return FMT_UNKNOWN;
    }
    
    bool loadTxtDeck(const char* path) {
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        
        while (f.available() && cardCount < MAX_CARDS) {
            String front = f.readStringUntil('\n');
            if (!f.available()) break;
            String back = f.readStringUntil('\n');
            
            front.trim();
            back.trim();
            
            if (front.length() > 0 && back.length() > 0) {
                strncpy(cards[cardCount].front, front.c_str(), MAX_TEXT - 1);
                strncpy(cards[cardCount].back, back.c_str(), MAX_TEXT - 1);
                cards[cardCount].seen = false;
                cardCount++;
            }
        }
        
        f.close();
        return cardCount > 0;
    }
    
    bool loadCsvDeck(const char* path, char delim) {
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        
        bool firstLine = true;
        
        while (f.available() && cardCount < MAX_CARDS) {
            String line = f.readStringUntil('\n');
            line.trim();
            
            if (line.length() == 0) continue;
            
            // Skip header row
            if (firstLine) {
                firstLine = false;
                if (isHeaderRow(line.c_str())) continue;
            }
            
            int delimPos = line.indexOf(delim);
            if (delimPos <= 0) continue;
            
            String front = line.substring(0, delimPos);
            String back = line.substring(delimPos + 1);
            
            // Remove quotes
            front.trim();
            back.trim();
            if (front.startsWith("\"")) front = front.substring(1);
            if (front.endsWith("\"")) front = front.substring(0, front.length() - 1);
            if (back.startsWith("\"")) back = back.substring(1);
            if (back.endsWith("\"")) back = back.substring(0, back.length() - 1);
            
            if (front.length() > 0 && back.length() > 0) {
                strncpy(cards[cardCount].front, front.c_str(), MAX_TEXT - 1);
                strncpy(cards[cardCount].back, back.c_str(), MAX_TEXT - 1);
                cards[cardCount].seen = false;
                cardCount++;
            }
        }
        
        f.close();
        return cardCount > 0;
    }
    
    bool loadJsonDeck(const char* path) {
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        
        String json = f.readString();
        f.close();
        
        // Simple JSON array parsing
        int pos = 0;
        while (cardCount < MAX_CARDS) {
            int objStart = json.indexOf('{', pos);
            if (objStart < 0) break;
            
            int objEnd = json.indexOf('}', objStart);
            if (objEnd < 0) break;
            
            String obj = json.substring(objStart, objEnd + 1);
            
            // Try different field names
            const char* frontKeys[] = {"front", "question", "term", "word", "kanji"};
            const char* backKeys[] = {"back", "answer", "definition", "meaning", "reading"};
            
            String front, back;
            
            for (int i = 0; i < 5 && front.length() == 0; i++) {
                front = extractJsonValue(obj, frontKeys[i]);
            }
            for (int i = 0; i < 5 && back.length() == 0; i++) {
                back = extractJsonValue(obj, backKeys[i]);
            }
            
            if (front.length() > 0 && back.length() > 0) {
                strncpy(cards[cardCount].front, front.c_str(), MAX_TEXT - 1);
                strncpy(cards[cardCount].back, back.c_str(), MAX_TEXT - 1);
                cards[cardCount].seen = false;
                cardCount++;
            }
            
            pos = objEnd + 1;
        }
        
        return cardCount > 0;
    }
    
    String extractJsonValue(const String& obj, const char* key) {
        String searchKey = String("\"") + key + "\"";
        int keyPos = obj.indexOf(searchKey);
        if (keyPos < 0) return "";
        
        int colonPos = obj.indexOf(':', keyPos);
        if (colonPos < 0) return "";
        
        int valueStart = colonPos + 1;
        while (valueStart < (int)obj.length() && (obj[valueStart] == ' ' || obj[valueStart] == '"')) {
            valueStart++;
        }
        
        int valueEnd = valueStart;
        bool inQuotes = (obj[valueStart - 1] == '"');
        
        if (inQuotes) {
            valueEnd = obj.indexOf('"', valueStart);
        } else {
            while (valueEnd < (int)obj.length() && obj[valueEnd] != ',' && obj[valueEnd] != '}') {
                valueEnd++;
            }
        }
        
        if (valueEnd < 0) valueEnd = obj.length();
        
        return obj.substring(valueStart, valueEnd);
    }
    
    bool isHeaderRow(const char* line) {
        const char* headers[] = {"front", "back", "question", "answer", "term", "definition", "word"};
        String lower = String(line);
        lower.toLowerCase();
        
        for (int i = 0; i < 7; i++) {
            if (lower.indexOf(headers[i]) >= 0) return true;
        }
        return false;
    }
    
    // ==========================================================================
    // Image Support
    // ==========================================================================
    bool isImagePath(const char* text) {
        if (!text) return false;
        const char* ext = strrchr(text, '.');
        if (!ext) return false;
        return (strcasecmp(ext, ".bmp") == 0 || 
                strcasecmp(ext, ".jpg") == 0 ||
                strcasecmp(ext, ".png") == 0);
    }
    
    bool drawFlashcardImage(const char* path, int x, int y, int maxW, int maxH) {
        // Build full path if relative
        char fullPath[128];
        if (path[0] == '/') {
            strncpy(fullPath, path, sizeof(fullPath) - 1);
        } else {
            snprintf(fullPath, sizeof(fullPath), "/flashcards/%s", path);
        }
        
        File f = SD.open(fullPath, FILE_READ);
        if (!f) {
            // Just show path as text
            display.setFont(&FreeSans9pt7b);
            display.setCursor(x, y + maxH / 2);
            display.print(path);
            return false;
        }
        
        // Simple BMP loading (monochrome only for now)
        // For full implementation, use TJpgDec or similar
        f.close();
        
        display.setFont(&FreeSans9pt7b);
        centerText("[Image]", x + maxW / 2, y + maxH / 2);
        return true;
    }
    
    // ==========================================================================
    // Statistics & Persistence
    // ==========================================================================
    void loadStats() {
        File f = SD.open(FLASHCARDS_STATS_PATH, FILE_READ);
        if (f) {
            f.read((uint8_t*)&stats, sizeof(FlashcardStats));
            f.close();
            if (!stats.isValid()) {
                stats = FlashcardStats();
            }
        }
    }
    
    void saveStats() {
        SD.mkdir("/.sumi");
        File f = SD.open(FLASHCARDS_STATS_PATH, FILE_WRITE);
        if (f) {
            f.write((uint8_t*)&stats, sizeof(FlashcardStats));
            f.close();
        }
    }
    
    void loadDeckMeta() {
        File f = SD.open(FLASHCARDS_DECKMETA_PATH, FILE_READ);
        if (f) {
            f.read((uint8_t*)&deckMeta, sizeof(DeckMetadataFile));
            f.close();
            if (!deckMeta.isValid()) {
                deckMeta = DeckMetadataFile();
            }
        }
    }
    
    void saveDeckMeta() {
        SD.mkdir("/.sumi");
        File f = SD.open(FLASHCARDS_DECKMETA_PATH, FILE_WRITE);
        if (f) {
            f.write((uint8_t*)&deckMeta, sizeof(DeckMetadataFile));
            f.close();
        }
    }
    
    DeckMetadata* findDeckMeta(const char* filename) {
        for (int i = 0; i < deckMeta.deckCount; i++) {
            if (strcmp(deckMeta.decks[i].filename, filename) == 0) {
                return &deckMeta.decks[i];
            }
        }
        return nullptr;
    }
    
    DeckMetadata* findOrCreateDeckMeta(const char* filename) {
        DeckMetadata* dm = findDeckMeta(filename);
        if (dm) return dm;
        
        if (deckMeta.deckCount >= 20) return nullptr;
        
        dm = &deckMeta.decks[deckMeta.deckCount++];
        memset(dm, 0, sizeof(DeckMetadata));
        strncpy(dm->filename, filename, 31);
        return dm;
    }
    
    void updateTodayStats() {
        uint32_t today = getCurrentDate();
        
        if (stats.lastStudyDate != today) {
            // New day - shift daily counts
            for (int i = 6; i > 0; i--) {
                stats.dailyCounts[i] = stats.dailyCounts[i - 1];
            }
            stats.dailyCounts[0] = 0;
            stats.cardsToday = 0;
            
            // Recalculate week total
            stats.cardsThisWeek = 0;
            for (int i = 0; i < 7; i++) {
                stats.cardsThisWeek += stats.dailyCounts[i];
            }
            
            stats.lastStudyDate = today;
        }
    }
    
    void updateStreak() {
        uint32_t today = getCurrentDate();
        uint32_t yesterday = today - 1;  // Simplified, doesn't handle month boundaries
        
        if (stats.lastStudyDate == 0 || stats.lastStudyDate < yesterday) {
            stats.currentStreak = 1;
        } else if (stats.lastStudyDate == yesterday) {
            stats.currentStreak++;
        }
        // If same day, streak stays the same
        
        if (stats.currentStreak > stats.bestStreak) {
            stats.bestStreak = stats.currentStreak;
        }
        
        stats.lastStudyDate = today;
    }
    
    uint32_t getCurrentDate() {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 100)) {
            return 20260101;  // Fallback
        }
        return (timeinfo.tm_year + 1900) * 10000 + 
               (timeinfo.tm_mon + 1) * 100 + 
               timeinfo.tm_mday;
    }
    
    const char* formatDate(uint32_t date) {
        static char buf[16];
        uint32_t today = getCurrentDate();
        
        if (date == today) {
            return "Today";
        } else if (date == today - 1) {
            return "Yesterday";
        } else {
            int diff = today - date;
            if (diff < 7) {
                snprintf(buf, 16, "%d days ago", diff);
            } else if (diff < 30) {
                snprintf(buf, 16, "%d weeks ago", diff / 7);
            } else {
                snprintf(buf, 16, "%d/%d", (date / 100) % 100, date % 100);
            }
            return buf;
        }
    }
};

#endif // FEATURE_FLASHCARDS
#endif // SUMI_PLUGIN_FLASHCARDS_H
