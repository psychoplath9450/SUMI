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
    PowerManager() : _lastActivityTime(0), _autoSleepEnabled(true), 
                     _needsTimeSync(false), _timeSyncInProgress(false) {}
    
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
    
    // Time sync - background/non-blocking
    void syncTimeInBackground();
    void requestTimeSync() { _needsTimeSync = true; }
    bool needsTimeSync() const { return _needsTimeSync && !_timeSyncInProgress; }
    void startBackgroundTimeSync();  // Non-blocking, starts task
    void checkBackgroundTimeSync();  // Call periodically to clean up
    
    // Wake verification (for boot loop prevention)
    bool verifyWakeupLongPress();
    
    // Portal cleanup
    void cleanupPortalResources();
    
    // Reading mode - suspend/resume services to free RAM
    void suspendForReading();
    void resumeAfterReading();
    
    // Diagnostics
    void printMemoryReport();
    void printFeatureFlags();

private:
    unsigned long _lastActivityTime;
    bool _autoSleepEnabled;
    bool _needsTimeSync;
    bool _timeSyncInProgress;
};

// Global instance
extern PowerManager powerManager;

// Legacy function wrappers for compatibility
inline void resetActivityTimer() { powerManager.resetActivityTimer(); }
inline void enterDeepSleep() { powerManager.enterDeepSleep(); }
inline void cleanupPortalResources() { powerManager.cleanupPortalResources(); }
inline void suspendForReading() { powerManager.suspendForReading(); }
inline void resumeAfterReading() { powerManager.resumeAfterReading(); }
inline void printMemoryReport() { powerManager.printMemoryReport(); }
inline void printFeatureFlags() { powerManager.printFeatureFlags(); }

#endif // SUMI_POWER_MANAGER_H
