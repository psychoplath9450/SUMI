/**
 * @file ToolSuite.cpp
 * @brief Utility tools implementation
 * @version 1.3.0
 * 
 * Uses runPluginSelfRefresh for proper display control
 * Raw button handling (no remapping)
 */

#include "plugins/ToolSuite.h"

#if FEATURE_GAMES


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

ToolSuiteApp::ToolSuiteApp() { 
    reset(); 
}

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

bool ToolSuiteApp::handleInput(Button btn) {
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
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold9pt7b);
        
        switch (_tool) {
            case TOOL_MENU:     drawMenuFull(); break;
            case TOOL_CALC:     drawCalcFull(); break;
            case TOOL_TIMER:    drawTimerFull(); break;
            case TOOL_STOPWATCH: drawStopwatchFull(); break;
        }
    } while (display.nextPage());
    
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

bool ToolSuiteApp::handleMenu(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (_menuCursor > 0) _menuCursor--;
            needsFullRedraw = true;
            return true;
        case BTN_DOWN:
            if (_menuCursor < 2) _menuCursor++;
            needsFullRedraw = true;
            return true;
        case BTN_CONFIRM:
            needsFullRedraw = true;
            if (_menuCursor == 0) { _tool = TOOL_CALC; resetCalc(); }
            else if (_menuCursor == 1) { _tool = TOOL_TIMER; }
            else if (_menuCursor == 2) { _tool = TOOL_STOPWATCH; }
            return true;
        case BTN_BACK:
            return false;
        default:
            return true;
    }
}

void ToolSuiteApp::drawMenuFull() {
    PluginUI::drawHeader("Tools", _screenW);
    
    const char* items[] = { "Calculator", "Timer", "Stopwatch" };
    
    int itemW = min(300, _screenW - 60);
    int itemH = 50;
    int spacing = 15;
    int totalH = 3 * itemH + 2 * spacing;
    int startY = PLUGIN_HEADER_H + (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - totalH) / 2;
    int x = (_screenW - itemW) / 2;
    
    for (int i = 0; i < 3; i++) {
        int y = startY + i * (itemH + spacing);
        PluginUI::drawMenuItem(items[i], x, y, itemW, itemH, i == _menuCursor);
    }
    
    PluginUI::drawFooter("", "OK:Select  BACK:Exit", _screenW, _screenH);
}

// =============================================================================
// CALCULATOR
// =============================================================================

void ToolSuiteApp::resetCalc() {
    _calcValue = 0;
    _calcOperand = 0;
    _calcOp = '\0';
    strcpy(_calcDisplay, "0");
    _calcDigits = 0;
    _calcNewNumber = true;
    _calcBtnRow = 3;
    _calcBtnCol = 1;
    _prevBtnRow = _calcBtnRow;
    _prevBtnCol = _calcBtnCol;
    _calcDisplayDirty = false;
    needsFullRedraw = true;
}

bool ToolSuiteApp::handleCalc(Button btn) {
    _prevBtnRow = _calcBtnRow;
    _prevBtnCol = _calcBtnCol;
    
    switch (btn) {
        case BTN_UP:
            if (_calcBtnRow > 0) _calcBtnRow--;
            return true;
        case BTN_DOWN:
            if (_calcBtnRow < 4) _calcBtnRow++;
            return true;
        case BTN_LEFT:
            if (_calcBtnCol > 0) _calcBtnCol--;
            return true;
        case BTN_RIGHT:
            if (_calcBtnCol < 3) _calcBtnCol++;
            return true;
        case BTN_CONFIRM:
            pressCalcButton();
            _calcDisplayDirty = true;
            needsFullRedraw = true;  // Redraw after button press
            return true;
        case BTN_BACK:
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
            else { strcpy(_calcDisplay, "Error"); return; }
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
    PluginUI::drawHeader("Calculator", _screenW);
    
    // Display area
    display.drawRect(_calcDispX, _calcDispY, _calcDispW, _calcDispH, GxEPD_BLACK);
    display.drawRect(_calcDispX + 1, _calcDispY + 1, _calcDispW - 2, _calcDispH - 2, GxEPD_BLACK);
    
    // Display text
    display.setTextSize(2);
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(_calcDisplay, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(_calcDispX + _calcDispW - tw - 15, _calcDispY + (_calcDispH + th) / 2);
    display.print(_calcDisplay);
    display.setTextSize(1);
    
    if (_calcOp != '\0') {
        display.setCursor(_calcDispX + 10, _calcDispY + 25);
        display.print(_calcOp);
    }
    
    // Draw buttons
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int idx = row * 4 + col;
            int x = _calcStartX + col * (_calcBtnW + 8);
            int y = _calcStartY + row * (_calcBtnH + 8);
            bool sel = (row == _calcBtnRow && col == _calcBtnCol);
            
            if (sel) {
                display.fillRect(x, y, _calcBtnW, _calcBtnH, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRect(x, y, _calcBtnW, _calcBtnH, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setTextSize(2);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(CALC_LABELS[idx], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(x + (_calcBtnW - tw) / 2, y + (_calcBtnH + th) / 2);
            display.print(CALC_LABELS[idx]);
            display.setTextSize(1);
            display.setTextColor(GxEPD_BLACK);
        }
    }
    
    PluginUI::drawFooter("", "BACK:Menu", _screenW, _screenH);
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

bool ToolSuiteApp::handleTimer(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (!_timerRunning && !_timerDone && _timerMinutes < 120) {
                _timerMinutes++;
                needsFullRedraw = true;
            }
            return true;
        case BTN_DOWN:
            if (!_timerRunning && !_timerDone && _timerMinutes > 1) {
                _timerMinutes--;
                needsFullRedraw = true;
            }
            return true;
        case BTN_LEFT:
            if (!_timerRunning && !_timerDone && _timerMinutes > 5) {
                _timerMinutes -= 5;
                needsFullRedraw = true;
            }
            return true;
        case BTN_RIGHT:
            if (!_timerRunning && !_timerDone && _timerMinutes < 115) {
                _timerMinutes += 5;
                needsFullRedraw = true;
            }
            return true;
        case BTN_CONFIRM:
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
        case BTN_BACK:
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
    PluginUI::drawHeader("Timer", _screenW);
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    
    if (_timerDone) {
        display.setTextSize(3);
        const char* msg = "TIME UP!";
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(msg, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw/2, centerY);
        display.print(msg);
        display.setTextSize(1);
        display.setCursor(centerX - 60, centerY + 50);
        display.print("Press OK to reset");
    } else if (_timerRunning) {
        unsigned long remaining = _timerDuration - (millis() - _timerStart);
        int mins = remaining / 60000;
        int secs = (remaining / 1000) % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        
        display.setTextSize(4);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw/2, centerY);
        display.print(buf);
        display.setTextSize(1);
        display.setCursor(centerX - 40, centerY + 60);
        display.print("OK: Pause");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d min", _timerMinutes);
        
        display.setTextSize(3);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw/2, centerY - 20);
        display.print(buf);
        display.setTextSize(1);
        display.setCursor(centerX - 80, centerY + 40);
        display.print("UP/DOWN: +/- 1 min");
        display.setCursor(centerX - 80, centerY + 60);
        display.print("LEFT/RIGHT: +/- 5 min");
        display.setCursor(centerX - 50, centerY + 85);
        display.print("OK: Start");
    }
    
    PluginUI::drawFooter("", _timerRunning ? "BACK:Stop" : "BACK:Menu", _screenW, _screenH);
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
    
    display.setPartialWindow(dispX, dispY, dispW, dispH);
    display.firstPage();
    do {
        display.fillRect(dispX, dispY, dispW, dispH, GxEPD_WHITE);
        display.setTextSize(4);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw/2, centerY + th/4);
        display.print(buf);
        display.setTextSize(1);
    } while (display.nextPage());
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

bool ToolSuiteApp::handleStopwatch(Button btn) {
    switch (btn) {
        case BTN_CONFIRM:
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
        case BTN_UP:
        case BTN_DOWN:
        case BTN_LEFT:
        case BTN_RIGHT:
            if (!_swRunning && _swElapsed > 0) {
                _swElapsed = 0;
                needsFullRedraw = true;
            }
            return true;
        case BTN_BACK:
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
    PluginUI::drawHeader("Stopwatch", _screenW);
    
    int centerX = _screenW / 2;
    int centerY = _screenH / 2;
    
    unsigned long elapsed = _swElapsed;
    if (_swRunning) elapsed += millis() - _swStart;
    
    int mins = elapsed / 60000;
    int secs = (elapsed / 1000) % 60;
    int ms = (elapsed / 10) % 100;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d.%02d", mins, secs, ms);
    
    display.setTextSize(4);
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(centerX - tw/2, centerY);
    display.print(buf);
    display.setTextSize(1);
    
    display.setCursor(centerX - 60, centerY + 70);
    display.print(_swRunning ? "OK: Stop" : "OK: Start");
    
    if (!_swRunning && _swElapsed > 0) {
        display.setCursor(centerX - 70, centerY + 95);
        display.print("Any D-pad: Reset");
    }
    
    PluginUI::drawFooter("", _swRunning ? "BACK:Pause" : "BACK:Menu", _screenW, _screenH);
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
    
    display.setPartialWindow(dispX, dispY, dispW, dispH);
    display.firstPage();
    do {
        display.fillRect(dispX, dispY, dispW, dispH, GxEPD_WHITE);
        display.setTextSize(4);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(buf, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(centerX - tw/2, centerY + th/4);
        display.print(buf);
        display.setTextSize(1);
    } while (display.nextPage());
}

#endif // FEATURE_GAMES
