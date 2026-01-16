/**
 * @file ToolSuite.h
 * @brief Utility tools for Sumi e-reader
 * @version 2.1.17
 * 
 * Tools: Calculator, Timer, Stopwatch
 * Full-screen layouts with proper button handling
 * Uses runPluginSelfRefresh for fine-grained display control
 */

#ifndef SUMI_PLUGIN_TOOLSUITE_H
#define SUMI_PLUGIN_TOOLSUITE_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "core/PluginHelpers.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

class ToolSuiteApp {
public:
    enum Tool { TOOL_MENU, TOOL_CALC, TOOL_TIMER, TOOL_STOPWATCH };
    
    // Required by runPluginSelfRefresh
    bool needsFullRedraw;
    
    ToolSuiteApp();
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();
    void drawPartial();
    bool update();
    
private:
    Tool _tool;
    int _menuCursor;
    int _screenW, _screenH;
    bool _landscape;
    
    // Calculator state
    double _calcValue;
    double _calcOperand;
    char _calcOp;
    char _calcDisplay[16];
    int _calcDigits;
    bool _calcNewNumber;
    int _calcBtnRow, _calcBtnCol;
    int _prevBtnRow, _prevBtnCol;
    bool _calcDisplayDirty;
    
    // Calculator layout
    int _calcBtnW, _calcBtnH;
    int _calcStartX, _calcStartY;
    int _calcDispX, _calcDispY, _calcDispW, _calcDispH;
    
    // Timer state
    unsigned long _timerDuration;
    unsigned long _timerStart;
    bool _timerRunning;
    bool _timerDone;
    int _timerMinutes;
    unsigned long _timerLastUpdate;
    
    // Stopwatch state
    unsigned long _swStart;
    unsigned long _swElapsed;
    bool _swRunning;
    unsigned long _swLastUpdate;
    
    void reset();
    void calculateCalcLayout();
    
    // Menu
    bool handleMenu(Button btn);
    void drawMenuFull();
    
    // Calculator
    void resetCalc();
    bool handleCalc(Button btn);
    void pressCalcButton();
    void calculate();
    void drawCalcFull();
    void drawCalcPartial();
    
    // Timer
    void resetTimer();
    bool handleTimer(Button btn);
    void drawTimerFull();
    void drawTimerPartial();
    
    // Stopwatch
    void resetStopwatch();
    bool handleStopwatch(Button btn);
    void drawStopwatchFull();
    void drawStopwatchPartial();
};

extern ToolSuiteApp toolSuiteApp;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_TOOLSUITE_H
