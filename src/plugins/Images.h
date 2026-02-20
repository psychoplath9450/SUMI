#pragma once

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <FsHelpers.h>
#include <ImageConverter.h>
#include <SDCardManager.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

/**
 * @file Images.h
 * @brief Image viewer for Sumi e-reader
 * @version 2.0.0
 *
 * Supports BMP, RAW, PNG, and JPG images.
 * PNG/JPG are converted on-the-fly to BMP via ImageConverterFactory.
 * Pan with D-pad, zoom with Center button (1x/2x/4x).
 */


class ImagesApp : public PluginInterface {
public:

  const char* name() const override { return "Images"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }
    static const int MAX_IMAGES = 20;
    static const int MAX_NAME_LEN = 32;

    enum ViewMode { MODE_BROWSER, MODE_VIEW };

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

    // Pan/zoom state
    int _imgW, _imgH;        // Full image dimensions (from BMP header)
    int _viewX, _viewY;      // Current view offset (top-left corner in image coords)
    int _zoom;               // 1 = fit, 2 = 2x, 4 = 4x
    bool _needsConvert;      // True if current image needs PNG/JPG → BMP conversion
    char _bmpPath[80];       // Path to the BMP to display (original or converted)

    void reset();
    void scanImages();
    void prepareImage();
    void drawBrowser();
    void drawImage();
    void drawBMPRegion(const char* path, int srcX, int srcY, int zoom);
    void drawRAW(const char* path);
    void readBmpDimensions(const char* path);

    bool isConvertible(const char* name) const;
    bool convertToBmp(const char* srcPath, const char* dstPath);
};


// === IMPLEMENTATION ===

static const char* IMAGES_TMP_BMP = "/.sumi/cache/_img_tmp.bmp";

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
                    prepareImage();
                    _mode = MODE_VIEW;
                }
                return true;
            case PluginButton::Back:
                return false;
            default:
                return false;
        }
    } else if (_mode == MODE_VIEW) {
        if (_zoom == 1) {
            // At fit-to-screen: L/R navigate, Center zooms in
            switch (btn) {
                case PluginButton::Left:
                case PluginButton::Up:
                    if (_cursor > 0) _cursor--;
                    else _cursor = _imageCount - 1;
                    prepareImage();
                    return true;
                case PluginButton::Right:
                case PluginButton::Down:
                    _cursor = (_cursor + 1) % _imageCount;
                    prepareImage();
                    return true;
                case PluginButton::Center:
                    // Zoom to 2x, center viewport
                    _zoom = 2;
                    _viewX = max(0, _imgW / 2 - _screenW / 4);
                    _viewY = max(0, _imgH / 2 - _screenH / 4);
                    return true;
                case PluginButton::Back:
                    _mode = MODE_BROWSER;
                    return true;
                default:
                    return true;
            }
        } else {
            // Zoomed in: D-pad pans, Center cycles zoom, Back returns to 1x
            int step = 128 / _zoom;  // Pan step in image pixels
            switch (btn) {
                case PluginButton::Left:
                    _viewX = max(0, _viewX - step);
                    return true;
                case PluginButton::Right: {
                    int maxX = max(0, _imgW - _screenW / _zoom);
                    _viewX = min(_viewX + step, maxX);
                    return true;
                }
                case PluginButton::Up:
                    _viewY = max(0, _viewY - step);
                    return true;
                case PluginButton::Down: {
                    int maxY = max(0, _imgH - _screenH / _zoom);
                    _viewY = min(_viewY + step, maxY);
                    return true;
                }
                case PluginButton::Center:
                    // Cycle: 2x → 4x → 1x
                    if (_zoom >= 4) {
                        _zoom = 1;
                        _viewX = 0;
                        _viewY = 0;
                    } else {
                        // Zoom in further, keep center point
                        int centerX = _viewX + _screenW / (2 * _zoom);
                        int centerY = _viewY + _screenH / (2 * _zoom);
                        _zoom *= 2;
                        _viewX = max(0, centerX - _screenW / (2 * _zoom));
                        _viewY = max(0, centerY - _screenH / (2 * _zoom));
                        int maxX = max(0, _imgW - _screenW / _zoom);
                        int maxY = max(0, _imgH - _screenH / _zoom);
                        _viewX = min(_viewX, maxX);
                        _viewY = min(_viewY, maxY);
                    }
                    return true;
                case PluginButton::Back:
                    // Back from zoomed → return to fit view
                    _zoom = 1;
                    _viewX = 0;
                    _viewY = 0;
                    return true;
                default:
                    return true;
            }
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
    _zoom = 1;
    _viewX = 0;
    _viewY = 0;
    _imgW = 0;
    _imgH = 0;
    _needsConvert = false;
    _bmpPath[0] = '\0';
}

bool ImagesApp::isConvertible(const char* name) const {
    const char* ext = strrchr(name, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0);
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

        const char* ext = strrchr(name, '.');
        if (ext) {
            bool supported = (strcasecmp(ext, ".bmp") == 0 ||
                            strcasecmp(ext, ".raw") == 0 ||
                            strcasecmp(ext, ".png") == 0 ||
                            strcasecmp(ext, ".jpg") == 0 ||
                            strcasecmp(ext, ".jpeg") == 0);
            if (supported) {
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

bool ImagesApp::convertToBmp(const char* srcPath, const char* dstPath) {
    ImageConvertConfig config;
    config.maxWidth = _screenW;
    config.maxHeight = _screenH;
    config.oneBit = true;
    config.logTag = "IMG";
    return ImageConverterFactory::convertToBmp(srcPath, dstPath, config);
}

void ImagesApp::readBmpDimensions(const char* path) {
    FsFile f = SdMan.open(path);
    if (!f) { _imgW = _screenW; _imgH = _screenH; return; }

    uint8_t header[26];
    if (f.read(header, 26) < 26) { f.close(); _imgW = _screenW; _imgH = _screenH; return; }
    f.close();

    if (header[0] != 'B' || header[1] != 'M') { _imgW = _screenW; _imgH = _screenH; return; }

    _imgW = abs(*(int32_t*)&header[18]);
    _imgH = abs(*(int32_t*)&header[22]);
}

void ImagesApp::prepareImage() {
    char srcPath[80];
    snprintf(srcPath, sizeof(srcPath), "/images/%s", _images[_cursor]);

    _zoom = 1;
    _viewX = 0;
    _viewY = 0;

    if (isConvertible(_images[_cursor])) {
        // Convert PNG/JPG to temp BMP
        _needsConvert = true;

        // Use full-resolution conversion (not screen-sized) for zoom support
        ImageConvertConfig config;
        config.maxWidth = 800;   // Max display width
        config.maxHeight = 800;  // Allow large images
        config.oneBit = true;
        config.logTag = "IMG";

        Serial.printf("[IMAGES] Converting %s to BMP...\n", _images[_cursor]);

        if (ImageConverterFactory::convertToBmp(srcPath, IMAGES_TMP_BMP, config)) {
            strncpy(_bmpPath, IMAGES_TMP_BMP, sizeof(_bmpPath) - 1);
            readBmpDimensions(_bmpPath);
            Serial.printf("[IMAGES] Converted: %dx%d\n", _imgW, _imgH);
        } else {
            Serial.println("[IMAGES] Conversion failed");
            _bmpPath[0] = '\0';
            _imgW = _screenW;
            _imgH = _screenH;
        }
    } else {
        _needsConvert = false;
        strncpy(_bmpPath, srcPath, sizeof(_bmpPath) - 1);

        const char* ext = strrchr(_images[_cursor], '.');
        if (ext && strcasecmp(ext, ".raw") == 0) {
            // RAW files are always screen-sized
            _imgW = _screenW;
            _imgH = _screenH;
        } else {
            readBmpDimensions(_bmpPath);
        }
    }
}

void ImagesApp::drawBrowser() {
    PluginUI::drawHeader(d_, "Images", _screenW);

    if (_imageCount == 0) {
        d_.setCursor(20, _screenH / 2 - 20);
        d_.print("No images in /images/");
        d_.setCursor(20, _screenH / 2 + 10);
        d_.print("Add BMP, PNG, or JPG files");
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
    // Handle RAW files separately (no pan/zoom)
    const char* ext = strrchr(_images[_cursor], '.');
    if (ext && strcasecmp(ext, ".raw") == 0) {
        char path[64];
        snprintf(path, sizeof(path), "/images/%s", _images[_cursor]);
        drawRAW(path);

        // Status bar
        d_.fillRect(0, _screenH - 28, _screenW, 28, GxEPD_WHITE);
        d_.drawLine(0, _screenH - 28, _screenW, _screenH - 28, GxEPD_BLACK);
        d_.setCursor(PLUGIN_MARGIN, _screenH - 8);
        d_.print(_images[_cursor]);
        return;
    }

    // BMP display (original or converted from PNG/JPG)
    if (_bmpPath[0] == '\0') {
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Conversion failed");
        return;
    }

    drawBMPRegion(_bmpPath, _viewX, _viewY, _zoom);

    // Status bar at bottom
    d_.fillRect(0, _screenH - 28, _screenW, 28, GxEPD_WHITE);
    d_.drawLine(0, _screenH - 28, _screenW, _screenH - 28, GxEPD_BLACK);

    char status[64];
    if (_zoom > 1) {
        snprintf(status, sizeof(status), "%s %dx (%d/%d)",
                 _images[_cursor], _zoom, _cursor + 1, _imageCount);
    } else {
        snprintf(status, sizeof(status), "%s (%d/%d)",
                 _images[_cursor], _cursor + 1, _imageCount);
    }
    d_.setCursor(PLUGIN_MARGIN, _screenH - 8);
    d_.print(status);

    d_.setCursor(_screenW - 100, _screenH - 8);
    d_.print(_zoom > 1 ? "D:Pan OK:Zoom" : "OK:Zoom");
}

void ImagesApp::drawBMPRegion(const char* path, int srcX, int srcY, int zoom) {
    FsFile f = SdMan.open(path);
    if (!f) {
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Failed to open file");
        return;
    }

    uint8_t header[54];
    if (f.read(header, 54) != 54 || header[0] != 'B' || header[1] != 'M') {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        d_.print("Invalid BMP");
        return;
    }

    int32_t width = *(int32_t*)&header[18];
    int32_t height = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];

    bool flipV = (height > 0);
    height = abs(height);
    width = abs(width);

    if (bpp != 1 && bpp != 24) {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2);
        char buf[48];
        snprintf(buf, sizeof(buf), "Unsupported: %d bpp", bpp);
        d_.print(buf);
        return;
    }

    d_.fillScreen(GxEPD_WHITE);

    int displayH = _screenH - 28;  // Leave room for status bar

    if (zoom <= 1) {
        // Fit to screen - center the image
        int offsetX = (_screenW - (int)width) / 2;
        int offsetY = (displayH - (int)height) / 2;
        if (offsetX < 0) offsetX = 0;
        if (offsetY < 0) offsetY = 0;

        int maxH = min((int)height, displayH);
        int maxW = min((int)width, _screenW);

        if (bpp == 1) {
            int rowBytes = ((width + 31) / 32) * 4;
            uint8_t* row = (uint8_t*)malloc(rowBytes);
            if (row) {
                for (int y = 0; y < maxH; y++) {
                    int imgY = flipV ? (height - 1 - y) : y;
                    f.seek(dataOffset + imgY * rowBytes);
                    f.read(row, rowBytes);
                    d_.drawBitmap(offsetX, offsetY + y, row, maxW, 1, GxEPD_WHITE, GxEPD_BLACK);
                }
                free(row);
            }
        } else if (bpp == 24) {
            int rowBytes = ((width * 3 + 3) / 4) * 4;
            uint8_t* row = (uint8_t*)malloc(rowBytes);
            if (row) {
                for (int y = 0; y < maxH; y++) {
                    int imgY = flipV ? (height - 1 - y) : y;
                    f.seek(dataOffset + imgY * rowBytes);
                    f.read(row, rowBytes);
                    for (int x = 0; x < maxW; x++) {
                        int idx = x * 3;
                        int gray = (row[idx] + row[idx+1] + row[idx+2]) / 3;
                        d_.drawPixel(offsetX + x, offsetY + y, gray > 128 ? GxEPD_WHITE : GxEPD_BLACK);
                    }
                }
                free(row);
            }
        }
    } else {
        // Zoomed view - render a region of the image at zoom level
        // Each screen pixel maps to 1/zoom image pixels
        int viewW = _screenW / zoom;   // Image pixels visible horizontally
        int viewH = displayH / zoom;   // Image pixels visible vertically

        // Clamp source region
        if (srcX + viewW > (int)width) srcX = max(0, (int)width - viewW);
        if (srcY + viewH > (int)height) srcY = max(0, (int)height - viewH);

        if (bpp == 1) {
            int rowBytes = ((width + 31) / 32) * 4;
            uint8_t* row = (uint8_t*)malloc(rowBytes);
            if (row) {
                for (int sy = 0; sy < viewH && sy + srcY < (int)height; sy++) {
                    int imgY = flipV ? (height - 1 - (srcY + sy)) : (srcY + sy);
                    f.seek(dataOffset + imgY * rowBytes);
                    f.read(row, rowBytes);

                    int screenY = sy * zoom;
                    for (int sx = 0; sx < viewW && sx + srcX < (int)width; sx++) {
                        int imgX = srcX + sx;
                        bool black = !(row[imgX >> 3] & (0x80 >> (imgX & 7)));
                        if (black) {
                            int screenX = sx * zoom;
                            d_.fillRect(screenX, screenY, zoom, zoom, GxEPD_BLACK);
                        }
                    }
                }
                free(row);
            }
        } else if (bpp == 24) {
            int rowBytes = ((width * 3 + 3) / 4) * 4;
            uint8_t* row = (uint8_t*)malloc(min(rowBytes, 2400));  // Limit alloc
            if (row) {
                for (int sy = 0; sy < viewH && sy + srcY < (int)height; sy++) {
                    int imgY = flipV ? (height - 1 - (srcY + sy)) : (srcY + sy);
                    f.seek(dataOffset + imgY * rowBytes);
                    f.read(row, min(rowBytes, 2400));

                    int screenY = sy * zoom;
                    for (int sx = 0; sx < viewW && sx + srcX < (int)width; sx++) {
                        int imgX = srcX + sx;
                        if (imgX * 3 + 2 < min(rowBytes, 2400)) {
                            int idx = imgX * 3;
                            int gray = (row[idx] + row[idx+1] + row[idx+2]) / 3;
                            if (gray <= 128) {
                                int screenX = sx * zoom;
                                d_.fillRect(screenX, screenY, zoom, zoom, GxEPD_BLACK);
                            }
                        }
                    }
                }
                free(row);
            }
        }
    }

    f.close();
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
    size_t expectedSize = (_screenW * _screenH) / 8;
    int bytesPerRow = _screenW / 8;

    if (fileSize < expectedSize) {
        f.close();
        d_.fillScreen(GxEPD_WHITE);
        d_.setCursor(20, _screenH / 2 - 10);
        d_.print("RAW file size mismatch");
        return;
    }

    d_.fillScreen(GxEPD_WHITE);

    uint8_t rowBuf[100];  // Max 800 pixels / 8 = 100 bytes
    for (int y = 0; y < _screenH - 28; y++) {
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

}  // namespace sumi

#endif  // FEATURE_PLUGINS
