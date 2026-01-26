#ifndef SUMI_HOME_ITEMS_H
#define SUMI_HOME_ITEMS_H

#include <Arduino.h>
#include "config.h"

/**
 * @file HomeItems.h
 * @brief Home screen item definitions and configuration
 * @version 1.3.0
 *
 * Defines the apps available on the home screen.
 * Demo plugin for development testing and visual demos.
 */

// =============================================================================
// Item Indices (0-14)
// =============================================================================

// Core Apps (0-2)
#define HOME_ITEM_LIBRARY       0
#define HOME_ITEM_FLASHCARDS    1
#define HOME_ITEM_NOTES         2

// Games - E-ink friendly (3-8)
#define HOME_ITEM_CHESS         3
#define HOME_ITEM_CHECKERS      4
#define HOME_ITEM_SUDOKU        5
#define HOME_ITEM_MINESWEEPER   6
#define HOME_ITEM_SOLITAIRE     7
#define HOME_ITEM_CUBE3D        8   // 3D Animation Demo

// Productivity (9)
#define HOME_ITEM_TODO          9

// Widgets (10)
#define HOME_ITEM_WEATHER       10

// Media (11-12)
#define HOME_ITEM_IMAGES        11
#define HOME_ITEM_MAPS          12

// ToolSuite - Consolidated (13)
#define HOME_ITEM_TOOLS         13

// System (14)
#define HOME_ITEM_SETTINGS      14

#define HOME_ITEMS_TOTAL        15
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
    { HOME_ITEM_CUBE3D,      "cube3d",      "Demo",     "3", "games" },
    
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
 */
inline void getDefaultHomeItems(uint8_t* bitmap) {
    memset(bitmap, 0, HOME_ITEMS_BYTES);
    
    // Always enable Settings
    bitmap[HOME_ITEM_SETTINGS / 8] |= (1 << (HOME_ITEM_SETTINGS % 8));
    
    // Library
    #if FEATURE_READER
    bitmap[HOME_ITEM_LIBRARY / 8] |= (1 << (HOME_ITEM_LIBRARY % 8));
    #endif
    
    // Flashcards
    #if FEATURE_FLASHCARDS
    bitmap[HOME_ITEM_FLASHCARDS / 8] |= (1 << (HOME_ITEM_FLASHCARDS % 8));
    #endif
    
    // Weather
    #if FEATURE_WEATHER
    bitmap[HOME_ITEM_WEATHER / 8] |= (1 << (HOME_ITEM_WEATHER % 8));
    #endif
    
    // Games (no Demo by default)
    #if FEATURE_GAMES
    bitmap[HOME_ITEM_CHESS / 8] |= (1 << (HOME_ITEM_CHESS % 8));
    bitmap[HOME_ITEM_SUDOKU / 8] |= (1 << (HOME_ITEM_SUDOKU % 8));
    // Demo (cube3d) not enabled by default
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
        case HOME_ITEM_CUBE3D:
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
            return true;
            
        default:
            return false;
    }
}

#endif
