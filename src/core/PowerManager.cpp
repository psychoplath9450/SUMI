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
#include <time.h>
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
#define LAST_BOOK_PATH "/.sumi/lastbook.bin"

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

// Load and display a BMP file for sleep screen with proper scaling
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
    
    int32_t imgWidth = *(int32_t*)&header[18];
    int32_t imgHeight = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];
    
    bool flipV = (imgHeight > 0);
    imgHeight = abs(imgHeight);
    imgWidth = abs(imgWidth);
    
    Serial.printf("[POWER] BMP: %dx%d, %d bpp, screen: %dx%d\n", imgWidth, imgHeight, bpp, screenW, screenH);
    
    if (bpp != 1 && bpp != 24 && bpp != 8) {
        f.close();
        Serial.printf("[POWER] Unsupported BMP depth: %d bpp\n", bpp);
        return false;
    }
    
    // Calculate scale to FIT the screen (maintain aspect ratio)
    float scaleX = (float)screenW / (float)imgWidth;
    float scaleY = (float)screenH / (float)imgHeight;
    float scale = min(scaleX, scaleY);  // Use smaller scale to fit entirely
    
    int destW = (int)(imgWidth * scale);
    int destH = (int)(imgHeight * scale);
    
    // Center on screen
    int offsetX = (screenW - destW) / 2;
    int offsetY = (screenH - destH) / 2;
    
    Serial.printf("[POWER] Scale: %.2f, dest: %dx%d, offset: %d,%d\n", scale, destW, destH, offsetX, offsetY);
    
    display.fillScreen(GxEPD_WHITE);
    
    f.seek(dataOffset);
    
    // 2x2 Bayer dithering thresholds for grayscale conversion
    const int bayer[4] = {51, 204, 153, 102};
    
    if (bpp == 1) {
        int rowBytes = ((imgWidth + 31) / 32) * 4;
        uint8_t* row = (uint8_t*)malloc(rowBytes);
        
        if (row) {
            for (int destY = 0; destY < destH; destY++) {
                // Map destination Y to source Y
                int srcY = (int)((destY / scale));
                if (srcY >= imgHeight) srcY = imgHeight - 1;
                int fileY = flipV ? (imgHeight - 1 - srcY) : srcY;
                
                f.seek(dataOffset + fileY * rowBytes);
                f.read(row, rowBytes);
                
                for (int destX = 0; destX < destW; destX++) {
                    // Map destination X to source X
                    int srcX = (int)((destX / scale));
                    if (srcX >= imgWidth) srcX = imgWidth - 1;
                    
                    int byteIdx = srcX / 8;
                    int bitIdx = 7 - (srcX % 8);
                    bool pixel = (row[byteIdx] >> bitIdx) & 1;
                    display.drawPixel(offsetX + destX, offsetY + destY, pixel ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    } else if (bpp == 24) {
        int rowBytes = ((imgWidth * 3 + 3) / 4) * 4;
        uint8_t* row = (uint8_t*)malloc(rowBytes);
        
        if (row) {
            for (int destY = 0; destY < destH; destY++) {
                int srcY = (int)((destY / scale));
                if (srcY >= imgHeight) srcY = imgHeight - 1;
                int fileY = flipV ? (imgHeight - 1 - srcY) : srcY;
                
                f.seek(dataOffset + fileY * rowBytes);
                f.read(row, rowBytes);
                
                for (int destX = 0; destX < destW; destX++) {
                    int srcX = (int)((destX / scale));
                    if (srcX >= imgWidth) srcX = imgWidth - 1;
                    
                    int idx = srcX * 3;
                    uint8_t b = row[idx];
                    uint8_t g = row[idx + 1];
                    uint8_t r = row[idx + 2];
                    
                    // Convert to grayscale (luminance formula)
                    int gray = (r * 77 + g * 150 + b * 29) >> 8;
                    
                    // Apply ordered dithering for better quality
                    int ditherIdx = ((destX & 1) + ((destY & 1) << 1));
                    bool pixel = (gray > bayer[ditherIdx]);
                    
                    display.drawPixel(offsetX + destX, offsetY + destY, pixel ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    } else if (bpp == 8) {
        // 8-bit grayscale or indexed
        int rowBytes = ((imgWidth + 3) / 4) * 4;
        uint8_t* row = (uint8_t*)malloc(rowBytes);
        
        if (row) {
            for (int destY = 0; destY < destH; destY++) {
                int srcY = (int)((destY / scale));
                if (srcY >= imgHeight) srcY = imgHeight - 1;
                int fileY = flipV ? (imgHeight - 1 - srcY) : srcY;
                
                f.seek(dataOffset + fileY * rowBytes);
                f.read(row, rowBytes);
                
                for (int destX = 0; destX < destW; destX++) {
                    int srcX = (int)((destX / scale));
                    if (srcX >= imgWidth) srcX = imgWidth - 1;
                    
                    uint8_t gray = row[srcX];
                    
                    // Apply ordered dithering
                    int ditherIdx = ((destX & 1) + ((destY & 1) << 1));
                    bool pixel = (gray > bayer[ditherIdx]);
                    
                    display.drawPixel(offsetX + destX, offsetY + destY, pixel ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    }
    
    f.close();
    Serial.println("[POWER] BMP rendered successfully with scaling");
    return true;
}

// Load and display a JPEG file for sleep screen with proper scaling
bool displaySleepJPEG(const char* path, int screenW, int screenH) {
    Serial.printf("[POWER] Loading JPEG: %s\n", path);
    
    // Get JPEG dimensions
    uint16_t jpgW = 0, jpgH = 0;
    TJpgDec.setCallback(_sleepJpgCallback);
    JRESULT dimResult = TJpgDec.getFsJpgSize(&jpgW, &jpgH, path, SD);
    
    if (dimResult != JDR_OK || jpgW == 0 || jpgH == 0) {
        Serial.printf("[POWER] Failed to get JPEG dimensions: %d\n", dimResult);
        return false;
    }
    
    Serial.printf("[POWER] JPEG size: %dx%d, screen: %dx%d\n", jpgW, jpgH, screenW, screenH);
    
    // Calculate the best integer scale (TJpgDec only supports 1, 2, 4, 8)
    // We want the image to fill as much of the screen as possible
    float scaleX = (float)jpgW / (float)screenW;
    float scaleY = (float)jpgH / (float)screenH;
    float idealScale = max(scaleX, scaleY);  // Scale to fit
    
    // Pick the closest integer scale that doesn't make image too big
    int scale = 1;
    if (idealScale > 6) scale = 8;
    else if (idealScale > 3) scale = 4;
    else if (idealScale > 1.5) scale = 2;
    else scale = 1;
    
    TJpgDec.setJpgScale(scale);
    
    int scaledW = jpgW / scale;
    int scaledH = jpgH / scale;
    
    // Center the image on screen
    _sleepCoverOffsetX = (screenW - scaledW) / 2;
    _sleepCoverOffsetY = (screenH - scaledH) / 2;
    if (_sleepCoverOffsetX < 0) _sleepCoverOffsetX = 0;
    if (_sleepCoverOffsetY < 0) _sleepCoverOffsetY = 0;
    
    Serial.printf("[POWER] Scale: %d, dest: %dx%d, offset: %d,%d\n", 
                  scale, scaledW, scaledH, _sleepCoverOffsetX, _sleepCoverOffsetY);
    
    // Clear screen and draw
    display.fillScreen(GxEPD_WHITE);
    
    // Decode and draw
    JRESULT result = TJpgDec.drawFsJpg(0, 0, path, SD);
    
    if (result != JDR_OK) {
        Serial.printf("[POWER] JPEG decode failed: %d\n", result);
        return false;
    }
    
    Serial.println("[POWER] JPEG rendered successfully");
    return true;
}

// Try to load and display a random image from the sleep folder
bool displayRandomSleepImage(int w, int h) {
    File dir = SD.open(SLEEP_IMAGE_FOLDER);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[POWER] No sleep images folder found");
        return false;
    }
    
    // Count valid image files (now includes JPG)
    int imageCount = 0;
    File file = dir.openNextFile();
    while (file) {
        String name = String(file.name());
        name.toLowerCase();
        if (name.endsWith(".raw") || name.endsWith(".bmp") || 
            name.endsWith(".jpg") || name.endsWith(".jpeg")) {
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
        
        if (nameLower.endsWith(".raw") || nameLower.endsWith(".bmp") ||
            nameLower.endsWith(".jpg") || nameLower.endsWith(".jpeg")) {
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
                
                if (nameLower.endsWith(".jpg") || nameLower.endsWith(".jpeg")) {
                    return displaySleepJPEG(fullPath, w, h);
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
    // First try to display the CURRENT book cover (most recently read)
    if (SD.exists(LAST_BOOK_PATH)) {
        File file = SD.open(LAST_BOOK_PATH, FILE_READ);
        if (file) {
            struct {
                uint32_t magic;
                char path[128];
                char title[64];
                char author[48];
                char coverPath[96];
            } lastBook;
            
            size_t read = file.read((uint8_t*)&lastBook, sizeof(lastBook));
            file.close();
            
            if (read >= sizeof(lastBook) && lastBook.magic == 0x4C415354 && lastBook.coverPath[0] != '\0') {
                if (SD.exists(lastBook.coverPath)) {
                    Serial.printf("[POWER] Using current book cover: %s\n", lastBook.coverPath);
                    
                    // Get JPEG dimensions
                    uint16_t jpgW = 0, jpgH = 0;
                    TJpgDec.setCallback(_sleepJpgCallback);
                    JRESULT dimResult = TJpgDec.getFsJpgSize(&jpgW, &jpgH, lastBook.coverPath, SD);
                    
                    if (dimResult == JDR_OK && jpgW > 0 && jpgH > 0) {
                        Serial.printf("[POWER] Cover size: %dx%d\n", jpgW, jpgH);
                        
                        // Calculate scale to FILL screen (full screen, no margins)
                        int scale = 1;
                        if (jpgW > w * 2 || jpgH > h * 2) scale = 4;
                        else if (jpgW > w || jpgH > h) scale = 2;
                        
                        TJpgDec.setJpgScale(scale);
                        
                        int scaledW = jpgW / scale;
                        int scaledH = jpgH / scale;
                        
                        // Center the cover (full screen)
                        _sleepCoverOffsetX = (w - scaledW) / 2;
                        _sleepCoverOffsetY = (h - scaledH) / 2;
                        if (_sleepCoverOffsetX < 0) _sleepCoverOffsetX = 0;
                        if (_sleepCoverOffsetY < 0) _sleepCoverOffsetY = 0;
                        
                        // Clear screen and draw
                        display.fillScreen(GxEPD_WHITE);
                        
                        // Decode and draw cover
                        JRESULT result = TJpgDec.drawFsJpg(0, 0, lastBook.coverPath, SD);
                        
                        if (result == JDR_OK) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    
    // Fallback: pick a random cover from cache
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
    
    // Calculate scale to FILL screen (full screen, no margins)
    int scale = 1;
    if (jpgW > w * 2 || jpgH > h * 2) scale = 4;
    else if (jpgW > w || jpgH > h) scale = 2;
    
    TJpgDec.setJpgScale(scale);
    
    int scaledW = jpgW / scale;
    int scaledH = jpgH / scale;
    
    // Center the cover (full screen)
    _sleepCoverOffsetX = (w - scaledW) / 2;
    _sleepCoverOffsetY = (h - scaledH) / 2;
    if (_sleepCoverOffsetX < 0) _sleepCoverOffsetX = 0;
    if (_sleepCoverOffsetY < 0) _sleepCoverOffsetY = 0;
    
    // Clear screen and draw
    display.fillScreen(GxEPD_WHITE);
    
    // Decode and draw cover
    JRESULT result = TJpgDec.drawFsJpg(0, 0, coverPath, SD);
    
    if (result != JDR_OK) {
        Serial.printf("[POWER] JPEG decode failed: %d\n", result);
        return false;
    }
    
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
            
            // Request time sync to happen after home screen is drawn
            // This prevents the screen from freezing during WiFi connect
            if (wifiManager.hasCredentials()) {
                _needsTimeSync = true;
                Serial.println("[POWER] Time sync requested (will happen in background)");
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

// Background time sync state
static unsigned long _bgSyncStartTime = 0;
static int _bgSyncState = 0;  // 0=idle, 1=connecting, 2=syncing, 3=done

void PowerManager::startBackgroundTimeSync() {
    if (!_needsTimeSync || _timeSyncInProgress) return;
    if (!wifiManager.hasCredentials()) {
        _needsTimeSync = false;
        return;
    }
    
    Serial.println("[POWER] Starting background time sync...");
    _timeSyncInProgress = true;
    _bgSyncStartTime = millis();
    _bgSyncState = 1;
    
    // Start WiFi connection (non-blocking start)
    WiFi.mode(WIFI_STA);
    WiFi.begin();  // Uses saved credentials
}

void PowerManager::checkBackgroundTimeSync() {
    if (!_timeSyncInProgress) return;
    
    unsigned long elapsed = millis() - _bgSyncStartTime;
    
    switch (_bgSyncState) {
        case 1:  // Connecting
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[POWER] BG sync: WiFi connected");
                
                // Configure NTP (non-blocking)
                int32_t tzOffset = settingsManager.weather.timezoneOffset;
                configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov");
                
                _bgSyncState = 2;
                _bgSyncStartTime = millis();
            } else if (elapsed > 5000) {
                // Timeout - give up
                Serial.println("[POWER] BG sync: WiFi connect timeout");
                _bgSyncState = 3;
            }
            break;
            
        case 2:  // Syncing NTP
            {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {  // Non-blocking check
                    char timeStr[32];
                    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
                    Serial.printf("[POWER] BG sync: Time synced! %s\n", timeStr);
                    _bgSyncState = 3;
                } else if (elapsed > 3000) {
                    Serial.println("[POWER] BG sync: NTP timeout (time may sync later)");
                    _bgSyncState = 3;
                }
            }
            break;
            
        case 3:  // Done - cleanup
            // Disconnect WiFi to save power
            WiFi.disconnect();
            WiFi.mode(WIFI_OFF);
            
            _timeSyncInProgress = false;
            _needsTimeSync = false;
            _bgSyncState = 0;
            Serial.println("[POWER] BG sync: Complete, WiFi off");
            break;
    }
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
// Reading Mode - Suspend/Resume Services
// =============================================================================
void PowerManager::suspendForReading() {
    Serial.println();
    Serial.println("[MEM] ========================================");
    Serial.println("[MEM]   SUSPENDING SERVICES FOR READING");
    Serial.println("[MEM] ========================================");
    Serial.printf("[MEM] Heap BEFORE suspend: %d bytes\n", ESP.getFreeHeap());
    
    // Turn off WiFi completely - biggest memory saver
    if (WiFi.getMode() != WIFI_OFF) {
        #if FEATURE_WEBSERVER
        webServer.stop();
        Serial.println("[MEM] WebServer stopped");
        #endif
        
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
        }
        WiFi.mode(WIFI_OFF);
        Serial.println("[MEM] WiFi OFF");
    }
    
    // Give time for cleanup
    delay(50);
    
    Serial.printf("[MEM] Heap AFTER suspend: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[MEM] Freed approximately %d bytes for reading\n", 
                  ESP.getFreeHeap() - ESP.getMinFreeHeap());
    Serial.println("[MEM] ========================================");
    Serial.println();
}

void PowerManager::resumeAfterReading() {
    Serial.println();
    Serial.println("[MEM] ========================================");
    Serial.println("[MEM]   RESUMING SERVICES AFTER READING");
    Serial.println("[MEM] ========================================");
    Serial.printf("[MEM] Heap: %d bytes\n", ESP.getFreeHeap());
    
    // WiFi stays off - user can manually connect via Settings if needed
    // This is intentional to preserve battery and memory
    Serial.println("[MEM] WiFi remains OFF (connect via Settings if needed)");
    Serial.println("[MEM] ========================================");
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
    Serial.println("[MEM] =====================================");
    Serial.println();
}

void PowerManager::printFeatureFlags() {
    Serial.println();
    Serial.println("[CFG] ========== FEATURE FLAGS ==========");
    Serial.printf("[CFG] FEATURE_READER:     %d\n", FEATURE_READER);
    
    Serial.printf("[CFG] FEATURE_FLASHCARDS: %d", FEATURE_FLASHCARDS);
    Serial.printf(" (max %d cards)\n", FC_MAX_CARDS);
    
    Serial.printf("[CFG] FEATURE_WEATHER:    %d", FEATURE_WEATHER);
    Serial.printf(" (%d days)\n", WEATHER_FORECAST_DAYS);
    
    Serial.printf("[CFG] FEATURE_LOCKSCREEN: %d\n", FEATURE_LOCKSCREEN);
    
    Serial.printf("[CFG] FEATURE_BLUETOOTH:  %d\n", FEATURE_BLUETOOTH);
    Serial.printf("[CFG] FEATURE_GAMES:      %d\n", FEATURE_GAMES);
    Serial.println("[CFG] =====================================");
    Serial.println();
}
