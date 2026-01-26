/**
 * @file Images.cpp
 * @brief Image viewer implementation
 * @version 1.3.0
 * 
 * Optimized with raw button handling
 * Fixed: Only lists file formats that are actually supported (.bmp, .raw)
 */

#include <SD.h>
#include "plugins/Images.h"

#if FEATURE_GAMES


ImagesApp::ImagesApp() { 
    reset(); 
}

void ImagesApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _itemH = 36;
    _itemsPerPage = (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 8) / _itemH;
    
    scanImages();
    _mode = MODE_BROWSER;
    _cursor = 0;
    _scroll = 0;
}

bool ImagesApp::handleInput(Button btn) {
    // Use raw buttons - no remapping
    
    if (_mode == MODE_BROWSER) {
        switch (btn) {
            case BTN_UP:
                if (_cursor > 0) {
                    _cursor--;
                    if (_cursor < _scroll) _scroll = _cursor;
                }
                return true;
            case BTN_DOWN:
                if (_cursor < _imageCount - 1) {
                    _cursor++;
                    if (_cursor >= _scroll + _itemsPerPage) _scroll++;
                }
                return true;
            case BTN_CONFIRM:
                if (_imageCount > 0) {
                    _mode = MODE_VIEW;
                }
                return true;
            case BTN_BACK:
                return false;
            default:
                return false;
        }
    } else if (_mode == MODE_VIEW) {
        switch (btn) {
            case BTN_LEFT:
            case BTN_UP:
                // Previous image
                if (_cursor > 0) _cursor--;
                else _cursor = _imageCount - 1;
                return true;
            case BTN_RIGHT:
            case BTN_DOWN:
                // Next image
                _cursor = (_cursor + 1) % _imageCount;
                return true;
            case BTN_BACK:
            case BTN_CONFIRM:
                _mode = MODE_BROWSER;
                return true;
            default:
                return true;
        }
    }
    return false;
}

void ImagesApp::draw() {
    if (_mode == MODE_BROWSER) {
        drawBrowser();
    } else {
        drawImage();
    }
}

void ImagesApp::reset() {
    _imageCount = 0;
    _cursor = 0;
    _scroll = 0;
    _mode = MODE_BROWSER;
}

void ImagesApp::scanImages() {
    _imageCount = 0;
    
    if (!SD.exists("/images")) {
        SD.mkdir("/images");
    }
    
    File dir = SD.open("/images");
    if (!dir) {
        Serial.println("[IMAGES] /images not found");
        return;
    }
    
    while (File entry = dir.openNextFile()) {
        if (_imageCount >= MAX_IMAGES) break;
        
        const char* name = entry.name();
        if (name[0] == '.') { entry.close(); continue; }
        
        int len = strlen(name);
        if (len > 4) {
            const char* ext = name + len - 4;
            // Only list formats we can actually display
            if (strcasecmp(ext, ".bmp") == 0 || 
                strcasecmp(ext, ".raw") == 0) {
                strncpy(_images[_imageCount], name, MAX_NAME_LEN - 1);
                _images[_imageCount][MAX_NAME_LEN - 1] = '\0';
                _imageCount++;
            }
        }
        entry.close();
    }
    dir.close();
    
    Serial.printf("[IMAGES] Found %d images\n", _imageCount);
}

void ImagesApp::drawBrowser() {
    PluginUI::drawHeader("Images", _screenW);
    
    if (_imageCount == 0) {
        display.setCursor(20, _screenH / 2 - 20);
        display.print("No images in /images/");
        display.setCursor(20, _screenH / 2 + 10);
        display.print("Add .bmp files via portal");
        PluginUI::drawFooter("", "", _screenW, _screenH);
        return;
    }
    
    int y = PLUGIN_HEADER_H + 4;
    for (int i = _scroll; i < min(_scroll + _itemsPerPage, _imageCount); i++) {
        bool sel = (i == _cursor);
        PluginUI::drawMenuItem(_images[i], PLUGIN_MARGIN, y, 
                               _screenW - 2 * PLUGIN_MARGIN, _itemH - 4, sel);
        y += _itemH;
    }
    
    char status[32];
    snprintf(status, sizeof(status), "%d/%d", _cursor + 1, _imageCount);
    PluginUI::drawFooter(status, "OK:View", _screenW, _screenH);
}

void ImagesApp::drawImage() {
    char path[64];
    snprintf(path, sizeof(path), "/images/%s", _images[_cursor]);
    
    // Check extension
    int len = strlen(_images[_cursor]);
    const char* ext = _images[_cursor] + len - 4;
    
    if (strcasecmp(ext, ".bmp") == 0) {
        drawBMP(path);
    } else if (strcasecmp(ext, ".raw") == 0) {
        drawRAW(path);
    } else {
        // Unsupported format placeholder
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2 - 20);
        display.print("Unsupported format");
        display.setCursor(20, _screenH / 2 + 10);
        display.print("Use .bmp files");
    }
    
    // Status bar at bottom
    display.fillRect(0, _screenH - 28, _screenW, 28, GxEPD_WHITE);
    display.drawLine(0, _screenH - 28, _screenW, _screenH - 28, GxEPD_BLACK);
    
    char status[64];
    snprintf(status, sizeof(status), "%s (%d/%d)", _images[_cursor], _cursor + 1, _imageCount);
    display.setCursor(PLUGIN_MARGIN, _screenH - 8);
    display.print(status);
    
    display.setCursor(_screenW - 110, _screenH - 8);
    display.print("L/R:Navigate");
}

void ImagesApp::drawRAW(const char* path) {
    File f = SD.open(path);
    if (!f) {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2);
        display.print("Failed to open file");
        return;
    }
    
    size_t fileSize = f.size();
    size_t expectedSize = (_screenW * _screenH) / 8;  // 1 bit per pixel
    int bytesPerRow = _screenW / 8;
    
    Serial.printf("[IMAGES] RAW: Display %dx%d, expected=%d bytes, file=%d bytes\n", 
                  _screenW, _screenH, expectedSize, fileSize);
    
    if (fileSize < expectedSize) {
        f.close();
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2 - 10);
        display.print("RAW file size mismatch");
        char buf[48];
        snprintf(buf, sizeof(buf), "Expected %d bytes", expectedSize);
        display.setCursor(20, _screenH / 2 + 15);
        display.print(buf);
        return;
    }
    
    display.fillScreen(GxEPD_WHITE);
    
    // Read and draw row by row
    uint8_t rowBuf[100];  // Max 800 pixels / 8 = 100 bytes
    for (int y = 0; y < _screenH - 28; y++) {  // Leave room for status bar
        f.read(rowBuf, bytesPerRow);
        for (int x = 0; x < _screenW; x++) {
            int byteIdx = x / 8;
            int bitIdx = 7 - (x % 8);
            if (rowBuf[byteIdx] & (1 << bitIdx)) {
                display.drawPixel(x, y, GxEPD_BLACK);
            }
        }
    }
    
    f.close();
}

void ImagesApp::drawBMP(const char* path) {
    File f = SD.open(path);
    if (!f) {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2);
        display.print("Failed to open file");
        return;
    }
    
    uint8_t header[54];
    if (f.read(header, 54) != 54) {
        f.close();
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2);
        display.print("Invalid BMP header");
        return;
    }
    
    if (header[0] != 'B' || header[1] != 'M') {
        f.close();
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2);
        display.print("Not a BMP file");
        return;
    }
    
    int32_t width = *(int32_t*)&header[18];
    int32_t height = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];
    
    Serial.printf("[IMAGES] BMP: %dx%d, %d bpp\n", width, height, bpp);
    
    if (bpp != 1 && bpp != 24) {
        f.close();
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20, _screenH / 2 - 10);
        display.print("Unsupported BMP depth");
        char buf[48];
        snprintf(buf, sizeof(buf), "Use 1-bit or 24-bit (%d bpp found)", bpp);
        display.setCursor(20, _screenH / 2 + 15);
        display.print(buf);
        return;
    }
    
    // Center image on screen
    int offsetX = (_screenW - abs(width)) / 2;
    int offsetY = (_screenH - 28 - abs(height)) / 2;  // Leave room for status bar
    if (offsetX < 0) offsetX = 0;
    if (offsetY < 0) offsetY = 0;
    
    display.fillScreen(GxEPD_WHITE);
    
    f.seek(dataOffset);
    
    bool flipV = (height > 0);
    height = abs(height);
    width = abs(width);
    
    int maxH = min((int)height, _screenH - 28);
    int maxW = min((int)width, _screenW);
    
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
                    // Simple grayscale conversion with threshold
                    int gray = (row[idx] + row[idx+1] + row[idx+2]) / 3;
                    display.drawPixel(offsetX + x, offsetY + y, gray > 128 ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    }
    
    f.close();
}

#endif // FEATURE_GAMES
