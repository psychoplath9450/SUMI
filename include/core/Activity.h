/**
 * @file Activity.h
 * @brief Activity Lifecycle Management System
 * @version 1.4.2
 *
 * Activity lifecycle management for plugins.
 * Each screen (Library, Reader, Weather, Settings) is an Activity with:
 * - onEnter(): Called when activity becomes active (allocate resources)
 * - onExit(): Called when leaving (free resources, save state)
 * - loop(): Called every frame to handle input and rendering
 *
 * This prevents memory leaks from incomplete cleanup and provides
 * consistent lifecycle across all screens.
 */

#ifndef SUMI_ACTIVITY_H
#define SUMI_ACTIVITY_H

#include <Arduino.h>
#include "config.h"

// Forward declarations
class Activity;

// =============================================================================
// Activity Manager - Handles activity transitions
// =============================================================================
class ActivityManager {
public:
    static ActivityManager& instance() {
        static ActivityManager mgr;
        return mgr;
    }
    
    /**
     * Transition to a new activity
     * Calls onExit() on current, deletes it, then onEnter() on new
     */
    void switchTo(Activity* newActivity);
    
    /**
     * Get current activity (may be nullptr at startup)
     */
    Activity* current() { return _current; }
    
    /**
     * Run one frame of the current activity
     */
    void loop();
    
    /**
     * Check if any activity is running
     */
    bool hasActivity() const { return _current != nullptr; }
    
private:
    ActivityManager() : _current(nullptr) {}
    Activity* _current;
};

// Global accessor
#define Activities ActivityManager::instance()

// =============================================================================
// Activity Base Class
// =============================================================================
class Activity {
protected:
    const char* _name;
    int _screenW;
    int _screenH;
    bool _landscape;
    
public:
    explicit Activity(const char* name) 
        : _name(name), _screenW(800), _screenH(480), _landscape(true) {}
    
    virtual ~Activity() = default;
    
    /**
     * Called when this activity becomes active
     * Override to allocate resources, load state, initialize display
     */
    virtual void onEnter() {
        Serial.printf("[ACT] Entering: %s\n", _name);
        MEM_LOG(_name);
    }
    
    /**
     * Called when leaving this activity
     * Override to free resources, save state, clean up
     * MUST free all allocated memory to prevent leaks
     */
    virtual void onExit() {
        Serial.printf("[ACT] Exiting: %s\n", _name);
        MEM_LOG(_name);
    }
    
    /**
     * Called every frame
     * Handle input, update state, render if needed
     */
    virtual void loop() = 0;
    
    /**
     * Return true to skip the 10ms loop delay (for responsive network ops)
     */
    virtual bool skipLoopDelay() { return false; }
    
    /**
     * Return true to prevent auto-sleep (during indexing, transfers, etc)
     */
    virtual bool preventAutoSleep() { return false; }
    
    /**
     * Set screen dimensions (called before onEnter)
     */
    void setScreenSize(int w, int h) {
        _screenW = w;
        _screenH = h;
        _landscape = (w > h);
    }
    
    const char* getName() const { return _name; }
    int getScreenWidth() const { return _screenW; }
    int getScreenHeight() const { return _screenH; }
    bool isLandscape() const { return _landscape; }
};

// =============================================================================
// Activity Manager Implementation (inline for header-only simplicity)
// =============================================================================
inline void ActivityManager::switchTo(Activity* newActivity) {
    Serial.printf("[ACT] Switch: %s -> %s\n", 
                  _current ? _current->getName() : "none",
                  newActivity ? newActivity->getName() : "none");
    
    MEM_LOG("before_switch");
    
    // Exit current activity
    if (_current) {
        _current->onExit();
        delete _current;
        _current = nullptr;
    }
    
    MEM_LOG("after_exit");
    
    // Enter new activity
    if (newActivity) {
        _current = newActivity;
        _current->onEnter();
    }
    
    MEM_LOG("after_enter");
}

inline void ActivityManager::loop() {
    if (_current) {
        _current->loop();
    }
}

#endif // SUMI_ACTIVITY_H
