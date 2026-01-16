/**
 * @file Maps.h
 * @brief Offline tile-based map viewer for Sumi e-reader
 * @version 2.1.29
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

#ifndef SUMI_PLUGIN_MAPS_H
#define SUMI_PLUGIN_MAPS_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class MapsApp {
public:
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
    
    // For partial refresh support
    bool needsFullRedraw;
    
    MapsApp();
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();
    void drawPartial();
    
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

extern MapsApp mapsApp;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_MAPS_H
