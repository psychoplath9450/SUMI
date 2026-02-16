#pragma once

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

/**
 * @file ToolSuite.h
 * @brief Utility tools for Sumi e-reader
 * @version 1.0.0
 * 
 * Tools: Calculator, Timer, Stopwatch
 * Full-screen layouts with proper button handling
 * Uses runPluginSelfRefresh for fine-grained display control
 */


class ToolSuiteApp : public PluginInterface {
public:

  const char* name() const override { return "Tools"; }
  PluginRunMode runMode() const override { return PluginRunMode::WithUpdate; }
    enum Tool { TOOL_MENU, TOOL_CALC, TOOL_TIMER, TOOL_STOPWATCH };
    
    // needsFullRedraw inherited from PluginInterface
    
    explicit ToolSuiteApp(PluginRenderer& renderer) : d_(renderer) { reset(); }
    void init(int screenW, int screenH) override;
    bool handleInput(PluginButton btn) override;
    void draw() override;
    void drawPartial();
    void drawFullScreen() { draw(); }
    bool update() override;
    
  PluginRenderer& d_;

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
    bool handleMenu(PluginButton btn);
    void drawMenuFull();
    
    // Calculator
    void resetCalc();
    bool handleCalc(PluginButton btn);
    void pressCalcButton();
    void calculate();
    void drawCalcFull();
    void drawCalcPartial();
    
    // Timer
    void resetTimer();
    bool handleTimer(PluginButton btn);
    void drawTimerFull();
    void drawTimerPartial();
    
    // Stopwatch
    void resetStopwatch();
    bool handleStopwatch(PluginButton btn);
    void drawStopwatchFull();
    void drawStopwatchPartial();
};


// === IMPLEMENTATION ===

/**
 * @file ToolSuite.cpp
 * @brief Utility tools implementation
 * @version 1.0.0
 * 
 * Uses runPluginSelfRefresh for proper display control
 * Raw button handling (no remapping)
 */

static const char* CALC_LABELS[] = {
    "C", "(", ")", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", ".", "=", " "
};

static const char CALC_CHARS[] = "C()/789*456-123+0.= ";

// =============================================================================
// Constructor & Init
// =============================================================================


void ToolSuiteApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    needsFullRedraw = true;
    
    Serial.printf("[TOOLS] init: %dx%d, landscape=%d\n", screenW, screenH, _landscape);
    
    _tool = TOOL_MENU;
    _menuCursor = 0;
    resetCalc();
    resetTimer();
    resetStopwatch();
    calculateCalcLayout();
}

void ToolSuiteApp::reset() {
    _tool = TOOL_MENU;
    _menuCursor = 0;
    needsFullRedraw = true;
}

void ToolSuiteApp::calculateCalcLayout() {
    int availH = _screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 20;
    int dispH = 60;
    int gridH = availH - dispH - 20;
    int gridW = _screenW - 40;
    
    _calcBtnW = (gridW - 3 * 8) / 4;
    _calcBtnH = (gridH - 4 * 8) / 5;
    
    if (_calcBtnH > 70) _calcBtnH = 70;
    if (_calcBtnW > 120) _calcBtnW = 120;
    
    int actualGridW = 4 * _calcBtnW + 3 * 8;
    int actualGridH = 5 * _calcBtnH + 4 * 8;
    
    _calcStartX = (_screenW - actualGridW) / 2;
    _calcStartY = PLUGIN_HEADER_H + dispH + 30;
    
    _calcDispW = actualGridW;
    _calcDispH = dispH;
    _calcDispX = _calcStartX;
    _calcDispY = PLUGIN_HEADER_H + 15;
    
    Serial.printf("[TOOLS] Calc layout: btnW=%d, btnH=%d\n", _calcBtnW, _calcBtnH);
}

// =============================================================================
// Main Interface
// =============================================================================

bool ToolSuiteApp::handleInput(PluginButton btn) {
    Serial.printf("[TOOLS] handleInput: btn=%d, tool=%d\n", btn, _tool);
    
    switch (_tool) {
        case TOOL_MENU:     return handleMenu(btn);
        case TOOL_CALC:     return handleCalc(btn);
        case TOOL_TIMER:    return handleTimer(btn);
        case TOOL_STOPWATCH: return handleStopwatch(btn);
    }
    return false;
}

void ToolSuiteApp::draw() {
    Serial.printf("[TOOLS] draw() tool=%d\n", _tool);
    
    d_.setPartialWindow(0, 0, d_.width(), d_.height()); // NO BLACK FLASH
    d_.firstPage();
    do {
        d_.fillScreen(GxEPD_WHITE);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        
        switch (_tool) {
            case TOOL_MENU:     drawMenuFull(); break;
            case TOOL_CALC:     drawCalcFull(); break;
            case TOOL_TIMER:    drawTimerFull(); break;
            case TOOL_STOPWATCH: drawStopwatchFull(); break;
        }
    } while (d_.nextPage());
    
    needsFullRedraw = false;
}

void ToolSuiteApp::drawPartial() {
    Serial.printf("[TOOLS] drawPartial() tool=%d\n", _tool);
    
    switch (_tool) {
        case TOOL_MENU:
            // Menu always needs full redraw
            draw();
            break;
        case TOOL_CALC:
            drawCalcPartial();
            break;
        case TOOL_TIMER:
            drawTimerPartial();
            break;
        case TOOL_STOPWATCH:
            drawStopwatchPartial();
            break;
    }
}

bool ToolSuiteApp::update() {
    if (_tool == TOOL_TIMER && _timerRunning) {
        unsigned long elapsed = millis() - _timerStart;
        if (elapsed >= _timerDuration) {
            _timerRunning = false;
            _timerDone = true;
            needsFullRedraw = true;
            return true;
        }
        if (millis() - _timerLastUpdate >= 1000) {
            _timerLastUpdate = millis();
            return true;
        }
    }
    
    if (_tool == TOOL_STOPWATCH && _swRunning) {
        if (millis() - _swLastUpdate >= 100) {
            _swLastUpdate = millis();
            return true;
        }
    }
    
    return false;
}

// =============================================================================
// MENU
// =============================================================================

bool ToolSuiteApp::handleMenu(PluginButton btn) {
    switch (btn) {
        case PluginButton::Up:
            if (_menuCursor > 0) _menuCursor--;
            needsFullRedraw = true;
            return true;
        case PluginButton::Down:
            if (_menuCursor < 2) _menuCursor++;
            needsFullRedraw = true;
            return true;
        case PluginButton::Center:
            needsFullRedraw = true;
            if (_menuCursor == 0) { _tool = TOOL_CALC; resetCalc(); }
            else if (_menuCursor == 1) { _tool = TOOL_TIMER; }
            else if (_menuCursor == 2) { _tool = TOOL_STOPWATCH; }
            return true;
        case PluginButton::Back:
            return false;
        default:
            return true;
    }
}

void ToolSuiteApp::drawMenuFull() {
    PluginUI::drawHeader(d_, "Tools", _screenW);
    
    const char* items[] = { "Calculator", "Timer", "Stopwatch" };
    
    int itemW = min(300, _screenW - 60);
    int itemH = 50;
    int spacing = 15;
    int totalH = 3 * itemH + 2 * spacing;
    int startY = PLUGIN_HEADER_H + (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - totalH) / 2;
    int x = (_screenW - itemW) / 2;
    
    for (int i = 0; i < 3; i++) {
        int y = startY + i * (itemH + spacing);
        PluginUI::drawMenuItem(d_, items[i], x, y, itemW, itemH, i == _menuCursor);
    }
    
    PluginUI::drawFooter(d_, "", "OK:Select  BACK:Exit", _screenW, _screenH);
}

// =============================================================================
// CALCULATOR
// =============================================================================

void ToolSuiteApp::resetCalc() {
    _calcValue = 0;
    _calcOperand = 0;
    _calcOp = '\0';
    snprintf(_calcDisplay, sizeof(_calcDisplay), "0");
    _calcDigits = 0;
    _calcNewNumber = true;
    _calcBtnRow = 3;
    _calcBtnCol = 1;
    _prevBtnRow = _calcBtnRow;
    _prevBtnCol = _calcBtnCol;
    _calcDisplayDirty = false;
    needsFullRedraw = true;
}

bool ToolSuiteApp::handleCalc(PluginButton btn) {
    _prevBtnRow = _calcBtnRow;
    _prevBtnCol = _calcBtnCol;
    
    switch (btn) {
        case PluginButton::Up:
            if (_calcBtnRow > 0) _calcBtnRow--;
            return true;
        case PluginButton::Down:
            if (_calcBtnRow < 4) _calcBtnRow++;
            return true;
        case PluginButton::Left:
            if (_calcBtnCol > 0) _calcBtnCol--;
            return true;
        case PluginButton::Right:
            if (_calcBtnCol < 3) _calcBtnCol++;
            return true;
        case PluginButton::Center:
            pressCalcButton();
            _calcDisplayDirty = true;
            // Partial refresh is fine for calculator display updates
            return true;
        case PluginButton::Back:
            _tool = TOOL_MENU;
            needsFullRedraw = true;
            return true;
        default:
            return true;
    }
}

void ToolSuiteApp::pressCalcButton() {
    char btn = CALC_CHARS[_calcBtnRow * 4 + _calcBtnCol];
    Serial.printf("[CALC] Button: '%c'\n", btn);
    
    if (btn >= '0' && btn <= '9') {
        if (_calcNewNumber) {
            _calcDisplay[0] = btn;
            _calcDisplay[1] = '\0';
            _calcDigits = 1;
            _calcNewNumber = false;
        } else if (_calcDigits < 12) {
            _calcDisplay[_calcDigits++] = btn;
            _calcDisplay[_calcDigits] = '\0';
        }
        _calcValue = atof(_calcDisplay);
    }
    else if (btn == '.') {
        if (!strchr(_calcDisplay, '.') && _calcDigits < 12) {
            if (_calcNewNumber) {
                strcpy(_calcDisplay, "0.");
                _calcDigits = 2;
                _calcNewNumber = false;
            } else {
                _calcDisplay[_calcDigits++] = '.';
                _calcDisplay[_calcDigits] = '\0';
            }
        }
    }
    else if (btn == 'C') {
        resetCalc();
    }
    else if (btn == '+' || btn == '-' || btn == '*' || btn == '/') {
        if (_calcOp != '\0' && !_calcNewNumber) calculate();
        _calcOperand = _calcValue;
        _calcOp = btn;
        _calcNewNumber = true;
    }
    else if (btn == '=') {
        if (_calcOp != '\0') {
            calculate();
            _calcOp = '\0';
        }
        _calcNewNumber = true;
    }
}

void ToolSuiteApp::calculate() {
    double result = 0;
    switch (_calcOp) {
        case '+': result = _calcOperand + _calcValue; break;
        case '-': result = _calcOperand - _calcValue; break;
        case '*': result = _calcOperand * _calcValue; break;
        case '/': 
            if (_calcValue != 0) result = _calcOperand / _calcValue;
            else { snprintf(_calcDisplay, sizeof(_calcDisplay), "Error"); return; }
            break;
        default: return;
    }
    
    _calcValue = result;
    if (result == (long)result && result >= -999999999 && result <= 999999999) {
        snprintf(_calcDisplay, sizeof(_calcDisplay), "%ld", (long)result);
    } else {
        snprintf(_calcDisplay, sizeof(_calcDisplay), "%.6g", result);
    }
    _calcDigits = strlen(_calcDisplay);
}

void ToolSuiteApp::drawCalcFull() {
    PluginUI::drawHeader(d_, "Calculator", _screenW);
    
    // Display area
    d_.drawRect(_calcDispX, _calcDispY, _calcDispW, _calcDispH, GxEPD_BLACK);
    d_.drawRect(_calcDispX + 1, _calcDispY + 1, _calcDispW - 2, _calcDispH - 2, GxEPD_BLACK);
    
    // Display text
    d_.setTextSize(2);
    int16_t tx, ty; uint16_t tw, th;
    d_.getTextBounds(_calcDisplay, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(_calcDispX + _calcDispW - tw - 15, _calcDispY + (_calcDispH + th) / 2);
    d_.print(_calcDisplay);
    d_.setTextSize(1);
    
    if (_calcOp != '\0') {
        d_.setCursor(_calcDispX + 10, _calcDispY + 25);
        d_.print(_calcOp);
    }
    
    // Draw buttons
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int idx = row * 4 + col;
            int x = _calcStartX + col * (_calcBtnW + 8);
            int y = _calcStartY + row * (_calcBtnH + 8);
            bool sel = (row == _calcBtnRow && col == _calcBtnCol);
            
            if (sel) {
                d_.fillRect(x, y, _calcBtnW, _calcBtnH, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRect(x, y, _calcBtnW, _calcBtnH, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setTextSize(2);
            int16_t tx, ty; uint16_t tw, th;
            d_.getTextBounds(CALC_LABELS[idx], 0, 0, &tx, &ty, &tw, &th);
            d_.setCursor(x + (_calcBtnW - tw) / 2, y + (_calcBtnH + th) / 2);
            d_.print(CALC_LABELS[idx]);
            d_.setTextSize(1);
            d_.setTextColor(GxEPD_BLACK);
        }
    }
    
    PluginUI::drawFooter(d_, "", "BACK:Menu", _screenW, _screenH);
}

void ToolSuiteApp::drawCalcPartial() {
    // For simplicity, do full redraw for calculator cursor movement
    draw();
}

// =============================================================================
// TIMER
// =============================================================================

void ToolSuiteApp::resetTimer() {
    _timerDuration = 0;
    _timerRunning = false;
    _timerDone = false;
    _timerMinutes = 5;
    _timerLastUpdate = 0;
    needsFullRedraw = true;
}

bool ToolSuiteApp::handleTimer(PluginButton btn) {
    switch (btn) {
        case PluginButton::Up:
            if (!_timerRunning && !_timerDone && _timerMinutes < 120) {
                _timerMinutes++;
                needsFullRedraw = true;
            }
            return true;
        case PluginButton::Down:
            if (!_timerRunning && !_timerDone && _timerMinutes > 1) {
                _timerMinutes--;
                needsFullRedraw = true;
            }
            return true;
        case PluginButton::Left:
            if (!_timerRunning && !_timerDone && _timerMinutes > 5) {
                _timerMinutes -= 5;
                needsFullRedraw = true;
            }
            return true;
        case PluginButton::Right:
            if (!_timerRunning && !_timerDone && _timerMinutes < 115) {
                _timerMinutes += 5;
                needsFullRedraw = true;
            }
            return true;
        case PluginButton::Center:
            if (_timerDone) {
                _timerDone = false;
            } else if (_timerRunning) {
                _timerRunning = false;
            } else {
                _timerDuration = _timerMinutes * 60000UL;
                _timerStart = millis();
                _timerLastUpdate = _timerStart;
                _timerRunning = true;
            }
            needsFullRedraw = true;
            return true;
        case PluginButton::Back:
            if (_timerRunning) {
                _timerRunning = false;
                needsFullRedraw = true;
                return true;
            }
            _tool = TOOL_MENU;
            needsFullRedraw = true;
            return true;
        default:
            return true;
    }
}

void ToolSuiteApp::drawTimerFull() {
    PluginUI::drawHeader(d_, "Timer", _screenW);
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    
    if (_timerDone) {
        d_.setTextSize(3);
        const char* msg = "TIME UP!";
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(msg, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(centerX - tw/2, centerY);
        d_.print(msg);
        d_.setTextSize(1);
        d_.setCursor(centerX - 60, centerY + 50);
        d_.print("Press OK to reset");
    } else if (_timerRunning) {
        unsigned long remaining = _timerDuration - (millis() - _timerStart);
        int mins = remaining / 60000;
        int secs = (remaining / 1000) % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        
        d_.setTextSize(4);
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(centerX - tw/2, centerY);
        d_.print(buf);
        d_.setTextSize(1);
        d_.setCursor(centerX - 40, centerY + 60);
        d_.print("OK: Pause");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d min", _timerMinutes);
        
        d_.setTextSize(3);
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(centerX - tw/2, centerY - 20);
        d_.print(buf);
        d_.setTextSize(1);
        d_.setCursor(centerX - 80, centerY + 40);
        d_.print("UP/DOWN: +/- 1 min");
        d_.setCursor(centerX - 80, centerY + 60);
        d_.print("LEFT/RIGHT: +/- 5 min");
        d_.setCursor(centerX - 50, centerY + 85);
        d_.print("OK: Start");
    }
    
    PluginUI::drawFooter(d_, "", _timerRunning ? "BACK:Stop" : "BACK:Menu", _screenW, _screenH);
}

void ToolSuiteApp::drawTimerPartial() {
    if (!_timerRunning) {
        draw();
        return;
    }
    
    // Partial update just the time display
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    int dispW = 200;
    int dispH = 60;
    int dispX = centerX - dispW/2;
    int dispY = centerY - dispH/2;
    
    unsigned long remaining = _timerDuration - (millis() - _timerStart);
    int mins = remaining / 60000;
    int secs = (remaining / 1000) % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    
    d_.setPartialWindow(dispX, dispY, dispW, dispH);
    d_.firstPage();
    do {
        d_.fillRect(dispX, dispY, dispW, dispH, GxEPD_WHITE);
        d_.setTextSize(4);
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(centerX - tw/2, centerY + th/4);
        d_.print(buf);
        d_.setTextSize(1);
    } while (d_.nextPage());
}

// =============================================================================
// STOPWATCH
// =============================================================================

void ToolSuiteApp::resetStopwatch() {
    _swStart = 0;
    _swElapsed = 0;
    _swRunning = false;
    _swLastUpdate = 0;
    needsFullRedraw = true;
}

bool ToolSuiteApp::handleStopwatch(PluginButton btn) {
    switch (btn) {
        case PluginButton::Center:
            if (_swRunning) {
                _swElapsed += millis() - _swStart;
                _swRunning = false;
            } else {
                _swStart = millis();
                _swLastUpdate = _swStart;
                _swRunning = true;
            }
            needsFullRedraw = true;
            return true;
        case PluginButton::Up:
        case PluginButton::Down:
        case PluginButton::Left:
        case PluginButton::Right:
            if (!_swRunning && _swElapsed > 0) {
                _swElapsed = 0;
                needsFullRedraw = true;
            }
            return true;
        case PluginButton::Back:
            if (_swRunning) {
                _swElapsed += millis() - _swStart;
                _swRunning = false;
                needsFullRedraw = true;
                return true;
            }
            _tool = TOOL_MENU;
            needsFullRedraw = true;
            return true;
        default:
            return true;
    }
}

void ToolSuiteApp::drawStopwatchFull() {
    PluginUI::drawHeader(d_, "Stopwatch", _screenW);
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    
    unsigned long elapsed = _swElapsed;
    if (_swRunning) elapsed += millis() - _swStart;
    
    int mins = elapsed / 60000;
    int secs = (elapsed / 1000) % 60;
    int ms = (elapsed / 10) % 100;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d.%02d", mins, secs, ms);
    
    d_.setTextSize(4);
    int16_t tx, ty; uint16_t tw, th;
    d_.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
    d_.setCursor(centerX - tw/2, centerY);
    d_.print(buf);
    d_.setTextSize(1);
    
    d_.setCursor(centerX - 60, centerY + 70);
    d_.print(_swRunning ? "OK: Stop" : "OK: Start");
    
    if (!_swRunning && _swElapsed > 0) {
        d_.setCursor(centerX - 70, centerY + 95);
        d_.print("Any D-pad: Reset");
    }
    
    PluginUI::drawFooter(d_, "", _swRunning ? "BACK:Pause" : "BACK:Menu", _screenW, _screenH);
}

void ToolSuiteApp::drawStopwatchPartial() {
    if (!_swRunning) {
        draw();
        return;
    }
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    int dispW = 260;
    int dispH = 60;
    int dispX = centerX - dispW/2;
    int dispY = centerY - dispH/2;
    
    unsigned long elapsed = _swElapsed + (millis() - _swStart);
    int mins = elapsed / 60000;
    int secs = (elapsed / 1000) % 60;
    int ms = (elapsed / 10) % 100;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d.%02d", mins, secs, ms);
    
    d_.setPartialWindow(dispX, dispY, dispW, dispH);
    d_.firstPage();
    do {
        d_.fillRect(dispX, dispY, dispW, dispH, GxEPD_WHITE);
        d_.setTextSize(4);
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(centerX - tw/2, centerY + th/4);
        d_.print(buf);
        d_.setTextSize(1);
    } while (d_.nextPage());
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
