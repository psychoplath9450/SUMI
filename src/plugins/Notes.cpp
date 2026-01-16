/**
 * @file Notes.cpp
 * @brief Text editor implementation
 * @version 2.1.16
 */

#include <SD.h>
#include "plugins/Notes.h"

#if FEATURE_GAMES

NotesApp notesApp;
NotesApp* g_notesInstance = nullptr;

// =============================================================================
// Constructor
// =============================================================================

NotesApp::NotesApp() { 
    g_notesInstance = this;
    reset(); 
}

// =============================================================================
// Public Methods
// =============================================================================

void NotesApp::init(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _landscape = isLandscapeMode(screenW, screenH);
    _itemH = 36;
    _itemsPerPage = (_screenH - PLUGIN_HEADER_H - PLUGIN_FOOTER_H) / _itemH;
    
    _lineH = 20;
    _editTop = PLUGIN_HEADER_H + 8;
    _editBottom = _screenH - PLUGIN_FOOTER_H - 8;
    _linesVisible = (_editBottom - _editTop) / _lineH;
    _charsPerLine = (_screenW - 2 * PLUGIN_MARGIN) / 8;
    
    scanNotes();
    _state = STATE_FILE_LIST;
    _cursor = 0;
    _scroll = 0;
    _modified = false;
    _lastKeystroke = 0;
    
    #if FEATURE_BLUETOOTH
    _btConnected = bluetoothManager.hasConnectedKeyboard();
    if (_btConnected) {
        strncpy(_btKeyboardName, bluetoothManager.getConnectedKeyboardName(), sizeof(_btKeyboardName) - 1);
    }
    #else
    _btConnected = false;
    #endif
}

bool NotesApp::handleInput(Button btn) {
    // Use raw buttons - no remapping
    
    switch (_state) {
        case STATE_FILE_LIST:
            return handleFileListInput(btn);
        case STATE_EDITOR:
            return handleEditorInput(btn);
        case STATE_BT_SELECT:
            return handleBTSelectInput(btn);
        case STATE_NEW_NOTE:
            return handleNewNoteInput(btn);
        default:
            return false;
    }
}

void NotesApp::draw() {
    switch (_state) {
        case STATE_FILE_LIST:
            drawFileList();
            break;
        case STATE_EDITOR:
            drawEditor();
            break;
        case STATE_BT_SELECT:
            drawBTSelect();
            break;
        case STATE_NEW_NOTE:
            drawNewNote();
            break;
    }
}

bool NotesApp::update() {
    if (_state == STATE_EDITOR && _modified && _lastKeystroke > 0) {
        if (millis() - _lastKeystroke > AUTO_SAVE_MS) {
            saveCurrentNote();
            _modified = false;
            _lastKeystroke = 0;
            return true;
        }
    }
    return false;
}

void NotesApp::onKeyPress(char c, uint8_t keyCode, uint8_t modifiers) {
    if (_state != STATE_EDITOR) return;
    
    if (keyCode == 0x2A) {  // Backspace
        if (_bufferLen > 0) {
            _bufferLen--;
            _buffer[_bufferLen] = '\0';
            _modified = true;
            _lastKeystroke = millis();
            drawEditorPartial();
        }
        return;
    }
    
    if (keyCode == 0x28) {  // Enter
        if (_bufferLen < BUFFER_SIZE - 2) {
            _buffer[_bufferLen++] = '\n';
            _buffer[_bufferLen] = '\0';
            _modified = true;
            _lastKeystroke = millis();
            scrollToEnd();
            drawEditorPartial();
        }
        return;
    }
    
    if (c >= 32 && c < 127 && _bufferLen < BUFFER_SIZE - 2) {
        _buffer[_bufferLen++] = c;
        _buffer[_bufferLen] = '\0';
        _modified = true;
        _lastKeystroke = millis();
        scrollToEnd();
        drawEditorPartial();
    }
}

// =============================================================================
// Private Methods
// =============================================================================

void NotesApp::reset() {
    _noteCount = 0;
    _cursor = 0;
    _scroll = 0;
    _viewScroll = 0;
    _bufferLen = 0;
    _buffer[0] = '\0';
    _currentFile[0] = '\0';
    _modified = false;
    _lastKeystroke = 0;
    _btConnected = false;
    _btKeyboardName[0] = '\0';
    _newNoteName[0] = '\0';
    _newNoteNameLen = 0;
}

void NotesApp::scanNotes() {
    _noteCount = 0;
    
    if (!SD.exists("/notes")) {
        SD.mkdir("/notes");
    }
    
    File dir = SD.open("/notes");
    if (!dir) return;
    
    while (File entry = dir.openNextFile()) {
        if (_noteCount >= MAX_NOTES - 1) break;
        
        const char* name = entry.name();
        if (name[0] == '.') { entry.close(); continue; }
        
        int len = strlen(name);
        if (len > 4 && strcasecmp(name + len - 4, ".txt") == 0) {
            strncpy(_notes[_noteCount], name, MAX_NAME_LEN - 1);
            _notes[_noteCount][MAX_NAME_LEN - 1] = '\0';
            _noteCount++;
        }
        entry.close();
    }
    dir.close();
    
    Serial.printf("[NOTES] Found %d notes\n", _noteCount);
}

bool NotesApp::handleFileListInput(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (_cursor > 0) {
                _cursor--;
                if (_cursor < _scroll) _scroll = _cursor;
            }
            return true;
            
        case BTN_DOWN:
            if (_cursor < _noteCount) {
                _cursor++;
                if (_cursor >= _scroll + _itemsPerPage) _scroll++;
            }
            return true;
            
        case BTN_CONFIRM:
            if (_cursor == _noteCount) {
                _newNoteName[0] = '\0';
                _newNoteNameLen = 0;
                _state = STATE_NEW_NOTE;
            } else if (_noteCount > 0) {
                openNote(_cursor);
                enterEditor();
            }
            return true;
            
        case BTN_LEFT:
            #if FEATURE_BLUETOOTH
            _btDeviceCursor = 0;
            _btDeviceCount = bluetoothManager.getPairedCount();
            _state = STATE_BT_SELECT;
            return true;
            #else
            return false;
            #endif
            
        case BTN_BACK:
            return false;
            
        default:
            return false;
    }
}

void NotesApp::drawFileList() {
    PluginUI::drawHeader("Notes", _screenW);
    
    #if FEATURE_BLUETOOTH
    int btIconX = _screenW - 30;
    int btIconY = 8;
    if (_btConnected) {
        display.setCursor(btIconX, btIconY + 12);
        display.print("BT");
        display.drawRect(btIconX - 4, btIconY, 28, 18, GxEPD_BLACK);
    } else {
        display.setCursor(btIconX - 2, btIconY + 12);
        display.setTextColor(GxEPD_BLACK);
        display.print("--");
    }
    #endif
    
    int y = PLUGIN_HEADER_H + 4;
    int displayCount = _noteCount + 1;
    
    for (int i = _scroll; i < min(_scroll + _itemsPerPage, displayCount); i++) {
        bool sel = (i == _cursor);
        
        if (i == _noteCount) {
            PluginUI::drawMenuItem("+ New Note", PLUGIN_MARGIN, y, 
                                   _screenW - 2 * PLUGIN_MARGIN, _itemH - 4, sel);
        } else {
            PluginUI::drawMenuItem(_notes[i], PLUGIN_MARGIN, y, 
                                   _screenW - 2 * PLUGIN_MARGIN, _itemH - 4, sel);
        }
        y += _itemH;
    }
    
    char status[32];
    snprintf(status, sizeof(status), "%d notes", _noteCount);
    
    #if FEATURE_BLUETOOTH
    PluginUI::drawFooter(status, "L:Bluetooth OK:Open", _screenW, _screenH);
    #else
    PluginUI::drawFooter(status, "OK:Open", _screenW, _screenH);
    #endif
}

void NotesApp::openNote(int idx) {
    if (idx >= _noteCount) return;
    
    snprintf(_currentFile, sizeof(_currentFile), "/notes/%s", _notes[idx]);
    
    File f = SD.open(_currentFile);
    if (!f) {
        _bufferLen = 0;
        _buffer[0] = '\0';
        return;
    }
    
    _bufferLen = f.read((uint8_t*)_buffer, BUFFER_SIZE - 1);
    if (_bufferLen < 0) _bufferLen = 0;
    _buffer[_bufferLen] = '\0';
    f.close();
    
    _modified = false;
    _viewScroll = 0;
    scrollToEnd();
    
    Serial.printf("[NOTES] Opened %s (%d bytes)\n", _currentFile, _bufferLen);
}

void NotesApp::createNewNote(const char* name) {
    snprintf(_currentFile, sizeof(_currentFile), "/notes/%s.txt", name);
    
    File f = SD.open(_currentFile, FILE_WRITE);
    if (f) {
        f.close();
    }
    
    _bufferLen = 0;
    _buffer[0] = '\0';
    _modified = false;
    _viewScroll = 0;
    
    if (_noteCount < MAX_NOTES - 1) {
        snprintf(_notes[_noteCount], MAX_NAME_LEN, "%s.txt", name);
        _noteCount++;
    }
    
    Serial.printf("[NOTES] Created new note: %s\n", _currentFile);
}

void NotesApp::enterEditor() {
    _state = STATE_EDITOR;
    _lastKeystroke = 0;
    
    refreshManager.setMode(RefreshManager::MODE_TYPING);
    
    #if FEATURE_BLUETOOTH
    bluetoothManager.setKeyCallback(onBluetoothKey);
    #endif
}

#if FEATURE_BLUETOOTH
void NotesApp::onBluetoothKey(const KeyEvent& event) {
    if (g_notesInstance && event.pressed) {
        g_notesInstance->onKeyPress(event.character, event.keyCode, event.modifiers);
    }
}
#endif

void NotesApp::exitEditor() {
    if (_modified) {
        saveCurrentNote();
    }
    
    refreshManager.setMode(RefreshManager::MODE_NORMAL);
    
    #if FEATURE_BLUETOOTH
    bluetoothManager.setKeyCallback(nullptr);
    #endif
    
    _state = STATE_FILE_LIST;
    scanNotes();
}

void NotesApp::saveCurrentNote() {
    if (_currentFile[0] == '\0') return;
    
    File f = SD.open(_currentFile, FILE_WRITE);
    if (f) {
        f.write((uint8_t*)_buffer, _bufferLen);
        f.close();
        Serial.printf("[NOTES] Saved %s (%d bytes)\n", _currentFile, _bufferLen);
    }
}

bool NotesApp::handleEditorInput(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (_viewScroll > 0) {
                _viewScroll--;
                return true;
            }
            return false;
            
        case BTN_DOWN:
            _viewScroll++;
            return true;
            
        case BTN_BACK:
            exitEditor();
            return true;
            
        default:
            return false;
    }
}

void NotesApp::scrollToEnd() {
    int lineCount = 1;
    for (int i = 0; i < _bufferLen; i++) {
        if (_buffer[i] == '\n') lineCount++;
    }
    
    if (lineCount > _linesVisible) {
        _viewScroll = lineCount - _linesVisible;
    } else {
        _viewScroll = 0;
    }
}

void NotesApp::drawEditor() {
    char title[48];
    const char* fname = strrchr(_currentFile, '/');
    fname = fname ? fname + 1 : _currentFile;
    snprintf(title, sizeof(title), "%s%s", fname, _modified ? "*" : "");
    
    PluginUI::drawHeader(title, _screenW);
    
    #if FEATURE_BLUETOOTH
    int btX = _screenW - 30;
    if (_btConnected) {
        display.fillCircle(btX + 10, 14, 6, GxEPD_BLACK);
        display.setCursor(btX + 6, 18);
        display.setTextColor(GxEPD_WHITE);
        display.print("B");
        display.setTextColor(GxEPD_BLACK);
    } else {
        display.drawCircle(btX + 10, 14, 6, GxEPD_BLACK);
    }
    #endif
    
    drawTextContent();
    
    const char* saveStatus = _modified ? "Modified" : "Saved";
    PluginUI::drawFooter(saveStatus, "BACK:Exit", _screenW, _screenH);
}

void NotesApp::drawTextContent() {
    int y = _editTop;
    int x = PLUGIN_MARGIN;
    int maxX = _screenW - PLUGIN_MARGIN;
    
    int lineNum = 0;
    int charIdx = 0;
    int lineStartX = x;
    
    while (charIdx < _bufferLen && y < _editBottom) {
        char c = _buffer[charIdx];
        
        if (c == '\n') {
            lineNum++;
            if (lineNum > _viewScroll) {
                y += _lineH;
            }
            x = lineStartX;
            charIdx++;
            continue;
        }
        
        if (lineNum < _viewScroll) {
            charIdx++;
            continue;
        }
        
        if (y >= _editTop && y < _editBottom) {
            display.setCursor(x, y + _lineH - 4);
            display.print(c);
        }
        
        x += 8;
        
        if (x >= maxX - 8) {
            lineNum++;
            y += _lineH;
            x = lineStartX;
        }
        
        charIdx++;
    }
    
    if (y >= _editTop && y < _editBottom) {
        display.fillRect(x, y, 8, _lineH - 2, GxEPD_BLACK);
    }
}

void NotesApp::drawEditorPartial() {
    if (!refreshManager.canPartialRefresh()) return;
    
    display.setPartialWindow(0, _editTop - 4, _screenW, _editBottom - _editTop + 8);
    display.firstPage();
    do {
        display.fillRect(0, _editTop - 4, _screenW, _editBottom - _editTop + 8, GxEPD_WHITE);
        drawTextContent();
    } while (display.nextPage());
    
    refreshManager.recordPartialRefresh();
}

#if FEATURE_BLUETOOTH
bool NotesApp::handleBTSelectInput(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (_btDeviceCursor > 0) _btDeviceCursor--;
            return true;
            
        case BTN_DOWN:
            if (_btDeviceCursor < _btDeviceCount - 1) _btDeviceCursor++;
            return true;
            
        case BTN_CONFIRM:
            if (_btDeviceCount > 0) {
                connectBTDevice(_btDeviceCursor);
            }
            _state = STATE_FILE_LIST;
            return true;
            
        case BTN_BACK:
            _state = STATE_FILE_LIST;
            return true;
            
        default:
            return false;
    }
}

void NotesApp::connectBTDevice(int idx) {
    const BTDevice* device = bluetoothManager.getPairedDevice(idx);
    if (!device) return;
    
    Serial.printf("[NOTES] Connecting to BT device: %s\n", device->name);
    
    if (bluetoothManager.connect(device->address)) {
        _btConnected = true;
        strncpy(_btKeyboardName, device->name, sizeof(_btKeyboardName) - 1);
        Serial.printf("[NOTES] Connected to %s\n", _btKeyboardName);
    } else {
        _btConnected = false;
        Serial.println("[NOTES] BT connection failed");
    }
}

void NotesApp::drawBTSelect() {
    PluginUI::drawHeader("Connect Keyboard", _screenW);
    
    if (_btDeviceCount == 0) {
        display.setCursor(20, _screenH / 2 - 20);
        display.print("No paired keyboards");
        display.setCursor(20, _screenH / 2 + 10);
        display.print("Pair in Settings > Bluetooth");
        PluginUI::drawFooter("", "BACK:Cancel", _screenW, _screenH);
        return;
    }
    
    int y = PLUGIN_HEADER_H + 4;
    for (int i = 0; i < _btDeviceCount && i < _itemsPerPage; i++) {
        const BTDevice* dev = bluetoothManager.getPairedDevice(i);
        if (!dev) continue;
        
        bool sel = (i == _btDeviceCursor);
        char label[64];
        snprintf(label, sizeof(label), "%s %s", 
                 dev->name,
                 bluetoothManager.isConnected(dev->address) ? "(Connected)" : "");
        
        PluginUI::drawMenuItem(label, PLUGIN_MARGIN, y, 
                               _screenW - 2 * PLUGIN_MARGIN, _itemH - 4, sel);
        y += _itemH;
    }
    
    PluginUI::drawFooter("", "OK:Connect BACK:Cancel", _screenW, _screenH);
}
#else
bool NotesApp::handleBTSelectInput(Button btn) {
    _state = STATE_FILE_LIST;
    return true;
}

void NotesApp::drawBTSelect() {
    PluginUI::drawHeader("Bluetooth", _screenW);
    display.setCursor(20, _screenH / 2);
    display.print("Bluetooth not available");
    PluginUI::drawFooter("", "OK:Back", _screenW, _screenH);
}
#endif

bool NotesApp::handleNewNoteInput(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (_newNoteNameLen > 0) {
                char& c = _newNoteName[_newNoteNameLen - 1];
                if (c >= 'a' && c < 'z') c++;
                else if (c == 'z') c = '0';
                else if (c >= '0' && c < '9') c++;
                else if (c == '9') c = 'a';
                else c = 'a';
            }
            return true;
            
        case BTN_DOWN:
            if (_newNoteNameLen > 0) {
                char& c = _newNoteName[_newNoteNameLen - 1];
                if (c > 'a' && c <= 'z') c--;
                else if (c == 'a') c = '9';
                else if (c > '0' && c <= '9') c--;
                else if (c == '0') c = 'z';
                else c = 'z';
            }
            return true;
            
        case BTN_RIGHT:
            if (_newNoteNameLen < MAX_NAME_LEN - 5) {
                _newNoteName[_newNoteNameLen++] = 'a';
                _newNoteName[_newNoteNameLen] = '\0';
            }
            return true;
            
        case BTN_LEFT:
            if (_newNoteNameLen > 0) {
                _newNoteNameLen--;
                _newNoteName[_newNoteNameLen] = '\0';
            }
            return true;
            
        case BTN_CONFIRM:
            if (_newNoteNameLen > 0) {
                createNewNote(_newNoteName);
                enterEditor();
            }
            return true;
            
        case BTN_BACK:
            _state = STATE_FILE_LIST;
            return true;
            
        default:
            return false;
    }
}

void NotesApp::drawNewNote() {
    PluginUI::drawHeader("New Note", _screenW);
    
    display.setCursor(20, _screenH / 2 - 40);
    display.print("Enter note name:");
    
    int nameY = _screenH / 2;
    display.setCursor(20, nameY);
    display.setTextSize(2);
    if (_newNoteNameLen > 0) {
        display.print(_newNoteName);
    }
    display.print("_");
    display.setTextSize(1);
    
    display.setCursor(20, _screenH / 2 + 40);
    display.print("Up/Down: Change letter");
    display.setCursor(20, _screenH / 2 + 60);
    display.print("Left/Right: Delete/Add");
    
    PluginUI::drawFooter("", "OK:Create BACK:Cancel", _screenW, _screenH);
}

#endif // FEATURE_GAMES
