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
 * @file Maps.h
 * @brief Offline tile-based map viewer for Sumi e-reader
 * @version 2.0.0
 *
 * Features:
 * - OpenStreetMap tile format support (z/x/y.png or .jpg)
 * - Pan with D-pad (tile-by-tile)
 * - Zoom levels with UP/DOWN
 * - Only loads visible tiles (memory efficient)
 * - Also supports single large map images (PNG, JPG, BMP)
 * - PNG/JPG decoded on-the-fly via ImageConverterFactory
 *
 * Directory structure for tiles:
 *   /maps/
 *     mapname/
 *       10/           <- zoom level
 *         512/        <- x tile
 *           384.png   <- y tile (256x256)
 *
 * Or single images:
 *   /maps/
 *     trail_map.png
 *     campus.bmp
 */


class MapsApp : public PluginInterface {
public:

  const char* name() const override { return "Maps"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }
    static const int MAX_MAPS = 16;
    static const int MAX_NAME_LEN = 32;
    static const int TILE_SIZE = 256;

    static const int MIN_ZOOM = 8;
    static const int MAX_ZOOM = 18;

    enum ViewMode {
        MODE_BROWSER,
        MODE_TILES,
        MODE_IMAGE,
        MODE_ZOOMING
    };

    explicit MapsApp(PluginRenderer& renderer) : d_(renderer) { reset(); }
    void init(int screenW, int screenH) override;
    bool handleInput(PluginButton btn) override;
    void draw() override;
    void drawPartial();

  PluginRenderer& d_;

private:
    struct MapEntry {
        char name[MAX_NAME_LEN];
        bool isTileMap;
        int minZoom;
        int maxZoom;
    };

    MapEntry _maps[MAX_MAPS];
    int _mapCount;
    int _cursor, _scroll;

    ViewMode _mode;
    int _currentMap;

    // Tile map state
    int _zoom;
    int _tileX, _tileY;
    int _tilesX, _tilesY;

    // Single image state
    int _imgW, _imgH;
    int _viewX, _viewY;
    int _imgZoom;
    char _imgBmpPath[80];

    int _screenW, _screenH;
    bool _landscape;
    int _itemH, _itemsPerPage;

    void reset();
    void scanMaps();
    void openMap(int idx);
    void detectZoomLevels(int idx);

    void drawBrowser();
    void drawTileMap();
    void drawSingleImage();
    void drawZoomIndicator();

    bool loadTile(int z, int x, int y, int screenX, int screenY);
    void drawTilePlaceholder(int screenX, int screenY, int z, int x, int y);
    void drawBmpAt(const char* bmpPath, int screenX, int screenY, int maxW, int maxH);

    void loadImageInfo(const char* path);
    void drawImageRegion(const char* path, int srcX, int srcY, int zoom);

    void buildTilePath(char* buf, int bufSize, int z, int x, int y, const char* ext);
    bool findTile(char* outPath, int pathSize, int z, int x, int y);
};


// === IMPLEMENTATION ===

static const char* MAPS_TMP_TILE = "/.sumi/cache/_tile_tmp.bmp";
static const char* MAPS_TMP_IMG = "/.sumi/cache/_map_tmp.bmp";

void MapsApp::reset() {
    _mapCount = 0;
    _cursor = 0;
    _scroll = 0;
    _mode = MODE_BROWSER;
    _currentMap = -1;
    _zoom = 12;
    _tileX = 0;
    _tileY = 0;
    _imgW = 0;
    _imgH = 0;
    _viewX = 0;
    _viewY = 0;
    _imgZoom = 1;
    _imgBmpPath[0] = '\0';
    needsFullRedraw = true;
}

void MapsApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _itemH = 52;
    _itemsPerPage = (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 20) / _itemH;
    if (_itemsPerPage < 1) _itemsPerPage = 1;

    _tilesX = (_screenW + TILE_SIZE - 1) / TILE_SIZE + 1;
    _tilesY = (_screenH + TILE_SIZE - 1) / TILE_SIZE + 1;

    reset();
    scanMaps();
}

// =============================================================================
// Input Handling
// =============================================================================
bool MapsApp::handleInput(PluginButton btn) {
    needsFullRedraw = true;

    if (_mode == MODE_BROWSER) {
        switch (btn) {
            case PluginButton::Up:
                if (_cursor > 0) {
                    _cursor--;
                    if (_cursor < _scroll) _scroll = _cursor;
                }
                return true;
            case PluginButton::Down:
                if (_cursor < _mapCount - 1) {
                    _cursor++;
                    if (_cursor >= _scroll + _itemsPerPage) _scroll++;
                }
                return true;
            case PluginButton::Center:
                if (_mapCount > 0) openMap(_cursor);
                return true;
            case PluginButton::Back:
                return false;
            default:
                return true;
        }
    }
    else if (_mode == MODE_TILES) {
        switch (btn) {
            case PluginButton::Left:  _tileX--; return true;
            case PluginButton::Right: _tileX++; return true;
            case PluginButton::Up:    _tileY--; return true;
            case PluginButton::Down:  _tileY++; return true;
            case PluginButton::Center:
                _mode = MODE_ZOOMING;
                needsFullRedraw = false;
                return true;
            case PluginButton::Back:
                _mode = MODE_BROWSER;
                return true;
            default: return true;
        }
    }
    else if (_mode == MODE_ZOOMING) {
        MapEntry& map = _maps[_currentMap];
        switch (btn) {
            case PluginButton::Up:
                if (_zoom < map.maxZoom) { _zoom++; _tileX *= 2; _tileY *= 2; }
                return true;
            case PluginButton::Down:
                if (_zoom > map.minZoom) { _zoom--; _tileX /= 2; _tileY /= 2; }
                return true;
            case PluginButton::Center:
            case PluginButton::Back:
                _mode = MODE_TILES;
                return true;
            default: return true;
        }
    }
    else if (_mode == MODE_IMAGE) {
        int step = 128 / _imgZoom;
        switch (btn) {
            case PluginButton::Left:
                _viewX = max(0, _viewX - step);
                return true;
            case PluginButton::Right:
                _viewX += step;
                if (_viewX + _screenW / _imgZoom > _imgW)
                    _viewX = max(0, _imgW - _screenW / _imgZoom);
                return true;
            case PluginButton::Up:
                _viewY = max(0, _viewY - step);
                return true;
            case PluginButton::Down:
                _viewY += step;
                if (_viewY + _screenH / _imgZoom > _imgH)
                    _viewY = max(0, _imgH - _screenH / _imgZoom);
                return true;
            case PluginButton::Center:
                _imgZoom = _imgZoom >= 4 ? 1 : _imgZoom * 2;
                if (_viewX + _screenW / _imgZoom > _imgW)
                    _viewX = max(0, _imgW - _screenW / _imgZoom);
                if (_viewY + _screenH / _imgZoom > _imgH)
                    _viewY = max(0, _imgH - _screenH / _imgZoom);
                return true;
            case PluginButton::Back:
                _mode = MODE_BROWSER;
                return true;
            default: return true;
        }
    }

    return true;
}

// =============================================================================
// Drawing
// =============================================================================
void MapsApp::draw() {
    if (_mode == MODE_BROWSER) drawBrowser();
    else if (_mode == MODE_TILES || _mode == MODE_ZOOMING) {
        drawTileMap();
        if (_mode == MODE_ZOOMING) drawZoomIndicator();
    } else if (_mode == MODE_IMAGE) drawSingleImage();
}

void MapsApp::drawPartial() { draw(); }

void MapsApp::drawBrowser() {
    PluginUI::drawHeader(d_, "Maps", _screenW);
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);

    if (_mapCount == 0) {
        d_.setCursor(20, _screenH / 2 - 30);
        d_.print("No maps found!");
        d_.setCursor(20, _screenH / 2);
        d_.print("Add tiles or images to /maps/");
        d_.setCursor(20, _screenH / 2 + 30);
        d_.print("Supports: PNG, JPG, BMP, OSM tiles");
        return;
    }

    int y = 60;
    int endIdx = min(_scroll + _itemsPerPage, _mapCount);

    for (int i = _scroll; i < endIdx; i++) {
        bool sel = (i == _cursor);
        MapEntry& map = _maps[i];

        if (sel) {
            d_.fillRoundRect(14, y - 4, _screenW - 28, _itemH - 4, 6, GxEPD_BLACK);
            d_.setTextColor(GxEPD_WHITE);
        } else {
            d_.drawRoundRect(14, y - 4, _screenW - 28, _itemH - 4, 6, GxEPD_BLACK);
            d_.setTextColor(GxEPD_BLACK);
        }

        d_.setFont(nullptr);
        d_.setCursor(26, y + 22);
        d_.print(map.isTileMap ? "T" : "I");

        d_.setCursor(60, y + 18);
        char shortName[24];
        strncpy(shortName, map.name, 23);
        shortName[23] = '\0';
        if (strlen(map.name) > 20) { shortName[20] = '\0'; strcat(shortName, "..."); }
        d_.print(shortName);

        d_.setCursor(60, y + 36);
        if (map.isTileMap) {
            char info[32];
            snprintf(info, sizeof(info), "Tiles z%d-%d", map.minZoom, map.maxZoom);
            d_.print(info);
        } else {
            d_.print("Image");
        }

        d_.setTextColor(GxEPD_BLACK);
        y += _itemH;
    }

    d_.setFont(nullptr);
    d_.drawLine(0, _screenH - 40, _screenW, _screenH - 40, GxEPD_BLACK);
    char status[24];
    snprintf(status, sizeof(status), "%d/%d", _cursor + 1, _mapCount);
    d_.setCursor(20, _screenH - 15);
    d_.print(status);
    d_.setCursor(_screenW - 80, _screenH - 15);
    d_.print("OK: Open");
}

void MapsApp::drawTileMap() {
    d_.fillScreen(GxEPD_WHITE);
    MapEntry& map = _maps[_currentMap];

    int startTileX = _tileX - _tilesX / 2;
    int startTileY = _tileY - _tilesY / 2;

    for (int ty = 0; ty < _tilesY; ty++) {
        for (int tx = 0; tx < _tilesX; tx++) {
            int screenX = tx * TILE_SIZE;
            int screenY = ty * TILE_SIZE;
            if (!loadTile(_zoom, startTileX + tx, startTileY + ty, screenX, screenY)) {
                drawTilePlaceholder(screenX, screenY, _zoom, startTileX + tx, startTileY + ty);
            }
        }
    }

    d_.fillRect(0, _screenH - 30, _screenW, 30, GxEPD_WHITE);
    d_.drawLine(0, _screenH - 30, _screenW, _screenH - 30, GxEPD_BLACK);
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    char info[48];
    snprintf(info, sizeof(info), "%s  Z:%d  [%d,%d]", map.name, _zoom, _tileX, _tileY);
    d_.setCursor(10, _screenH - 10);
    d_.print(info);
    d_.setCursor(_screenW - 120, _screenH - 10);
    d_.print("OK: Zoom");
}

void MapsApp::drawZoomIndicator() {
    int boxW = 160, boxH = 100;
    int boxX = (_screenW - boxW) / 2, boxY = (_screenH - boxH) / 2;

    d_.fillRect(boxX, boxY, boxW, boxH, GxEPD_WHITE);
    d_.drawRect(boxX, boxY, boxW, boxH, GxEPD_BLACK);
    d_.drawRect(boxX + 1, boxY + 1, boxW - 2, boxH - 2, GxEPD_BLACK);

    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    int16_t tx, ty; uint16_t tw, th;

    d_.getTextBounds("ZOOM", 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(boxX + (boxW - tw) / 2, boxY + 28);
    d_.print("ZOOM");

    d_.setTextSize(2);
    char zoomStr[8];
    snprintf(zoomStr, sizeof(zoomStr), "%d", _zoom);
    d_.getTextBounds(zoomStr, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(boxX + (boxW - tw) / 2, boxY + 65);
    d_.print(zoomStr);
    d_.setTextSize(1);

    MapEntry& map = _maps[_currentMap];
    char range[24];
    snprintf(range, sizeof(range), "(%d-%d)", map.minZoom, map.maxZoom);
    d_.getTextBounds(range, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(boxX + (boxW - tw) / 2, boxY + boxH - 10);
    d_.print(range);
}

void MapsApp::drawSingleImage() {
    d_.fillScreen(GxEPD_WHITE);

    const char* displayPath = _imgBmpPath[0] != '\0' ? _imgBmpPath : nullptr;
    if (displayPath) {
        drawImageRegion(displayPath, _viewX, _viewY, _imgZoom);
    } else {
        d_.setCursor(20, _screenH / 2);
        d_.print("Failed to load image");
    }

    d_.fillRect(0, _screenH - 30, _screenW, 30, GxEPD_WHITE);
    d_.drawLine(0, _screenH - 30, _screenW, _screenH - 30, GxEPD_BLACK);
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);

    MapEntry& map = _maps[_currentMap];
    char info[48];
    snprintf(info, sizeof(info), "%s  %dx  [%d,%d]", map.name, _imgZoom, _viewX, _viewY);
    d_.setCursor(10, _screenH - 10);
    d_.print(info);
    d_.setCursor(_screenW - 120, _screenH - 10);
    d_.print(_imgZoom > 1 ? "D:Pan OK:Zoom" : "OK: Zoom");
}

// =============================================================================
// Map Scanning
// =============================================================================
void MapsApp::scanMaps() {
    _mapCount = 0;

    FsFile dir = SdMan.open("/maps");
    if (!dir) {
        SdMan.mkdir("/maps");
        return;
    }

    while (FsFile entry = dir.openNextFile()) {
        if (_mapCount >= MAX_MAPS) break;
        char name[64]; entry.getName(name, sizeof(name));
        if (name[0] == '.') { entry.close(); continue; }

        MapEntry& map = _maps[_mapCount];
        strncpy(map.name, name, MAX_NAME_LEN - 1);
        map.name[MAX_NAME_LEN - 1] = '\0';

        if (entry.isDirectory()) {
            map.isTileMap = true;
            map.minZoom = MIN_ZOOM;
            map.maxZoom = MAX_ZOOM;
            detectZoomLevels(_mapCount);
            _mapCount++;
        } else {
            const char* ext = strrchr(name, '.');
            if (ext && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".bmp") == 0 ||
                       strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)) {
                map.isTileMap = false;
                map.minZoom = 1;
                map.maxZoom = 4;
                _mapCount++;
            }
        }
        entry.close();
    }
    dir.close();
    Serial.printf("[MAPS] Found %d maps\n", _mapCount);
}

void MapsApp::detectZoomLevels(int idx) {
    MapEntry& map = _maps[idx];
    char path[64];
    snprintf(path, sizeof(path), "/maps/%s", map.name);

    FsFile dir = SdMan.open(path);
    if (!dir) return;

    int minFound = 99, maxFound = 0;
    while (FsFile entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
            char _zn[16]; entry.getName(_zn, sizeof(_zn));
            int z = atoi(_zn);
            if (z >= MIN_ZOOM && z <= MAX_ZOOM) {
                if (z < minFound) minFound = z;
                if (z > maxFound) maxFound = z;
            }
        }
        entry.close();
    }
    dir.close();

    if (minFound <= maxFound) {
        map.minZoom = minFound;
        map.maxZoom = maxFound;
    }
}

void MapsApp::openMap(int idx) {
    _currentMap = idx;
    MapEntry& map = _maps[idx];

    if (map.isTileMap) {
        _mode = MODE_TILES;
        _zoom = (map.minZoom + map.maxZoom) / 2;
        _tileX = 1 << (_zoom - 1);
        _tileY = 1 << (_zoom - 1);
    } else {
        _mode = MODE_IMAGE;
        _viewX = 0;
        _viewY = 0;
        _imgZoom = 1;
        _imgBmpPath[0] = '\0';

        char path[64];
        snprintf(path, sizeof(path), "/maps/%s", map.name);

        const char* ext = strrchr(map.name, '.');
        if (ext && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                   strcasecmp(ext, ".jpeg") == 0)) {
            // Convert PNG/JPG to BMP for display
            ImageConvertConfig config;
            config.maxWidth = 800;
            config.maxHeight = 800;
            config.oneBit = true;
            config.logTag = "MAP";

            if (ImageConverterFactory::convertToBmp(path, MAPS_TMP_IMG, config)) {
                strncpy(_imgBmpPath, MAPS_TMP_IMG, sizeof(_imgBmpPath) - 1);
                loadImageInfo(_imgBmpPath);
            } else {
                loadImageInfo(path);
            }
        } else {
            strncpy(_imgBmpPath, path, sizeof(_imgBmpPath) - 1);
            loadImageInfo(path);
        }

        Serial.printf("[MAPS] Opened: %s (%dx%d)\n", map.name, _imgW, _imgH);
    }
}

// =============================================================================
// Tile Loading
// =============================================================================
void MapsApp::buildTilePath(char* buf, int bufSize, int z, int x, int y, const char* ext) {
    MapEntry& map = _maps[_currentMap];
    snprintf(buf, bufSize, "/maps/%s/%d/%d/%d%s", map.name, z, x, y, ext);
}

bool MapsApp::findTile(char* outPath, int pathSize, int z, int x, int y) {
    buildTilePath(outPath, pathSize, z, x, y, ".png");
    if (SdMan.exists(outPath)) return true;
    buildTilePath(outPath, pathSize, z, x, y, ".jpg");
    if (SdMan.exists(outPath)) return true;
    buildTilePath(outPath, pathSize, z, x, y, ".bmp");
    if (SdMan.exists(outPath)) return true;
    return false;
}

void MapsApp::drawBmpAt(const char* bmpPath, int screenX, int screenY, int maxW, int maxH) {
    FsFile f = SdMan.open(bmpPath);
    if (!f) return;

    uint8_t header[54];
    if (f.read(header, 54) != 54 || header[0] != 'B' || header[1] != 'M') { f.close(); return; }

    int32_t width = abs(*(int32_t*)&header[18]);
    int32_t height = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];

    bool flipV = (height > 0);
    height = abs(height);
    int drawW = min((int)width, maxW);
    int drawH = min((int)height, maxH);

    if (bpp == 1) {
        int rowBytes = ((width + 31) / 32) * 4;
        uint8_t* row = (uint8_t*)malloc(rowBytes);
        if (row) {
            for (int y = 0; y < drawH; y++) {
                int imgY = flipV ? (height - 1 - y) : y;
                f.seek(dataOffset + imgY * rowBytes);
                f.read(row, rowBytes);
                d_.drawBitmap(screenX, screenY + y, row, drawW, 1, GxEPD_WHITE, GxEPD_BLACK);
            }
            free(row);
        }
    } else if (bpp == 24) {
        int rowBytes = ((width * 3 + 3) / 4) * 4;
        uint8_t* row = (uint8_t*)malloc(min(rowBytes, 768));
        if (row) {
            for (int y = 0; y < drawH; y++) {
                int imgY = flipV ? (height - 1 - y) : y;
                f.seek(dataOffset + imgY * rowBytes);
                f.read(row, min(rowBytes, 768));
                for (int x = 0; x < drawW; x++) {
                    if (x * 3 + 2 < min(rowBytes, 768)) {
                        int idx = x * 3;
                        int gray = (row[idx] + row[idx+1] + row[idx+2]) / 3;
                        d_.drawPixel(screenX + x, screenY + y, gray > 128 ? GxEPD_WHITE : GxEPD_BLACK);
                    }
                }
            }
            free(row);
        }
    }

    f.close();
}

bool MapsApp::loadTile(int z, int x, int y, int screenX, int screenY) {
    char tilePath[80];
    if (!findTile(tilePath, sizeof(tilePath), z, x, y)) return false;

    const char* ext = strrchr(tilePath, '.');
    if (ext && strcasecmp(ext, ".bmp") == 0) {
        drawBmpAt(tilePath, screenX, screenY, TILE_SIZE, TILE_SIZE);
        return true;
    }

    // PNG/JPG tile — convert to temp BMP then draw
    ImageConvertConfig config;
    config.maxWidth = TILE_SIZE;
    config.maxHeight = TILE_SIZE;
    config.oneBit = true;
    config.quickMode = true;  // Fast threshold for tiles
    config.logTag = "MAP";

    if (ImageConverterFactory::convertToBmp(tilePath, MAPS_TMP_TILE, config)) {
        drawBmpAt(MAPS_TMP_TILE, screenX, screenY, TILE_SIZE, TILE_SIZE);
        return true;
    }

    return false;
}

void MapsApp::drawTilePlaceholder(int screenX, int screenY, int z, int x, int y) {
    d_.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
    for (int i = 0; i < TILE_SIZE; i += 16) {
        d_.drawPixel(screenX + i, screenY + i, GxEPD_BLACK);
        d_.drawPixel(screenX + TILE_SIZE - i - 1, screenY + i, GxEPD_BLACK);
    }
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    d_.setCursor(screenX + 4, screenY + 8);
    d_.print(x); d_.print(","); d_.print(y);
}

// =============================================================================
// Image Loading
// =============================================================================
void MapsApp::loadImageInfo(const char* path) {
    FsFile file = SdMan.open(path);
    if (!file) { _imgW = _screenW; _imgH = _screenH; return; }

    uint8_t header[32];
    file.read(header, 32);

    if (header[0] == 'B' && header[1] == 'M') {
        _imgW = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
        _imgH = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
        if (_imgH < 0) _imgH = -_imgH;
    } else if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G') {
        _imgW = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
        _imgH = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
    } else {
        _imgW = _screenW * 2;
        _imgH = _screenH * 2;
    }

    file.close();
}

void MapsApp::drawImageRegion(const char* path, int srcX, int srcY, int zoom) {
    FsFile f = SdMan.open(path);
    if (!f) { d_.setCursor(20, _screenH / 2); d_.print("Open failed"); return; }

    uint8_t header[54];
    if (f.read(header, 54) != 54 || header[0] != 'B' || header[1] != 'M') {
        f.close(); d_.setCursor(20, _screenH / 2); d_.print("Invalid BMP"); return;
    }

    int32_t width = abs(*(int32_t*)&header[18]);
    int32_t height = *(int32_t*)&header[22];
    uint16_t bpp = *(uint16_t*)&header[28];
    uint32_t dataOffset = *(uint32_t*)&header[10];

    bool flipV = (height > 0);
    height = abs(height);
    int displayH = _screenH - 30;

    if (bpp != 1 && bpp != 24) { f.close(); return; }

    if (zoom <= 1) {
        int offsetX = max(0, (_screenW - (int)width) / 2);
        int offsetY = max(0, (displayH - (int)height) / 2);
        int drawW = min((int)width, _screenW);
        int drawH = min((int)height, displayH);

        if (bpp == 1) {
            int rowBytes = ((width + 31) / 32) * 4;
            uint8_t* row = (uint8_t*)malloc(rowBytes);
            if (row) {
                for (int y = 0; y < drawH; y++) {
                    int imgY = flipV ? (height - 1 - y) : y;
                    f.seek(dataOffset + imgY * rowBytes);
                    f.read(row, rowBytes);
                    d_.drawBitmap(offsetX, offsetY + y, row, drawW, 1, GxEPD_WHITE, GxEPD_BLACK);
                }
                free(row);
            }
        } else {
            int rowBytes = ((width * 3 + 3) / 4) * 4;
            uint8_t* row = (uint8_t*)malloc(min(rowBytes, 2400));
            if (row) {
                for (int y = 0; y < drawH; y++) {
                    int imgY = flipV ? (height - 1 - y) : y;
                    f.seek(dataOffset + imgY * rowBytes);
                    f.read(row, min(rowBytes, 2400));
                    for (int x = 0; x < drawW; x++) {
                        if (x * 3 + 2 < min(rowBytes, 2400)) {
                            int idx = x * 3;
                            int gray = (row[idx] + row[idx+1] + row[idx+2]) / 3;
                            d_.drawPixel(offsetX + x, offsetY + y, gray > 128 ? GxEPD_WHITE : GxEPD_BLACK);
                        }
                    }
                }
                free(row);
            }
        }
    } else {
        int viewW = _screenW / zoom;
        int viewH = displayH / zoom;
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
                        if (black) d_.fillRect(sx * zoom, screenY, zoom, zoom, GxEPD_BLACK);
                    }
                }
                free(row);
            }
        } else {
            int rowBytes = ((width * 3 + 3) / 4) * 4;
            uint8_t* row = (uint8_t*)malloc(min(rowBytes, 2400));
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
                            if (gray <= 128) d_.fillRect(sx * zoom, screenY, zoom, zoom, GxEPD_BLACK);
                        }
                    }
                }
                free(row);
            }
        }
    }

    f.close();
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
