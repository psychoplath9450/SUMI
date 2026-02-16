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
 * @file Maps.h
 * @brief Offline tile-based map viewer for Sumi e-reader
 * @version 1.0.0
 * 
 * Features:
 * - OpenStreetMap tile format support (z/x/y.png)
 * - Pan with D-pad (tile-by-tile)
 * - Zoom levels with UP/DOWN
 * - Only loads visible tiles (memory efficient)
 * - Also supports single large map images
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
    
    // Supported zoom levels (typical OSM range for offline use)
    static const int MIN_ZOOM = 8;
    static const int MAX_ZOOM = 18;
    
    enum ViewMode {
        MODE_BROWSER,    // Map/folder selection
        MODE_TILES,      // Tile-based map viewing
        MODE_IMAGE,      // Single image pan/zoom
        MODE_ZOOMING     // Zoom mode active
    };
    
    // needsFullRedraw inherited from PluginInterface
    
    explicit MapsApp(PluginRenderer& renderer) : d_(renderer) { reset(); }
    void init(int screenW, int screenH) override;
    bool handleInput(PluginButton btn) override;
    void draw() override;
    void drawPartial();
    
  PluginRenderer& d_;

private:
    // Map entries (can be folders with tiles or single images)
    struct MapEntry {
        char name[MAX_NAME_LEN];
        bool isTileMap;      // true = folder with z/x/y tiles
        int minZoom;         // Available zoom range (tiles only)
        int maxZoom;
    };
    
    MapEntry _maps[MAX_MAPS];
    int _mapCount;
    int _cursor, _scroll;
    
    // Current view state
    ViewMode _mode;
    int _currentMap;
    
    // Tile map state
    int _zoom;               // Current zoom level
    int _tileX, _tileY;      // Current center tile coordinates
    int _tilesX, _tilesY;    // Number of tiles visible on screen
    
    // Single image state
    int _imgW, _imgH;        // Full image dimensions
    int _viewX, _viewY;      // Current view position (top-left)
    int _imgZoom;            // Image zoom (1 = 1:1, 2 = 2x, etc)
    
    // Screen info
    int _screenW, _screenH;
    bool _landscape;
    int _itemH, _itemsPerPage;
    
    // Methods
    void reset();
    void scanMaps();
    void openMap(int idx);
    void detectZoomLevels(int idx);
    
    // Drawing
    void drawBrowser();
    void drawTileMap();
    void drawSingleImage();
    void drawZoomIndicator();
    
    // Tile handling
    bool loadTile(int z, int x, int y, int screenX, int screenY);
    void drawTilePlaceholder(int screenX, int screenY, int z, int x, int y);
    
    // Image handling
    void loadImageInfo(const char* path);
    void drawImageRegion(const char* path, int srcX, int srcY);
    
    // Helpers
    void buildTilePath(char* buf, int bufSize, int z, int x, int y);
    bool tileExists(int z, int x, int y);
};


// === IMPLEMENTATION ===

/**
 * @file Maps.cpp
 * @brief Offline tile-based map viewer implementation
 * @version 1.0.0
 */

// =============================================================================
// Constructor & Init
// =============================================================================

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
    needsFullRedraw = true;
}

void MapsApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _itemH = 52;
    _itemsPerPage = (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 20) / _itemH;
    if (_itemsPerPage < 1) _itemsPerPage = 1;
    
    // Calculate tiles that fit on screen
    _tilesX = (_screenW + TILE_SIZE - 1) / TILE_SIZE + 1;  // +1 for partial tiles
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
                if (_mapCount > 0) {
                    openMap(_cursor);
                }
                return true;
            case PluginButton::Back:
                return false;  // Exit app
            default:
                return true;
        }
    }
    else if (_mode == MODE_TILES) {
        switch (btn) {
            case PluginButton::Left:
                _tileX--;
                return true;
            case PluginButton::Right:
                _tileX++;
                return true;
            case PluginButton::Up:
                _tileY--;
                return true;
            case PluginButton::Down:
                _tileY++;
                return true;
            case PluginButton::Center:
                _mode = MODE_ZOOMING;
                needsFullRedraw = false;  // Just show zoom indicator
                return true;
            case PluginButton::Back:
                _mode = MODE_BROWSER;
                return true;
            default:
                return true;
        }
    }
    else if (_mode == MODE_ZOOMING) {
        MapEntry& map = _maps[_currentMap];
        switch (btn) {
            case PluginButton::Up:
                if (_zoom < map.maxZoom) {
                    // Zoom in - tile coords double
                    _zoom++;
                    _tileX *= 2;
                    _tileY *= 2;
                }
                return true;
            case PluginButton::Down:
                if (_zoom > map.minZoom) {
                    // Zoom out - tile coords halve
                    _zoom--;
                    _tileX /= 2;
                    _tileY /= 2;
                }
                return true;
            case PluginButton::Center:
            case PluginButton::Back:
                _mode = MODE_TILES;
                return true;
            default:
                return true;
        }
    }
    else if (_mode == MODE_IMAGE) {
        int step = TILE_SIZE / _imgZoom;  // Pan step size
        switch (btn) {
            case PluginButton::Left:
                _viewX -= step;
                if (_viewX < 0) _viewX = 0;
                return true;
            case PluginButton::Right:
                _viewX += step;
                if (_viewX + _screenW / _imgZoom > _imgW) 
                    _viewX = max(0, _imgW - _screenW / _imgZoom);
                return true;
            case PluginButton::Up:
                _viewY -= step;
                if (_viewY < 0) _viewY = 0;
                return true;
            case PluginButton::Down:
                _viewY += step;
                if (_viewY + _screenH / _imgZoom > _imgH)
                    _viewY = max(0, _imgH - _screenH / _imgZoom);
                return true;
            case PluginButton::Center:
                // Cycle zoom: 1x -> 2x -> 4x -> 1x
                _imgZoom = _imgZoom >= 4 ? 1 : _imgZoom * 2;
                // Clamp view position
                if (_viewX + _screenW / _imgZoom > _imgW)
                    _viewX = max(0, _imgW - _screenW / _imgZoom);
                if (_viewY + _screenH / _imgZoom > _imgH)
                    _viewY = max(0, _imgH - _screenH / _imgZoom);
                return true;
            case PluginButton::Back:
                _mode = MODE_BROWSER;
                return true;
            default:
                return true;
        }
    }
    
    return true;
}

// =============================================================================
// Drawing
// =============================================================================
void MapsApp::draw() {
    if (_mode == MODE_BROWSER) {
        drawBrowser();
    } else if (_mode == MODE_TILES || _mode == MODE_ZOOMING) {
        drawTileMap();
        if (_mode == MODE_ZOOMING) {
            drawZoomIndicator();
        }
    } else if (_mode == MODE_IMAGE) {
        drawSingleImage();
    }
}

void MapsApp::drawPartial() {
    // For now, just do full draw
    draw();
}

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
        d_.setFont(nullptr);
        d_.print("Supports: PNG, BMP, OSM tiles");
        return;
    }
    
    int y = 60;
    int endIdx = _scroll + _itemsPerPage;
    if (endIdx > _mapCount) endIdx = _mapCount;
    
    for (int i = _scroll; i < endIdx; i++) {
        bool sel = (i == _cursor);
        MapEntry& map = _maps[i];
        
        // Selection highlight
        if (sel) {
            d_.fillRoundRect(14, y - 4, _screenW - 28, _itemH - 4, 6, GxEPD_BLACK);
            d_.setTextColor(GxEPD_WHITE);
        } else {
            d_.drawRoundRect(14, y - 4, _screenW - 28, _itemH - 4, 6, GxEPD_BLACK);
            d_.setTextColor(GxEPD_BLACK);
        }
        
        // Map icon
        d_.setFont(nullptr);
        d_.setCursor(26, y + 22);
        d_.print(map.isTileMap ? "T" : "I");  // T=Tiles, I=Image
        
        // Map name
        d_.setFont(nullptr);
        d_.setCursor(60, y + 18);
        
        // Truncate long names
        char shortName[24];
        strncpy(shortName, map.name, 23);
        shortName[23] = '\0';
        if (strlen(map.name) > 20) {
            shortName[20] = '\0';
            strcat(shortName, "...");
        }
        d_.print(shortName);
        
        // Type/zoom info
        d_.setFont(nullptr);
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
    
    // Footer
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
    
    // Calculate starting tile position to center view
    int startTileX = _tileX - _tilesX / 2;
    int startTileY = _tileY - _tilesY / 2;
    
    // Draw visible tiles
    for (int ty = 0; ty < _tilesY; ty++) {
        for (int tx = 0; tx < _tilesX; tx++) {
            int tileXCoord = startTileX + tx;
            int tileYCoord = startTileY + ty;
            int screenX = tx * TILE_SIZE;
            int screenY = ty * TILE_SIZE;
            
            // Try to load and draw the tile
            if (!loadTile(_zoom, tileXCoord, tileYCoord, screenX, screenY)) {
                drawTilePlaceholder(screenX, screenY, _zoom, tileXCoord, tileYCoord);
            }
        }
    }
    
    // Draw info bar at bottom
    d_.fillRect(0, _screenH - 30, _screenW, 30, GxEPD_WHITE);
    d_.drawLine(0, _screenH - 30, _screenW, _screenH - 30, GxEPD_BLACK);
    
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    
    char info[48];
    snprintf(info, sizeof(info), "%s  Z:%d  [%d,%d]", 
             map.name, _zoom, _tileX, _tileY);
    d_.setCursor(10, _screenH - 10);
    d_.print(info);
    
    // Controls hint
    d_.setCursor(_screenW - 120, _screenH - 10);
    d_.print("OK: Zoom");
}

void MapsApp::drawZoomIndicator() {
    // Draw zoom selector overlay
    int boxW = 160;
    int boxH = 100;
    int boxX = (_screenW - boxW) / 2;
    int boxY = (_screenH - boxH) / 2;
    
    // White box with border
    d_.fillRect(boxX, boxY, boxW, boxH, GxEPD_WHITE);
    d_.drawRect(boxX, boxY, boxW, boxH, GxEPD_BLACK);
    d_.drawRect(boxX + 1, boxY + 1, boxW - 2, boxH - 2, GxEPD_BLACK);
    
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    
    // Title
    int16_t tx, ty; uint16_t tw, th;
    d_.getTextBounds("ZOOM", 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(boxX + (boxW - tw) / 2, boxY + 28);
    d_.print("ZOOM");
    
    // Current zoom level - big
    d_.setFont(nullptr);
    d_.setTextSize(2);
    char zoomStr[8];
    snprintf(zoomStr, sizeof(zoomStr), "%d", _zoom);
    d_.getTextBounds(zoomStr, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(boxX + (boxW - tw) / 2, boxY + 65);
    d_.print(zoomStr);
    d_.setTextSize(1);
    
    // Range indicator
    MapEntry& map = _maps[_currentMap];
    d_.setFont(nullptr);
    char range[24];
    snprintf(range, sizeof(range), "(%d-%d)", map.minZoom, map.maxZoom);
    d_.getTextBounds(range, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(boxX + (boxW - tw) / 2, boxY + boxH - 10);
    d_.print(range);
}

void MapsApp::drawSingleImage() {
    d_.fillScreen(GxEPD_WHITE);
    
    MapEntry& map = _maps[_currentMap];
    char path[64];
    snprintf(path, sizeof(path), "/maps/%s", map.name);
    
    drawImageRegion(path, _viewX, _viewY);
    
    // Info bar
    d_.fillRect(0, _screenH - 30, _screenW, 30, GxEPD_WHITE);
    d_.drawLine(0, _screenH - 30, _screenW, _screenH - 30, GxEPD_BLACK);
    
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    
    char info[48];
    snprintf(info, sizeof(info), "%s  %dx  [%d,%d]", 
             map.name, _imgZoom, _viewX, _viewY);
    d_.setCursor(10, _screenH - 10);
    d_.print(info);
    
    d_.setCursor(_screenW - 100, _screenH - 10);
    d_.print("OK: Zoom");
}

// =============================================================================
// Map Scanning
// =============================================================================
void MapsApp::scanMaps() {
    _mapCount = 0;
    
    FsFile dir = SdMan.open("/maps");
    if (!dir) {
        Serial.println("[MAPS] Creating /maps directory");
        SdMan.mkdir("/maps");
        return;
    }
    
    while (FsFile entry = dir.openNextFile()) {
        if (_mapCount >= MAX_MAPS) break;
        
        char name[64]; entry.getName(name, sizeof(name));
        
        // Skip hidden files
        if (name[0] == '.') {
            entry.close();
            continue;
        }
        
        MapEntry& map = _maps[_mapCount];
        strncpy(map.name, name, MAX_NAME_LEN - 1);
        map.name[MAX_NAME_LEN - 1] = '\0';
        
        if (entry.isDirectory()) {
            // Check if it's a tile map (has numbered subdirectories)
            map.isTileMap = true;
            map.minZoom = MIN_ZOOM;
            map.maxZoom = MAX_ZOOM;
            detectZoomLevels(_mapCount);
            _mapCount++;
        } else {
            // Check if it's a supported image format
            const char* ext = strrchr(name, '.');
            if (ext && (strcasecmp(ext, ".png") == 0 || 
                       strcasecmp(ext, ".bmp") == 0 ||
                       strcasecmp(ext, ".jpg") == 0)) {
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
        Serial.printf("[MAPS] %s: zoom %d-%d\n", map.name, minFound, maxFound);
    }
}

void MapsApp::openMap(int idx) {
    _currentMap = idx;
    MapEntry& map = _maps[idx];
    
    if (map.isTileMap) {
        _mode = MODE_TILES;
        _zoom = (map.minZoom + map.maxZoom) / 2;  // Start at middle zoom
        
        // Find center tile (roughly center of available tiles)
        // For now, start at 0,0 - user can navigate
        _tileX = 1 << (_zoom - 1);  // Middle of world at this zoom
        _tileY = 1 << (_zoom - 1);
        
        Serial.printf("[MAPS] Opening tile map: %s at z%d\n", map.name, _zoom);
    } else {
        _mode = MODE_IMAGE;
        _viewX = 0;
        _viewY = 0;
        _imgZoom = 1;
        
        char path[64];
        snprintf(path, sizeof(path), "/maps/%s", map.name);
        loadImageInfo(path);
        
        Serial.printf("[MAPS] Opening image: %s (%dx%d)\n", map.name, _imgW, _imgH);
    }
}

// =============================================================================
// Tile Loading
// =============================================================================
void MapsApp::buildTilePath(char* buf, int bufSize, int z, int x, int y) {
    MapEntry& map = _maps[_currentMap];
    snprintf(buf, bufSize, "/maps/%s/%d/%d/%d.png", map.name, z, x, y);
}

bool MapsApp::tileExists(int z, int x, int y) {
    char path[80];
    buildTilePath(path, sizeof(path), z, x, y);
    return SdMan.exists(path);
}

bool MapsApp::loadTile(int z, int x, int y, int screenX, int screenY) {
    char path[80];
    buildTilePath(path, sizeof(path), z, x, y);
    
    // Also try .jpg extension
    if (!SdMan.exists(path)) {
        snprintf(path + strlen(path) - 4, 5, ".jpg");
        if (!SdMan.exists(path)) {
            return false;
        }
    }
    
    FsFile file = SdMan.open(path);
    if (!file) return false;
    
    // For now, just draw a placeholder - proper PNG decoding would need a library
    // In a real implementation, you'd use a PNG decoder here
    
    // Simple BMP loading for tiles (if they're BMP format)
    // Check magic number
    uint8_t header[2];
    file.read(header, 2);
    file.close();
    
    if (header[0] == 'B' && header[1] == 'M') {
        // It's a BMP - draw it
        // (simplified - would need proper BMP decoder for arbitrary sizes)
        d_.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
        return true;
    }
    
    // For PNG tiles, draw as loaded indicator for now
    d_.fillRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_WHITE);
    d_.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
    
    // Draw tile coords in center
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    char coords[16];
    snprintf(coords, sizeof(coords), "%d/%d", x, y);
    int16_t tx, ty; uint16_t tw, th;
    d_.getTextBounds(coords, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(screenX + (TILE_SIZE - tw) / 2, screenY + TILE_SIZE / 2);
    d_.print(coords);
    
    return true;
}

void MapsApp::drawTilePlaceholder(int screenX, int screenY, int z, int x, int y) {
    // Draw empty tile with grid pattern
    d_.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
    
    // Light crosshatch pattern
    for (int i = 0; i < TILE_SIZE; i += 16) {
        d_.drawPixel(screenX + i, screenY + i, GxEPD_BLACK);
        d_.drawPixel(screenX + TILE_SIZE - i - 1, screenY + i, GxEPD_BLACK);
    }
    
    // Tile coordinates (small, in corner)
    d_.setFont(nullptr);  // Default font
    d_.setTextColor(GxEPD_BLACK);
    d_.setCursor(screenX + 4, screenY + 8);
    d_.print(x);
    d_.print(",");
    d_.print(y);
}

// =============================================================================
// Image Loading
// =============================================================================
void MapsApp::loadImageInfo(const char* path) {
    FsFile file = SdMan.open(path);
    if (!file) {
        _imgW = _screenW;
        _imgH = _screenH;
        return;
    }
    
    // Check file type and read dimensions
    uint8_t header[32];
    file.read(header, 32);
    
    if (header[0] == 'B' && header[1] == 'M') {
        // BMP file
        _imgW = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
        _imgH = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
        if (_imgH < 0) _imgH = -_imgH;  // Handle bottom-up BMPs
    } else if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G') {
        // PNG file - dimensions at bytes 16-23 (width, height as 4-byte big-endian)
        _imgW = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
        _imgH = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
    } else {
        // Unknown format - assume screen size
        _imgW = _screenW * 2;
        _imgH = _screenH * 2;
    }
    
    file.close();
    Serial.printf("[MAPS] Image: %dx%d\n", _imgW, _imgH);
}

void MapsApp::drawImageRegion(const char* path, int srcX, int srcY) {
    // Simplified image drawing - for a real implementation you'd want
    // proper partial image decoding
    
    // For now, show position indicator
    d_.setFont(nullptr);
    d_.setTextColor(GxEPD_BLACK);
    
    // Draw a mini-map showing current position
    int miniW = 100;
    int miniH = 75;
    int miniX = _screenW - miniW - 10;
    int miniY = 10;
    
    d_.drawRect(miniX, miniY, miniW, miniH, GxEPD_BLACK);
    
    // Show viewport rectangle
    if (_imgW > 0 && _imgH > 0) {
        int viewW = _screenW / _imgZoom;
        int viewH = _screenH / _imgZoom;
        int rx = miniX + (srcX * miniW / _imgW);
        int ry = miniY + (srcY * miniH / _imgH);
        int rw = max(4, viewW * miniW / _imgW);
        int rh = max(4, viewH * miniH / _imgH);
        d_.fillRect(rx, ry, rw, rh, GxEPD_BLACK);
    }
    
    // Instructions
    d_.setCursor(20, _screenH / 2);
    d_.print("Use D-pad to pan");
    d_.setCursor(20, _screenH / 2 + 25);
    d_.print("OK to change zoom");
    
    // For actual image display, you'd decode and blit the relevant region
    // This would require a proper image decoding library
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
