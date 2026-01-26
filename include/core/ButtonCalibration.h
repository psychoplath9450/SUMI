/**
 * @file ButtonCalibration.h
 * @brief Button ADC calibration system for Sumi firmware
 * @version 1.3.0
 *
 * Allows users to calibrate button ADC values for their specific hardware.
 * Different resistor values on community-built devices may require
 * different thresholds.
 *
 * Usage:
 * 1. Hold POWER button for 10 seconds during boot to enter calibration
 * 2. Follow on-screen instructions to press each button
 * 3. Calibration values are saved to NVS and persist across reboots
 */

#ifndef SUMI_BUTTON_CALIBRATION_H
#define SUMI_BUTTON_CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// Forward declare display
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

/**
 * @brief Calibration data for button ADC values
 */
struct ButtonCalibrationData {
    static constexpr uint32_t MAGIC = 0x42544E43;  // "BTNC"

    uint32_t magic = 0;
    uint16_t threshold = BTN_THRESHOLD;

    // ADC1 values (GPIO1)
    uint16_t rightVal = BTN_RIGHT_VAL;
    uint16_t leftVal = BTN_LEFT_VAL;
    uint16_t confirmVal = BTN_CONFIRM_VAL;
    uint16_t backVal = BTN_BACK_VAL;

    // ADC2 values (GPIO2)
    uint16_t downVal = BTN_DOWN_VAL;
    uint16_t upVal = BTN_UP_VAL;

    bool isValid() const { return magic == MAGIC; }
    void setDefaults();
};

/**
 * @brief Button calibration system
 */
class ButtonCalibration {
public:
    ButtonCalibration();

    /**
     * @brief Load calibration from NVS
     * @return true if valid calibration was loaded
     */
    bool load();

    /**
     * @brief Save calibration to NVS
     * @return true if successful
     */
    bool save();

    /**
     * @brief Clear calibration and revert to defaults
     */
    void clear();

    /**
     * @brief Get calibration data
     */
    const ButtonCalibrationData& getData() const { return _data; }

    /**
     * @brief Check if calibration mode should be entered
     */
    bool shouldEnterCalibration();

    /**
     * @brief Run the calibration wizard
     * @return true if calibration completed successfully
     */
    bool runCalibrationWizard();

    /**
     * @brief Read button using calibrated values
     * @return Button enum value
     */
    Button readButton() const;

    /**
     * @brief Print calibration values to serial
     */
    void printCalibration() const;

private:
    ButtonCalibrationData _data;

    void showScreen(const char* title, const char* line1,
                    const char* line2, const char* line3);
    void waitForAnyButton();
    bool waitForConfirmOrBack();
    bool calibrateButton(const char* name, const char* instruction,
                         int gpioPin, uint16_t& outValue);
    uint16_t calculateThreshold(const ButtonCalibrationData& data);
    void showCalibrationResults(const ButtonCalibrationData& data);
};

// Global instance
extern ButtonCalibration buttonCalibration;

#endif // SUMI_BUTTON_CALIBRATION_H
