/**
 * @file SetupWizard.cpp
 * @brief Setup screens and animations implementation
 * @version 1.5.0
 */

#include "core/SetupWizard.h"
#include "core/WiFiManager.h"
#include "core/ButtonInput.h"
#include "core/PowerManager.h"
#include <WiFi.h>

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// External button function (backup declaration)
extern Button readButton();

// External portal access flags from WebServer
extern volatile bool g_portalAccessed;
extern volatile bool g_portalAccessedOnHomeWiFi;

// =============================================================================
// Setup Step Tracking
// =============================================================================
// Step 1: Connect to hotspot (active until AP has client)
// Step 2: Open browser (active when AP has client, until portal accessed)
// Step 3: Add home WiFi (active when portal accessed, until user reconnects to portal on home network)
// Step 4: Deploy (active when user has accessed portal via home network)

int getSetupStep() {
    bool hasAPClient = WiFi.softAPgetStationNum() > 0;
    bool portalAccessed = g_portalAccessed;
    bool portalAccessedOnHomeWiFi = g_portalAccessedOnHomeWiFi;
    
    if (portalAccessedOnHomeWiFi) {
        return 4;  // User reconnected to portal on home network - ready to deploy
    } else if (portalAccessed) {
        return 3;  // Portal accessed on hotspot - waiting for user to reconnect on home network
    } else if (hasAPClient) {
        return 2;  // Client connected to AP - waiting for portal access
    } else {
        return 1;  // Waiting for client to connect to AP
    }
}

// =============================================================================
// Ripple Animation State (for non-blocking animation)
// =============================================================================
static bool rippleAnimationActive = false;
static unsigned long lastRippleFrame = 0;
static int currentRippleFrame = 0;
static int rippleLogoY = 0;
static int rippleLogoH = 0;

// =============================================================================
// Ripple Animation Helper - Draws one frame of the SUMI logo with ripples
// =============================================================================
void drawRippleFrame(int frame, int logoY, int logoH) {
    int w = display.width();
    int centerX = w / 2;
    int centerY = logoY + logoH / 2;
    
    // Calculate max radius to reach corners of logo area
    // Distance from center to corner: sqrt(centerX² + (logoH/2)²)
    int maxRadius = 250;  // Reaches corners of ~275px tall area
    
    // Clear logo area
    display.fillRect(0, logoY, w, logoH, GxEPD_WHITE);
    
    // Draw 3 expanding rings
    for (int ring = 0; ring < 3; ring++) {
        int phase = (frame + ring * 2) % 6;
        int radius = 30 + (phase * (maxRadius - 30)) / 5;
        
        // Draw circle outline (2px thick)
        display.drawCircle(centerX, centerY, radius, GxEPD_BLACK);
        display.drawCircle(centerX, centerY, radius + 1, GxEPD_BLACK);
    }
    
    // Draw SUMI text on top
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(2);
    display.setTextColor(GxEPD_BLACK);
    
    int16_t x1, y1;
    uint16_t tw, th;
    display.getTextBounds("SUMI", 0, 0, &x1, &y1, &tw, &th);
    display.setCursor(centerX - tw/2, centerY + th/3);
    display.print("SUMI");
    display.setTextSize(1);
}

// =============================================================================
// Start ripple animation (non-blocking)
// =============================================================================
void startRippleAnimation() {
    int h = display.height();
    int w = display.width();
    
    // Calculate logo area (same as showSetupScreen)
    int cardHeight = 105;
    int cardSpacing = 8;
    int statusHeight = 65;
    int statusY = h - statusHeight - 6;
    int totalCardsHeight = (cardHeight * 4) + (cardSpacing * 3);
    int cardsStartY = statusY - totalCardsHeight - 10;
    
    rippleLogoY = 0;
    rippleLogoH = cardsStartY;
    rippleAnimationActive = true;
    currentRippleFrame = 0;
    lastRippleFrame = millis();
    
    // Set up partial window for animation
    display.setPartialWindow(0, rippleLogoY, w, rippleLogoH);
}

// =============================================================================
// Stop ripple animation
// =============================================================================
void stopRippleAnimation() {
    rippleAnimationActive = false;
    display.setFullWindow();
}

// =============================================================================
// Update ripple animation (call from main loop)
// Returns true if a frame was drawn
// =============================================================================
bool updateRippleAnimation() {
    if (!rippleAnimationActive) return false;
    
    unsigned long now = millis();
    if (now - lastRippleFrame < 500) return false;  // 2Hz = 500ms
    
    lastRippleFrame = now;
    
    display.firstPage();
    do {
        drawRippleFrame(currentRippleFrame, rippleLogoY, rippleLogoH);
    } while (display.nextPage());
    
    currentRippleFrame = (currentRippleFrame + 1) % 6;
    return true;
}

// =============================================================================
// Check if ripple animation is running
// =============================================================================
bool isRippleAnimationActive() {
    return rippleAnimationActive;
}

// =============================================================================
// Deploy Animation - Shows full setup screen, then starts ripple
// =============================================================================
void playDeployAnimation() {
    display.setRotation(3);
    
    // First: render the full static setup screen
    showSetupScreen();
    
    // Then: start the non-blocking ripple animation
    startRippleAnimation();
}

// =============================================================================
// Animation Frame (legacy - kept for compatibility)
// =============================================================================
void showAnimationFrame(int frame, bool firstFrame) {
    // No longer used - playDeployAnimation handles everything
}

// =============================================================================
// Deployed Screen (device is ready)
// =============================================================================
// Draw static deploy complete screen (called first, returns immediately)
void showDeployedScreenStatic() {
    int w = display.width();
    int h = display.height();
    int centerX = w / 2;
    int margin = 30;
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        int y = 90;
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(2);
        display.setTextColor(GxEPD_BLACK);
        int16_t x1, y1;
        uint16_t tw, th;
        display.getTextBounds("SUMI", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("SUMI");
        display.setTextSize(1);
        y += 45;
        
        display.setFont(&FreeSansBold9pt7b);
        display.getTextBounds("INK, SIMPLIFIED", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("INK, SIMPLIFIED");
        y += 70;
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        display.getTextBounds("Your device is ready", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("Your device is ready");
        y += 55;
        
        int cardTop = y;
        int cardHeight = 310;
        display.drawRoundRect(margin, cardTop, w - margin*2, cardHeight, 14, GxEPD_BLACK);
        
        y = cardTop + 45;
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(margin + 25, y);
        display.print("ACCESS THE PORTAL ANYTIME");
        y += 55;
        
        const char* steps[] = {
            "Open Settings from home",
            "Select 'Open Portal'",
            "Choose WiFi or Hotspot",
            "Visit sumi.local"
        };
        
        for (int i = 0; i < 4; i++) {
            display.fillCircle(margin + 45, y - 5, 16, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(margin + 39, y + 2);
            display.print(i + 1);
            display.setTextColor(GxEPD_BLACK);
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(margin + 75, y);
            display.print(steps[i]);
            y += 55;
        }
        
        y = h - 75;
        display.drawFastHLine(margin, y, w - margin*2, GxEPD_BLACK);
        y += 45;
        display.setFont(&FreeSansBold12pt7b);
        display.getTextBounds("Press any button to continue", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("Press any button to continue");
        
    } while (display.nextPage());
    
    display.setTextSize(1);
}

// Animate marching border until button pressed (called after loading completes)
void animateDeployedBorder() {
    // Simple approach: just wait for any button press
    // The animation was causing button detection issues
    Serial.println("[DEPLOY] Waiting for button press to continue...");
    
    unsigned long startTime = millis();
    
    while (true) {
        // Poll buttons frequently
        Button btn = readButton();
        if (btn == BTN_POWER) {
            // Power button goes to sleep even during setup
            powerManager.enterDeepSleep();
            return;
        }
        if (btn != BTN_NONE) {
            Serial.printf("[DEPLOY] Button %d pressed - exiting\n", btn);
            delay(200);  // Debounce
            break;
        }
        
        // Also check raw GPIO for any button
        // Give user feedback that we're waiting
        if ((millis() - startTime) > 10000) {
            // After 10 seconds, just continue anyway
            Serial.println("[DEPLOY] Timeout - continuing automatically");
            break;
        }
        
        delay(50);  // Poll every 50ms
    }
    
    // Clear screen before returning
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
}

// Combined function for backwards compatibility
void showDeployedScreen() {
    showDeployedScreenStatic();
}

// =============================================================================
// Setup Screen (static version for returning to setup)
// =============================================================================
void showSetupScreen() {
    int w = display.width();
    int h = display.height();
    int centerX = w / 2;
    int margin = 20;
    
    // Check current connection state
    bool wifiConnected = wifiManager.isConnected();
    String connectedIP = wifiConnected ? wifiManager.getIP() : "";
    String connectedSSID = wifiConnected ? wifiManager.getSSID() : "";
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Calculate where cards start (from bottom up)
        int cardWidth = w - margin * 2;
        int cardHeight = 105;
        int cardSpacing = 8;
        int statusHeight = 65;
        int statusY = h - statusHeight - 6;
        int totalCardsHeight = (cardHeight * 4) + (cardSpacing * 3);
        int cardsStartY = statusY - totalCardsHeight - 10;
        
        // Center logo in the space above cards
        int logoCenter = cardsStartY / 2;
        int maxRippleRadius = 250;  // Match animation - reaches corners of logo area
        
        // Draw static ripple rings - 3 rings
        for (int ring = 0; ring < 3; ring++) {
            int radius = 30 + ring * (maxRippleRadius - 30) / 2;
            display.drawCircle(centerX, logoCenter, radius, GxEPD_BLACK);
            display.drawCircle(centerX, logoCenter, radius + 1, GxEPD_BLACK);
        }
        
        // Title - SUMI with white background box (no SETUP subtitle)
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(2);
        display.setTextColor(GxEPD_BLACK);
        int16_t x1, y1;
        uint16_t tw, th;
        display.getTextBounds("SUMI", 0, 0, &x1, &y1, &tw, &th);
        
        // White box behind text
        int padding = 10;
        display.fillRect(centerX - tw/2 - padding, logoCenter - th/2 - padding, 
                        tw + padding*2, th + padding*2, GxEPD_WHITE);
        
        display.setCursor(centerX - tw/2, logoCenter + th/3);
        display.print("SUMI");
        display.setTextSize(1);
        
        // Clear card area to prevent ring bleed-through
        display.fillRect(0, cardsStartY, w, h - cardsStartY, GxEPD_WHITE);
        
        // Get current setup step
        int currentStep = getSetupStep();
        
        // Cards positioned from bottom up
        int y = cardsStartY;
        
        // Helper function-like drawing for each card
        // Card 1: Connect to hotspot
        bool card1Active = (currentStep == 1);
        if (card1Active) {
            display.fillRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        } else {
            display.drawRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        }
        
        // Number circle
        if (card1Active) {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
        } else {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        }
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        display.setCursor(margin + 32, y + cardHeight/2 + 8);
        display.print("1");
        
        // Text
        display.setTextColor(card1Active ? GxEPD_WHITE : GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 35);
        display.print("Connect to WiFi:");
        
        String apName = wifiManager.getAPName();
        display.setCursor(margin + 80, y + 65);
        display.print(apName);
        
        display.setCursor(margin + 80, y + 95);
        display.print("(no password)");
        
        y += cardHeight + cardSpacing;
        
        // Card 2: Open browser
        bool card2Active = (currentStep == 2);
        if (card2Active) {
            display.fillRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        } else {
            display.drawRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        }
        
        // Number circle
        if (card2Active) {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
        } else {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        }
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 31, y + cardHeight/2 + 8);
        display.print("2");
        
        // Text
        display.setTextColor(card2Active ? GxEPD_WHITE : GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 35);
        display.print("Open browser to:");
        
        display.setCursor(margin + 80, y + 65);
        display.print("sumi.local");
        
        display.setCursor(margin + 80, y + 95);
        display.print("or 192.168.4.1");
        
        y += cardHeight + cardSpacing;
        
        // Card 3: Add home WiFi
        bool card3Active = (currentStep == 3);
        if (card3Active) {
            display.fillRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        } else {
            display.drawRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        }
        
        // Number circle
        if (card3Active) {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
        } else {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        }
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 31, y + cardHeight/2 + 8);
        display.print("3");
        
        // Text
        display.setTextColor(card3Active ? GxEPD_WHITE : GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 35);
        display.print("Add your home WiFi");
        
        display.setCursor(margin + 80, y + 65);
        display.print("Then switch back");
        
        display.setCursor(margin + 80, y + 95);
        display.print("to the same network");
        
        y += cardHeight + cardSpacing;
        
        // Card 4: Configure & Deploy
        bool card4Active = (currentStep == 4);
        if (card4Active) {
            display.fillRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        } else {
            display.drawRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        }
        
        // Number circle
        if (card4Active) {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
        } else {
            display.fillCircle(margin + 40, y + cardHeight/2, 24, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        }
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 31, y + cardHeight/2 + 8);
        display.print("4");
        
        // Text
        display.setTextColor(card4Active ? GxEPD_WHITE : GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 35);
        display.print("Upload books, pick apps");
        
        display.setCursor(margin + 80, y + 65);
        display.print("Process once, read forever");
        
        display.setCursor(margin + 80, y + 95);
        display.print("Then hit Deploy!");
        
        // Status bar at bottom showing connection mode
        if (wifiConnected && currentStep == 4) {
            // Both SUMI and user on home network - ready to deploy
            display.fillRoundRect(margin, statusY, cardWidth, statusHeight, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(margin + 15, statusY + 28);
            display.print("CONNECTED: ");
            display.print(connectedSSID);
            
            display.setCursor(margin + 15, statusY + 55);
            display.print("Ready! Visit ");
            display.print(connectedIP);
        } else if (wifiConnected) {
            // SUMI connected but user still on hotspot (step 3)
            display.drawRoundRect(margin, statusY, cardWidth, statusHeight, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(margin + 15, statusY + 28);
            display.print("SUMI on: ");
            display.print(connectedSSID);
            
            display.setCursor(margin + 15, statusY + 55);
            display.print("Switch to same WiFi");
        } else {
            // Hotspot mode - show outlined box
            display.drawRoundRect(margin, statusY, cardWidth, statusHeight, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(margin + 15, statusY + 28);
            display.print("MODE: Hotspot");
            
            display.setCursor(margin + 15, statusY + 55);
            display.print("Waiting for WiFi setup...");
        }
        
    } while (display.nextPage());
    
    display.setTextSize(1);
}

// =============================================================================
// Connected Screen
// =============================================================================
void showConnectedScreen() {
    int w = display.width();
    int h = display.height();
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        int16_t x1, y1;
        uint16_t tw, th;
        display.getTextBounds("Connected!", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(w/2 - tw/2, h/2);
        display.print("Connected!");
        
    } while (display.nextPage());
}
