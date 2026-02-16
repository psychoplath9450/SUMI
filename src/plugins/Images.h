#pragma once

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <SDCardManager.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

/**
 * @file Images.h
 * @brief Image viewer for Sumi e-reader
 * @version 1.0.0
 * 
 * Supports BMP and RAW images (1-bit preferred for e-ink)
 */


class ImagesApp : public PluginInterface {
public:

  const char* name() const override { return "Images"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }
    static const int MAX_IMAGES = 20;
    static const int MAX_NAME_LEN = 24;
    
    enum ViewMode { MODE_BROWSER, MODE_VIEW, MODE_SLIDESHOW };
    
    explicit ImagesApp(PluginRenderer& renderer) : d_(renderer) { reset(); }
    void init(int screenW, int screenH) override;
    bool handleInput(PluginButton btn) override;
    void draw() override;
    
  PluginRenderer& d_;

private:
    char _images[MAX_IMAGES][MAX_NAME_LEN];
    int _imageCount;
    ViewMode _mode;
    int _cursor, _scroll;
    int _screenW, _screenH;
    bool _landscape;
    int _itemH, _itemsPerPage;
    
    void reset();
    void scanImages();
    void drawBrowser();
    void drawImage();
    void drawBMP(const char* path);
    void drawRAW(const char* path);
};


// === IMPLEMENTATION ===

/**
 * @file Images.cpp
 * @brief Image viewer implementation
 * @version 1.0.0
 * 
 * Displays BMP and RAW images from SD card with slideshow mode.
 * Supports 1-bit, 8-bit grayscale, and 24-bit color BMPs.
 */



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

bool ImagesApp::handleInput(PluginButton btn) {
    // Use raw buttons - no remapping
    
    if (_mode == MODE_BROWSER) {
        switch (btn) {
            case PluginButton::Up:
                if (_cursor > 0) {
                    _cursor--;
                    if (_cursor < _scroll) _scroll = _cursor;
                }
                return true;
            case PluginButton::Down:
                if (_cursor < _imageCount - 1) {
                    _cursor++;
                    if (_cursor >= _scroll + _itemsPerPage) _scroll++;
                }
                return true;
            case PluginButton::Center:
                if (_imageCount > 0) {
                    _mode = MODE_VIEW;
                }
                return true;
            case PluginButton::Back:
                return false;
            default:
                return false;
        }
    } else if (_mode == MODE_VIEW) {
        switch (btn) {
            case PluginButton::Left:
            case PluginButton::Up:
                // Previous image
                if (_cursor > 0) _cursor--;
                else _cursor = _imageCount - 1;
                return true;
            case PluginButton::Right:
            case PluginButton::Down:
                // Next image
                _cursor = (_cursor + 1) % _imageCount;
                return true;
            case PluginButton::Back:
            case PluginButton::Center:
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
    
    if (!SdMan.exists("/images")) {
        SdMan.mkdir("/images");
    }
    
    FsFile dir = SdMan.open("/images");
    if (!dir) {
        Serial.println("[IMAGES] /images not found");
        return;
    }
    
    while (FsFile entry = dir.openNextFile()) {
        if (_imageCount >= MAX_IMAGES) break;
        
        char name[64]; entry.getName(name, sizeof(name));
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
    PluginUI::drawHeader(d_, "Images", _screenW);
    
    if (_imageCount == 0) {
        d_.setCursor(20, _screenH / 2 - 20);
        d_.print("No images in /images/");
        d_.setCursor(20, _screenH / 2 + 10);
        d_.print("Add .bmp files via portal");
        PluginUI::drawFooter(d_, "", "", _screenW, _screenH);
        return;
    }
    
    int y = PLUGIN_HEADER_H + 4;
    for (int i = _scroll; i < min(_scroll + _itemsPerPage, _imageCount); i++) {
        bool sel = (i == _cursor);
        PluginUI::drawMenuItem(d_, _images[i], PLUGIN_MARGIN, y, 
                               _screenW - 2 * PLUGIN_MARGIN, _itemH - 4, sel);
        y += _itemH;
    }
    
    char status[32];
    snprintf(status, sizeof(status), "%d/%d", _cursor + 1, _imageCount);
    PluginUI::drawFooter(d_, status, "OK:View", _screenW, _screenH);
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
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2 - 20);
        d_.print("Unsupported format");
        d_.setCursor(20, _screenH / 2 + 10);
        d_.print("Use .bmp files");
    }
    
    // Status bar at bottom
    d_.fillRect(0, _screenH - 28, _screenW, 28, GxEPD_WHITE);
    d_.drawLine(0, _screenH - 28, _screenW, _screenH - 28, GxEPD_BLACK);
    
    char status[64];
    snprintf(status, sizeof(status), "%s (%d/%d)", _images[_cursor], _cursor + 1, _imageCount);
    d_.setCursor(PLUGIN_MARGIN, _screenH - 8);
    d_.print(status);
    
    d_.setCursor(_screenW - 110, _screenH - 8);
    d_.print("L/R:Navigate");
}

void ImagesApp::drawRAW(const char* path) {
    FsFile f = SdMan.open(path);
    if (!f) {
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Failed to open file");
        return;
    }
    
    size_t fileSize = f.size();
    size_t expectedSize = (_screenW * _screenH) / 8;  // 1 bit per pixel
    int bytesPerRow = _screenW / 8;
    
    Serial.printf("[IMAGES] RAW: Display %dx%d, expected=%d bytes, file=%d bytes\n", 
                  _screenW, _screenH, expectedSize, fileSize);
    
    if (fileSize < expectedSize) {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2 - 10);
        d_.print("RAW file size mismatch");
        char buf[48];
        snprintf(buf, sizeof(buf), "Expected %d bytes", expectedSize);
        d_.setCursor(20, _screenH / 2 + 15);
        d_.print(buf);
        return;
    }
    
    d_.fillScreen(GxEPD_WHITE);
    
    // Read and draw row by row
    uint8_t rowBuf[100];  // Max 800 pixels / 8 = 100 bytes
    for (int y = 0; y < _screenH - 28; y++) {  // Leave room for status bar
        f.read(rowBuf, bytesPerRow);
        for (int x = 0; x < _screenW; x++) {
            int byteIdx = x / 8;
            int bitIdx = 7 - (x % 8);
            if (rowBuf[byteIdx] & (1 << bitIdx)) {
                d_.drawPixel(x, y, GxEPD_BLACK);
            }
        }
    }
    
    f.close();
}

void ImagesApp::drawBMP(const char* path) {
    FsFile f = SdMan.open(path);
    if (!f) {
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Failed to open file");
        return;
    }
    
    uint8_t header[54];
    if (f.read(header, 54) != 54) {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Invalid BMP header");
        return;
    }
    
    if (header[0] != 'B' || header[1] != 'M') {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Not a BMP file");
        return;
    }
    
    int32_t width = *(int32_t*)&header[18];
    int32_t height = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];
    
    // Only log once (not inside paged loop)
    static char lastPath[64] = "";
    if (strcmp(path, lastPath) != 0) {
        Serial.printf("[IMAGES] BMP: %dx%d, %d bpp\n", width, height, bpp);
        strncpy(lastPath, path, sizeof(lastPath) - 1);
    }
    
    if (bpp != 1 && bpp != 24) {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2 - 10);
        d_.print("Unsupported BMP depth");
        char buf[48];
        snprintf(buf, sizeof(buf), "Use 1-bit or 24-bit (%d bpp found)", bpp);
        d_.setCursor(20, _screenH / 2 + 15);
        d_.print(buf);
        return;
    }
    
    // Center image on screen
    int offsetX = (_screenW - abs(width)) / 2;
    int offsetY = (_screenH - 28 - abs(height)) / 2;  // Leave room for status bar
    if (offsetX < 0) offsetX = 0;
    if (offsetY < 0) offsetY = 0;
    
    d_.fillScreen(GxEPD_WHITE);
    
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
                
                // Use drawBitmap for entire row (much faster than pixel-by-pixel)
                d_.drawBitmap(offsetX, offsetY + y, row, maxW, 1, GxEPD_WHITE, GxEPD_BLACK);
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
                    d_.drawPixel(offsetX + x, offsetY + y, gray > 128 ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
            free(row);
        }
    }
    
    f.close();
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
