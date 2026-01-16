/**
 * @file PowerManager.cpp
 * @brief Power management implementation
 * @version 2.1.29
 * 
 * Fixed: Added BMP loading support for sleep screen images
 */

#include "core/PowerManager.h"
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <SD.h>
#include <TJpg_Decoder.h>
#include "core/BatteryMonitor.h"
#include "core/WiFiManager.h"
#include "core/SettingsManager.h"

#if FEATURE_WEBSERVER
#include "core/WebServer.h"
#endif

// External references
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern bool portal_active;
extern SettingsManager settingsManager;

// Global instance
PowerManager powerManager;

// Folder paths
#define SLEEP_IMAGE_FOLDER "/images"
#define COVER_CACHE_FOLDER "/.sumi/covers"

// JPEG cover rendering for sleep screen
static int _sleepCoverOffsetX = 0;
static int _sleepCoverOffsetY = 0;

// TJpg_Decoder callback for sleep screen covers
static bool _sleepJpgCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    int drawX = x + _sleepCoverOffsetX;
    int drawY = y + _sleepCoverOffsetY;
    
    // Draw with ordered dithering
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = drawX + i;
            int py = drawY + j;
            
            uint16_t color = bitmap[j * w + i];
            uint8_t r = ((color >> 11) & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t b = (color & 0x1F) << 3;
            
            int gray = (r * 77 + g * 150 + b * 29) >> 8;
            
            // 2x2 Bayer dithering
            int threshold[4] = {64, 192, 240, 128};
            int ditherIdx = ((px & 1) + (py & 1) * 2);
            
            display.drawPixel(px, py, (gray > threshold[ditherIdx]) ? GxEPD_WHITE : GxEPD_BLACK);
        }
    }
    return true;
}

// =============================================================================
// Sleep Screen Helpers
// =============================================================================

// Draw the "wake me up..." sleep screen
void drawWakeMeUp(int w, int h) {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // Draw "wake me up..." centered
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(2);
    
    const char* text = "wake me up...";
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
    
    int x = (w - tw) / 2;
    int y = (h + th) / 2;
    
    display.setCursor(x, y);
    display.print(text);
    
    display.setTextSize(1);
}

// Load and display a BMP file for sleep screen
bool displaySleepBMP(const char* path, int screenW, int screenH) {
    File f = SD.open(path);
    if (!f) {
        Serial.println("[POWER] Failed to open BMP file");
        return false;
    }
    
    uint8_t header[54];
    if (f.read(header, 54) != 54) {
        f.close();
        Serial.println("[POWER] Invalid BMP header");
        return false;
    }
    
    if (header[0] != 'B' || header[1] != 'M') {
        f.close();
        Serial.println("[POWER] Not a BMP file");
        return false;
    }
    
    int32_t width = *(int32_t*)&header[18];
    int32_t height = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];
    
    Serial.printf("[POWER] BMP: %dx%d, %d bpp\n", width, height, bpp);
    
    if (bpp != 1 && bpp != 24) {
        f.close();
        Serial.printf("[POWER] Unsupported BMP depth: %d bpp\n", bpp);
        return false;
    }
    
    // Center image on screen
    int offsetX = (screenW - abs(width)) / 2;
    int offsetY = (screenH - abs(height)) / 2;
    if (offsetX < 0) offsetX = 0;
    if (offsetY < 0) offsetY = 0;
    
    display.fillScreen(GxEPD_WHITE);
    
    f.seek(dataOffset);
    
    bool flipV = (height > 0);
    height = abs(height);
    width = abs(width);
    
    int maxH = min((int)height, screenH);
    int maxW = min((int)width, screenW);
    
    if (bpp == 1) {
        int rowBytes = ((width + 31) / 32) * 4;
        uint8_t* row = (uint8_t*)malloc(rowBytes);
        
        if (row) {
            for (int y = 0; y < maxH; y++) {
                int srcY = flipV ? (height - 1 - y) : y;
                f.seek(dataOffset + srcY * rowBytes);
                f.read(row, rowBytes);
                
                for (int x = 0; x < maxW; x++) {
                    int byteIdx = x / 8;
                    int bitIdx = 7 - (x % 8);
                    bool pixel = (row[byteIdx] >> bitIdx) & 1;
                    display.drawPixel(offsetX + x, offsetY + y, pixel ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    } else if (bpp == 24) {
        int rowBytes = ((width * 3 + 3) / 4) * 4;
        uint8_t* row = (uint8_t*)malloc(rowBytes);
        
        if (row) {
            for (int y = 0; y < maxH; y++) {
                int srcY = flipV ? (height - 1 - y) : y;
                f.seek(dataOffset + srcY * rowBytes);
                f.read(row, rowBytes);
                
                for (int x = 0; x < maxW; x++) {
                    int idx = x * 3;
                    // Grayscale conversion with threshold
                    int gray = (row[idx] + row[idx+1] + row[idx+2]) / 3;
                    display.drawPixel(offsetX + x, offsetY + y, gray > 128 ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    }
    
    f.close();
    return true;
}

// Try to load and display a random image from the sleep folder
bool displayRandomSleepImage(int w, int h) {
    File dir = SD.open(SLEEP_IMAGE_FOLDER);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[POWER] No sleep images folder found");
        return false;
    }
    
    // Count valid image files
    int imageCount = 0;
    File file = dir.openNextFile();
    while (file) {
        String name = String(file.name());
        name.toLowerCase();
        if (name.endsWith(".raw") || name.endsWith(".bmp")) {
            imageCount++;
        }
        file = dir.openNextFile();
    }
    dir.close();
    
    if (imageCount == 0) {
        Serial.println("[POWER] No images in sleep folder");
        return false;
    }
    
    // Pick a random image
    int targetIdx = random(0, imageCount);
    Serial.printf("[POWER] Picking image %d of %d\n", targetIdx + 1, imageCount);
    
    // Find and load that image
    dir = SD.open(SLEEP_IMAGE_FOLDER);
    int currentIdx = 0;
    file = dir.openNextFile();
    while (file) {
        String name = String(file.name());
        String nameLower = name;
        nameLower.toLowerCase();
        
        if (nameLower.endsWith(".raw") || nameLower.endsWith(".bmp")) {
            if (currentIdx == targetIdx) {
                // Found our image
                char fullPath[64];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", SLEEP_IMAGE_FOLDER, file.name());
                Serial.printf("[POWER] Loading sleep image: %s\n", fullPath);
                file.close();
                dir.close();
                
                // Load and display the image based on extension
                if (nameLower.endsWith(".bmp")) {
                    return displaySleepBMP(fullPath, w, h);
                }
                
                // For .raw files: use actual display dimensions (orientation-aware)
                // Files should be created to match display: w x h, 1-bit per pixel
                File imgFile = SD.open(fullPath);
                if (!imgFile) {
                    Serial.println("[POWER] Failed to open image file");
                    return false;
                }
                
                size_t fileSize = imgFile.size();
                size_t expectedSize = (w * h) / 8;  // 1 bit per pixel
                int bytesPerRow = w / 8;
                
                Serial.printf("[POWER] Display: %dx%d, expected=%d bytes, file=%d bytes\n", 
                              w, h, expectedSize, fileSize);
                
                if (fileSize >= expectedSize) {
                    display.fillScreen(GxEPD_WHITE);
                    
                    // Read and draw row by row using actual display width
                    uint8_t rowBuf[100];  // Max 800 pixels / 8 = 100 bytes
                    for (int y = 0; y < h; y++) {
                        imgFile.read(rowBuf, bytesPerRow);
                        for (int x = 0; x < w; x++) {
                            int byteIdx = x / 8;
                            int bitIdx = 7 - (x % 8);
                            if (rowBuf[byteIdx] & (1 << bitIdx)) {
                                display.drawPixel(x, y, GxEPD_BLACK);
                            }
                        }
                    }
                    imgFile.close();
                    return true;
                } else {
                    Serial.println("[POWER] File size mismatch - wrong orientation?");
                }
                
                imgFile.close();
                return false;
            }
            currentIdx++;
        }
        file = dir.openNextFile();
    }
    dir.close();
    return false;
}

// Try to display a random book cover from the cache
bool displayRandomBookCover(int w, int h) {
    File dir = SD.open(COVER_CACHE_FOLDER);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[POWER] No covers folder found");
        return false;
    }
    
    // Count cover files (.raw are actually JPEG)
    int coverCount = 0;
    File file = dir.openNextFile();
    while (file) {
        String name = String(file.name());
        name.toLowerCase();
        if (name.endsWith(".raw")) {
            coverCount++;
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();
    
    if (coverCount == 0) {
        Serial.println("[POWER] No cached covers found");
        return false;
    }
    
    // Pick a random cover
    int targetIdx = random(0, coverCount);
    Serial.printf("[POWER] Picking cover %d of %d\n", targetIdx + 1, coverCount);
    
    // Find that cover
    dir = SD.open(COVER_CACHE_FOLDER);
    int currentIdx = 0;
    file = dir.openNextFile();
    char coverPath[96] = {0};
    
    while (file) {
        String name = String(file.name());
        String nameLower = name;
        nameLower.toLowerCase();
        
        if (nameLower.endsWith(".raw")) {
            if (currentIdx == targetIdx) {
                snprintf(coverPath, sizeof(coverPath), "%s/%s", COVER_CACHE_FOLDER, file.name());
                file.close();
                break;
            }
            currentIdx++;
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();
    
    if (strlen(coverPath) == 0) {
        Serial.println("[POWER] Cover not found");
        return false;
    }
    
    Serial.printf("[POWER] Loading cover: %s\n", coverPath);
    
    // Get JPEG dimensions
    uint16_t jpgW = 0, jpgH = 0;
    TJpgDec.setCallback(_sleepJpgCallback);
    JRESULT dimResult = TJpgDec.getFsJpgSize(&jpgW, &jpgH, coverPath, SD);
    
    if (dimResult != JDR_OK || jpgW == 0 || jpgH == 0) {
        Serial.printf("[POWER] Failed to get JPEG dimensions: %d\n", dimResult);
        return false;
    }
    
    Serial.printf("[POWER] Cover size: %dx%d\n", jpgW, jpgH);
    
    // Calculate scale to fit screen (leave some margin)
    int maxW = w - 40;
    int maxH = h - 80;
    int scale = 1;
    
    if (jpgW > maxW * 2 || jpgH > maxH * 2) scale = 4;
    else if (jpgW > maxW || jpgH > maxH) scale = 2;
    
    TJpgDec.setJpgScale(scale);
    
    int scaledW = jpgW / scale;
    int scaledH = jpgH / scale;
    
    // Center the cover
    _sleepCoverOffsetX = (w - scaledW) / 2;
    _sleepCoverOffsetY = (h - scaledH) / 2 - 20;
    if (_sleepCoverOffsetX < 0) _sleepCoverOffsetX = 0;
    if (_sleepCoverOffsetY < 0) _sleepCoverOffsetY = 0;
    
    // Clear screen and draw
    display.fillScreen(GxEPD_WHITE);
    
    // Draw border
    display.drawRect(_sleepCoverOffsetX - 3, _sleepCoverOffsetY - 3, 
                     scaledW + 6, scaledH + 6, GxEPD_BLACK);
    display.drawRect(_sleepCoverOffsetX - 2, _sleepCoverOffsetY - 2,
                     scaledW + 4, scaledH + 4, GxEPD_BLACK);
    
    // Decode and draw cover
    JRESULT result = TJpgDec.drawFsJpg(0, 0, coverPath, SD);
    
    if (result != JDR_OK) {
        Serial.printf("[POWER] JPEG decode failed: %d\n", result);
        return false;
    }
    
    // Add "SUMI" text below
    display.setFont(&FreeSansBold9pt7b);
    display.setTextSize(1);
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds("SUMI", 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((w - tw) / 2, _sleepCoverOffsetY + scaledH + 35);
    display.print("SUMI");
    
    return true;
}

// =============================================================================
// Sleep Functions
// =============================================================================
void PowerManager::enterDeepSleep() {
    Serial.println("[POWER] Entering deep sleep...");
    
    int w = display.width();
    int h = display.height();
    
    // Get sleep style setting: 0=Book Cover, 1=Shuffle Images, 2=Wake Me Up
    uint8_t sleepStyle = settingsManager.display.sleepStyle;
    Serial.printf("[POWER] Sleep style: %d\n", sleepStyle);
    
    display.setFullWindow();
    display.firstPage();
    do {
        bool drawn = false;
        
        switch (sleepStyle) {
            case 0:  // Book Cover (current/most recent)
                drawn = displayRandomBookCover(w, h);
                break;
            case 1:  // Shuffle Images
                drawn = displayRandomSleepImage(w, h);
                break;
            case 2:  // Wake Me Up text
            default:
                drawWakeMeUp(w, h);
                drawn = true;
                break;
        }
        
        // Fallback to Wake Me Up if nothing drawn
        if (!drawn) {
            drawWakeMeUp(w, h);
        }
    } while (display.nextPage());
    
    // Power down
    display.powerOff();
    WiFi.mode(WIFI_OFF);
    
    // === FIXED: Use correct ESP32-C3 deep sleep wake configuration ===
    // GPIO3 is the power button - configure it as wake source
    pinMode(BTN_GPIO3, INPUT_PULLUP);
    
    // For ESP32-C3, use gpio_wakeup with deep sleep
    gpio_wakeup_enable((gpio_num_t)BTN_GPIO3, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    
    delay(100);
    esp_deep_sleep_start();
}

void PowerManager::enterLightSleep() {
    // Light sleep for shorter idle periods
    Serial.println("[POWER] Entering light sleep...");
    display.powerOff();
    
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable((gpio_num_t)BTN_GPIO3, GPIO_INTR_LOW_LEVEL);
    
    esp_light_sleep_start();
    
    // Woke up
    Serial.println("[POWER] Woke from light sleep");
    resetActivityTimer();
}

// =============================================================================
// Wake Verification (Boot Loop Prevention)
// =============================================================================
bool PowerManager::verifyWakeupLongPress() {
    pinMode(BTN_GPIO3, INPUT_PULLUP);
    
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_GPIO) {
        return true;  // Not waking from sleep, proceed normally
    }
    
    Serial.println("[POWER] Verifying wake-up long press...");
    
    // Wait for button release or timeout
    unsigned long start = millis();
    while (digitalRead(BTN_GPIO3) == LOW) {
        if (millis() - start >= POWER_BUTTON_WAKEUP_MS) {
            Serial.println("[POWER] Wake-up confirmed!");
            
            // Fast time sync - 3 second connect, skip timezone lookup, brief NTP wait
            if (wifiManager.hasCredentials()) {
                wifiManager.syncTimeFast();
            }
            
            return true;
        }
        delay(10);
    }
    
    // Button released too early - go back to sleep
    Serial.println("[POWER] Wake-up cancelled - returning to sleep");
    enterDeepSleep();
    return false;  // Won't reach here
}

// =============================================================================
// Background Time Sync
// =============================================================================
void PowerManager::syncTimeInBackground() {
    if (!wifiManager.hasCredentials()) {
        Serial.println("[POWER] No WiFi credentials - skipping time sync");
        return;
    }
    
    Serial.println("[POWER] Background time sync...");
    wifiManager.syncTime();
}

// =============================================================================
// Portal Cleanup
// =============================================================================
void PowerManager::cleanupPortalResources() {
    Serial.println();
    Serial.println("[SUMI] ========================================");
    Serial.println("[SUMI]   CLEANING UP PORTAL RESOURCES");
    Serial.println("[SUMI] ========================================");
    Serial.printf("[SUMI] Heap BEFORE cleanup: %d bytes\n", ESP.getFreeHeap());
    
    #if FEATURE_WEBSERVER
    webServer.stop();
    Serial.println("[SUMI] WebServer stopped");
    #endif
    
    // Stop AP mode
    wifiManager.stopAP();
    Serial.println("[SUMI] WiFi AP stopped");
    
    // Disconnect STA if connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        Serial.println("[SUMI] WiFi STA disconnected");
    }
    
    // Turn WiFi completely off
    WiFi.mode(WIFI_OFF);
    Serial.println("[SUMI] WiFi OFF");
    
    portal_active = false;
    delay(100);
    
    Serial.printf("[SUMI] Heap AFTER cleanup: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[SUMI] ========================================");
    Serial.println();
}

// =============================================================================
// Diagnostics
// =============================================================================
void PowerManager::printMemoryReport() {
    Serial.println();
    Serial.println("[MEM] ========== MEMORY REPORT ==========");
    Serial.printf("[MEM] Free Heap:     %6d bytes\n", ESP.getFreeHeap());
    Serial.printf("[MEM] Min Free Heap: %6d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("[MEM] Max Alloc:     %6d bytes\n", ESP.getMaxAllocHeap());
    #if SUMI_LOW_MEMORY
    Serial.println("[MEM] Mode: LOW MEMORY (ESP32-C3)");
    #else
    Serial.println("[MEM] Mode: STANDARD (ESP32/S3)");
    #endif
    Serial.println("[MEM] =====================================");
    Serial.println();
}

void PowerManager::printFeatureFlags() {
    Serial.println();
    Serial.println("[CFG] ========== FEATURE FLAGS ==========");
    Serial.printf("[CFG] FEATURE_READER:     %d", FEATURE_READER);
    #if READER_LITE
    Serial.print(" (LITE)");
    #endif
    Serial.println();
    
    Serial.printf("[CFG] FEATURE_FLASHCARDS: %d", FEATURE_FLASHCARDS);
    Serial.printf(" (max %d cards)\n", FC_MAX_CARDS);
    
    Serial.printf("[CFG] FEATURE_WEATHER:    %d", FEATURE_WEATHER);
    #if WEATHER_LITE
    Serial.print(" (LITE)");
    #endif
    Serial.printf(" (%d days)\n", WEATHER_FORECAST_DAYS);
    
    Serial.printf("[CFG] FEATURE_LOCKSCREEN: %d", FEATURE_LOCKSCREEN);
    #if LOCKSCREEN_LITE
    Serial.print(" (LITE)");
    #endif
    Serial.println();
    
    Serial.printf("[CFG] FEATURE_BLUETOOTH:  %d\n", FEATURE_BLUETOOTH);
    Serial.printf("[CFG] FEATURE_GAMES:      %d\n", FEATURE_GAMES);
    Serial.println("[CFG] =====================================");
    Serial.println();
}
