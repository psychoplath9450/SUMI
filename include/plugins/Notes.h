/**
 * @file Notes.h
 * @brief Text editor with Bluetooth keyboard support for Sumi e-reader
 * @version 2.1.26
 */

#ifndef SUMI_PLUGIN_NOTES_H
#define SUMI_PLUGIN_NOTES_H

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "core/PluginHelpers.h"
#include "core/SettingsManager.h"
#include "core/RefreshManager.h"

#if FEATURE_BLUETOOTH
#include "core/BluetoothManager.h"
#endif

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern RefreshManager refreshManager;

class NotesApp;
extern NotesApp* g_notesInstance;

class NotesApp {
public:
    static const int MAX_NOTES = 20;
    static const int MAX_NAME_LEN = 32;
    static const int BUFFER_SIZE = 4096;
    static const int AUTO_SAVE_MS = 5000;
    
    enum State {
        STATE_FILE_LIST,
        STATE_EDITOR,
        STATE_BT_SELECT,
        STATE_NEW_NOTE
    };
    
    NotesApp();
    void init(int screenW, int screenH);
    bool handleInput(Button btn);
    void draw();
    bool update();
    void onKeyPress(char c, uint8_t keyCode, uint8_t modifiers);
    
private:
    char _notes[MAX_NOTES][MAX_NAME_LEN];
    int _noteCount;
    char _currentFile[64];
    char _buffer[BUFFER_SIZE];
    int _bufferLen;
    bool _modified;
    unsigned long _lastKeystroke;
    char _newNoteName[MAX_NAME_LEN];
    int _newNoteNameLen;
    State _state;
    int _cursor, _scroll;
    int _viewScroll;
    int _screenW, _screenH;
    bool _landscape;
    int _itemH, _itemsPerPage;
    int _lineH, _editTop, _editBottom, _linesVisible, _charsPerLine;
    bool _btConnected;
    char _btKeyboardName[64];
    #if FEATURE_BLUETOOTH
    int _btDeviceCursor;
    int _btDeviceCount;
    #endif
    
    void reset();
    void scanNotes();
    bool handleFileListInput(Button btn);
    void drawFileList();
    void openNote(int idx);
    void createNewNote(const char* name);
    void enterEditor();
    void exitEditor();
    void saveCurrentNote();
    bool handleEditorInput(Button btn);
    void scrollToEnd();
    void drawEditor();
    void drawTextContent();
    void drawEditorPartial();
    bool handleBTSelectInput(Button btn);
    void drawBTSelect();
    bool handleNewNoteInput(Button btn);
    void drawNewNote();
    
    #if FEATURE_BLUETOOTH
    static void onBluetoothKey(const KeyEvent& event);
    void connectBTDevice(int idx);
    #endif
};

extern NotesApp notesApp;

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_NOTES_H
