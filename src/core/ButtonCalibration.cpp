/**
 * @file ButtonCalibration.cpp
 * @brief Button calibration system implementation
 * @version 1.3.0
 */

#include "core/ButtonCalibration.h"

// Global instance
ButtonCalibration buttonCalibration;

// =============================================================================
// ButtonCalibrationData
// =============================================================================

void ButtonCalibrationData::setDefaults() {
    magic = 0;  // Not calibrated
    threshold = BTN_THRESHOLD;
    rightVal = BTN_RIGHT_VAL;
    leftVal = BTN_LEFT_VAL;
    confirmVal = BTN_CONFIRM_VAL;
    backVal = BTN_BACK_VAL;
    downVal = BTN_DOWN_VAL;
    upVal = BTN_UP_VAL;
}

// =============================================================================
// ButtonCalibration
// =============================================================================

ButtonCalibration::ButtonCalibration() {
    _data.setDefaults();
}

bool ButtonCalibration::load() {
    Preferences prefs;
    prefs.begin("sumi_btn", true);  // Read-only

    size_t size = prefs.getBytes("cal", &_data, sizeof(ButtonCalibrationData));
    prefs.end();

    if (size != sizeof(ButtonCalibrationData) || !_data.isValid()) {
        SUMI_LOGLN("[BTN_CAL] No valid calibration found, using defaults");
        _data.setDefaults();
        return false;
    }

    SUMI_LOGLN("[BTN_CAL] Loaded calibration from NVS");
    printCalibration();
    return true;
}

bool ButtonCalibration::save() {
    _data.magic = ButtonCalibrationData::MAGIC;

    Preferences prefs;
    prefs.begin("sumi_btn", false);  // Read-write

    size_t written = prefs.putBytes("cal", &_data, sizeof(ButtonCalibrationData));
    prefs.end();

    if (written != sizeof(ButtonCalibrationData)) {
        SUMI_LOGLN("[BTN_CAL] Failed to save calibration");
        return false;
    }

    SUMI_LOGLN("[BTN_CAL] Calibration saved to NVS");
    return true;
}

void ButtonCalibration::clear() {
    Preferences prefs;
    prefs.begin("sumi_btn", false);
    prefs.clear();
    prefs.end();

    _data.setDefaults();
    SUMI_LOGLN("[BTN_CAL] Calibration cleared, using defaults");
}

bool ButtonCalibration::shouldEnterCalibration() {
    // Read raw ADC values
    int adc1 = analogRead(BTN_GPIO1);
    int adc2 = analogRead(BTN_GPIO2);

    // Check if both CONFIRM and BACK are pressed (rough check)
    bool confirmPressed = abs(adc1 - _data.confirmVal) < 200;
    bool backPressed = abs(adc1 - _data.backVal) < 200;

    // Alternative: Check for POWER button held for 10 seconds
    int gpio3 = digitalRead(BTN_GPIO3);
    if (gpio3 == LOW) {
        // Power button held - wait and check duration
        unsigned long start = millis();
        while (digitalRead(BTN_GPIO3) == LOW) {
            if (millis() - start > 10000) {
                return true;  // 10 second hold triggers calibration
            }
            delay(10);
        }
    }

    return false;
}

bool ButtonCalibration::runCalibrationWizard() {
    SUMI_LOGLN("[BTN_CAL] Starting calibration wizard");

    // Show intro screen
    showScreen("Button Calibration",
               "This wizard will help calibrate",
               "your button values.",
               "Press any button to continue...");
    waitForAnyButton();

    // Calibrate each button
    ButtonCalibrationData newData;
    newData.threshold = BTN_THRESHOLD;

    // RIGHT button
    if (!calibrateButton("RIGHT", "Press the RIGHT button",
                        BTN_GPIO1, newData.rightVal)) {
        return false;
    }

    // LEFT button
    if (!calibrateButton("LEFT", "Press the LEFT button",
                        BTN_GPIO1, newData.leftVal)) {
        return false;
    }

    // CONFIRM button
    if (!calibrateButton("CONFIRM", "Press the CONFIRM button",
                        BTN_GPIO1, newData.confirmVal)) {
        return false;
    }

    // BACK button
    if (!calibrateButton("BACK", "Press the BACK button",
                        BTN_GPIO1, newData.backVal)) {
        return false;
    }

    // UP button
    if (!calibrateButton("UP", "Press the UP button",
                        BTN_GPIO2, newData.upVal)) {
        return false;
    }

    // DOWN button
    if (!calibrateButton("DOWN", "Press the DOWN button",
                        BTN_GPIO2, newData.downVal)) {
        return false;
    }

    // Calculate optimal threshold
    newData.threshold = calculateThreshold(newData);

    // Show results
    showCalibrationResults(newData);

    // Ask to save
    showScreen("Save Calibration?",
               "Press CONFIRM to save",
               "Press BACK to cancel",
               "");

    if (waitForConfirmOrBack()) {
        _data = newData;
        save();
        showScreen("Calibration Saved!",
                   "Your button values have been saved.",
                   "Device will reboot...",
                   "");
        delay(2000);
        ESP.restart();
        return true;
    } else {
        showScreen("Calibration Cancelled",
                   "Using previous values.",
                   "",
                   "");
        delay(1500);
        return false;
    }
}

Button ButtonCalibration::readButton() const {
    int adc1 = analogRead(BTN_GPIO1);
    int adc2 = analogRead(BTN_GPIO2);
    int gpio3 = digitalRead(BTN_GPIO3);

    // Power button (GPIO3)
    if (gpio3 == LOW) {
        return BTN_POWER;
    }

    // GPIO2 (ADC2) handles UP/DOWN
    if (adc2 < _data.threshold) {
        return BTN_DOWN;
    }
    if (abs(adc2 - _data.upVal) < _data.threshold) {
        return BTN_UP;
    }

    // GPIO1 (ADC1) handles LEFT/RIGHT/CONFIRM/BACK
    if (adc1 < _data.threshold) {
        return BTN_RIGHT;
    }
    if (abs(adc1 - _data.leftVal) < _data.threshold) {
        return BTN_LEFT;
    }
    if (abs(adc1 - _data.confirmVal) < _data.threshold) {
        return BTN_CONFIRM;
    }
    if (abs(adc1 - _data.backVal) < _data.threshold) {
        return BTN_BACK;
    }

    return BTN_NONE;
}

void ButtonCalibration::printCalibration() const {
    SUMI_LOGLN("[BTN_CAL] Current calibration:");
    SUMI_LOGF("  Threshold: %d\n", _data.threshold);
    SUMI_LOGF("  RIGHT:   %d\n", _data.rightVal);
    SUMI_LOGF("  LEFT:    %d\n", _data.leftVal);
    SUMI_LOGF("  CONFIRM: %d\n", _data.confirmVal);
    SUMI_LOGF("  BACK:    %d\n", _data.backVal);
    SUMI_LOGF("  UP:      %d\n", _data.upVal);
    SUMI_LOGF("  DOWN:    %d\n", _data.downVal);
}

// =============================================================================
// Private Methods
// =============================================================================

void ButtonCalibration::showScreen(const char* title, const char* line1,
                                   const char* line2, const char* line3) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);

        int y = 60;
        int16_t tx, ty;
        uint16_t tw, th;

        // Title
        display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((display.width() - tw) / 2, y);
        display.print(title);

        // Lines
        display.setFont(&FreeSans9pt7b);
        y += 50;

        if (line1[0]) {
            display.getTextBounds(line1, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor((display.width() - tw) / 2, y);
            display.print(line1);
            y += 30;
        }

        if (line2[0]) {
            display.getTextBounds(line2, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor((display.width() - tw) / 2, y);
            display.print(line2);
            y += 30;
        }

        if (line3[0]) {
            display.getTextBounds(line3, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor((display.width() - tw) / 2, y);
            display.print(line3);
        }
    } while (display.nextPage());
}

void ButtonCalibration::waitForAnyButton() {
    // Wait for release first
    while (readButton() != BTN_NONE) {
        delay(20);
    }
    // Wait for press
    while (readButton() == BTN_NONE) {
        delay(20);
    }
    // Wait for release
    while (readButton() != BTN_NONE) {
        delay(20);
    }
}

bool ButtonCalibration::waitForConfirmOrBack() {
    while (true) {
        Button btn = readButton();
        if (btn == BTN_CONFIRM) {
            while (readButton() != BTN_NONE) delay(20);
            return true;
        }
        if (btn == BTN_BACK) {
            while (readButton() != BTN_NONE) delay(20);
            return false;
        }
        delay(20);
    }
}

bool ButtonCalibration::calibrateButton(const char* name, const char* instruction,
                                        int gpioPin, uint16_t& outValue) {
    char line3[64];

    // Wait for button release first
    while (analogRead(gpioPin) < 3000) {
        delay(20);
    }

    showScreen(name, instruction, "Hold the button steady...", "");

    // Wait for button press
    unsigned long startWait = millis();
    while (analogRead(gpioPin) > 3500) {
        if (millis() - startWait > 30000) {
            // Timeout
            showScreen("Timeout", "Button press not detected.",
                      "Calibration cancelled.", "");
            delay(2000);
            return false;
        }
        delay(20);
    }

    // Sample the ADC value multiple times
    int samples[20];
    for (int i = 0; i < 20; i++) {
        samples[i] = analogRead(gpioPin);
        delay(10);
    }

    // Calculate average
    int sum = 0;
    int count = 0;
    for (int i = 0; i < 20; i++) {
        sum += samples[i];
        count++;
    }

    outValue = sum / count;

    snprintf(line3, sizeof(line3), "Recorded value: %d", outValue);
    showScreen(name, "Button recorded!", line3, "");

    // Wait for release
    while (analogRead(gpioPin) < 3500) {
        delay(20);
    }

    delay(500);
    return true;
}

uint16_t ButtonCalibration::calculateThreshold(const ButtonCalibrationData& data) {
    // Find minimum distance between any two button values
    uint16_t vals[] = {data.rightVal, data.leftVal, data.confirmVal,
                      data.backVal, data.upVal, data.downVal};

    int minDist = 9999;
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            int dist = abs(vals[i] - vals[j]);
            if (dist > 0 && dist < minDist) {
                minDist = dist;
            }
        }
    }

    // Threshold should be less than half the minimum distance
    uint16_t threshold = minDist / 3;
    if (threshold < 50) threshold = 50;
    if (threshold > 200) threshold = 200;

    return threshold;
}

void ButtonCalibration::showCalibrationResults(const ButtonCalibrationData& data) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);

        display.setCursor(20, 40);
        display.print("Calibration Results");

        display.setFont(&FreeSans9pt7b);
        int y = 80;

        char buf[64];

        snprintf(buf, sizeof(buf), "Threshold: %d", data.threshold);
        display.setCursor(40, y); display.print(buf); y += 25;

        snprintf(buf, sizeof(buf), "RIGHT: %d", data.rightVal);
        display.setCursor(40, y); display.print(buf); y += 25;

        snprintf(buf, sizeof(buf), "LEFT: %d", data.leftVal);
        display.setCursor(40, y); display.print(buf); y += 25;

        snprintf(buf, sizeof(buf), "CONFIRM: %d", data.confirmVal);
        display.setCursor(40, y); display.print(buf); y += 25;

        snprintf(buf, sizeof(buf), "BACK: %d", data.backVal);
        display.setCursor(40, y); display.print(buf); y += 25;

        snprintf(buf, sizeof(buf), "UP: %d", data.upVal);
        display.setCursor(40, y); display.print(buf); y += 25;

        snprintf(buf, sizeof(buf), "DOWN: %d", data.downVal);
        display.setCursor(40, y); display.print(buf);

    } while (display.nextPage());

    delay(3000);
}
