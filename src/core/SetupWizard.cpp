/**
 * @file SetupWizard.cpp
 * @brief Setup screens and animations implementation
 * @version 2.1.16
 */

#include "core/SetupWizard.h"
#include "core/WiFiManager.h"

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// Deploy Animation - Sequential appearance, no movement
// =============================================================================
void playDeployAnimation() {
    display.setRotation(3);
    
    int w = display.width();
    int h = display.height();
    int centerX = w / 2;
    int margin = 30;
    
    int16_t x1, y1;
    uint16_t tw, th;
    
    // Frame 1: Clear screen, show S
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(3);
        display.setTextColor(GxEPD_BLACK);
        display.getTextBounds("S", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, 120);
        display.print("S");
    } while (display.nextPage());
    delay(150);
    
    // Frame 2-4: SU, SUM, SUMI
    const char* letters[] = {"SU", "SUM", "SUMI"};
    for (int i = 0; i < 3; i++) {
        display.setPartialWindow(0, 40, w, 120);
        display.firstPage();
        do {
            display.fillRect(0, 40, w, 120, GxEPD_WHITE);
            display.setFont(&FreeSansBold12pt7b);
            display.setTextSize(3);
            display.setTextColor(GxEPD_BLACK);
            display.getTextBounds(letters[i], 0, 0, &x1, &y1, &tw, &th);
            display.setCursor(centerX - tw/2, 120);
            display.print(letters[i]);
        } while (display.nextPage());
        delay(120);
    }
    delay(200);
    
    // Frame 5: INK,
    display.setPartialWindow(0, 140, w, 50);
    display.firstPage();
    do {
        display.fillRect(0, 140, w, 50, GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        display.getTextBounds("INK,", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2 - 40, 170);
        display.print("INK,");
    } while (display.nextPage());
    delay(300);
    
    // Frame 6: INK, SIMPLIFIED
    display.setPartialWindow(0, 140, w, 50);
    display.firstPage();
    do {
        display.fillRect(0, 140, w, 50, GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        display.getTextBounds("INK, SIMPLIFIED", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, 170);
        display.print("INK, SIMPLIFIED");
    } while (display.nextPage());
    delay(400);
    
    // Frame 7: WiFi card appears
    int y = 205;
    int cardHeight = 130;
    display.setPartialWindow(0, 200, w, cardHeight + 10);
    display.firstPage();
    do {
        display.fillRect(0, 200, w, cardHeight + 10, GxEPD_WHITE);
        display.fillRoundRect(margin, y, w - margin*2, cardHeight, 14, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(margin + 25, y + 45);
        display.print("CONNECT TO WIFI");
        
        display.setFont(&FreeSansBold12pt7b);
        String apName = wifiManager.getAPName();
        display.setCursor(margin + 25, y + 95);
        display.print(apName);
    } while (display.nextPage());
    delay(250);
    
    // Frame 8: Browser card appears
    y += cardHeight + 25;
    cardHeight = 170;
    display.setPartialWindow(0, y - 5, w, cardHeight + 10);
    display.firstPage();
    do {
        display.fillRect(0, y - 5, w, cardHeight + 10, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.drawRoundRect(margin, y, w - margin*2, cardHeight, 14, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(margin + 25, y + 45);
        display.print("OPEN IN BROWSER");
        
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(margin + 25, y + 95);
        display.print("sumi.local");
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(margin + 25, y + 135);
        display.print("or 192.168.4.1");
    } while (display.nextPage());
    delay(250);
    
    // Frame 9: No password required
    int footerY = h - 75;
    display.setPartialWindow(0, footerY - 10, w, 50);
    display.firstPage();
    do {
        display.fillRect(0, footerY - 10, w, 50, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.getTextBounds("No password required", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, footerY);
        display.print("No password required");
    } while (display.nextPage());
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
            "Select Portal",
            "Select Start Portal",
            "Connect & visit sumi.local"
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
        
        int cardHeight = 130;
        display.fillRoundRect(margin, y, w - margin*2, cardHeight, 14, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(margin + 25, y + 45);
        display.print("CONNECT TO WIFI");
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        String apName = wifiManager.getAPName();
        display.setCursor(margin + 25, y + 95);
        display.print(apName);
        
        y += cardHeight + 25;
        
        cardHeight = 170;
        display.drawRoundRect(margin, y, w - margin*2, cardHeight, 14, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(margin + 25, y + 45);
        display.print("OPEN IN BROWSER");
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        display.setCursor(margin + 25, y + 95);
        display.print("sumi.local");
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(margin + 25, y + 135);
        display.print("or 192.168.4.1");
        
        y += cardHeight + 45;
        
        display.setFont(&FreeSansBold12pt7b);
        display.getTextBounds("No password required", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(centerX - tw/2, y);
        display.print("No password required");
        
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
