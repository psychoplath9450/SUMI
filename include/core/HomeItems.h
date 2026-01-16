#ifndef SUMI_HOME_ITEMS_H
#define SUMI_HOME_ITEMS_H

#include <Arduino.h>
#include "config.h"

/**
 * @file HomeItems.h
 * @brief Home screen item definitions and configuration
 * @version 2.1.16
 *
 * Defines the apps available on the home screen (11 apps + ToolSuite + Settings).
 *
 * REMOVED: Snake, Tetris, Pacman, TicTacToe, Connect4, Gomoku, Dots,
 *          Battleship, Nonogram, Sokoban, Yahtzee, Mastermind, Memory,
 *          Blackjack, WordSearch, Calculator, Clock, Countdown, Counter,
 *          Dice, Converter, Random, Ruler, Astronomy, Birthdays,
 *          ReadingStats, Trivia, MathTrainer, Vocabulary, Sketch,
 *          2048, Habits, Quotes, Wordle (memory optimization)
 */

// =============================================================================
// Item Indices (0-12)
// =============================================================================

// Core Apps (0-2)
#define HOME_ITEM_LIBRARY       0
#define HOME_ITEM_FLASHCARDS    1
#define HOME_ITEM_NOTES         2

// Games - E-ink friendly only (3-7) - REMOVED: Wordle, 2048
#define HOME_ITEM_CHESS         3
#define HOME_ITEM_CHECKERS      4
#define HOME_ITEM_SUDOKU        5
#define HOME_ITEM_MINESWEEPER   6
#define HOME_ITEM_SOLITAIRE     7

// Productivity (8) - REMOVED: Habits
#define HOME_ITEM_TODO          8

// Widgets (9) - REMOVED: Quotes
#define HOME_ITEM_WEATHER       9

// Media (10-11)
#define HOME_ITEM_IMAGES        10
#define HOME_ITEM_MAPS          11

// ToolSuite - Consolidated (12)
#define HOME_ITEM_TOOLS         12

// System (13)
#define HOME_ITEM_SETTINGS      13

#define HOME_ITEMS_TOTAL        14
// HOME_ITEMS_BYTES defined in config.h

// =============================================================================
// Item Info
// =============================================================================
struct HomeItemInfo {
    uint8_t index;
    const char* id;
    const char* label;
    const char* icon;
    const char* category;
};

static const HomeItemInfo HOME_ITEMS[] = {
    // Core
    { HOME_ITEM_LIBRARY,     "library",     "Library",     "B", "core" },
    { HOME_ITEM_FLASHCARDS,  "flashcards",  "Flashcards",  "F", "core" },
    { HOME_ITEM_NOTES,       "notes",       "Notes",       "N", "core" },
    
    // Games
    { HOME_ITEM_CHESS,       "chess",       "Chess",       "C", "games" },
    { HOME_ITEM_CHECKERS,    "checkers",    "Checkers",    "K", "games" },
    { HOME_ITEM_SUDOKU,      "sudoku",      "Sudoku",      "9", "games" },
    { HOME_ITEM_MINESWEEPER, "minesweeper", "Mines",       "M", "games" },
    { HOME_ITEM_SOLITAIRE,   "solitaire",   "Solitaire",   "S", "games" },
    
    // Productivity
    { HOME_ITEM_TODO,        "todo",        "To-Do",       "T", "tools" },
    
    // Widgets
    { HOME_ITEM_WEATHER,     "weather",     "Weather",     "~", "widgets" },
    
    // Media
    { HOME_ITEM_IMAGES,      "images",      "Images",      "I", "media" },
    { HOME_ITEM_MAPS,        "maps",        "Maps",        "P", "media" },
    
    // Tools
    { HOME_ITEM_TOOLS,       "tools",       "Tools",       "X", "tools" },
    
    // System
    { HOME_ITEM_SETTINGS,    "settings",    "Settings",    "*", "system" },
};

#define HOME_ITEMS_COUNT (sizeof(HOME_ITEMS) / sizeof(HOME_ITEMS[0]))

// =============================================================================
// Helpers
// =============================================================================
inline const HomeItemInfo* getHomeItemByIndex(uint8_t index) {
    for (size_t i = 0; i < HOME_ITEMS_COUNT; i++) {
        if (HOME_ITEMS[i].index == index) return &HOME_ITEMS[i];
    }
    return nullptr;
}

inline const HomeItemInfo* getHomeItemById(const char* id) {
    for (size_t i = 0; i < HOME_ITEMS_COUNT; i++) {
        if (strcmp(HOME_ITEMS[i].id, id) == 0) return &HOME_ITEMS[i];
    }
    return nullptr;
}

inline int8_t getHomeItemIndex(const char* id) {
    const HomeItemInfo* item = getHomeItemById(id);
    return item ? item->index : -1;
}

/**
 * @brief Get feature-aware default home items
 * 
 * Default apps: Library, Flashcards, Chess, Sudoku, Weather, Settings
 * Only enables items that are actually available based on feature flags.
 * This prevents showing items unavailable on low-memory devices.
 */
inline void getDefaultHomeItems(uint8_t* bitmap) {
    memset(bitmap, 0, HOME_ITEMS_BYTES);
    
    // Always enable Settings (required for device configuration - cannot be removed)
    bitmap[HOME_ITEM_SETTINGS / 8] |= (1 << (HOME_ITEM_SETTINGS % 8));
    
    // Library - only if FEATURE_READER is enabled
    #if FEATURE_READER
    bitmap[HOME_ITEM_LIBRARY / 8] |= (1 << (HOME_ITEM_LIBRARY % 8));
    #endif
    
    // Flashcards - only if FEATURE_FLASHCARDS is enabled
    #if FEATURE_FLASHCARDS
    bitmap[HOME_ITEM_FLASHCARDS / 8] |= (1 << (HOME_ITEM_FLASHCARDS % 8));
    #endif
    
    // Weather - only if FEATURE_WEATHER is enabled
    #if FEATURE_WEATHER
    bitmap[HOME_ITEM_WEATHER / 8] |= (1 << (HOME_ITEM_WEATHER % 8));
    #endif
    
    // Chess and Sudoku - only if FEATURE_GAMES is enabled
    #if FEATURE_GAMES
    bitmap[HOME_ITEM_CHESS / 8] |= (1 << (HOME_ITEM_CHESS % 8));
    bitmap[HOME_ITEM_SUDOKU / 8] |= (1 << (HOME_ITEM_SUDOKU % 8));
    #endif
}

inline bool isHomeItemEnabled(const uint8_t* bitmap, uint8_t idx) {
    if (idx >= HOME_ITEMS_TOTAL) return false;
    return (bitmap[idx / 8] & (1 << (idx % 8))) != 0;
}

inline void setHomeItemEnabled(uint8_t* bitmap, uint8_t idx, bool enabled) {
    if (idx >= HOME_ITEMS_TOTAL) return;
    if (enabled) bitmap[idx / 8] |= (1 << (idx % 8));
    else bitmap[idx / 8] &= ~(1 << (idx % 8));
}

/**
 * @brief Check if a home item can be launched based on feature flags
 * 
 * Returns true if the required feature flag is enabled for this item.
 * This can be used by the portal to grey out unavailable items.
 */
inline bool isHomeItemAvailable(uint8_t idx) {
    switch (idx) {
        case HOME_ITEM_LIBRARY:
            #if FEATURE_READER
            return true;
            #else
            return false;
            #endif
            
        case HOME_ITEM_FLASHCARDS:
            #if FEATURE_FLASHCARDS
            return true;
            #else
            return false;
            #endif
            
        case HOME_ITEM_WEATHER:
            #if FEATURE_WEATHER
            return true;
            #else
            return false;
            #endif
            
        case HOME_ITEM_NOTES:
        case HOME_ITEM_CHESS:
        case HOME_ITEM_CHECKERS:
        case HOME_ITEM_SUDOKU:
        case HOME_ITEM_MINESWEEPER:
        case HOME_ITEM_SOLITAIRE:
        case HOME_ITEM_TODO:
        case HOME_ITEM_IMAGES:
        case HOME_ITEM_MAPS:
        case HOME_ITEM_TOOLS:
            #if FEATURE_GAMES
            return true;
            #else
            return false;
            #endif
            
        case HOME_ITEM_SETTINGS:
            return true;  // Always available
            
        default:
            return false;
    }
}

#endif
