/**
 * @file SetupWizard.cpp
 * @brief Setup screens and animations implementation
 * @version 1.3.0
 */

#include "core/SetupWizard.h"
#include "core/WiFiManager.h"

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// Deploy Animation - Now just shows setup screen directly
// =============================================================================
void playDeployAnimation() {
    display.setRotation(3);
    // Skip animation, just show the setup screen
    showSetupScreen();
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
void showDeployedScreen() {
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
            "Select Open Portal",
            "Choose: Home WiFi or Hotspot",
            "Visit sumi.local in browser"
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
        
        int y = 50;
        
        // Title
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(2);
        display.setTextColor(GxEPD_BLACK);
        int16_t x1, y1;
        uint16_t tw, th;
        display.getTextBounds("SUMI", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("SUMI");
        display.setTextSize(1);
        y += 28;
        
        display.setFont(&FreeSansBold9pt7b);
        display.getTextBounds("SETUP", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("SETUP");
        y += 25;
        
        // Card dimensions - larger to fill screen
        int cardWidth = w - margin * 2;
        int cardHeight = 115;
        int cardSpacing = 10;
        
        // Card 1: Connect to hotspot (black filled)
        display.fillRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        
        // Number circle (white on black)
        display.fillCircle(margin + 40, y + cardHeight/2, 26, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        display.setCursor(margin + 32, y + cardHeight/2 + 9);
        display.print("1");
        
        // Text - larger fonts
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 38);
        display.print("Connect to WiFi:");
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        String apName = wifiManager.getAPName();
        display.setCursor(margin + 80, y + 70);
        display.print(apName);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 100);
        display.print("(no password)");
        
        y += cardHeight + cardSpacing;
        
        // Card 2: Open browser (outlined)
        display.drawRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        
        // Number circle
        display.fillCircle(margin + 40, y + cardHeight/2, 26, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 31, y + cardHeight/2 + 9);
        display.print("2");
        
        // Text - larger fonts
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 38);
        display.print("Open browser:");
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        display.setCursor(margin + 80, y + 70);
        display.print("sumi.local");
        
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 100);
        display.print("or 192.168.4.1");
        
        y += cardHeight + cardSpacing;
        
        // Card 3: Connect SUMI to home WiFi (outlined)
        display.drawRoundRect(margin, y, cardWidth, cardHeight, 12, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        
        // Number circle
        display.fillCircle(margin + 40, y + cardHeight/2, 26, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 31, y + cardHeight/2 + 9);
        display.print("3");
        
        // Text - larger fonts
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 80, y + 38);
        display.print("Connect SUMI to WiFi");
        
        display.setCursor(margin + 80, y + 70);
        display.print("Then switch your phone");
        
        display.setCursor(margin + 80, y + 100);
        display.print("to the same network");
        
        y += cardHeight + cardSpacing;
        
        // Status bar at bottom showing connection mode
        int statusHeight = 70;
        int statusY = h - statusHeight - 8;
        
        if (wifiConnected) {
            // Connected to home WiFi - show filled box
            display.fillRoundRect(margin, statusY, cardWidth, statusHeight, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(margin + 15, statusY + 30);
            display.print("CONNECTED: ");
            display.print(connectedSSID);
            
            display.setCursor(margin + 15, statusY + 58);
            display.print("Portal: http://");
            display.print(connectedIP);
        } else {
            // Hotspot mode - show outlined box
            display.drawRoundRect(margin, statusY, cardWidth, statusHeight, 10, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(margin + 15, statusY + 30);
            display.print("MODE: Hotspot");
            
            display.setCursor(margin + 15, statusY + 58);
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
