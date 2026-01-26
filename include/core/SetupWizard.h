#ifndef SUMI_SETUP_WIZARD_H
#define SUMI_SETUP_WIZARD_H

/**
 * @file SetupWizard.h
 * @brief Setup screens, animations, and deployment flow
 * @version 1.3.0
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"

// =============================================================================
// Setup Wizard Screens
// =============================================================================

// Show a single frame of the SUMI logo animation (frame 0-4)
void showAnimationFrame(int frame, bool firstFrame);

// Play full deploy animation sequence
void playDeployAnimation();

// Show the "device is ready" screen with portal instructions
void showDeployedScreen();

// Show the WiFi setup screen (connect to AP, visit sumi.local)
void showSetupScreen();

// Show the "connected" confirmation screen
void showConnectedScreen();

#endif // SUMI_SETUP_WIZARD_H
