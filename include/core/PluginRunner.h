#ifndef SUMI_PLUGIN_RUNNER_H
#define SUMI_PLUGIN_RUNNER_H

/**
 * @file PluginRunner.h
 * @brief Template functions for running plugins
 * @version 2.1.16
 * 
 * Refactored to eliminate code duplication in display refresh handling.
 * Provides three plugin runner modes:
 * - runPluginSimple: Standard plugins with managed refresh
 * - runPluginSelfRefresh: Plugins that handle their own partial updates
 * - runPluginWithUpdate: Plugins with periodic update loops (timers, etc.)
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"
#include "core/RefreshManager.h"
#include "core/PowerManager.h"

// External references
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern RefreshManager refreshManager;
extern void showHomeScreen();
extern Button readButton();

// =============================================================================
// Display Refresh Helpers (shared by all plugin runners)
// =============================================================================
namespace PluginDisplay {

/**
 * Draw plugin content with standard setup (clear screen, set colors/font)
 */
template<typename T>
inline void drawContent(T& plugin) {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold9pt7b);
    plugin.draw();
}

/**
 * Perform a full refresh of the display (with black flash to clear ghosting)
 */
template<typename T>
inline void doFullRefresh(T& plugin) {
    display.setFullWindow();
    display.firstPage();
    do {
        drawContent(plugin);
    } while (display.nextPage());
    refreshManager.recordFullRefresh();
}

/**
 * Perform a partial refresh of the display (fast, no flash)
 */
template<typename T>
inline void doPartialRefresh(T& plugin) {
    // Use setPartialWindow for smoother updates (like HomeScreen does)
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();
    do {
        drawContent(plugin);
    } while (display.nextPage());
    refreshManager.recordPartialRefresh();
}

/**
 * Smart refresh - chooses full or partial based on ghosting counter
 */
template<typename T>
inline void doSmartRefresh(T& plugin) {
    if (refreshManager.mustFullRefresh() && refreshManager.canFullRefresh()) {
        doFullRefresh(plugin);
    } else {
        doPartialRefresh(plugin);
    }
}

/**
 * Clear screen and return to home
 */
inline void exitToHome() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
    showHomeScreen();
}

} // namespace PluginDisplay

// =============================================================================
// Plugin Runner Templates
// =============================================================================

/**
 * Run a simple plugin with standard refresh behavior
 * 
 * Suitable for plugins requiring only draw() and handleInput()
 * Examples: Notes, Checkers, Sudoku, Minesweeper, Solitaire, etc.
 */
template<typename T>
void runPluginSimple(T& plugin, const char* title) {
    Serial.printf("[PLUGIN] Starting: %s\n", title);
    plugin.init(display.width(), display.height());
    refreshManager.reset();
    
    // Initial full draw
    PluginDisplay::doFullRefresh(plugin);
    
    Button lastBtn = BTN_NONE;
    
    while (true) {
        Button btn = readButton();
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            Serial.printf("[PLUGIN] Button pressed: %d\n", btn);
            powerManager.resetActivityTimer();
            
            // Handle power button - enter deep sleep
            if (btn == BTN_POWER) {
                Serial.println("[PLUGIN] Power button - entering deep sleep");
                powerManager.enterDeepSleep();
                return;
            }
            
            // Handle back button
            if (btn == BTN_BACK) {
                Serial.println("[PLUGIN] Back button detected");
                bool consumed = plugin.handleInput(btn);
                Serial.printf("[PLUGIN] handleInput returned: %s\n", consumed ? "true" : "false");
                if (!consumed) {
                    Serial.println("[PLUGIN] Exiting to home");
                    refreshManager.reset();
                    showHomeScreen();
                    return;
                }
            } else {
                plugin.handleInput(btn);
            }
            
            PluginDisplay::doSmartRefresh(plugin);
        }
        lastBtn = btn;
        delay(30);
    }
}

/**
 * Run a plugin that handles its own partial refresh
 * 
 * Suitable for applications requiring fine-grained control over which regions refresh
 * Examples: Chess (only refreshes changed squares)
 * 
 * Plugin requirements:
 * - Must have needsFullRedraw boolean member
 * - Must implement draw() for full redraw
 * - Must implement drawPartial() for partial updates
 * - May implement update() for animations (return true if redraw needed)
 */
template<typename T>
void runPluginSelfRefresh(T& plugin, const char* title) {
    Serial.printf("[PLUGIN] Starting (self-refresh): %s\n", title);
    plugin.init(display.width(), display.height());
    
    plugin.needsFullRedraw = true;
    plugin.draw();
    
    Button lastBtn = BTN_NONE;
    int partialCount = 0;
    const int FULL_REFRESH_INTERVAL = 15;
    
    while (true) {
        Button btn = readButton();
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            Serial.printf("[PLUGIN] Button pressed: %d\n", btn);
            powerManager.resetActivityTimer();
            
            // Handle power button - enter deep sleep
            if (btn == BTN_POWER) {
                Serial.println("[PLUGIN] Power button - entering deep sleep");
                powerManager.enterDeepSleep();
                return;
            }
            
            // Handle back button
            if (btn == BTN_BACK) {
                Serial.println("[PLUGIN] Back button detected");
                bool consumed = plugin.handleInput(btn);
                Serial.printf("[PLUGIN] handleInput returned: %s\n", consumed ? "true" : "false");
                if (!consumed) {
                    Serial.println("[PLUGIN] Exiting to home");
                    PluginDisplay::exitToHome();
                    return;
                }
            } else {
                plugin.handleInput(btn);
            }
            
            // Force full refresh periodically to clear ghosting
            if (++partialCount >= FULL_REFRESH_INTERVAL) {
                plugin.needsFullRedraw = true;
                partialCount = 0;
            }
            
            if (plugin.needsFullRedraw) {
                plugin.draw();
            } else {
                plugin.drawPartial();
            }
        }
        
        // Check for animation/timer updates
        if (plugin.update()) {
            if (plugin.needsFullRedraw) {
                plugin.draw();
            } else {
                plugin.drawPartial();
            }
        }
        
        lastBtn = btn;
        delay(30);
    }
}

/**
 * Run a plugin with periodic update loop
 * 
 * Suitable for plugins requiring regular updates (timers, clocks, animations)
 * Examples: ToolSuite (has stopwatch/timer), Weather (might auto-refresh)
 * 
 * Plugin requirements:
 * - Must implement update() that returns true if display needs refresh
 */
template<typename T>
void runPluginWithUpdate(T& plugin, const char* title) {
    Serial.printf("[PLUGIN] Starting (with update): %s\n", title);
    plugin.init(display.width(), display.height());
    refreshManager.reset();
    
    // Initial full draw
    PluginDisplay::doFullRefresh(plugin);
    
    Button lastBtn = BTN_NONE;
    unsigned long lastUpdate = 0;
    const unsigned long UPDATE_INTERVAL = 1000;
    
    while (true) {
        Button btn = readButton();
        bool needsRedraw = false;
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            Serial.printf("[PLUGIN] Button pressed: %d\n", btn);
            powerManager.resetActivityTimer();
            
            // Handle power button - enter deep sleep
            if (btn == BTN_POWER) {
                Serial.println("[PLUGIN] Power button - entering deep sleep");
                powerManager.enterDeepSleep();
                return;
            }
            
            // Handle back button
            if (btn == BTN_BACK) {
                Serial.println("[PLUGIN] Back button detected");
                bool consumed = plugin.handleInput(btn);
                Serial.printf("[PLUGIN] handleInput returned: %s\n", consumed ? "true" : "false");
                if (!consumed) {
                    Serial.println("[PLUGIN] Exiting to home");
                    refreshManager.reset();
                    showHomeScreen();
                    return;
                }
            } else {
                plugin.handleInput(btn);
            }
            needsRedraw = true;
        }
        
        // Periodic update check
        if (millis() - lastUpdate >= UPDATE_INTERVAL) {
            if (plugin.update()) {
                needsRedraw = true;
            }
            lastUpdate = millis();
        }
        
        if (needsRedraw) {
            PluginDisplay::doSmartRefresh(plugin);
        }
        
        lastBtn = btn;
        delay(30);
    }
}

/**
 * Run a continuous animation plugin
 * 
 * Suitable for real-time(ish) graphics demos that need continuous frame updates
 * Optimized for e-paper at ~2Hz to avoid ghosting
 * Examples: Cube3D, animation demos, demoscene experiments
 * 
 * Plugin requirements:
 * - Must implement init(w, h)
 * - Must implement draw() that handles its own partial refresh
 * - Must implement drawFullScreen() for initial render
 * - Must implement handleInput(btn) that returns false to exit
 * - Must implement isRunning() to check if animation should continue
 */
template<typename T>
void runPluginAnimation(T& plugin, const char* title) {
    Serial.printf("[PLUGIN] Starting (animation): %s\n", title);
    plugin.init(display.width(), display.height());
    
    // Initial full draw
    plugin.drawFullScreen();
    
    Button lastBtn = BTN_NONE;
    unsigned long lastFrameTime = 0;
    const unsigned long FRAME_INTERVAL = 50;  // Check input at 20Hz, animation is self-throttled
    
    while (plugin.isRunning()) {
        Button btn = readButton();
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            Serial.printf("[PLUGIN] Button pressed: %d\n", btn);
            powerManager.resetActivityTimer();
            
            // Handle power button - enter deep sleep
            if (btn == BTN_POWER) {
                Serial.println("[PLUGIN] Power button - entering deep sleep");
                powerManager.enterDeepSleep();
                return;
            }
            
            // Let plugin handle input (including BACK)
            bool consumed = plugin.handleInput(btn);
            if (!consumed && btn == BTN_BACK) {
                Serial.println("[PLUGIN] Animation exiting to home");
                PluginDisplay::exitToHome();
                return;
            }
        }
        lastBtn = btn;
        
        // Let the plugin handle its own frame timing and partial refresh
        // This is key for animations - they manage their own display updates
        if (millis() - lastFrameTime >= FRAME_INTERVAL) {
            plugin.draw();
            lastFrameTime = millis();
        }
        
        delay(10);  // Short delay to keep input responsive
    }
    
    // Animation ended (isRunning() returned false)
    Serial.println("[PLUGIN] Animation ended, returning to home");
    PluginDisplay::exitToHome();
}

#endif // SUMI_PLUGIN_RUNNER_H
