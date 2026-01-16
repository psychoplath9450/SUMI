#ifndef SUMI_BUTTON_INPUT_H
#define SUMI_BUTTON_INPUT_H

/**
 * @file ButtonInput.h
 * @brief Physical button reading and orientation handling
 * @version 2.1.16
 */

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Button Reading Functions
// =============================================================================

// Set orientation for button remapping (landscape swaps directions)
void setButtonOrientation(bool landscape);

// Get human-readable button name for debugging
const char* getButtonName(Button btn);

// Read raw button state (no orientation adjustment)
Button readRawButton();

// Read button with orientation adjustment and Bluetooth injection
Button readButton();

// Wait for button press and release (blocking)
Button waitForButtonPress();

// Bluetooth keyboard injection (if enabled)
#if FEATURE_BLUETOOTH
void injectButton(Button btn);
#endif

#endif // SUMI_BUTTON_INPUT_H
