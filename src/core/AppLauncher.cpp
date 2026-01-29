/**
 * @file AppLauncher.cpp
 * @brief App launching with on-demand memory allocation
 * @version 1.4.2
 * 
 * All plugins are allocated when opened and freed when closed.
 * This saves ~40KB+ of RAM when plugins aren't in use.
 */

#include "core/AppLauncher.h"
#include "core/HomeScreen.h"
#include "core/SettingsScreen.h"
#include "core/ButtonInput.h"
#include "core/PluginRunner.h"
#include "core/SettingsState.h"
#include "core/PowerManager.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// External state from main.cpp
enum AppScreen { APP_SCREEN_HOME, APP_SCREEN_SETTINGS };
extern AppScreen currentAppScreen;

// Plugin headers only - no global instances!
#if FEATURE_READER
#include "plugins/Library.h"
#endif

#if FEATURE_FLASHCARDS
#include "plugins/Flashcards.h"
#endif

#if FEATURE_WEATHER
#include "plugins/Weather.h"
#endif

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
#include "plugins/Cube3D.h"
#endif

// For placeholder app fallback
static Button lastButton = BTN_NONE;

// Forward declaration
void showHomeScreen();

// =============================================================================
// App Launcher - All plugins allocated on-demand, freed on exit
// =============================================================================
void openAppByItemIndex(uint8_t itemIndex) {
    const HomeItemInfo* itemInfo = getHomeItemByIndex(itemIndex);
    const char* appName = itemInfo ? itemInfo->label : "Unknown";
    
    Serial.printf("[APP] Opening: %s (index=%d)\n", appName, itemIndex);
    Serial.printf("[APP] Free heap: %d\n", ESP.getFreeHeap());
    
    // Settings screen (part of core, not allocated)
    if (itemIndex == HOME_ITEM_SETTINGS) {
        currentAppScreen = APP_SCREEN_SETTINGS;
        settingsInit();
        showSettingsScreen();
        return;
    }
    
    // Library - allocated on demand, handles its own display refresh
#if FEATURE_READER
    if (itemIndex == HOME_ITEM_LIBRARY) {
        // Show loading overlay immediately
        display.setPartialWindow(display.width()/2 - 100, display.height()/2 - 25, 200, 50);
        display.firstPage();
        do {
            display.fillRect(display.width()/2 - 100, display.height()/2 - 25, 200, 50, GxEPD_WHITE);
            display.drawRect(display.width()/2 - 100, display.height()/2 - 25, 200, 50, GxEPD_BLACK);
            display.drawRect(display.width()/2 - 99, display.height()/2 - 24, 198, 48, GxEPD_BLACK);
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(display.width()/2 - 45, display.height()/2 + 5);
            display.print("Opening...");
        } while (display.nextPage());
        
        // Library needs ~35KB contiguous block. Check before attempting allocation.
        size_t freeHeap = ESP.getFreeHeap();
        size_t maxBlock = ESP.getMaxAllocHeap();
        Serial.printf("[APP] Memory check: free=%d, maxBlock=%d\n", freeHeap, maxBlock);
        
        if (maxBlock < 35000) {
            Serial.println("[APP] ERROR: Not enough contiguous memory for Library!");
            Serial.println("[APP] Try rebooting the device to defragment memory.");
            // Show message on screen
            display.setFullWindow();
            display.firstPage();
            do {
                display.fillScreen(GxEPD_WHITE);
                display.setFont(&FreeSansBold12pt7b);
                display.setTextColor(GxEPD_BLACK);
                display.setCursor(40, 200);
                display.print("Memory fragmented");
                display.setCursor(40, 240);
                display.setFont(&FreeSans9pt7b);
                display.print("Please reboot to continue.");
                display.setCursor(40, 280);
                display.print("(Hold power button)");
            } while (display.nextPage());
            delay(3000);
            showHomeScreen();
            return;
        }
        
        runPluginAllocDirect<LibraryApp>("Library");
        return;
    }
#endif

    // Flashcards - allocated on demand
#if FEATURE_FLASHCARDS
    if (itemIndex == HOME_ITEM_FLASHCARDS) {
        runPluginAllocSelfRefresh<FlashcardsApp>("Flashcards");
        return;
    }
#endif

    // Weather - allocated on demand
#if FEATURE_WEATHER
    if (itemIndex == HOME_ITEM_WEATHER) {
        runPluginAllocSelfRefresh<WeatherApp>("Weather");
        return;
    }
#endif

    // Games and productivity - all allocated on demand
#if FEATURE_GAMES
    switch (itemIndex) {
        case HOME_ITEM_NOTES:
            runPluginAllocSimple<NotesApp>("Notes");
            return;
            
        case HOME_ITEM_CHESS:
            runPluginAllocSelfRefresh<ChessGame>("Chess");
            return;
            
        case HOME_ITEM_CHECKERS:
            runPluginAllocSimple<CheckersGame>("Checkers");
            return;
            
        case HOME_ITEM_SUDOKU:
            runPluginAllocSelfRefresh<SudokuGame>("Sudoku");
            return;
            
        case HOME_ITEM_MINESWEEPER:
            runPluginAllocSimple<MinesweeperGame>("Minesweeper");
            return;
            
        case HOME_ITEM_SOLITAIRE:
            runPluginAllocSimple<SolitaireGame>("Solitaire");
            return;
        
        case HOME_ITEM_CUBE3D:
            runPluginAllocAnimation<Cube3DApp>("Demo");
            return;
            
        case HOME_ITEM_TODO:
            runPluginAllocSimple<TodoApp>("To-Do");
            return;
            
        case HOME_ITEM_IMAGES:
            runPluginAllocSimple<ImagesApp>("Images");
            return;
            
        case HOME_ITEM_MAPS:
            runPluginAllocSimple<MapsApp>("Maps");
            return;
            
        case HOME_ITEM_TOOLS:
            runPluginAllocSelfRefresh<ToolSuiteApp>("Tools");
            return;
            
        default:
            break;
    }
#endif
    
    // Fallback - show placeholder for unimplemented apps
    currentAppScreen = APP_SCREEN_HOME;
    showAppPlaceholder(appName);
    
    while (true) {
        Button btn = readButton();
        if (btn != BTN_NONE && lastButton == BTN_NONE) {
            if (btn == BTN_POWER) {
                powerManager.enterDeepSleep();
                return;
            }
            if (btn == BTN_BACK) {
                delay(100);
                showHomeScreen();
                break;
            }
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

#if FEATURE_READER
// Special launcher for book widget - opens Library and resumes last book
void openLibraryWithResume() {
    Serial.println("[LAUNCHER] Opening Library with resume...");
    
    // Show immediate feedback
    int w = display.width();
    int h = display.height();
    display.setPartialWindow(w/2 - 100, h/2 - 25, 200, 50);
    display.firstPage();
    do {
        display.fillRect(w/2 - 100, h/2 - 25, 200, 50, GxEPD_WHITE);
        display.drawRect(w/2 - 100, h/2 - 25, 200, 50, GxEPD_BLACK);
        display.drawRect(w/2 - 99, h/2 - 24, 198, 48, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(w/2 - 55, h/2 + 5);
        display.print("Resuming...");
    } while (display.nextPage());
    
    // Check heap before allocation
    if (ESP.getMaxAllocHeap() < 50000) {
        Serial.println("[LAUNCHER] Low heap - cannot open Library");
        showHomeScreen();
        return;
    }
    
    LibraryApp* plugin = new LibraryApp();
    if (!plugin) {
        Serial.println("[LAUNCHER] Library allocation failed!");
        showHomeScreen();
        return;
    }
    
    // Initialize with autoResume=true
    plugin->init(w, h, true);
    plugin->draw();
    
    Button lastBtn = BTN_NONE;
    unsigned long lastBtnTime = 0;
    unsigned long lastProcessTime = 0;  // When we last processed a button
    const unsigned long debounceMs = 50;  // Short debounce for responsiveness
    const unsigned long repeatDelayMs = 100;  // Allow repeat after this delay post-draw
    
    while (true) {
        Button btn = readButton();
        unsigned long now = millis();
        
        bool shouldProcess = false;
        
        // Process button if: 
        // 1. New button press (btn != lastBtn) after debounce, OR
        // 2. Navigation button (LEFT/RIGHT) held after draw completed (enables rapid browsing)
        if (btn != BTN_NONE && (now - lastBtnTime >= debounceMs)) {
            if (lastBtn == BTN_NONE) {
                // New button press
                shouldProcess = true;
            } else if ((btn == BTN_LEFT || btn == BTN_RIGHT) && btn == lastBtn) {
                // Same navigation button held - allow repeat after draw completes
                if (now - lastProcessTime >= repeatDelayMs) {
                    shouldProcess = true;
                }
            }
        }
        
        if (shouldProcess) {
            powerManager.resetActivityTimer();
            lastBtnTime = now;
            lastProcessTime = now;
            
            if (btn == BTN_POWER) {
                delete plugin;
                powerManager.enterDeepSleep();
                return;
            }
            
            if (btn == BTN_BACK) {
                bool consumed = plugin->handleInput(btn);
                if (!consumed) {
                    break;
                }
            } else {
                plugin->handleInput(btn);
            }
            
            if (plugin->needsRedraw()) {
                plugin->draw();
                
                // After draw completes, allow immediate repeat if holding LEFT/RIGHT
                lastProcessTime = millis();
                
                Button postDrawBtn = readButton();
                if (postDrawBtn == BTN_LEFT || postDrawBtn == BTN_RIGHT) {
                    lastBtnTime = millis();
                } else if (postDrawBtn == BTN_NONE) {
                    lastBtn = BTN_NONE;
                    lastBtnTime = millis() - debounceMs;
                }
            }
        }
        
        lastBtn = btn;
        delay(15);
    }
    
    // Cleanup
    display.setPartialWindow(w/2 - 100, h/2 - 25, 200, 50);
    display.firstPage();
    do {
        display.fillRect(w/2 - 100, h/2 - 25, 200, 50, GxEPD_WHITE);
        display.drawRect(w/2 - 100, h/2 - 25, 200, 50, GxEPD_BLACK);
        display.drawRect(w/2 - 99, h/2 - 24, 198, 48, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(w/2 - 55, h/2 + 5);
        display.print("Closing...");
    } while (display.nextPage());
    
    delete plugin;
    showHomeScreen();
}
#else
void openLibraryWithResume() {
    showHomeScreen();
}
#endif
