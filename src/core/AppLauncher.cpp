/**
 * @file AppLauncher.cpp
 * @brief App launching implementation
 * @version 2.1.16
 */

#include "core/AppLauncher.h"
#include "core/HomeScreen.h"
#include "core/SettingsScreen.h"
#include "core/ButtonInput.h"
#include "core/PluginRunner.h"
#include "core/SettingsState.h"

// External state from main.cpp
enum Screen { SCREEN_HOME, SCREEN_SETTINGS };
extern Screen currentScreen;

// Plugin instances (extern - defined in src/plugins/*.cpp)
#if FEATURE_READER
#include "plugins/Library.h"
extern LibraryApp libraryApp;
#endif

#if FEATURE_FLASHCARDS
#include "plugins/Flashcards.h"
extern FlashcardsApp flashcardsApp;
#endif

// Weather - uses FEATURE_WEATHER (separate from games)
#if FEATURE_WEATHER
#include "plugins/Weather.h"
extern WeatherApp weatherApp;
#endif

// Games and productivity plugins - only when FEATURE_GAMES is enabled
#if FEATURE_GAMES
#include "plugins/Notes.h"
#include "plugins/ChessGame.h"
#include "plugins/Checkers.h"
#include "plugins/Sudoku.h"
#include "plugins/Minesweeper.h"
#include "plugins/Solitaire.h"
#include "plugins/TodoList.h"
#include "plugins/Images.h"
#include "plugins/Maps.h"
#include "plugins/ToolSuite.h"

extern NotesApp notesApp;
extern ChessGame chessGame;
extern CheckersGame checkersGame;
extern SudokuGame sudokuGame;
extern MinesweeperGame minesweeperGame;
extern SolitaireGame solitaireGame;
extern TodoApp todoApp;
extern ImagesApp imagesApp;
extern MapsApp mapsApp;
extern ToolSuiteApp toolSuiteApp;
#endif

// For placeholder app fallback
static Button lastButton = BTN_NONE;

// Forward declaration
void showHomeScreen();

// =============================================================================
// App Launcher
// =============================================================================
void openAppByItemIndex(uint8_t itemIndex) {
    const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
    const char* appName = itemInfo ? itemInfo->label : "Unknown";
    
    Serial.printf("[APP] Opening: %s (index=%d)\n", appName, itemIndex);
    
    if (itemIndex == HOME_ITEM_SETTINGS) {
        currentScreen = SCREEN_SETTINGS;
        settingsInit();
        showSettingsScreen();
        return;
    }
    
    // Library - always available with FEATURE_READER
#if FEATURE_READER
    if (itemIndex == HOME_ITEM_LIBRARY) {
        runPluginSimple(libraryApp, "Library");
        return;
    }
#endif

    // Flashcards - separate feature
#if FEATURE_FLASHCARDS
    if (itemIndex == HOME_ITEM_FLASHCARDS) {
        runPluginSimple(flashcardsApp, "Flashcards");
        return;
    }
#endif

    // Weather - separate feature (requires WiFi for API)
#if FEATURE_WEATHER
    if (itemIndex == HOME_ITEM_WEATHER) {
        runPluginSelfRefresh(weatherApp, "Weather");
        return;
    }
#endif

    // Games and productivity plugins - only when FEATURE_GAMES is enabled
#if FEATURE_GAMES
    switch (itemIndex) {
        case HOME_ITEM_NOTES:
            runPluginSimple(notesApp, "Notes");
            return;
            
        // Games (turn-based, e-ink friendly)
        case HOME_ITEM_CHESS:
            runPluginSelfRefresh(chessGame, "Chess");
            return;
        case HOME_ITEM_CHECKERS:
            runPluginSimple(checkersGame, "Checkers");
            return;
        case HOME_ITEM_SUDOKU:
            runPluginSimple(sudokuGame, "Sudoku");
            return;
        case HOME_ITEM_MINESWEEPER:
            runPluginSimple(minesweeperGame, "Minesweeper");
            return;
        case HOME_ITEM_SOLITAIRE:
            runPluginSimple(solitaireGame, "Solitaire");
            return;
            
        // Productivity
        case HOME_ITEM_TODO:
            runPluginSimple(todoApp, "To-Do");
            return;
            
        // Media
        case HOME_ITEM_IMAGES:
            runPluginSimple(imagesApp, "Images");
            return;
        case HOME_ITEM_MAPS:
            runPluginSimple(mapsApp, "Maps");
            return;
            
        // ToolSuite (consolidated tools)
        case HOME_ITEM_TOOLS:
            runPluginSelfRefresh(toolSuiteApp, "Tools");
            return;
            
        default:
            break;
    }
#endif
    
    // Fallback - show placeholder
    currentScreen = SCREEN_HOME;
    showAppPlaceholder(appName);
    
    while (true) {
        Button btn = readButton();
        if (btn == BTN_BACK && lastButton == BTN_NONE) {
            delay(100);
            showHomeScreen();
            break;
        }
        lastButton = btn;
        delay(50);
    }
}

void openApp(int selectionIndex) {
    uint8_t itemIndex = getItemIndexForPosition(selectionIndex);
    if (itemIndex != 0xFF) {
        openAppByItemIndex(itemIndex);
    }
}
