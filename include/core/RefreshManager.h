#ifndef SUMI_REFRESH_MANAGER_H
#define SUMI_REFRESH_MANAGER_H

/**
 * @file RefreshManager.h
 * @brief E-ink display refresh rate management
 * @version 2.1.16
 * 
 * Enforces minimum refresh intervals to prevent:
 * - Ghosting from too-frequent partial refreshes
 * - Display damage from rapid full refreshes
 * - Wasted CPU cycles on impossible refresh rates
 */

#include <Arduino.h>

// =============================================================================
// Timing Constants - THE LAW
// =============================================================================
#define REFRESH_FULL_MIN_MS      2000   // 2 seconds between full refreshes
#define REFRESH_PARTIAL_MIN_MS   500    // 500ms between partial refreshes  
#define REFRESH_FAST_MIN_MS      300    // 300ms for games (use sparingly)
#define REFRESH_TYPING_MIN_MS    100    // 100ms for typing (max ghosting tolerance)
#define PARTIAL_BEFORE_FULL      20     // Force full refresh every 20 partials

class RefreshManager {
public:
    enum RefreshMode {
        MODE_NORMAL,    // Standard operation (500ms partial, 2s full)
        MODE_FAST,      // Game mode (300ms partial) - causes more ghosting
        MODE_TYPING,    // Text input mode (100ms partial) - max ghosting tolerance
        MODE_QUALITY    // Reading mode - prefers full refreshes
    };
    
    void begin() {
        _lastFull = 0;
        _lastPartial = 0;
        _partialCount = 0;
        _mode = MODE_NORMAL;
    }
    
    void setMode(RefreshMode mode) { _mode = mode; }
    RefreshMode getMode() const { return _mode; }
    
    bool canPartialRefresh() {
        unsigned long minInterval;
        switch (_mode) {
            case MODE_TYPING: minInterval = REFRESH_TYPING_MIN_MS; break;
            case MODE_FAST:   minInterval = REFRESH_FAST_MIN_MS; break;
            default:          minInterval = REFRESH_PARTIAL_MIN_MS; break;
        }
        return (millis() - _lastPartial >= minInterval);
    }
    
    bool canFullRefresh() {
        return (millis() - _lastFull >= REFRESH_FULL_MIN_MS);
    }
    
    bool mustFullRefresh() {
        return (_partialCount >= PARTIAL_BEFORE_FULL);
    }
    
    void recordPartialRefresh() {
        _lastPartial = millis();
        _partialCount++;
    }
    
    void recordFullRefresh() {
        _lastFull = millis();
        _lastPartial = millis();
        _partialCount = 0;
    }
    
    void reset() {
        _partialCount = 0;
        _lastFull = millis();
        _lastPartial = millis();
    }
    
    int getPartialCount() const { return _partialCount; }
    unsigned long timeSinceFullRefresh() const { return millis() - _lastFull; }
    
private:
    unsigned long _lastFull;
    unsigned long _lastPartial;
    int _partialCount;
    RefreshMode _mode;
};

extern RefreshManager refreshManager;

#endif
