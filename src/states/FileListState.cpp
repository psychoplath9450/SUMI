#include "FileListState.h"

#include <Arduino.h>
#include <EInkDisplay.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <esp_system.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "../core/BootMode.h"
#include "../core/Core.h"
#include "../content/LibraryIndex.h"
#include "../ui/Elements.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"

namespace sumi {

FileListState::FileListState(GfxRenderer& renderer)
    : renderer_(renderer),
      selectedIndex_(0),
      needsRender_(true),
      hasSelection_(false),
      goHome_(false),
      firstRender_(true),
      currentScreen_(Screen::Browse),
      confirmView_{} {
  strcpy(currentDir_, "/");
  selectedPath_[0] = '\0';
}

FileListState::~FileListState() = default;

void FileListState::setDirectory(const char* dir) {
  if (dir && dir[0] != '\0') {
    strncpy(currentDir_, dir, sizeof(currentDir_) - 1);
    currentDir_[sizeof(currentDir_) - 1] = '\0';
  } else {
    strcpy(currentDir_, "/");
  }
}

void FileListState::enter(Core& core) {
  Serial.printf("[FILES] Entering, dir: %s\n", currentDir_);

  // Preserve position when returning from Reader via boot transition
  const auto& transition = getTransition();
  bool preservePosition = transition.isValid() && transition.returnTo == ReturnTo::FILE_MANAGER;

  if (preservePosition) {
    // Restore directory from settings
    strncpy(currentDir_, core.settings.fileListDir, sizeof(currentDir_) - 1);
    currentDir_[sizeof(currentDir_) - 1] = '\0';
    // Consume the transition so it doesn't fire again on next entry
    clearTransition();
  }

  needsRender_ = true;
  hasSelection_ = false;
  goHome_ = false;
  firstRender_ = true;
  currentScreen_ = Screen::Browse;
  selectedPath_[0] = '\0';

  loadFiles(core);

  if (preservePosition && !files_.empty()) {
    selectedIndex_ = core.settings.fileListSelectedIndex;

    // Clamp to valid range
    if (selectedIndex_ >= files_.size()) {
      selectedIndex_ = files_.size() - 1;
    }

    // Verify filename matches, search if not
    if (strcasecmp(files_[selectedIndex_].name.c_str(), core.settings.fileListSelectedName) != 0) {
      for (size_t i = 0; i < files_.size(); i++) {
        if (strcasecmp(files_[i].name.c_str(), core.settings.fileListSelectedName) == 0) {
          selectedIndex_ = i;
          break;
        }
      }
    }
  } else {
    selectedIndex_ = 0;
  }
}

void FileListState::exit(Core& core) { Serial.println("[FILES] Exiting"); }

void FileListState::loadFiles(Core& core) {
  files_.clear();
  files_.reserve(512);  // Pre-allocate for large libraries

  FsFile dir;
  auto result = core.storage.openDir(currentDir_, dir);
  if (!result.ok()) {
    Serial.printf("[FILES] Failed to open dir: %s\n", currentDir_);
    return;
  }

  char name[256];
  FsFile entry;

  // Collect all entries (no hard limit during collection)
  while ((entry = dir.openNextFile())) {
    entry.getName(name, sizeof(name));

    if (isHidden(name)) {
      entry.close();
      continue;
    }

    bool isDir = entry.isDirectory();
    entry.close();

    if (isDir || isSupportedFile(name)) {
      files_.push_back({std::string(name), isDir, -1, false, 0});
    } else if (isConvertibleFile(name)) {
      files_.push_back({std::string(name), false, -1, true, 0});
    }
  }
  dir.close();

  // Safety check - prevent OOM on extreme cases
  constexpr size_t MAX_ENTRIES = 1000;
  if (files_.size() > MAX_ENTRIES) {
    Serial.printf("[FILES] Warning: truncated to %zu entries\n", MAX_ENTRIES);
    files_.resize(MAX_ENTRIES);
    files_.shrink_to_fit();
  }

  // Sort: directories first, then supported files, then unsupported (convertible),
  // each group sorted by natural sort (case-insensitive)
  std::sort(files_.begin(), files_.end(), [](const FileEntry& a, const FileEntry& b) {
    // Group ordering: dirs > supported > unsupported
    auto groupRank = [](const FileEntry& f) -> int {
      if (f.isDir) return 0;
      if (!f.unsupported) return 1;
      return 2;
    };
    int ga = groupRank(a), gb = groupRank(b);
    if (ga != gb) return ga < gb;

    const char* s1 = a.name.c_str();
    const char* s2 = b.name.c_str();

    while (*s1 && *s2) {
      const auto uc = [](char c) { return static_cast<unsigned char>(c); };
      if (std::isdigit(uc(*s1)) && std::isdigit(uc(*s2))) {
        // Skip leading zeros
        while (*s1 == '0') s1++;
        while (*s2 == '0') s2++;

        // Compare by digit length first
        int len1 = 0, len2 = 0;
        while (std::isdigit(uc(s1[len1]))) len1++;
        while (std::isdigit(uc(s2[len2]))) len2++;
        if (len1 != len2) return len1 < len2;

        // Same length: compare digit by digit
        for (int i = 0; i < len1; i++) {
          if (s1[i] != s2[i]) return s1[i] < s2[i];
        }
        s1 += len1;
        s2 += len2;
      } else {
        char c1 = std::tolower(uc(*s1));
        char c2 = std::tolower(uc(*s2));
        if (c1 != c2) return c1 < c2;
        s1++;
        s2++;
      }
    }
    return *s1 == '\0' && *s2 != '\0';
  });

  Serial.printf("[FILES] Loaded %zu entries\n", files_.size());

  // Match files against library index for progress display
  LibraryIndex::Entry libEntries[128];
  int libCount = LibraryIndex::loadAll(core, libEntries, 128);
  if (libCount > 0) {
    for (auto& f : files_) {
      if (f.isDir) continue;
      // Build full path for hash
      char fullPath[512];
      size_t dirLen = strlen(currentDir_);
      if (currentDir_[dirLen - 1] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", currentDir_, f.name.c_str());
      } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentDir_, f.name.c_str());
      }
      uint32_t hash = LibraryIndex::hashPath(fullPath);
      for (int j = 0; j < libCount; j++) {
        if (libEntries[j].pathHash == hash) {
          f.progressPercent = libEntries[j].progressPercent();
          f.contentHint = libEntries[j].contentHint;
          break;
        }
      }
    }
    Serial.printf("[FILES] Matched %d library entries\n", libCount);
  }
}

bool FileListState::isHidden(const char* name) const {
  if (name[0] == '.') return true;
  if (FsHelpers::isHiddenFsItem(name)) return true;
  if (strncmp(name, "FOUND.", 6) == 0) return true;
  return false;
}

bool FileListState::isSupportedFile(const char* name) const {
  const char* ext = strrchr(name, '.');
  if (!ext) return false;
  ext++;  // Skip the dot

  // Case-insensitive extension check (matches ContentTypes.cpp)
  if (strcasecmp(ext, "epub") == 0) return true;
  if (strcasecmp(ext, "xtc") == 0) return true;
  if (strcasecmp(ext, "xtch") == 0) return true;
  if (strcasecmp(ext, "xtg") == 0) return true;
  if (strcasecmp(ext, "xth") == 0) return true;
  if (strcasecmp(ext, "txt") == 0) return true;
  if (strcasecmp(ext, "md") == 0) return true;
  if (strcasecmp(ext, "markdown") == 0) return true;
  return false;
}

bool FileListState::isConvertibleFile(const char* name) const {
  const char* ext = strrchr(name, '.');
  if (!ext) return false;
  ext++;  // Skip the dot

  // Common book/document formats convertible via sumi.page
  if (strcasecmp(ext, "pdf") == 0) return true;
  if (strcasecmp(ext, "docx") == 0) return true;
  if (strcasecmp(ext, "doc") == 0) return true;
  if (strcasecmp(ext, "rtf") == 0) return true;
  if (strcasecmp(ext, "odt") == 0) return true;
  if (strcasecmp(ext, "fb2") == 0) return true;
  if (strcasecmp(ext, "mobi") == 0) return true;
  if (strcasecmp(ext, "azw") == 0) return true;
  if (strcasecmp(ext, "azw3") == 0) return true;
  if (strcasecmp(ext, "djvu") == 0) return true;
  if (strcasecmp(ext, "html") == 0) return true;
  if (strcasecmp(ext, "htm") == 0) return true;
  if (strcasecmp(ext, "mhtml") == 0) return true;

  // Comic/manga archives
  if (strcasecmp(ext, "cbz") == 0) return true;
  if (strcasecmp(ext, "cbr") == 0) return true;
  if (strcasecmp(ext, "cb7") == 0) return true;

  // Image formats (for wallpapers/sleep screens)
  if (strcasecmp(ext, "jpg") == 0) return true;
  if (strcasecmp(ext, "jpeg") == 0) return true;
  if (strcasecmp(ext, "png") == 0) return true;
  if (strcasecmp(ext, "gif") == 0) return true;
  if (strcasecmp(ext, "webp") == 0) return true;
  if (strcasecmp(ext, "tiff") == 0) return true;
  if (strcasecmp(ext, "tif") == 0) return true;

  return false;
}

void FileListState::showConvertMessage(Core& core, const char* filename) {
  Theme& theme = THEME_MANAGER.mutableCurrent();
  renderer_.clearScreen(theme.backgroundColor);

  const int screenW = renderer_.getScreenWidth();
  const int screenH = renderer_.getScreenHeight();
  const int lineH = renderer_.getLineHeight(theme.uiFontId);
  const int titleLineH = renderer_.getLineHeight(theme.readerFontId);

  // Figure out what kind of file this is for a helpful hint
  const char* ext = strrchr(filename, '.');
  const char* formatHint = "EPUB";
  if (ext) {
    ext++;
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "webp") == 0 || strcasecmp(ext, "tiff") == 0 ||
        strcasecmp(ext, "tif") == 0) {
      formatHint = "BMP";
    } else if (strcasecmp(ext, "cbz") == 0 || strcasecmp(ext, "cbr") == 0 ||
               strcasecmp(ext, "cb7") == 0) {
      formatHint = "EPUB (comic mode)";
    }
  }

  // Layout the message vertically centered
  const int totalH = titleLineH + lineH * 5 + 40;  // title + 5 body lines + spacing
  int y = (screenH - totalH) / 2;

  // Title
  renderer_.drawCenteredText(theme.readerFontId, y, "Needs Conversion!", theme.primaryTextBlack, BOLD);
  y += titleLineH + 20;

  // Truncated filename
  char nameLine[48];
  if (strlen(filename) > 36) {
    snprintf(nameLine, sizeof(nameLine), "%.33s...", filename);
  } else {
    strncpy(nameLine, filename, sizeof(nameLine) - 1);
    nameLine[sizeof(nameLine) - 1] = '\0';
  }
  renderer_.drawCenteredText(theme.uiFontId, y, nameLine, theme.primaryTextBlack);
  y += lineH + 12;

  // Body message
  renderer_.drawCenteredText(theme.uiFontId, y, "This format isn't native to SUMI.", theme.primaryTextBlack);
  y += lineH + 4;
  renderer_.drawCenteredText(theme.uiFontId, y, "Head over to sumi.page and drop", theme.primaryTextBlack);
  y += lineH + 2;
  renderer_.drawCenteredText(theme.uiFontId, y, "it in the converter. It'll convert", theme.primaryTextBlack);
  y += lineH + 2;

  // Target format hint
  char hintLine[48];
  snprintf(hintLine, sizeof(hintLine), "to %s for you.", formatHint);
  renderer_.drawCenteredText(theme.uiFontId, y, hintLine, theme.primaryTextBlack);
  y += lineH + 20;

  // URL
  renderer_.drawCenteredText(theme.readerFontId, y, "sumi.page/convert", theme.primaryTextBlack, BOLD);

  renderer_.displayBuffer();
}

StateTransition FileListState::update(Core& core) {
  // Process input events
  Event e;
  while (core.events.pop(e)) {
    switch (e.type) {
      case EventType::ButtonPress:
        if (currentScreen_ == Screen::ConvertInfo) {
          // Any button dismisses the convert info message
          currentScreen_ = Screen::Browse;
          needsRender_ = true;
        } else if (currentScreen_ == Screen::ConfirmDelete) {
          // Confirmation dialog input
          switch (e.button) {
            case Button::Up:
            case Button::Down:
              confirmView_.toggleSelection();
              needsRender_ = true;
              break;
            case Button::Center:
              if (confirmView_.isYesSelected()) {
                // Execute delete inline (like SettingsState pattern)
                const FileEntry& entry = files_[selectedIndex_];
                char pathBuf[512];  // currentDir_(256) + '/' + name(128)
                size_t dirLen = strlen(currentDir_);
                if (currentDir_[dirLen - 1] == '/') {
                  snprintf(pathBuf, sizeof(pathBuf), "%s%s", currentDir_, entry.name.c_str());
                } else {
                  snprintf(pathBuf, sizeof(pathBuf), "%s/%s", currentDir_, entry.name.c_str());
                }

                // Check if trying to delete the currently active book
                const char* activeBook = core.settings.lastBookPath;
                if (activeBook[0] != '\0' && strcmp(pathBuf, activeBook) == 0) {
                  ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Cannot delete active book");
                  vTaskDelay(1500 / portTICK_PERIOD_MS);
                } else {
                  ui::centeredMessage(renderer_, THEME, THEME.uiFontId, "Deleting...");

                  Result<void> result = entry.isDir ? core.storage.rmdir(pathBuf) : core.storage.remove(pathBuf);

                  const char* msg = result.ok() ? "Deleted" : "Delete failed";
                  ui::centeredMessage(renderer_, THEME, THEME.uiFontId, msg);
                  vTaskDelay(1000 / portTICK_PERIOD_MS);

                  loadFiles(core);
                  if (selectedIndex_ >= files_.size()) {
                    selectedIndex_ = files_.empty() ? 0 : files_.size() - 1;
                  }
                }
              }
              currentScreen_ = Screen::Browse;
              needsRender_ = true;
              break;
            case Button::Back:
            case Button::Left:
              currentScreen_ = Screen::Browse;
              needsRender_ = true;
              break;
            default:
              break;
          }
        } else {
          // Normal browse mode
          switch (e.button) {
            case Button::Up:
              navigateUp(core);
              break;
            case Button::Down:
              navigateDown(core);
              break;
            case Button::Left:
              break;
            case Button::Right:
              goHome_ = true;
              break;
            case Button::Center:
              openSelected(core);
              break;
            case Button::Back:
              goBack(core);
              break;
            case Button::Power:
              break;
          }
        }
        break;

      default:
        break;
    }
  }

  // If a file was selected, transition to reader
  if (hasSelection_) {
    hasSelection_ = false;
    return StateTransition::to(StateId::Reader);
  }

  // Return to home if requested
  if (goHome_) {
    goHome_ = false;
    strcpy(currentDir_, "/");  // Reset for next entry
    return StateTransition::to(StateId::Home);
  }

  return StateTransition::stay(StateId::FileList);
}

void FileListState::render(Core& core) {
  if (!needsRender_) {
    return;
  }

  Theme& theme = THEME_MANAGER.mutableCurrent();

  if (currentScreen_ == Screen::ConfirmDelete) {
    ui::render(renderer_, theme, confirmView_);
    confirmView_.needsRender = false;
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  if (currentScreen_ == Screen::ConvertInfo) {
    // Already rendered by showConvertMessage(), just clear the flag
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  renderer_.clearScreen(theme.backgroundColor);

  // Title with page indicator
  char title[40];
  if (getTotalPages() > 1) {
    snprintf(title, sizeof(title), "File Browser (%d/%d)", getCurrentPage(), getTotalPages());
  } else {
    strcpy(title, "File Browser");
  }
  renderer_.drawCenteredText(theme.readerFontId, 10, title, theme.primaryTextBlack, BOLD);

  // Empty state
  if (files_.empty()) {
    renderer_.drawText(theme.uiFontId, 20, 60, "No files found", theme.primaryTextBlack);
    renderer_.displayBuffer();
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  // Draw current page of items
  constexpr int listStartY = 60;
  const int itemHeight = theme.menuItemHeight + theme.itemSpacing;
  const int pageItems = getPageItems();
  const int pageStart = getPageStartIndex();
  const int pageEnd = std::min(pageStart + pageItems, static_cast<int>(files_.size()));

  for (int i = pageStart; i < pageEnd; i++) {
    const int y = listStartY + (i - pageStart) * itemHeight;
    ui::fileEntry(renderer_, theme, y, files_[i].name.c_str(), files_[i].isDir,
                  static_cast<size_t>(i) == selectedIndex_, files_[i].progressPercent,
                  files_[i].unsupported, files_[i].contentHint);
  }

  if (firstRender_) {
    renderer_.displayBuffer(EInkDisplay::HALF_REFRESH);
    firstRender_ = false;
  } else {
    renderer_.displayBuffer();
  }
  needsRender_ = false;
  core.display.markDirty();
}

void FileListState::navigateUp(Core& core) {
  if (files_.empty()) return;

  if (selectedIndex_ > 0) {
    selectedIndex_--;
  } else {
    selectedIndex_ = files_.size() - 1;  // Wrap to last item
  }
  needsRender_ = true;
}

void FileListState::navigateDown(Core& core) {
  if (files_.empty()) return;

  if (selectedIndex_ + 1 < files_.size()) {
    selectedIndex_++;
  } else {
    selectedIndex_ = 0;  // Wrap to first item
  }
  needsRender_ = true;
}

void FileListState::openSelected(Core& core) {
  if (files_.empty()) {
    return;
  }

  const FileEntry& entry = files_[selectedIndex_];

  // Build full path
  size_t dirLen = strlen(currentDir_);
  if (currentDir_[dirLen - 1] == '/') {
    snprintf(selectedPath_, sizeof(selectedPath_), "%s%s", currentDir_, entry.name.c_str());
  } else {
    snprintf(selectedPath_, sizeof(selectedPath_), "%s/%s", currentDir_, entry.name.c_str());
  }

  if (entry.isDir) {
    // Enter directory
    strncpy(currentDir_, selectedPath_, sizeof(currentDir_) - 1);
    currentDir_[sizeof(currentDir_) - 1] = '\0';
    selectedIndex_ = 0;
    loadFiles(core);
    needsRender_ = true;

    // Save directory for return after mode switch
    strncpy(core.settings.fileListDir, currentDir_, sizeof(core.settings.fileListDir) - 1);
    core.settings.fileListDir[sizeof(core.settings.fileListDir) - 1] = '\0';
    core.settings.fileListSelectedName[0] = '\0';
    core.settings.fileListSelectedIndex = 0;
  } else if (entry.unsupported) {
    // Show friendly conversion message
    showConvertMessage(core, entry.name.c_str());
    currentScreen_ = Screen::ConvertInfo;
  } else {
    // Save position for return
    strncpy(core.settings.fileListDir, currentDir_, sizeof(core.settings.fileListDir) - 1);
    core.settings.fileListDir[sizeof(core.settings.fileListDir) - 1] = '\0';
    strncpy(core.settings.fileListSelectedName, entry.name.c_str(), sizeof(core.settings.fileListSelectedName) - 1);
    core.settings.fileListSelectedName[sizeof(core.settings.fileListSelectedName) - 1] = '\0';
    core.settings.fileListSelectedIndex = selectedIndex_;

    // Select file - transition to Reader mode via restart
    Serial.printf("[FILES] Selected: %s\n", selectedPath_);
    showTransitionNotification("Opening book...");
    saveTransition(BootMode::READER, selectedPath_, ReturnTo::FILE_MANAGER);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ESP.restart();
  }
}

void FileListState::goBack(Core& core) {
  // Navigate to parent directory or go home if at root
  if (strcmp(currentDir_, "/") == 0) {
    // At root - go back to Home
    goHome_ = true;
    return;
  }

  // Find last slash and truncate
  char* lastSlash = strrchr(currentDir_, '/');
  if (lastSlash && lastSlash != currentDir_) {
    *lastSlash = '\0';
  } else {
    strcpy(currentDir_, "/");
  }

  selectedIndex_ = 0;
  loadFiles(core);
  needsRender_ = true;
}

void FileListState::promptDelete(Core& core) {
  if (files_.empty()) return;

  const FileEntry& entry = files_[selectedIndex_];
  const char* typeStr = entry.isDir ? "folder" : "file";

  char line1[48];
  snprintf(line1, sizeof(line1), "Delete this %s?", typeStr);

  char line2[48];
  if (entry.name.length() > 40) {
    snprintf(line2, sizeof(line2), "%.37s...", entry.name.c_str());
  } else {
    strncpy(line2, entry.name.c_str(), sizeof(line2) - 1);
    line2[sizeof(line2) - 1] = '\0';
  }

  confirmView_.setup("Confirm Delete", line1, line2);
  currentScreen_ = Screen::ConfirmDelete;
  needsRender_ = true;
}

int FileListState::getPageItems() const {
  const Theme& theme = THEME_MANAGER.current();
  constexpr int listStartY = 60;
  constexpr int bottomMargin = 70;
  const int availableHeight = renderer_.getScreenHeight() - listStartY - bottomMargin;
  const int itemHeight = theme.menuItemHeight + theme.itemSpacing;
  return std::max(1, availableHeight / itemHeight);
}

int FileListState::getTotalPages() const {
  if (files_.empty()) return 1;
  const int pageItems = getPageItems();
  return (static_cast<int>(files_.size()) + pageItems - 1) / pageItems;
}

int FileListState::getCurrentPage() const {
  const int pageItems = getPageItems();
  return selectedIndex_ / pageItems + 1;
}

int FileListState::getPageStartIndex() const {
  const int pageItems = getPageItems();
  return (selectedIndex_ / pageItems) * pageItems;
}

}  // namespace sumi
