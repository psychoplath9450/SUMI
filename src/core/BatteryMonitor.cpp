#include "core/BatteryMonitor.h"

// Global instance
BatteryMonitor batteryMonitor;

// =============================================================================
// Constructor
// =============================================================================
BatteryMonitor::BatteryMonitor()
    : _voltage(0)
    , _lastVoltage(0)
    , _percent(100)
    , _charging(false)
    , _sampleIndex(0)
    , _sampleCount(0)
    , _lastUpdate(0)
{
    memset(_samples, 0, sizeof(_samples));
}

// =============================================================================
// Initialize
// =============================================================================
void BatteryMonitor::begin() {
    // Configure ADC
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V
    
    // Take initial readings
    for (int i = 0; i < 10; i++) {
        _samples[i] = analogRead(PIN_BATTERY);
        delay(10);
    }
    _sampleCount = 10;
    
    // Initial calculation
    _voltage = readVoltage();
    _lastVoltage = _voltage;
    _percent = voltageToPercent(_voltage);
    
    Serial.printf("[BAT] Initial: %.2fV (%d%%)\n", _voltage, _percent);
}

// =============================================================================
// Update (call periodically)
// =============================================================================
void BatteryMonitor::update() {
    unsigned long now = millis();
    
    // Only update every UPDATE_INTERVAL
    if (now - _lastUpdate < UPDATE_INTERVAL) return;
    _lastUpdate = now;
    
    // Add new sample
    _samples[_sampleIndex] = analogRead(PIN_BATTERY);
    _sampleIndex = (_sampleIndex + 1) % 10;
    if (_sampleCount < 10) _sampleCount++;
    
    _lastVoltage = _voltage;
    _voltage = readVoltage();
    _percent = voltageToPercent(_voltage);
    
    // Detect charging (voltage increasing significantly)
    _charging = (_voltage > _lastVoltage + 0.05f);
    
    // Log if significant change
    if (abs(_voltage - _lastVoltage) > 0.1f) {
        Serial.printf("[BAT] %.2fV (%d%%)%s\n", 
            _voltage, _percent, _charging ? " [Charging]" : "");
    }
}

// =============================================================================
// Read Voltage (averaged from samples)
// =============================================================================
float BatteryMonitor::readVoltage() {
    if (_sampleCount == 0) return 0;
    
    // Average all samples
    uint32_t sum = 0;
    for (int i = 0; i < _sampleCount; i++) {
        sum += _samples[i];
    }
    uint32_t avg = sum / _sampleCount;
    
    // Convert to voltage
    // ADC is 12-bit (0-4095), reference is 3.3V
    // Account for voltage divider
    float voltage = (avg / 4095.0f) * 3.3f * DIVIDER_RATIO;
    
    return voltage;
}

// =============================================================================
// Convert Voltage to Percentage (LiPo discharge curve)
// =============================================================================
uint8_t BatteryMonitor::voltageToPercent(float voltage) {
    // LiPo discharge curve approximation
    // LiPo discharge curve is non-linear: stays at ~3.7V for most of discharge
    
    if (voltage >= VOLTAGE_FULL) return 100;
    if (voltage <= VOLTAGE_EMPTY) return 0;
    
    // Piecewise linear approximation of LiPo curve
    if (voltage >= 4.1f) {
        // 90-100%: 4.1V - 4.2V
        return 90 + (uint8_t)((voltage - 4.1f) / 0.1f * 10);
    } else if (voltage >= 3.9f) {
        // 70-90%: 3.9V - 4.1V
        return 70 + (uint8_t)((voltage - 3.9f) / 0.2f * 20);
    } else if (voltage >= 3.7f) {
        // 40-70%: 3.7V - 3.9V (flat region)
        return 40 + (uint8_t)((voltage - 3.7f) / 0.2f * 30);
    } else if (voltage >= 3.5f) {
        // 20-40%: 3.5V - 3.7V
        return 20 + (uint8_t)((voltage - 3.5f) / 0.2f * 20);
    } else if (voltage >= 3.3f) {
        // 5-20%: 3.3V - 3.5V
        return 5 + (uint8_t)((voltage - 3.3f) / 0.2f * 15);
    } else {
        // 0-5%: 3.0V - 3.3V (danger zone)
        return (uint8_t)((voltage - 3.0f) / 0.3f * 5);
    }
}
