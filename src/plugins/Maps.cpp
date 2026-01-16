/**
 * @file Maps.cpp
 * @brief Offline tile-based map viewer implementation
 * @version 2.1.29
 */

#include "plugins/Maps.h"

#if FEATURE_GAMES

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

MapsApp mapsApp;

// =============================================================================
// Constructor & Init
// =============================================================================
MapsApp::MapsApp() {
    reset();
}

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
bool MapsApp::handleInput(Button btn) {
    needsFullRedraw = true;
    
    if (_mode == MODE_BROWSER) {
        switch (btn) {
            case BTN_UP:
                if (_cursor > 0) {
                    _cursor--;
                    if (_cursor < _scroll) _scroll = _cursor;
                }
                return true;
            case BTN_DOWN:
                if (_cursor < _mapCount - 1) {
                    _cursor++;
                    if (_cursor >= _scroll + _itemsPerPage) _scroll++;
                }
                return true;
            case BTN_CONFIRM:
                if (_mapCount > 0) {
                    openMap(_cursor);
                }
                return true;
            case BTN_BACK:
                return false;  // Exit app
            default:
                return true;
        }
    }
    else if (_mode == MODE_TILES) {
        switch (btn) {
            case BTN_LEFT:
                _tileX--;
                return true;
            case BTN_RIGHT:
                _tileX++;
                return true;
            case BTN_UP:
                _tileY--;
                return true;
            case BTN_DOWN:
                _tileY++;
                return true;
            case BTN_CONFIRM:
                _mode = MODE_ZOOMING;
                needsFullRedraw = false;  // Just show zoom indicator
                return true;
            case BTN_BACK:
                _mode = MODE_BROWSER;
                return true;
            default:
                return true;
        }
    }
    else if (_mode == MODE_ZOOMING) {
        MapEntry& map = _maps[_currentMap];
        switch (btn) {
            case BTN_UP:
                if (_zoom < map.maxZoom) {
                    // Zoom in - tile coords double
                    _zoom++;
                    _tileX *= 2;
                    _tileY *= 2;
                }
                return true;
            case BTN_DOWN:
                if (_zoom > map.minZoom) {
                    // Zoom out - tile coords halve
                    _zoom--;
                    _tileX /= 2;
                    _tileY /= 2;
                }
                return true;
            case BTN_CONFIRM:
            case BTN_BACK:
                _mode = MODE_TILES;
                return true;
            default:
                return true;
        }
    }
    else if (_mode == MODE_IMAGE) {
        int step = TILE_SIZE / _imgZoom;  // Pan step size
        switch (btn) {
            case BTN_LEFT:
                _viewX -= step;
                if (_viewX < 0) _viewX = 0;
                return true;
            case BTN_RIGHT:
                _viewX += step;
                if (_viewX + _screenW / _imgZoom > _imgW) 
                    _viewX = max(0, _imgW - _screenW / _imgZoom);
                return true;
            case BTN_UP:
                _viewY -= step;
                if (_viewY < 0) _viewY = 0;
                return true;
            case BTN_DOWN:
                _viewY += step;
                if (_viewY + _screenH / _imgZoom > _imgH)
                    _viewY = max(0, _imgH - _screenH / _imgZoom);
                return true;
            case BTN_CONFIRM:
                // Cycle zoom: 1x -> 2x -> 4x -> 1x
                _imgZoom = _imgZoom >= 4 ? 1 : _imgZoom * 2;
                // Clamp view position
                if (_viewX + _screenW / _imgZoom > _imgW)
                    _viewX = max(0, _imgW - _screenW / _imgZoom);
                if (_viewY + _screenH / _imgZoom > _imgH)
                    _viewY = max(0, _imgH - _screenH / _imgZoom);
                return true;
            case BTN_BACK:
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
    PluginUI::drawHeader("Maps", _screenW);
    
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    if (_mapCount == 0) {
        display.setCursor(20, _screenH / 2 - 30);
        display.print("No maps found!");
        
        display.setCursor(20, _screenH / 2);
        display.print("Add tiles or images to /maps/");
        
        display.setCursor(20, _screenH / 2 + 30);
        display.setFont(&FreeSans9pt7b);
        display.print("Supports: PNG, BMP, OSM tiles");
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
            display.fillRoundRect(14, y - 4, _screenW - 28, _itemH - 4, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(14, y - 4, _screenW - 28, _itemH - 4, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        
        // Map icon
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(26, y + 22);
        display.print(map.isTileMap ? "T" : "I");  // T=Tiles, I=Image
        
        // Map name
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(60, y + 18);
        
        // Truncate long names
        char shortName[24];
        strncpy(shortName, map.name, 23);
        shortName[23] = '\0';
        if (strlen(map.name) > 20) {
            shortName[20] = '\0';
            strcat(shortName, "...");
        }
        display.print(shortName);
        
        // Type/zoom info
        display.setFont(&FreeSans9pt7b);
        display.setCursor(60, y + 36);
        if (map.isTileMap) {
            char info[32];
            snprintf(info, sizeof(info), "Tiles z%d-%d", map.minZoom, map.maxZoom);
            display.print(info);
        } else {
            display.print("Image");
        }
        
        display.setTextColor(GxEPD_BLACK);
        y += _itemH;
    }
    
    // Footer
    display.setFont(&FreeSans9pt7b);
    display.drawLine(0, _screenH - 40, _screenW, _screenH - 40, GxEPD_BLACK);
    
    char status[24];
    snprintf(status, sizeof(status), "%d/%d", _cursor + 1, _mapCount);
    display.setCursor(20, _screenH - 15);
    display.print(status);
    
    display.setCursor(_screenW - 80, _screenH - 15);
    display.print("OK: Open");
}

void MapsApp::drawTileMap() {
    display.fillScreen(GxEPD_WHITE);
    
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
    display.fillRect(0, _screenH - 30, _screenW, 30, GxEPD_WHITE);
    display.drawLine(0, _screenH - 30, _screenW, _screenH - 30, GxEPD_BLACK);
    
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    char info[48];
    snprintf(info, sizeof(info), "%s  Z:%d  [%d,%d]", 
             map.name, _zoom, _tileX, _tileY);
    display.setCursor(10, _screenH - 10);
    display.print(info);
    
    // Controls hint
    display.setCursor(_screenW - 120, _screenH - 10);
    display.print("OK: Zoom");
}

void MapsApp::drawZoomIndicator() {
    // Draw zoom selector overlay
    int boxW = 160;
    int boxH = 100;
    int boxX = (_screenW - boxW) / 2;
    int boxY = (_screenH - boxH) / 2;
    
    // White box with border
    display.fillRect(boxX, boxY, boxW, boxH, GxEPD_WHITE);
    display.drawRect(boxX, boxY, boxW, boxH, GxEPD_BLACK);
    display.drawRect(boxX + 1, boxY + 1, boxW - 2, boxH - 2, GxEPD_BLACK);
    
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Title
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds("ZOOM", 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(boxX + (boxW - tw) / 2, boxY + 28);
    display.print("ZOOM");
    
    // Current zoom level - big
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(2);
    char zoomStr[8];
    snprintf(zoomStr, sizeof(zoomStr), "%d", _zoom);
    display.getTextBounds(zoomStr, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(boxX + (boxW - tw) / 2, boxY + 65);
    display.print(zoomStr);
    display.setTextSize(1);
    
    // Range indicator
    MapEntry& map = _maps[_currentMap];
    display.setFont(&FreeSans9pt7b);
    char range[24];
    snprintf(range, sizeof(range), "(%d-%d)", map.minZoom, map.maxZoom);
    display.getTextBounds(range, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(boxX + (boxW - tw) / 2, boxY + boxH - 10);
    display.print(range);
}

void MapsApp::drawSingleImage() {
    display.fillScreen(GxEPD_WHITE);
    
    MapEntry& map = _maps[_currentMap];
    char path[64];
    snprintf(path, sizeof(path), "/maps/%s", map.name);
    
    drawImageRegion(path, _viewX, _viewY);
    
    // Info bar
    display.fillRect(0, _screenH - 30, _screenW, 30, GxEPD_WHITE);
    display.drawLine(0, _screenH - 30, _screenW, _screenH - 30, GxEPD_BLACK);
    
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    char info[48];
    snprintf(info, sizeof(info), "%s  %dx  [%d,%d]", 
             map.name, _imgZoom, _viewX, _viewY);
    display.setCursor(10, _screenH - 10);
    display.print(info);
    
    display.setCursor(_screenW - 100, _screenH - 10);
    display.print("OK: Zoom");
}

// =============================================================================
// Map Scanning
// =============================================================================
void MapsApp::scanMaps() {
    _mapCount = 0;
    
    File dir = SD.open("/maps");
    if (!dir) {
        Serial.println("[MAPS] Creating /maps directory");
        SD.mkdir("/maps");
        return;
    }
    
    while (File entry = dir.openNextFile()) {
        if (_mapCount >= MAX_MAPS) break;
        
        const char* name = entry.name();
        
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
    
    File dir = SD.open(path);
    if (!dir) return;
    
    int minFound = 99, maxFound = 0;
    
    while (File entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
            int z = atoi(entry.name());
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
    return SD.exists(path);
}

bool MapsApp::loadTile(int z, int x, int y, int screenX, int screenY) {
    char path[80];
    buildTilePath(path, sizeof(path), z, x, y);
    
    // Also try .jpg extension
    if (!SD.exists(path)) {
        snprintf(path + strlen(path) - 4, 5, ".jpg");
        if (!SD.exists(path)) {
            return false;
        }
    }
    
    File file = SD.open(path);
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
        display.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
        return true;
    }
    
    // For PNG tiles, draw as loaded indicator for now
    display.fillRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_WHITE);
    display.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
    
    // Draw tile coords in center
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    char coords[16];
    snprintf(coords, sizeof(coords), "%d/%d", x, y);
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(coords, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(screenX + (TILE_SIZE - tw) / 2, screenY + TILE_SIZE / 2);
    display.print(coords);
    
    return true;
}

void MapsApp::drawTilePlaceholder(int screenX, int screenY, int z, int x, int y) {
    // Draw empty tile with grid pattern
    display.drawRect(screenX, screenY, TILE_SIZE, TILE_SIZE, GxEPD_BLACK);
    
    // Light crosshatch pattern
    for (int i = 0; i < TILE_SIZE; i += 16) {
        display.drawPixel(screenX + i, screenY + i, GxEPD_BLACK);
        display.drawPixel(screenX + TILE_SIZE - i - 1, screenY + i, GxEPD_BLACK);
    }
    
    // Tile coordinates (small, in corner)
    display.setFont(nullptr);  // Default font
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(screenX + 4, screenY + 8);
    display.print(x);
    display.print(",");
    display.print(y);
}

// =============================================================================
// Image Loading
// =============================================================================
void MapsApp::loadImageInfo(const char* path) {
    File file = SD.open(path);
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
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Draw a mini-map showing current position
    int miniW = 100;
    int miniH = 75;
    int miniX = _screenW - miniW - 10;
    int miniY = 10;
    
    display.drawRect(miniX, miniY, miniW, miniH, GxEPD_BLACK);
    
    // Show viewport rectangle
    if (_imgW > 0 && _imgH > 0) {
        int viewW = _screenW / _imgZoom;
        int viewH = _screenH / _imgZoom;
        int rx = miniX + (srcX * miniW / _imgW);
        int ry = miniY + (srcY * miniH / _imgH);
        int rw = max(4, viewW * miniW / _imgW);
        int rh = max(4, viewH * miniH / _imgH);
        display.fillRect(rx, ry, rw, rh, GxEPD_BLACK);
    }
    
    // Instructions
    display.setCursor(20, _screenH / 2);
    display.print("Use D-pad to pan");
    display.setCursor(20, _screenH / 2 + 25);
    display.print("OK to change zoom");
    
    // For actual image display, you'd decode and blit the relevant region
    // This would require a proper image decoding library
}

#endif // FEATURE_GAMES
