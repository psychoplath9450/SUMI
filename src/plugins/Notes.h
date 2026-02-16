#pragma once

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <SDCardManager.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

// =============================================================================
// Notes — Landscape-mode distraction-free writing app (BYOK-style)
//
// Designed for BLE keyboard input. When connected, user gets a clean wide
// writing surface with minimal chrome. Physical buttons handle navigation
// when no keyboard is present.
// =============================================================================

class NotesApp;
extern NotesApp* g_notesInstance;

class NotesApp : public PluginInterface {
public:
  const char* name() const override { return "Notes"; }
  PluginRunMode runMode() const override { return PluginRunMode::WithUpdate; }
  bool wantsLandscape() const override { return true; }

  static constexpr int MAX_NOTES = 20;
  static constexpr int MAX_NAME_LEN = 32;
  static constexpr int BUFFER_SIZE = 4096;
  static constexpr int AUTO_SAVE_MS = 3000;

  enum Screen {
    SCREEN_FILE_LIST,
    SCREEN_EDITOR,
    SCREEN_NEW_NOTE,
  };

  explicit NotesApp(PluginRenderer& renderer);
  ~NotesApp();
  void init(int screenW, int screenH) override;
  bool handleInput(PluginButton btn) override;
  bool handleChar(char c) override;
  void draw() override;
  bool update() override;
  void cleanup() override;

  PluginRenderer& d_;

private:
  // File management
  char notes_[MAX_NOTES][MAX_NAME_LEN];
  int noteCount_ = 0;
  char currentFile_[64] = {0};

  // Text buffer
  char buf_[BUFFER_SIZE];
  int bufLen_ = 0;
  int cursorPos_ = 0;
  bool modified_ = false;
  unsigned long lastKeystroke_ = 0;

  // New note naming
  char newName_[MAX_NAME_LEN];
  int newNameLen_ = 0;

  // UI state
  Screen screen_ = SCREEN_FILE_LIST;
  int listCursor_ = 0;
  int listScroll_ = 0;
  int viewScrollLine_ = 0;

  // Layout (computed in init)
  int W_ = 800, H_ = 480;
  int statusBarH_ = 28;
  int marginX_ = 16;
  int marginY_ = 8;
  int lineH_ = 22;
  int charW_ = 10;
  int charsPerLine_ = 76;
  int linesVisible_ = 18;
  int editTop_ = 36;
  int editBottom_ = 450;

  // File list layout
  int itemH_ = 40;
  int itemsPerPage_ = 10;

  // Methods
  void reset();
  void computeLayout();
  void scanNotes();
  int countWords() const;
  int countLines() const;

  // Editor
  void openNote(int idx);
  void createNote(const char* noteName);
  void saveNote();
  void insertChar(char c);
  void deleteCharBack();
  void deleteCharForward();
  void moveCursorUp();
  void moveCursorDown();
  void ensureCursorVisible();
  int cursorToLine() const;
  int cursorToCol() const;
  int lineColToPos(int line, int col) const;

  // Drawing
  void drawFileList();
  void drawEditor();
  void drawNewNote();
  void drawStatusBar(const char* left, const char* right);
};


// =============================================================================
// IMPLEMENTATION
// =============================================================================

NotesApp* g_notesInstance = nullptr;

NotesApp::NotesApp(PluginRenderer& renderer) : d_(renderer) {
  g_notesInstance = this;
  reset();
}

NotesApp::~NotesApp() {
  g_notesInstance = nullptr;
}

void NotesApp::reset() {
  noteCount_ = 0;
  bufLen_ = 0;
  buf_[0] = '\0';
  cursorPos_ = 0;
  modified_ = false;
  lastKeystroke_ = 0;
  currentFile_[0] = '\0';
  newName_[0] = '\0';
  newNameLen_ = 0;
  screen_ = SCREEN_FILE_LIST;
  listCursor_ = 0;
  listScroll_ = 0;
  viewScrollLine_ = 0;
}

void NotesApp::init(int screenW, int screenH) {
  W_ = screenW;
  H_ = screenH;
  computeLayout();
  scanNotes();
  screen_ = SCREEN_FILE_LIST;
  Serial.printf("[NOTES] Init %dx%d, %d chars/line, %d lines visible\n",
                W_, H_, charsPerLine_, linesVisible_);
}

void NotesApp::computeLayout() {
  charW_ = d_.getTextWidth("M");
  if (charW_ < 6) charW_ = 10;
  lineH_ = d_.getLineHeight() + 4;
  if (lineH_ < 14) lineH_ = 22;

  statusBarH_ = lineH_ + 8;
  editTop_ = statusBarH_ + marginY_;
  editBottom_ = H_ - marginY_ - 4;
  charsPerLine_ = (W_ - 2 * marginX_ - 8) / charW_;  // -8 for scroll indicator
  linesVisible_ = (editBottom_ - editTop_) / lineH_;

  itemH_ = lineH_ + 14;
  itemsPerPage_ = (H_ - statusBarH_ - 40) / itemH_;

  Serial.printf("[NOTES] Layout: charW=%d lineH=%d cpl=%d linesVis=%d\n",
                charW_, lineH_, charsPerLine_, linesVisible_);
}

void NotesApp::cleanup() {
  if (modified_) saveNote();
}

void NotesApp::scanNotes() {
  noteCount_ = 0;

  if (!SdMan.exists("/notes")) {
    SdMan.mkdir("/notes");
  }

  FsFile dir = SdMan.open("/notes");
  if (!dir) return;

  while (FsFile entry = dir.openNextFile()) {
    if (noteCount_ >= MAX_NOTES - 1) { entry.close(); break; }

    char fname[64];
    entry.getName(fname, sizeof(fname));
    if (fname[0] == '.') { entry.close(); continue; }

    int len = strlen(fname);
    if (len > 4 && strcasecmp(fname + len - 4, ".txt") == 0) {
      strncpy(notes_[noteCount_], fname, MAX_NAME_LEN - 1);
      notes_[noteCount_][MAX_NAME_LEN - 1] = '\0';
      noteCount_++;
    }
    entry.close();
  }
  dir.close();
}

int NotesApp::countWords() const {
  int words = 0;
  bool inWord = false;
  for (int i = 0; i < bufLen_; i++) {
    bool ws = (buf_[i] == ' ' || buf_[i] == '\n' || buf_[i] == '\t');
    if (!ws && !inWord) words++;
    inWord = !ws;
  }
  return words;
}

int NotesApp::countLines() const {
  if (bufLen_ == 0) return 1;
  int lines = 1;
  int col = 0;
  for (int i = 0; i < bufLen_; i++) {
    if (buf_[i] == '\n') { lines++; col = 0; continue; }
    col++;
    if (col >= charsPerLine_) { lines++; col = 0; }
  }
  return lines;
}

// =============================================================================
// Text buffer operations
// =============================================================================

void NotesApp::insertChar(char c) {
  if (bufLen_ >= BUFFER_SIZE - 2) return;
  memmove(buf_ + cursorPos_ + 1, buf_ + cursorPos_, bufLen_ - cursorPos_);
  buf_[cursorPos_] = c;
  cursorPos_++;
  bufLen_++;
  buf_[bufLen_] = '\0';
  modified_ = true;
  lastKeystroke_ = millis();
  ensureCursorVisible();
}

void NotesApp::deleteCharBack() {
  if (cursorPos_ <= 0) return;
  cursorPos_--;
  memmove(buf_ + cursorPos_, buf_ + cursorPos_ + 1, bufLen_ - cursorPos_ - 1);
  bufLen_--;
  buf_[bufLen_] = '\0';
  modified_ = true;
  lastKeystroke_ = millis();
  ensureCursorVisible();
}

void NotesApp::deleteCharForward() {
  if (cursorPos_ >= bufLen_) return;
  memmove(buf_ + cursorPos_, buf_ + cursorPos_ + 1, bufLen_ - cursorPos_ - 1);
  bufLen_--;
  buf_[bufLen_] = '\0';
  modified_ = true;
  lastKeystroke_ = millis();
}

void NotesApp::moveCursorUp() {
  int line = cursorToLine();
  int col = cursorToCol();
  if (line > 0) {
    cursorPos_ = lineColToPos(line - 1, col);
    ensureCursorVisible();
  }
}

void NotesApp::moveCursorDown() {
  int line = cursorToLine();
  int col = cursorToCol();
  int totalLines = countLines();
  if (line < totalLines - 1) {
    cursorPos_ = lineColToPos(line + 1, col);
    ensureCursorVisible();
  }
}

int NotesApp::cursorToLine() const {
  int line = 0, col = 0;
  for (int i = 0; i < cursorPos_ && i < bufLen_; i++) {
    if (buf_[i] == '\n') { line++; col = 0; continue; }
    col++;
    if (col >= charsPerLine_) { line++; col = 0; }
  }
  return line;
}

int NotesApp::cursorToCol() const {
  int col = 0;
  for (int i = cursorPos_ - 1; i >= 0; i--) {
    if (buf_[i] == '\n') break;
    col++;
    if (col >= charsPerLine_) break;
  }
  return col;
}

int NotesApp::lineColToPos(int targetLine, int targetCol) const {
  int line = 0, col = 0, pos = 0;
  while (pos < bufLen_ && line < targetLine) {
    if (buf_[pos] == '\n') { line++; col = 0; pos++; continue; }
    col++;
    pos++;
    if (col >= charsPerLine_) { line++; col = 0; }
  }
  // Advance to targetCol within this line
  while (pos < bufLen_ && col < targetCol) {
    if (buf_[pos] == '\n') break;
    col++;
    pos++;
    if (col >= charsPerLine_) break;
  }
  return pos;
}

void NotesApp::ensureCursorVisible() {
  int curLine = cursorToLine();
  if (curLine < viewScrollLine_) {
    viewScrollLine_ = curLine;
  } else if (curLine >= viewScrollLine_ + linesVisible_) {
    viewScrollLine_ = curLine - linesVisible_ + 1;
  }
}

// =============================================================================
// File operations
// =============================================================================

void NotesApp::openNote(int idx) {
  if (idx >= noteCount_) return;

  snprintf(currentFile_, sizeof(currentFile_), "/notes/%s", notes_[idx]);

  FsFile f = SdMan.open(currentFile_);
  if (!f) { bufLen_ = 0; buf_[0] = '\0'; return; }

  bufLen_ = f.read((uint8_t*)buf_, BUFFER_SIZE - 1);
  if (bufLen_ < 0) bufLen_ = 0;
  buf_[bufLen_] = '\0';
  f.close();

  cursorPos_ = bufLen_;
  modified_ = false;
  viewScrollLine_ = 0;
  ensureCursorVisible();

  Serial.printf("[NOTES] Opened %s (%d bytes, %d words)\n",
                currentFile_, bufLen_, countWords());
}

void NotesApp::createNote(const char* noteName) {
  snprintf(currentFile_, sizeof(currentFile_), "/notes/%s.txt", noteName);

  FsFile f = SdMan.open(currentFile_, (O_WRONLY | O_CREAT | O_TRUNC));
  if (f) f.close();

  bufLen_ = 0;
  buf_[0] = '\0';
  cursorPos_ = 0;
  modified_ = false;
  viewScrollLine_ = 0;

  if (noteCount_ < MAX_NOTES - 1) {
    snprintf(notes_[noteCount_], MAX_NAME_LEN, "%s.txt", noteName);
    noteCount_++;
  }
}

void NotesApp::saveNote() {
  if (currentFile_[0] == '\0') return;

  FsFile f = SdMan.open(currentFile_, (O_WRONLY | O_CREAT | O_TRUNC));
  if (f) {
    f.write((uint8_t*)buf_, bufLen_);
    f.close();
    modified_ = false;
    Serial.printf("[NOTES] Saved %s (%d bytes)\n", currentFile_, bufLen_);
  }
}

// =============================================================================
// Input handling
// =============================================================================

bool NotesApp::handleChar(char c) {
  if (screen_ == SCREEN_EDITOR) {
    if (c == '\b') {
      deleteCharBack();
    } else if (c == 127) {
      deleteCharForward();
    } else if (c >= 32 || c == '\n' || c == '\t') {
      if (c == '\t') {
        insertChar(' ');
        insertChar(' ');
      } else {
        insertChar(c);
      }
    }
    needsFullRedraw = true;
    return true;
  }

  if (screen_ == SCREEN_NEW_NOTE) {
    if (c == '\b') {
      if (newNameLen_ > 0) {
        newNameLen_--;
        newName_[newNameLen_] = '\0';
      }
    } else if (c == '\n') {
      if (newNameLen_ > 0) {
        createNote(newName_);
        screen_ = SCREEN_EDITOR;
      }
    } else if (c >= 32 && c < 127 && newNameLen_ < MAX_NAME_LEN - 5) {
      newName_[newNameLen_++] = c;
      newName_[newNameLen_] = '\0';
    }
    needsFullRedraw = true;
    return true;
  }

  if (screen_ == SCREEN_FILE_LIST && c == '\n') {
    if (listCursor_ == noteCount_) {
      newName_[0] = '\0';
      newNameLen_ = 0;
      screen_ = SCREEN_NEW_NOTE;
    } else {
      openNote(listCursor_);
      screen_ = SCREEN_EDITOR;
    }
    needsFullRedraw = true;
    return true;
  }

  return false;
}

bool NotesApp::handleInput(PluginButton btn) {
  if (screen_ == SCREEN_FILE_LIST) {
    switch (btn) {
      case PluginButton::Up:
        if (listCursor_ > 0) {
          listCursor_--;
          if (listCursor_ < listScroll_) listScroll_ = listCursor_;
        }
        return true;
      case PluginButton::Down:
        if (listCursor_ < noteCount_) {
          listCursor_++;
          if (listCursor_ >= listScroll_ + itemsPerPage_) listScroll_++;
        }
        return true;
      case PluginButton::Center:
        if (listCursor_ == noteCount_) {
          newName_[0] = '\0';
          newNameLen_ = 0;
          screen_ = SCREEN_NEW_NOTE;
        } else {
          openNote(listCursor_);
          screen_ = SCREEN_EDITOR;
        }
        return true;
      case PluginButton::Back:
        return false;
      default:
        return false;
    }
  }

  if (screen_ == SCREEN_EDITOR) {
    switch (btn) {
      case PluginButton::Up:    moveCursorUp(); return true;
      case PluginButton::Down:  moveCursorDown(); return true;
      case PluginButton::Left: {
        if (cursorPos_ > 0) { cursorPos_--; ensureCursorVisible(); }
        return true;
      }
      case PluginButton::Right: {
        if (cursorPos_ < bufLen_) { cursorPos_++; ensureCursorVisible(); }
        return true;
      }
      case PluginButton::Back:
        if (modified_) saveNote();
        screen_ = SCREEN_FILE_LIST;
        scanNotes();
        return true;
      case PluginButton::Center:
        insertChar('\n');
        needsFullRedraw = true;
        return true;
      default:
        return false;
    }
  }

  if (screen_ == SCREEN_NEW_NOTE) {
    switch (btn) {
      case PluginButton::Up:
        if (newNameLen_ > 0) {
          char& c = newName_[newNameLen_ - 1];
          c = (c >= 'a' && c < 'z') ? c + 1 : (c == 'z') ? '0' :
              (c >= '0' && c < '9') ? c + 1 : 'a';
        }
        return true;
      case PluginButton::Down:
        if (newNameLen_ > 0) {
          char& c = newName_[newNameLen_ - 1];
          c = (c > 'a' && c <= 'z') ? c - 1 : (c == 'a') ? '9' :
              (c > '0' && c <= '9') ? c - 1 : 'z';
        }
        return true;
      case PluginButton::Right:
        if (newNameLen_ < MAX_NAME_LEN - 5) {
          newName_[newNameLen_++] = 'a';
          newName_[newNameLen_] = '\0';
        }
        return true;
      case PluginButton::Left:
        if (newNameLen_ > 0) {
          newNameLen_--;
          newName_[newNameLen_] = '\0';
        }
        return true;
      case PluginButton::Center:
        if (newNameLen_ > 0) {
          createNote(newName_);
          screen_ = SCREEN_EDITOR;
        }
        return true;
      case PluginButton::Back:
        screen_ = SCREEN_FILE_LIST;
        return true;
      default:
        return false;
    }
  }

  return false;
}

bool NotesApp::update() {
  if (screen_ == SCREEN_EDITOR && modified_ && lastKeystroke_ > 0) {
    if (millis() - lastKeystroke_ > AUTO_SAVE_MS) {
      saveNote();
      lastKeystroke_ = 0;
      needsFullRedraw = true;
      return true;
    }
  }
  return false;
}

// =============================================================================
// Drawing
// =============================================================================

void NotesApp::drawStatusBar(const char* left, const char* right) {
  d_.drawLine(0, statusBarH_ - 1, W_, statusBarH_ - 1, GxEPD_BLACK);

  d_.setTextColor(GxEPD_BLACK);
  d_.setCursor(marginX_, statusBarH_ - 8);
  d_.print(left);

  if (right && right[0]) {
    int rw = d_.getTextWidth(right);
    d_.setCursor(W_ - marginX_ - rw, statusBarH_ - 8);
    d_.print(right);
  }
}

void NotesApp::draw() {
  d_.fillScreen(GxEPD_WHITE);

  switch (screen_) {
    case SCREEN_FILE_LIST: drawFileList(); break;
    case SCREEN_EDITOR:    drawEditor(); break;
    case SCREEN_NEW_NOTE:  drawNewNote(); break;
  }
}

void NotesApp::drawFileList() {
  drawStatusBar("Notes", "");

  int y = statusBarH_ + 8;
  int displayCount = noteCount_ + 1;

  for (int i = listScroll_; i < displayCount && i < listScroll_ + itemsPerPage_; i++) {
    bool sel = (i == listCursor_);
    int x = marginX_;
    int w = W_ - 2 * marginX_;

    if (sel) {
      d_.drawRect(x, y + 1, w, itemH_ - 2, GxEPD_BLACK);
      d_.drawRect(x + 1, y + 2, w - 2, itemH_ - 4, GxEPD_BLACK);
    }

    d_.setCursor(x + 12, y + itemH_ / 2 + 4);
    d_.setTextColor(GxEPD_BLACK);

    if (i == noteCount_) {
      d_.print("+ New Note");
    } else {
      char display[MAX_NAME_LEN];
      strncpy(display, notes_[i], MAX_NAME_LEN - 1);
      display[MAX_NAME_LEN - 1] = '\0';
      int len = strlen(display);
      if (len > 4 && strcasecmp(display + len - 4, ".txt") == 0) {
        display[len - 4] = '\0';
      }
      d_.print(display);
    }

    if (!sel && i < noteCount_) {
      d_.drawLine(x + 10, y + itemH_ - 1, x + w - 10, y + itemH_ - 1, GxEPD_BLACK);
    }

    y += itemH_;
  }

  // Bottom hint
  d_.setCursor(marginX_, H_ - 20);
  d_.print("OK: Open    Back: Exit");
}

void NotesApp::drawEditor() {
  // Build status strings
  const char* fname = strrchr(currentFile_, '/');
  fname = fname ? fname + 1 : currentFile_;
  char titleBuf[48];
  strncpy(titleBuf, fname, sizeof(titleBuf) - 1);
  titleBuf[sizeof(titleBuf) - 1] = '\0';
  int tlen = strlen(titleBuf);
  if (tlen > 4 && strcasecmp(titleBuf + tlen - 4, ".txt") == 0) {
    titleBuf[tlen - 4] = '\0';
  }
  if (modified_) strncat(titleBuf, " *", sizeof(titleBuf) - strlen(titleBuf) - 1);

  char rightStatus[32];
  snprintf(rightStatus, sizeof(rightStatus), "%d words", countWords());

  drawStatusBar(titleBuf, rightStatus);

  // Draw text
  int x = marginX_;
  int y = editTop_;
  int line = 0;
  int col = 0;
  int cursorScreenX = -1, cursorScreenY = -1;

  for (int i = 0; i <= bufLen_; i++) {
    if (i == cursorPos_) {
      if (line >= viewScrollLine_ && line < viewScrollLine_ + linesVisible_) {
        cursorScreenX = x + col * charW_;
        cursorScreenY = y + (line - viewScrollLine_) * lineH_;
      }
    }

    if (i >= bufLen_) break;
    char c = buf_[i];

    if (c == '\n') {
      line++;
      col = 0;
      continue;
    }

    if (line >= viewScrollLine_ && line < viewScrollLine_ + linesVisible_) {
      int screenY = y + (line - viewScrollLine_) * lineH_;
      int screenX = x + col * charW_;

      if (screenY < editBottom_) {
        d_.setCursor(screenX, screenY + lineH_ - 4);
        d_.print(c);
      }
    }

    col++;
    if (col >= charsPerLine_) {
      line++;
      col = 0;
    }
  }

  // Cursor: thin vertical bar
  if (cursorScreenX >= 0 && cursorScreenY >= 0) {
    d_.fillRect(cursorScreenX, cursorScreenY + 2, 2, lineH_ - 4, GxEPD_BLACK);
  }

  // Scroll indicator
  int totalLines = countLines();
  if (totalLines > linesVisible_) {
    int trackH = editBottom_ - editTop_ - 20;
    int thumbH = (trackH * linesVisible_) / totalLines;
    if (thumbH < 10) thumbH = 10;
    int maxScroll = totalLines - linesVisible_;
    if (maxScroll < 1) maxScroll = 1;
    int thumbY = editTop_ + 10 + (trackH - thumbH) * viewScrollLine_ / maxScroll;
    d_.fillRect(W_ - 4, thumbY, 3, thumbH, GxEPD_BLACK);
  }
}

void NotesApp::drawNewNote() {
  drawStatusBar("New Note", "");

  int centerY = H_ / 2;

  d_.setCursor(marginX_, centerY - 40);
  d_.print("Note name:");

  int fieldX = marginX_;
  int fieldY = centerY - 10;
  int fieldW = W_ / 2;
  int fieldH = lineH_ + 8;
  d_.drawRect(fieldX, fieldY, fieldW, fieldH, GxEPD_BLACK);

  d_.setCursor(fieldX + 6, fieldY + fieldH - 6);
  if (newNameLen_ > 0) {
    d_.print(newName_);
  }
  int curX = fieldX + 6 + newNameLen_ * charW_;
  d_.fillRect(curX, fieldY + 4, 2, fieldH - 8, GxEPD_BLACK);

  d_.setCursor(marginX_, centerY + 40);
  d_.print("Type name or use Up/Down/L/R    OK: Create    Back: Cancel");
}

}  // namespace sumi

#endif  // FEATURE_PLUGINS
