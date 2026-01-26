/**
 * @file TodoList.h
 * @brief Simple to-do list plugin
 * @version 1.3.0
 */

#ifndef SUMI_PLUGIN_TODO_H
#define SUMI_PLUGIN_TODO_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class TodoApp {
public:
    static const int MAX_VISIBLE = 8;
    static const int MAX_TEXT = 40;
    
    TodoApp();
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();

private:
    char _texts[MAX_VISIBLE][MAX_TEXT];
    bool _done[MAX_VISIBLE];
    int _visibleCount;
    int _scrollStart;
    int _cursor, _count;
    int _screenW, _screenH;
    bool _landscape;
    int _itemH;
    
    void loadVisible();
    void toggleCurrent();
};


#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_TODO_H
