#ifndef SUMI_BATTERY_MONITOR_H
#define SUMI_BATTERY_MONITOR_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Battery Monitor Class
// =============================================================================
class BatteryMonitor {
public:
    BatteryMonitor();
    
    void begin();
    void update();
    
    // Readings
    float getVoltage() const { return _voltage; }
    uint8_t getPercent() const { return _percent; }
    bool isCharging() const { return _charging; }
    bool isLow() const { return _percent <= 20; }
    bool isCritical() const { return _percent <= 5; }
    
    // Thresholds
    static constexpr float VOLTAGE_FULL = 4.2f;
    static constexpr float VOLTAGE_NOMINAL = 3.7f;
    static constexpr float VOLTAGE_LOW = 3.5f;
    static constexpr float VOLTAGE_CRITICAL = 3.3f;
    static constexpr float VOLTAGE_EMPTY = 3.0f;
    
    // Voltage divider ratio (hardware-specific)
    static constexpr float DIVIDER_RATIO = 2.0f;

private:
    float _voltage;
    float _lastVoltage;
    uint8_t _percent;
    bool _charging;
    
    uint32_t _samples[10];
    int _sampleIndex;
    int _sampleCount;
    
    unsigned long _lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 10000;  // 10 seconds
    
    float readVoltage();
    uint8_t voltageToPercent(float voltage);
};

// Global instance
extern BatteryMonitor batteryMonitor;

#endif // SUMI_BATTERY_MONITOR_H
