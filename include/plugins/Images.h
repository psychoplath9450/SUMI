/**
 * @file Images.h
 * @brief Image viewer for Sumi e-reader
 * @version 1.3.0
 * 
 * Supports BMP and RAW images (1-bit preferred for e-ink)
 */

#ifndef SUMI_PLUGIN_IMAGES_H
#define SUMI_PLUGIN_IMAGES_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class ImagesApp {
public:
    static const int MAX_IMAGES = 20;
    static const int MAX_NAME_LEN = 24;
    
    enum ViewMode { MODE_BROWSER, MODE_VIEW, MODE_SLIDESHOW };
    
    ImagesApp();
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();
    
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


#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_IMAGES_H
