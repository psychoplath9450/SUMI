/**
 * @file ButtonInput.cpp
 * @brief Physical button reading implementation
 * @version 1.3.0
 */

#include "core/ButtonInput.h"

// =============================================================================
// State
// =============================================================================
static bool isLandscape = false;

#if FEATURE_BLUETOOTH
static Button injectedButton = BTN_NONE;

void injectButton(Button btn) {
    injectedButton = btn;
}
#endif

// =============================================================================
// Button Functions
// =============================================================================
void setButtonOrientation(bool landscape) {
    isLandscape = landscape;
}

const char* getButtonName(Button btn) {
    switch (btn) {
        case BTN_NONE: return "NONE";
        case BTN_UP: return "UP";
        case BTN_DOWN: return "DOWN";
        case BTN_LEFT: return "LEFT";
        case BTN_RIGHT: return "RIGHT";
        case BTN_CONFIRM: return "CONFIRM";
        case BTN_BACK: return "BACK";
        case BTN_POWER: return "POWER";
        default: return "?";
    }
}

Button readRawButton() {
    if (digitalRead(BTN_GPIO3) == LOW) return BTN_POWER;
    
    int btn1 = analogRead(BTN_GPIO1);
    int btn2 = analogRead(BTN_GPIO2);
    
    // Buttons on GPIO1 - check in order of lowest to highest ADC value
    if (btn1 < BTN_RIGHT_VAL + BTN_THRESHOLD) return BTN_RIGHT;
    else if (btn1 < BTN_LEFT_VAL + BTN_THRESHOLD) return BTN_LEFT;
    else if (btn1 < BTN_CONFIRM_VAL + BTN_THRESHOLD) return BTN_CONFIRM;
    else if (btn1 < BTN_BACK_VAL + BTN_THRESHOLD + 200) return BTN_BACK;  // Extended range for BACK
    
    // Buttons on GPIO2
    if (btn2 < BTN_DOWN_VAL + BTN_THRESHOLD) return BTN_DOWN;
    else if (btn2 < BTN_UP_VAL + BTN_THRESHOLD) return BTN_UP;
    
    return BTN_NONE;
}

Button readButton() {
    #if FEATURE_BLUETOOTH
    // Check for injected button from Bluetooth keyboard first
    if (injectedButton != BTN_NONE) {
        Button btn = injectedButton;
        injectedButton = BTN_NONE;  // Clear after reading
        return btn;
    }
    #endif
    
    Button raw = readRawButton();
    
    // Debug: log non-NONE buttons
    if (raw != BTN_NONE) {
        Serial.printf("[BTN] Raw button: %d (%s), isLandscape=%d\n", raw, getButtonName(raw), isLandscape);
    }
    
    if (!isLandscape || raw == BTN_NONE || raw == BTN_CONFIRM || raw == BTN_BACK || raw == BTN_POWER) {
        return raw;
    }
    // Remap for landscape
    Button mapped;
    switch (raw) {
        case BTN_UP: mapped = BTN_LEFT; break;
        case BTN_DOWN: mapped = BTN_RIGHT; break;
        case BTN_LEFT: mapped = BTN_DOWN; break;
        case BTN_RIGHT: mapped = BTN_UP; break;
        default: mapped = raw; break;
    }
    if (mapped != raw) {
        Serial.printf("[BTN] Remapped to: %d (%s)\n", mapped, getButtonName(mapped));
    }
    return mapped;
}

Button waitForButtonPress() {
    Button btn = BTN_NONE;
    while (btn == BTN_NONE) {
        btn = readButton();
        delay(20);
    }
    while (readButton() != BTN_NONE) delay(20);
    delay(30);
    return btn;
}
