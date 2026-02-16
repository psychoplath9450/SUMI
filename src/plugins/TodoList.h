#pragma once

/**
 * @file TodoList.h
 * @brief Simple to-do list — ported from SUMI
 *
 * Reads/writes /data/todo.txt on SD card.
 * Format: X Completed task / - Pending task
 * Priority: !!! urgent, !! high, ! normal
 */

#include "../config.h"

#if FEATURE_PLUGINS

#include <Arduino.h>
#include <SDCardManager.h>
#include <cstring>

#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

class TodoApp : public PluginInterface {
 public:
  static constexpr int MAX_VISIBLE = 8;
  static constexpr int MAX_TEXT = 40;

  explicit TodoApp(PluginRenderer& renderer) : d_(renderer) {}

  const char* name() const override { return "TodoList"; }
  PluginRunMode runMode() const override { return PluginRunMode::Simple; }

  void init(int screenW, int screenH) override {
    screenW_ = screenW;
    screenH_ = screenH;
    landscape_ = isLandscapeMode(screenW, screenH);
    itemH_ = landscape_ ? 40 : 45;
    cursor_ = 0;
    loadVisible();
  }

  bool handleInput(PluginButton btn) override {
    switch (btn) {
      case PluginButton::Up:
        if (cursor_ > 0) { cursor_--; loadVisible(); }
        return true;
      case PluginButton::Down:
        if (cursor_ < count_ - 1) { cursor_++; loadVisible(); }
        return true;
      case PluginButton::Center:
        toggleCurrent();
        return true;
      case PluginButton::Left:
      case PluginButton::Right:
        return true;
      case PluginButton::Back:
        return false;
      default:
        return false;
    }
  }

  void draw() override {
    PluginUI::drawHeader(d_, "To-Do List", screenW_);

    if (count_ == 0) {
      d_.setCursor(20, screenH_ / 2 - 30);
      d_.print("No tasks found.");
      d_.setCursor(20, screenH_ / 2);
      d_.print("Add tasks via /data/todo.txt");
      d_.setCursor(20, screenH_ / 2 + 25);
      d_.print("Format: X Task (X=done, -=pending)");
      PluginUI::drawFooter(d_, "", "", screenW_, screenH_);
      return;
    }

    int y = PLUGIN_HEADER_H + 8;
    for (int i = 0; i < visibleCount_; i++) {
      bool sel = (i == cursor_ - scrollStart_);
      int x = PLUGIN_MARGIN;
      int w = screenW_ - 2 * PLUGIN_MARGIN;

      if (sel) {
        d_.drawRect(x, y, w, itemH_ - 4, GxEPD_BLACK);
        d_.drawRect(x + 1, y + 1, w - 2, itemH_ - 6, GxEPD_BLACK);
      }

      // Priority markers
      int priority = 0;
      const char* taskText = texts_[i];
      while (*taskText == '!') { priority++; taskText++; }
      if (priority > 0 && *taskText == ' ') taskText++;

      int prioX = x + 4;
      int prioY = y + itemH_ / 2;
      if (priority >= 3) {
        d_.fillCircle(prioX, prioY, 4, GxEPD_BLACK);
      } else if (priority == 2) {
        d_.drawCircle(prioX, prioY, 4, GxEPD_BLACK);
        d_.fillCircle(prioX, prioY, 1, GxEPD_BLACK);
      } else if (priority == 1) {
        d_.drawCircle(prioX, prioY, 3, GxEPD_BLACK);
      }

      // Checkbox
      int cbX = x + (priority > 0 ? 18 : 8);
      int cbY = y + (itemH_ - 20) / 2;
      int cbS = 18;
      d_.drawRect(cbX, cbY, cbS, cbS, GxEPD_BLACK);

      if (done_[i]) {
        d_.drawLine(cbX + 3, cbY + 9, cbX + 7, cbY + 13, GxEPD_BLACK);
        d_.drawLine(cbX + 7, cbY + 13, cbX + 14, cbY + 4, GxEPD_BLACK);
        d_.drawLine(cbX + 3, cbY + 10, cbX + 7, cbY + 14, GxEPD_BLACK);
        d_.drawLine(cbX + 7, cbY + 14, cbX + 14, cbY + 5, GxEPD_BLACK);
      }

      // Task text
      d_.setCursor(cbX + cbS + 12, y + itemH_ - 14);
      d_.print(taskText);

      if (done_[i]) {
        int textY = y + itemH_ / 2;
        int textW = strlen(taskText) * 6;
        d_.drawLine(cbX + cbS + 12, textY, cbX + cbS + 12 + textW, textY, GxEPD_BLACK);
      }

      y += itemH_;
    }

    int completed = 0;
    for (int i = 0; i < visibleCount_; i++) if (done_[i]) completed++;

    char status[32];
    snprintf(status, sizeof(status), "%d/%d done", completed, count_);
    PluginUI::drawFooter(d_, status, "OK:Toggle", screenW_, screenH_);
  }

 private:
  PluginRenderer& d_;
  char texts_[MAX_VISIBLE][MAX_TEXT];
  bool done_[MAX_VISIBLE];
  int visibleCount_ = 0;
  int scrollStart_ = 0;
  int cursor_ = 0;
  int count_ = 0;
  int screenW_ = 0, screenH_ = 0;
  bool landscape_ = false;
  int itemH_ = 40;

  void loadVisible() {
    count_ = 0;
    visibleCount_ = 0;
    scrollStart_ = 0;

    if (!SdMan.exists("/data")) SdMan.mkdir("/data");

    FsFile f;
    if (!SdMan.openFileForRead("TODO", "/data/todo.txt", f)) {
      // Create sample file
      FsFile wf;
      if (SdMan.openFileForWrite("TODO", "/data/todo.txt", wf)) {
        wf.println("- Sample task 1");
        wf.println("- Sample task 2");
        wf.println("X Completed task");
        wf.close();
      }
      if (!SdMan.openFileForRead("TODO", "/data/todo.txt", f)) return;
    }

    // Count total lines
    int totalLines = 0;
    char buf[MAX_TEXT + 4];
    while (f.available()) {
      int len = f.fgets(buf, sizeof(buf));
      if (len > 1) totalLines++;
    }
    count_ = totalLines;

    // Calculate visible window
    int maxVisible = (screenH_ - PLUGIN_HEADER_H - PLUGIN_FOOTER_H - 16) / itemH_;
    if (maxVisible > MAX_VISIBLE) maxVisible = MAX_VISIBLE;

    int start = 0;
    if (cursor_ >= maxVisible) start = cursor_ - maxVisible + 1;
    scrollStart_ = start;

    // Re-read visible items
    f.seekSet(0);
    int line = 0;
    while (f.available() && visibleCount_ < maxVisible) {
      int len = f.fgets(buf, sizeof(buf));
      if (len <= 0) continue;

      // Trim trailing whitespace
      while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == ' ')) buf[--len] = '\0';
      if (len < 2) { line++; continue; }

      if (line >= start && visibleCount_ < maxVisible) {
        done_[visibleCount_] = (buf[0] == 'X' || buf[0] == 'x');
        const char* text = buf + 2;
        strncpy(texts_[visibleCount_], text, MAX_TEXT - 1);
        texts_[visibleCount_][MAX_TEXT - 1] = '\0';
        visibleCount_++;
      }
      line++;
    }
    f.close();
  }

  void toggleCurrent() {
    int idx = cursor_ - scrollStart_;
    if (idx < 0 || idx >= visibleCount_) return;
    done_[idx] = !done_[idx];

    // Read all lines, modify target, rewrite
    FsFile f;
    if (!SdMan.openFileForRead("TODO", "/data/todo.txt", f)) return;

    char lines[20][MAX_TEXT + 4];
    int lineCount = 0;
    while (f.available() && lineCount < 20) {
      int len = f.fgets(lines[lineCount], MAX_TEXT + 2);
      if (len <= 0) continue;
      while (len > 0 && (lines[lineCount][len - 1] == '\r' || lines[lineCount][len - 1] == '\n'))
        lines[lineCount][--len] = '\0';
      lineCount++;
    }
    f.close();

    int targetLine = scrollStart_ + idx;
    if (targetLine < lineCount && strlen(lines[targetLine]) >= 2) {
      lines[targetLine][0] = done_[idx] ? 'X' : '-';
    }

    FsFile wf;
    if (SdMan.openFileForWrite("TODO", "/data/todo.txt", wf)) {
      for (int i = 0; i < lineCount; i++) wf.println(lines[i]);
      wf.close();
    }
  }
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
