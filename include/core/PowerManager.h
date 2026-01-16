#ifndef SUMI_POWER_MANAGER_H
#define SUMI_POWER_MANAGER_H

/**
 * @file PowerManager.h
 * @brief Power management - sleep, wake, activity tracking
 * @version 2.1.16
 */

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Power Constants
// =============================================================================
const unsigned long POWER_BUTTON_WAKEUP_MS = 1000;
const unsigned long POWER_BUTTON_SLEEP_MS = 1000;
const unsigned long AUTO_SLEEP_TIMEOUT_MS = 300000;  // 5 minutes default

// =============================================================================
// Power Manager Class
// =============================================================================
class PowerManager {
public:
    PowerManager() : _lastActivityTime(0), _autoSleepEnabled(true) {}
    
    // Activity tracking
    void resetActivityTimer() { _lastActivityTime = millis(); }
    unsigned long getIdleTime() const { return millis() - _lastActivityTime; }
    bool shouldAutoSleep() const { 
        return _autoSleepEnabled && (getIdleTime() > AUTO_SLEEP_TIMEOUT_MS); 
    }
    
    // Auto-sleep control
    void enableAutoSleep(bool enable) { _autoSleepEnabled = enable; }
    bool isAutoSleepEnabled() const { return _autoSleepEnabled; }
    
    // Sleep functions
    void enterDeepSleep();
    void enterLightSleep();
    
    // Time sync
    void syncTimeInBackground();
    
    // Wake verification (for boot loop prevention)
    bool verifyWakeupLongPress();
    
    // Portal cleanup
    void cleanupPortalResources();
    
    // Diagnostics
    void printMemoryReport();
    void printFeatureFlags();

private:
    unsigned long _lastActivityTime;
    bool _autoSleepEnabled;
};

// Global instance
extern PowerManager powerManager;

// Legacy function wrappers for compatibility
inline void resetActivityTimer() { powerManager.resetActivityTimer(); }
inline void enterDeepSleep() { powerManager.enterDeepSleep(); }
inline void cleanupPortalResources() { powerManager.cleanupPortalResources(); }
inline void printMemoryReport() { powerManager.printMemoryReport(); }
inline void printFeatureFlags() { powerManager.printFeatureFlags(); }

#endif // SUMI_POWER_MANAGER_H
