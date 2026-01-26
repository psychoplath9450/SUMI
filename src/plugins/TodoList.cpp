/**
 * @file TodoList.cpp
 * @brief Simple to-do list plugin implementation
 * @version 1.3.0
 * 
 * Optimized with raw button handling
 */

#include <SD.h>
#include "plugins/TodoList.h"

#if FEATURE_GAMES


TodoApp::TodoApp() 
    : _cursor(0), _count(0), _screenW(800), _screenH(480), 
      _visibleCount(0), _scrollStart(0), _landscape(true), _itemH(40) {
    memset(_texts, 0, sizeof(_texts));
    memset(_done, 0, sizeof(_done));
}

void TodoApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _itemH = _landscape ? 40 : 45;
    _cursor = 0;
    loadVisible();
}

bool TodoApp::handleInput(Button btn) {
    // Use raw buttons - no remapping
    
    switch (btn) {
        case BTN_UP:
            if (_cursor > 0) {
                _cursor--;
                loadVisible();
            }
            return true;
        case BTN_DOWN:
            if (_cursor < _count - 1) {
                _cursor++;
                loadVisible();
            }
            return true;
        case BTN_CONFIRM:
            toggleCurrent();
            return true;
        case BTN_LEFT:
        case BTN_RIGHT:
            // Could add delete or edit functionality
            return true;
        case BTN_BACK:
            return false;
        default:
            return false;
    }
}

void TodoApp::draw() {
    PluginUI::drawHeader("To-Do List", _screenW);
    
    if (_count == 0) {
        display.setCursor(20, _screenH / 2 - 20);
        display.print("No tasks found.");
        display.setCursor(20, _screenH / 2 + 10);
        display.print("Add tasks via /data/todo.txt");
        display.setCursor(20, _screenH / 2 + 35);
        display.print("Format: X Task (X=done, -=pending)");
        PluginUI::drawFooter("", "", _screenW, _screenH);
        return;
    }
    
    int y = PLUGIN_HEADER_H + 8;
    for (int i = 0; i < _visibleCount; i++) {
        bool sel = (i == _cursor - _scrollStart);
        int x = PLUGIN_MARGIN;
        int w = _screenW - 2 * PLUGIN_MARGIN;
        
        // Selection highlight
        if (sel) {
            display.drawRect(x, y, w, _itemH - 4, GxEPD_BLACK);
            display.drawRect(x + 1, y + 1, w - 2, _itemH - 6, GxEPD_BLACK);
        }
        
        // Checkbox
        int cbX = x + 8;
        int cbY = y + (_itemH - 20) / 2;
        int cbS = 18;
        display.drawRect(cbX, cbY, cbS, cbS, GxEPD_BLACK);
        
        if (_done[i]) {
            // Draw checkmark
            display.drawLine(cbX + 3, cbY + 9, cbX + 7, cbY + 13, GxEPD_BLACK);
            display.drawLine(cbX + 7, cbY + 13, cbX + 14, cbY + 4, GxEPD_BLACK);
            display.drawLine(cbX + 3, cbY + 10, cbX + 7, cbY + 14, GxEPD_BLACK);
            display.drawLine(cbX + 7, cbY + 14, cbX + 14, cbY + 5, GxEPD_BLACK);
        }
        
        // Task text
        display.setCursor(cbX + cbS + 12, y + _itemH - 14);
        
        // Strike through if done
        if (_done[i]) {
            // Draw text with strikethrough effect
            int textY = y + _itemH / 2;
            display.print(_texts[i]);
            int textW = strlen(_texts[i]) * 6;
            display.drawLine(cbX + cbS + 12, textY, cbX + cbS + 12 + textW, textY, GxEPD_BLACK);
        } else {
            display.print(_texts[i]);
        }
        
        y += _itemH;
    }
    
    // Count completed
    int completed = 0;
    for (int i = 0; i < _visibleCount; i++) {
        if (_done[i]) completed++;
    }
    
    char status[32];
    snprintf(status, sizeof(status), "%d/%d done", completed, _count);
    PluginUI::drawFooter(status, "OK:Toggle", _screenW, _screenH);
}

void TodoApp::loadVisible() {
    _count = 0;
    _visibleCount = 0;
    _scrollStart = 0;
    
    if (!SD.exists("/data")) {
        SD.mkdir("/data");
    }
    
    File f = SD.open("/data/todo.txt");
    if (!f) {
        // Create sample file
        f = SD.open("/data/todo.txt", FILE_WRITE);
        if (f) {
            f.println("- Sample task 1");
            f.println("- Sample task 2");
            f.println("X Completed task");
            f.close();
            f = SD.open("/data/todo.txt");
        }
        if (!f) return;
    }
    
    // Count total lines first
    int totalLines = 0;
    while (f.available()) {
        char buf[MAX_TEXT + 2];
        int len = f.readBytesUntil('\n', buf, MAX_TEXT);
        if (len > 0) totalLines++;
    }
    _count = totalLines;
    
    // Calculate visible window
    int maxVisible = (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 16) / _itemH;
    if (maxVisible > MAX_VISIBLE) maxVisible = MAX_VISIBLE;
    
    int start = 0;
    if (_cursor >= maxVisible) {
        start = _cursor - maxVisible + 1;
    }
    _scrollStart = start;
    
    // Re-read file to get visible items
    f.seek(0);
    int line = 0;
    while (f.available() && _visibleCount < maxVisible) {
        char buf[MAX_TEXT + 2];
        int len = f.readBytesUntil('\n', buf, MAX_TEXT);
        buf[len] = '\0';
        
        // Trim trailing whitespace
        while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n' || buf[len-1] == ' ')) {
            buf[--len] = '\0';
        }
        
        if (len < 2) { line++; continue; }
        
        if (line >= start && _visibleCount < maxVisible) {
            _done[_visibleCount] = (buf[0] == 'X' || buf[0] == 'x');
            // Skip status char and space
            const char* text = buf + 2;
            strncpy(_texts[_visibleCount], text, MAX_TEXT - 1);
            _texts[_visibleCount][MAX_TEXT - 1] = '\0';
            _visibleCount++;
        }
        line++;
    }
    
    f.close();
}

void TodoApp::toggleCurrent() {
    int idx = _cursor - _scrollStart;
    if (idx < 0 || idx >= _visibleCount) return;
    
    _done[idx] = !_done[idx];
    
    // Save changes back to file
    // Read all lines, modify the target, rewrite
    File f = SD.open("/data/todo.txt");
    if (!f) return;
    
    char lines[20][MAX_TEXT + 4];
    int lineCount = 0;
    
    while (f.available() && lineCount < 20) {
        int len = f.readBytesUntil('\n', lines[lineCount], MAX_TEXT + 2);
        lines[lineCount][len] = '\0';
        // Trim
        while (len > 0 && (lines[lineCount][len-1] == '\r' || lines[lineCount][len-1] == '\n')) {
            lines[lineCount][--len] = '\0';
        }
        lineCount++;
    }
    f.close();
    
    // Modify the target line
    int targetLine = _scrollStart + idx;
    if (targetLine < lineCount && strlen(lines[targetLine]) >= 2) {
        lines[targetLine][0] = _done[idx] ? 'X' : '-';
    }
    
    // Rewrite file
    f = SD.open("/data/todo.txt", FILE_WRITE);
    if (f) {
        for (int i = 0; i < lineCount; i++) {
            f.println(lines[i]);
        }
        f.close();
    }
}

#endif // FEATURE_GAMES
