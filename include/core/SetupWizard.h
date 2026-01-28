#ifndef SUMI_SETUP_WIZARD_H
#define SUMI_SETUP_WIZARD_H

/**
 * @file SetupWizard.h
 * @brief Setup screens, animations, and deployment flow
 * @version 1.5.0
 */

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"

// =============================================================================
// Setup Wizard Screens
// =============================================================================

// Get current setup step (1-4 based on progress)
int getSetupStep();

// Draw one frame of the ripple animation (for logo area)
void drawRippleFrame(int frame, int logoY, int logoH);

// Start non-blocking ripple animation
void startRippleAnimation();

// Stop ripple animation
void stopRippleAnimation();

// Update ripple animation (call from main loop) - returns true if frame drawn
bool updateRippleAnimation();

// Check if ripple animation is active
bool isRippleAnimationActive();

// Show a single frame of the SUMI logo animation (legacy - kept for compatibility)
void showAnimationFrame(int frame, bool firstFrame);

// Play full deploy animation sequence (shows setup screen, starts ripple)
void playDeployAnimation();

// Show the "device is ready" screen with portal instructions
void showDeployedScreen();

// Animate marching border until button pressed (call after loading complete)
void animateDeployedBorder();

// Show the WiFi setup screen with static ripple logo
void showSetupScreen();

// Show the "connected" confirmation screen
void showConnectedScreen();

#endif // SUMI_SETUP_WIZARD_H
