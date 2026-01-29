#ifndef SUMI_PLUGIN_RUNNER_H
#define SUMI_PLUGIN_RUNNER_H

/**
 * @file PluginRunner.h
 * @brief Template functions for running plugins
 * @version 1.3.0
 * 
 * Centralized display refresh handling for plugins.
 * Provides three plugin runner modes:
 * - runPluginSimple: Standard plugins with managed refresh
 * - runPluginSelfRefresh: Plugins that handle their own partial updates
 * - runPluginWithUpdate: Plugins with periodic update loops (timers, etc.)
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
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

// =============================================================================
// On-Demand Allocated Plugin Runners (saves RAM when plugin not in use)
// =============================================================================

/**
 * Run a simple plugin with on-demand allocation
 * Plugin is created on entry, destroyed on exit - saves RAM
 */
template<typename T>
void runPluginAllocSimple(const char* title) {
    Serial.printf("[PLUGIN] Allocating: %s\n", title);
    Serial.printf("[PLUGIN] Free heap before: %d\n", ESP.getFreeHeap());
    
    T* plugin = new T();
    if (!plugin) {
        Serial.println("[PLUGIN] Allocation failed!");
        showHomeScreen();
        return;
    }
    
    Serial.printf("[PLUGIN] Free heap after alloc: %d\n", ESP.getFreeHeap());
    Serial.printf("[PLUGIN] Starting: %s\n", title);
    
    plugin->init(display.width(), display.height());
    refreshManager.reset();
    
    // Initial full draw
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold9pt7b);
        plugin->draw();
    } while (display.nextPage());
    refreshManager.recordFullRefresh();
    
    Button lastBtn = BTN_NONE;
    
    while (true) {
        Button btn = readButton();
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            powerManager.resetActivityTimer();
            
            if (btn == BTN_POWER) {
                delete plugin;
                powerManager.enterDeepSleep();
                return;
            }
            
            if (btn == BTN_BACK) {
                bool consumed = plugin->handleInput(btn);
                if (!consumed) {
                    break;  // Exit loop to cleanup
                }
            } else {
                plugin->handleInput(btn);
            }
            
            // Smart refresh
            if (refreshManager.mustFullRefresh() && refreshManager.canFullRefresh()) {
                display.setFullWindow();
                display.firstPage();
                do {
                    display.fillScreen(GxEPD_WHITE);
                    display.setTextColor(GxEPD_BLACK);
                    display.setFont(&FreeSansBold9pt7b);
                    plugin->draw();
                } while (display.nextPage());
                refreshManager.recordFullRefresh();
            } else {
                display.setPartialWindow(0, 0, display.width(), display.height());
                display.firstPage();
                do {
                    display.fillScreen(GxEPD_WHITE);
                    display.setTextColor(GxEPD_BLACK);
                    display.setFont(&FreeSansBold9pt7b);
                    plugin->draw();
                } while (display.nextPage());
                refreshManager.recordPartialRefresh();
            }
        }
        lastBtn = btn;
        delay(30);
    }
    
    // Cleanup
    Serial.printf("[PLUGIN] Freeing: %s\n", title);
    delete plugin;
    Serial.printf("[PLUGIN] Free heap after free: %d\n", ESP.getFreeHeap());
    
    refreshManager.reset();
    showHomeScreen();
}

/**
 * Run a self-refresh plugin with on-demand allocation
 */
template<typename T>
void runPluginAllocSelfRefresh(const char* title) {
    Serial.printf("[PLUGIN] Allocating: %s\n", title);
    Serial.printf("[PLUGIN] Free heap before: %d\n", ESP.getFreeHeap());
    
    T* plugin = new T();
    if (!plugin) {
        Serial.println("[PLUGIN] Allocation failed!");
        showHomeScreen();
        return;
    }
    
    Serial.printf("[PLUGIN] Free heap after alloc: %d\n", ESP.getFreeHeap());
    Serial.printf("[PLUGIN] Starting (self-refresh): %s\n", title);
    
    plugin->init(display.width(), display.height());
    plugin->needsFullRedraw = true;
    plugin->draw();
    
    Button lastBtn = BTN_NONE;
    int partialCount = 0;
    const int FULL_REFRESH_INTERVAL = 15;
    
    while (true) {
        Button btn = readButton();
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            powerManager.resetActivityTimer();
            
            if (btn == BTN_POWER) {
                delete plugin;
                powerManager.enterDeepSleep();
                return;
            }
            
            if (btn == BTN_BACK) {
                bool consumed = plugin->handleInput(btn);
                if (!consumed) {
                    break;  // Exit loop to cleanup
                }
            } else {
                plugin->handleInput(btn);
            }
            
            if (++partialCount >= FULL_REFRESH_INTERVAL) {
                plugin->needsFullRedraw = true;
                partialCount = 0;
            }
            
            if (plugin->needsFullRedraw) {
                plugin->draw();
            } else {
                plugin->drawPartial();
            }
        }
        
        if (plugin->update()) {
            if (plugin->needsFullRedraw) {
                plugin->draw();
            } else {
                plugin->drawPartial();
            }
        }
        
        lastBtn = btn;
        delay(30);
    }
    
    // Cleanup
    Serial.printf("[PLUGIN] Freeing: %s\n", title);
    delete plugin;
    Serial.printf("[PLUGIN] Free heap after free: %d\n", ESP.getFreeHeap());
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
    showHomeScreen();
}

/**
 * Run a plugin that handles ALL its own display refresh
 * Plugin's draw() methods must include their own firstPage/nextPage loops
 * Used for complex plugins like Library that need fine-grained control
 * 
 * Features button repeat for LEFT/RIGHT to enable rapid navigation through cover art
 */
template<typename T>
void runPluginAllocDirect(const char* title) {
    Serial.printf("[PLUGIN] Allocating: %s\n", title);
    Serial.printf("[PLUGIN] Free heap before: %d\n", ESP.getFreeHeap());
    
    T* plugin = new T();
    if (!plugin) {
        Serial.println("[PLUGIN] Allocation failed!");
        showHomeScreen();
        return;
    }
    
    Serial.printf("[PLUGIN] Free heap after alloc: %d\n", ESP.getFreeHeap());
    Serial.printf("[PLUGIN] Starting (direct): %s\n", title);
    
    plugin->init(display.width(), display.height());
    
    // Initial draw - plugin handles its own display refresh
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
                // This is the key fix: allow browsing while holding button
                if (now - lastProcessTime >= repeatDelayMs) {
                    shouldProcess = true;
                }
            }
        }
        
        if (shouldProcess) {
            Serial.printf("[PLUGIN] Button: %d (repeat=%d)\n", btn, lastBtn == btn);
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
                    break;  // Exit loop to cleanup
                }
            } else {
                plugin->handleInput(btn);
            }
            
            // Plugin handles its own refresh in handleInput or draw
            // Only call draw if plugin indicates it needs redraw
            if (plugin->needsRedraw()) {
                plugin->draw();
                
                // After draw completes, allow immediate repeat if holding LEFT/RIGHT
                // Update lastProcessTime so the repeat delay starts from draw completion
                lastProcessTime = millis();
                
                // Check if still holding a navigation button for rapid browsing
                Button postDrawBtn = readButton();
                if (postDrawBtn == BTN_LEFT || postDrawBtn == BTN_RIGHT) {
                    // Keep lastBtn set to the direction button to enable repeat
                    // Don't reset - the repeat logic will trigger on next iteration
                    lastBtnTime = millis();
                } else if (postDrawBtn == BTN_NONE) {
                    // Button released - reset for immediate new press
                    lastBtn = BTN_NONE;
                    lastBtnTime = millis() - debounceMs;
                }
            }
        }
        
        lastBtn = btn;
        delay(15);  // Fast polling for responsive input
    }
    
    // Show closing overlay
    display.setPartialWindow(display.width()/2 - 100, display.height()/2 - 25, 200, 50);
    display.firstPage();
    do {
        display.fillRect(display.width()/2 - 100, display.height()/2 - 25, 200, 50, GxEPD_WHITE);
        display.drawRect(display.width()/2 - 100, display.height()/2 - 25, 200, 50, GxEPD_BLACK);
        display.drawRect(display.width()/2 - 99, display.height()/2 - 24, 198, 48, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(display.width()/2 - 40, display.height()/2 + 5);
        display.print("Closing...");
    } while (display.nextPage());
    
    // Cleanup
    Serial.printf("[PLUGIN] Freeing: %s\n", title);
    delete plugin;
    Serial.printf("[PLUGIN] Free heap after free: %d\n", ESP.getFreeHeap());
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
    showHomeScreen();
}

/**
 * Run an animation plugin with on-demand allocation
 */
template<typename T>
void runPluginAllocAnimation(const char* title) {
    Serial.printf("[PLUGIN] Allocating: %s\n", title);
    Serial.printf("[PLUGIN] Free heap before: %d\n", ESP.getFreeHeap());
    
    T* plugin = new T();
    if (!plugin) {
        Serial.println("[PLUGIN] Allocation failed!");
        showHomeScreen();
        return;
    }
    
    Serial.printf("[PLUGIN] Free heap after alloc: %d\n", ESP.getFreeHeap());
    Serial.printf("[PLUGIN] Starting (animation): %s\n", title);
    
    plugin->init(display.width(), display.height());
    plugin->drawFullScreen();
    
    Button lastBtn = BTN_NONE;
    unsigned long lastFrameTime = 0;
    const unsigned long FRAME_INTERVAL = 50;
    
    while (plugin->isRunning()) {
        Button btn = readButton();
        
        if (btn != BTN_NONE && lastBtn == BTN_NONE) {
            powerManager.resetActivityTimer();
            
            if (btn == BTN_POWER) {
                delete plugin;
                powerManager.enterDeepSleep();
                return;
            }
            
            bool consumed = plugin->handleInput(btn);
            if (!consumed && btn == BTN_BACK) {
                break;  // Exit loop to cleanup
            }
        }
        lastBtn = btn;
        
        if (millis() - lastFrameTime >= FRAME_INTERVAL) {
            plugin->draw();
            lastFrameTime = millis();
        }
        
        delay(10);
    }
    
    // Cleanup
    Serial.printf("[PLUGIN] Freeing: %s\n", title);
    delete plugin;
    Serial.printf("[PLUGIN] Free heap after free: %d\n", ESP.getFreeHeap());
    
    PluginDisplay::exitToHome();
}

#endif // SUMI_PLUGIN_RUNNER_H
