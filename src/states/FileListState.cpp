#include "FileListState.h"

#include <Arduino.h>
#include <EInkDisplay.h>
#include <EpubChapterParser.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <PageCache.h>
#include <SDCardManager.h>
#include <Utf8.h>
#include <esp_system.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "../FontManager.h"
#include "../core/BootMode.h"
#include "../core/Core.h"
#include "../content/LibraryIndex.h"
#include "../ui/Elements.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"

#if FEATURE_PLUGINS
#include "PluginHostState.h"
#include "PluginListState.h"
#endif

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
    utf8SafeCopy(currentDir_, dir, sizeof(currentDir_));
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
    utf8SafeCopy(currentDir_, core.settings.fileListDir, sizeof(currentDir_));
    // Consume the transition so it doesn't fire again on next entry
    clearTransition();
  }

  needsRender_ = true;
  hasSelection_ = false;
  goHome_ = false;
  firstRender_ = true;
  currentScreen_ = Screen::Browse;
  selectedPath_[0] = '\0';
#if FEATURE_PLUGINS
  launchPlugin_ = false;
#endif

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
  // Conservative pre-reserve so we don't throw std::bad_alloc on a
  // fragmented heap (free heap on Home with arena held is typically
  // ~29 KB with <16 KB contiguous — reserve(512) tried to grab ~16 KB
  // and threw, which aborts because ESP-IDF can't unwind C++ exceptions).
  // Vector grows geometrically as needed for actual entry counts.
  files_.reserve(32);

  FsFile dir;
  auto result = core.storage.openDir(currentDir_, dir);
  if (!result.ok()) {
    Serial.printf("[FILES] Failed to open dir: %s\n", currentDir_);
    return;
  }

  char name[256];
  FsFile entry;

  // Hard cap enforced DURING collection — push_back on a 380 KB heap
  // device can OOM well before a degenerate "directory with 100k
  // entries" finishes iterating. Pre-Batch-9-audit the limit
  // was applied AFTER the loop via resize/shrink_to_fit, which still
  // attempted every push_back on the way to the cap.
  constexpr size_t MAX_ENTRIES = 1000;

  while ((entry = dir.openNextFile()) && files_.size() < MAX_ENTRIES) {
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
  if (entry) {
    Serial.printf("[FILES] Warning: capped at %zu entries — directory has more\n",
                  MAX_ENTRIES);
    entry.close();
  }
  dir.close();

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
      if (dirLen > 0 && currentDir_[dirLen - 1] == '/') {
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
  // Always hide FOUND.* (filesystem recovery artifacts)
  if (strncmp(name, "FOUND.", 6) == 0) return true;
  // Dotfiles and FS hidden items: respect user setting
  if (!sumi::core.settings.showHiddenFiles) {
    if (name[0] == '.') return true;
    if (FsHelpers::isHiddenFsItem(name)) return true;
  }
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
  if (strcasecmp(ext, "comic") == 0) return true;

  // Image formats (opened via Images app)
  if (strcasecmp(ext, "bmp") == 0) return true;
  if (strcasecmp(ext, "png") == 0) return true;
  if (strcasecmp(ext, "jpg") == 0) return true;
  if (strcasecmp(ext, "jpeg") == 0) return true;

  // Flashcard deck formats (opened via Flashcards app, not the reader)
  if (strcasecmp(ext, "tsv") == 0) return true;
  if (strcasecmp(ext, "csv") == 0) return true;
  return false;
}

bool FileListState::isImageFile(const char* name) const {
  const char* ext = strrchr(name, '.');
  if (!ext) return false;
  ext++;
  return (strcasecmp(ext, "bmp") == 0 ||
          strcasecmp(ext, "png") == 0 ||
          strcasecmp(ext, "jpg") == 0 ||
          strcasecmp(ext, "jpeg") == 0);
}

bool FileListState::isFlashcardFile(const char* name) const {
  const char* ext = strrchr(name, '.');
  if (!ext) return false;
  ext++;
  return (strcasecmp(ext, "tsv") == 0 || strcasecmp(ext, "csv") == 0);
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

  // Image formats not natively supported (convert via sumi.page)
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
    if (strcasecmp(ext, "gif") == 0 ||
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

  // Truncated filename (UTF-8 safe — `%.33s` and strncpy would slice a
  // CJK codepoint mid-sequence, rendered later as '?'). Copy through
  // utf8SafeCopy and append "..." only if the source actually overflowed.
  char nameLine[48];
  if (strlen(filename) > 36) {
    // Reserve 3 bytes at the end for "..." + a spare for the terminator.
    char head[34];
    utf8SafeCopy(head, filename, sizeof(head));
    snprintf(nameLine, sizeof(nameLine), "%s...", head);
  } else {
    utf8SafeCopy(nameLine, filename, sizeof(nameLine));
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
          // Any button dismisses the info message.
          currentScreen_ = Screen::Browse;
          needsRender_ = true;
        } else if (currentScreen_ == Screen::IndexConfirm) {
          // Center starts indexing; anything else cancels.
          if (e.button == Button::Center) {
            if (!startIndexing(core)) {
              // EPUB-only / open failure → bail out via the Done screen,
              // which surfaces the error to the user instead of a silent
              // dismissal.
              currentScreen_ = Screen::IndexDone;
              needsRender_ = true;
            }
          } else {
            currentScreen_ = Screen::Browse;
            needsRender_ = true;
          }
        } else if (currentScreen_ == Screen::Indexing) {
          // Cancel mid-indexing via Back or Power. Other buttons ignored
          // so a stray press doesn't disturb the parser.
          if (e.button == Button::Back || e.button == Button::Power) {
            Serial.printf("[INDEX] User cancelled at chapter %u of %u\n",
                          (unsigned)indexCurrentSpine_, (unsigned)indexTotalSpines_);
            indexResult_ = 2;  // cancelled
            finishIndexing(core);
            currentScreen_ = Screen::IndexDone;
            needsRender_ = true;
          }
        } else if (currentScreen_ == Screen::IndexDone) {
          // Any button returns to browse. Reload the file list so any
          // newly-built caches are reflected (e.g. progress bar update).
          currentScreen_ = Screen::Browse;
          loadFiles(core);
          needsRender_ = true;
        } else if (currentScreen_ == Screen::FileAction) {
          switch (e.button) {
            case Button::Up:
              actionView_.moveUp();
              needsRender_ = true;
              break;
            case Button::Down:
              actionView_.moveDown();
              needsRender_ = true;
              break;
            case Button::Center:
              switch (actionView_.currentAction()) {
                case ui::FileActionMenuView::ActionIndex: {
                  // Build full path the same way openSelected / promptDelete
                  // do (currentDir_ + name with separator handling).
                  if (selectedIndex_ < files_.size()) {
                    const FileEntry& entry = files_[selectedIndex_];
                    char pathBuf[256];
                    size_t dirLen = strlen(currentDir_);
                    const bool trailing = (dirLen > 0 && currentDir_[dirLen - 1] == '/');
                    if (trailing) {
                      snprintf(pathBuf, sizeof(pathBuf), "%s%s",
                               currentDir_, entry.name.c_str());
                    } else {
                      snprintf(pathBuf, sizeof(pathBuf), "%s/%s",
                               currentDir_, entry.name.c_str());
                    }
                    enterIndexConfirm(core, pathBuf);
                  } else {
                    currentScreen_ = Screen::Browse;
                    needsRender_ = true;
                  }
                  break;
                }
                case ui::FileActionMenuView::ActionDelete:
                  // Hand off to the existing confirm-delete flow.
                  promptDelete(core);
                  break;
                case ui::FileActionMenuView::ActionCancel:
                default:
                  currentScreen_ = Screen::Browse;
                  needsRender_ = true;
                  break;
              }
              break;
            case Button::Back:
            case Button::Left:
              currentScreen_ = Screen::Browse;
              needsRender_ = true;
              break;
            default:
              break;
          }
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
                // Execute delete inline (like SettingsState pattern).
                // Defensive bound: should match promptDelete's check, but
                // covers the race where files_ was reloaded between
                // showing the dialog and Yes-confirming (won't happen in
                // single-task flow but cheap insurance).
                if (files_.empty() || selectedIndex_ >= files_.size()) {
                  currentScreen_ = Screen::Browse;
                  needsRender_ = true;
                  break;
                }
                const FileEntry& entry = files_[selectedIndex_];
                char pathBuf[512];  // currentDir_(256) + '/' + name(128)
                size_t dirLen = strlen(currentDir_);
                const bool trailing = (dirLen > 0 && currentDir_[dirLen - 1] == '/');
                if (trailing) {
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
          // Normal browse mode.
          //
          // Button layout (discoverable, no long-press needed):
          //   Up/Down : navigate list
          //   Center  : open file / enter directory
          //   Back    : up one level (home if at root)
          //   Left    : jump to root (shortcut when deep in folders)
          //   Right   : delete selected item (with confirmation)
          //   Power   : reserved for sleep
          switch (e.button) {
            case Button::Up:
              navigateUp(core);
              break;
            case Button::Down:
              navigateDown(core);
              break;
            case Button::Left:
              // Jump to root. Only does something when not already at root.
              if (strcmp(currentDir_, "/") != 0) {
                strcpy(currentDir_, "/");
                selectedIndex_ = 0;
                loadFiles(core);
                needsRender_ = true;
              }
              break;
            case Button::Right:
              // Straight to delete-confirm (pre-0.6.0 behavior). The
              // file-action popup that briefly hosted "Index for faster
              // reading" was retired in 0.6.2 — on-device cold-extend
              // indexing was too inconsistent on long books under heap
              // pressure (Issue #23), and the right path is to pre-build
              // the cache off-device via sumi.page/process. The popup
              // code (promptFileAction / FileActionMenuView / Screen::
              // FileAction handler) is intentionally left in place but
              // unreachable from UI; a follow-up sweep can remove it.
              promptDelete(core);
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

  // Drive the on-device indexer one spine per update tick. Each tick
  // synchronously renders "Chapter N of M" then runs the (blocking)
  // page-cache build for that spine. The render-before-work order
  // means the screen shows the chapter the user is waiting on, not
  // the one just completed.
  if (currentScreen_ == Screen::Indexing) {
    if (indexCurrentSpine_ >= indexTotalSpines_) {
      indexResult_ = 1;  // success
      finishIndexing(core);
      currentScreen_ = Screen::IndexDone;
      needsRender_ = true;
    } else {
      indexOneSpineStep(core);
    }
  }

  // If a file was selected, transition to reader
  if (hasSelection_) {
    hasSelection_ = false;
    return StateTransition::to(StateId::Reader);
  }

#if FEATURE_PLUGINS
  // If a plugin launch was requested (e.g., flashcard deck selected)
  if (launchPlugin_) {
    launchPlugin_ = false;
    return StateTransition::to(StateId::PluginHost);
  }
#endif

  // Return to home if requested
  if (goHome_) {
    goHome_ = false;
    strcpy(currentDir_, "/");  // Reset for next entry
    return StateTransition::to(StateId::Home);
  }

  // Open book in reader
  if (pendingOpen_) {
    pendingOpen_ = false;
    return StateTransition::to(StateId::Reader);
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

  if (currentScreen_ == Screen::FileAction) {
    ui::render(renderer_, theme, actionView_);
    actionView_.needsRender = false;
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  if (currentScreen_ == Screen::ConvertInfo) {
    // Already rendered by showConvertMessage(), just clear the flag.
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  if (currentScreen_ == Screen::IndexConfirm) {
    renderIndexConfirm(core);
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  if (currentScreen_ == Screen::Indexing) {
    // Painted on demand by indexOneSpineStep before each spine; the
    // periodic re-render path doesn't need to do anything beyond
    // clearing the flag.
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  if (currentScreen_ == Screen::IndexDone) {
    renderIndexDone(core);
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

  // Same defensive clamp as promptDelete: navigate{Up,Down} bound
  // selectedIndex_, but a stale index from before a list refresh
  // would otherwise read past files_.size() here.
  if (selectedIndex_ >= files_.size()) {
    selectedIndex_ = files_.size() - 1;
  }

  const FileEntry& entry = files_[selectedIndex_];

  // Build full path
  size_t dirLen = strlen(currentDir_);
  if (dirLen > 0 && currentDir_[dirLen - 1] == '/') {
    snprintf(selectedPath_, sizeof(selectedPath_), "%s%s", currentDir_, entry.name.c_str());
  } else {
    snprintf(selectedPath_, sizeof(selectedPath_), "%s/%s", currentDir_, entry.name.c_str());
  }

  if (entry.isDir) {
    // Enter directory
    utf8SafeCopy(currentDir_, selectedPath_, sizeof(currentDir_));
    selectedIndex_ = 0;
    loadFiles(core);
    needsRender_ = true;

    // Save directory for return after mode switch
    utf8SafeCopy(core.settings.fileListDir, currentDir_, sizeof(core.settings.fileListDir));
    core.settings.fileListSelectedName[0] = '\0';
    core.settings.fileListSelectedIndex = 0;
  } else if (entry.unsupported) {
    // Show friendly conversion message
    showConvertMessage(core, entry.name.c_str());
    currentScreen_ = Screen::ConvertInfo;
  } else if (isImageFile(entry.name.c_str())) {
#if FEATURE_PLUGINS
    // Launch the Images app directly
    if (pluginHost_) {
      for (int i = 0; i < PluginListState::pluginCount; i++) {
        if (strcmp(PluginListState::plugins[i].name, "Images") == 0) {
          pluginHost_->setPluginFactory(PluginListState::plugins[i].factory);
          launchPlugin_ = true;
          Serial.println("[FILES] Launching Images app for image file");
          return;
        }
      }
    }
#endif
  } else if (isFlashcardFile(entry.name.c_str())) {
#if FEATURE_PLUGINS
    // Launch the Flashcards app directly
    if (pluginHost_) {
      for (int i = 0; i < PluginListState::pluginCount; i++) {
        if (strcmp(PluginListState::plugins[i].name, "Flashcards") == 0) {
          pluginHost_->setPluginFactory(PluginListState::plugins[i].factory);
          launchPlugin_ = true;
          Serial.println("[FILES] Launching Flashcards app for deck file");
          return;
        }
      }
    }
    // Fallback if plugin system unavailable
    showConvertMessage(core, entry.name.c_str());
    currentScreen_ = Screen::ConvertInfo;
#else
    showConvertMessage(core, entry.name.c_str());
    currentScreen_ = Screen::ConvertInfo;
#endif
  } else {
    // Save position for return
    utf8SafeCopy(core.settings.fileListDir, currentDir_, sizeof(core.settings.fileListDir));
    utf8SafeCopy(core.settings.fileListSelectedName, entry.name.c_str(), sizeof(core.settings.fileListSelectedName));
    core.settings.fileListSelectedIndex = selectedIndex_;

    // Select file - transition to Reader state
    Serial.printf("[FILES] Selected: %s\n", selectedPath_);
    utf8SafeCopy(core.buf.path, selectedPath_, sizeof(core.buf.path));
    // Save lastBookPath for cold boot "continue reading"
    utf8SafeCopy(core.settings.lastBookPath, selectedPath_, sizeof(core.settings.lastBookPath));
    core.settings.transitionReturnTo = 1;  // ReturnTo::FILE_MANAGER
    core.settings.saveToFile();
    pendingOpen_ = true;
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

void FileListState::promptFileAction(Core& core) {
  (void)core;
  if (files_.empty()) return;
  if (selectedIndex_ >= files_.size()) {
    selectedIndex_ = files_.size() - 1;
  }

  const FileEntry& entry = files_[selectedIndex_];

  // Truncate long names to fit the popup title width. Same UTF-8-safe
  // truncation pattern as the delete dialog so CJK file names don't
  // split mid-codepoint.
  char titleBuf[ui::FileActionMenuView::MAX_TITLE_LEN];
  if (entry.name.length() > 60) {
    char head[58];
    utf8SafeCopy(head, entry.name.c_str(), sizeof(head));
    snprintf(titleBuf, sizeof(titleBuf), "%s...", head);
  } else {
    utf8SafeCopy(titleBuf, entry.name.c_str(), sizeof(titleBuf));
  }
  actionView_.setup(titleBuf);
  currentScreen_ = Screen::FileAction;
  needsRender_ = true;
}

void FileListState::promptDelete(Core& core) {
  if (files_.empty()) return;

  // Defense-in-depth: navigateUp/navigateDown bound selectedIndex_ to
  // [0, files_.size()-1], but a stale index from before a loadFiles()
  // shrank the list could otherwise read out-of-bounds. Same fix
  // pattern as the post-delete clamp at line 428.
  if (selectedIndex_ >= files_.size()) {
    selectedIndex_ = files_.size() - 1;
  }

  const FileEntry& entry = files_[selectedIndex_];
  const char* typeStr = entry.isDir ? "folder" : "file";

  char line1[48];
  snprintf(line1, sizeof(line1), "Delete this %s?", typeStr);

  char line2[48];
  if (entry.name.length() > 40) {
    // UTF-8 safe truncation: `%.37s` slices bytes, which corrupts CJK.
    char head[38];
    utf8SafeCopy(head, entry.name.c_str(), sizeof(head));
    snprintf(line2, sizeof(line2), "%s...", head);
  } else {
    utf8SafeCopy(line2, entry.name.c_str(), sizeof(line2));
  }

  confirmView_.setup("Confirm Delete", line1, line2);
  currentScreen_ = Screen::ConfirmDelete;
  (void)core;
  needsRender_ = true;
}

void FileListState::enterIndexConfirm(Core& core, const char* filename) {
  (void)core;
  utf8SafeCopy(indexBookPath_, filename ? filename : "", sizeof(indexBookPath_));
  indexResult_ = 0;
  indexCurrentSpine_ = 0;
  indexTotalSpines_ = 0;
  currentScreen_ = Screen::IndexConfirm;
  needsRender_ = true;
}

void FileListState::renderIndexConfirm(Core& core) {
  (void)core;
  Theme& theme = THEME_MANAGER.mutableCurrent();
  renderer_.clearScreen(theme.backgroundColor);

  const int pageH = renderer_.getScreenHeight();
  const int pageW = renderer_.getScreenWidth();
  const int lineH = renderer_.getLineHeight(theme.menuFontId);
  int y = pageH / 2 - lineH * 6;

  renderer_.drawCenteredText(theme.readerFontId, y, "Index for Faster Reading",
                             theme.primaryTextBlack, EpdFontFamily::BOLD);
  y += lineH + 16;

  // Truncate filename UTF-8-safely so a long CJK title doesn't bleed off-screen.
  char shortName[44];
  const char* baseName = indexBookPath_;
  const char* slash = strrchr(indexBookPath_, '/');
  if (slash && *(slash + 1)) baseName = slash + 1;
  if (baseName && baseName[0]) {
    utf8SafeCopy(shortName, baseName, sizeof(shortName));
    renderer_.drawCenteredText(theme.menuFontId, y, shortName, theme.primaryTextBlack);
    y += lineH + 14;
  }

  renderer_.drawCenteredText(theme.menuFontId, y,
                             "Pre-builds every chapter's page cache so",
                             theme.primaryTextBlack);
  y += lineH + 2;
  renderer_.drawCenteredText(theme.menuFontId, y,
                             "reads load instantly, no Bluetooth-stalls.",
                             theme.primaryTextBlack);
  y += lineH + 14;
  renderer_.drawCenteredText(theme.menuFontId, y,
                             "Tradeoff: font, font size, line spacing,",
                             theme.primaryTextBlack);
  y += lineH + 2;
  renderer_.drawCenteredText(theme.menuFontId, y,
                             "hyphenation get baked in at this moment.",
                             theme.primaryTextBlack);
  y += lineH + 2;
  renderer_.drawCenteredText(theme.menuFontId, y,
                             "Changing them later rebuilds the cache.",
                             theme.primaryTextBlack);
  y += lineH + 14;
  renderer_.drawCenteredText(theme.menuFontId, y,
                             "Takes 1-10 minutes. Leave it alone.",
                             theme.primaryTextBlack);
  y += lineH + 20;

  // Two big buttons, matching the ConfirmDialogView style.
  constexpr int buttonWidth = 140;
  constexpr int buttonHeight = 50;
  constexpr int buttonSpacing = 30;
  constexpr int totalWidth = buttonWidth * 2 + buttonSpacing;
  const int startX = (pageW - totalWidth) / 2;

  // Start button — selected by default (Center confirms).
  renderer_.fillRect(startX, y, buttonWidth, buttonHeight, theme.selectionFillBlack);
  const int startTextW = renderer_.getTextWidth(theme.menuFontId, "Start");
  renderer_.drawText(theme.menuFontId,
                     startX + (buttonWidth - startTextW) / 2,
                     y + (buttonHeight - renderer_.getFontAscenderSize(theme.menuFontId)) / 2,
                     "Start", theme.selectionTextBlack);

  // Cancel button.
  const int cancelX = startX + buttonWidth + buttonSpacing;
  renderer_.drawRect(cancelX, y, buttonWidth, buttonHeight, theme.primaryTextBlack);
  const int cancelTextW = renderer_.getTextWidth(theme.menuFontId, "Cancel");
  renderer_.drawText(theme.menuFontId,
                     cancelX + (buttonWidth - cancelTextW) / 2,
                     y + (buttonHeight - renderer_.getFontAscenderSize(theme.menuFontId)) / 2,
                     "Cancel", theme.primaryTextBlack);

  renderer_.displayBuffer();
}

bool FileListState::startIndexing(Core& core) {
  // Only EPUBs can be indexed on-device for now — XTC is already indexed
  // and other formats don't go through the page-cache pipeline.
  if (!indexBookPath_[0]) return false;
  const size_t pathLen = strlen(indexBookPath_);
  if (pathLen < 5 || strcasecmp(indexBookPath_ + pathLen - 5, ".epub") != 0) {
    indexResult_ = 3;  // error
    return false;
  }

  // Save the user's current external font so we can reload it after.
  // The indexer releases it for the duration of the parse to free
  // 8-15 KB of heap (same trick as the in-reader cold extend).
  indexFontWasLoaded_ = (FONT_MANAGER.getExternalFont() != nullptr);
  if (indexFontWasLoaded_) {
    utf8SafeCopy(indexSavedFont_, core.settings.readerFont, sizeof(indexSavedFont_));
    Serial.printf("[INDEX] Releasing external font '%s' for indexing\n", indexSavedFont_);
    FONT_MANAGER.unloadExternalFont();
  } else {
    indexSavedFont_[0] = '\0';
  }

  // Note: we DO NOT call ble::deinit() here even though it would free
  // ~48 KB. ble::init() on the re-init path has been observed to
  // synchronously block loopTask past the task watchdog timeout, which
  // would crash mid-indexing. Trust that ~20 KB free + font released +
  // background cache task NOT running is enough for the parser; if not,
  // the parser's safety threshold bails per-spine and the per-spine
  // cache stays partial — still strictly better than today.
  indexBleWasInitd_ = false;  // reserved for future deinit attempt

  // Open the EPUB. Use the global cache dir so the per-book subdirectory
  // matches what the reader will read at runtime.
  indexProvider_ = std::unique_ptr<EpubProvider>(new (std::nothrow) EpubProvider());
  if (!indexProvider_) {
    indexResult_ = 3;
    return false;
  }
  auto openResult = indexProvider_->open(indexBookPath_, SUMI_CACHE_DIR);
  if (!openResult.ok() || !indexProvider_->getEpub()) {
    Serial.printf("[INDEX] Failed to open EPUB '%s' for indexing\n", indexBookPath_);
    indexProvider_.reset();
    indexResult_ = 3;
    return false;
  }

  indexTotalSpines_ = indexProvider_->getEpub()->getSpineItemsCount();
  indexCurrentSpine_ = 0;
  indexStartMs_ = millis();
  Serial.printf("[INDEX] Starting on-device indexing of '%s' (%u spines)\n",
                indexBookPath_, (unsigned)indexTotalSpines_);
  currentScreen_ = Screen::Indexing;
  needsRender_ = true;
  return true;
}

void FileListState::indexOneSpineStep(Core& core) {
  // Defensive: somebody got us here without an open provider.
  if (!indexProvider_ || !indexProvider_->getEpub()) {
    indexResult_ = 3;
    currentScreen_ = Screen::IndexDone;
    needsRender_ = true;
    return;
  }
  if (indexCurrentSpine_ >= indexTotalSpines_) {
    indexResult_ = 1;  // success
    currentScreen_ = Screen::IndexDone;
    needsRender_ = true;
    return;
  }

  // Show "Chapter <currentSpine + 1> of <total>" BEFORE the spine work
  // so the user has something on screen during the blocking parse.
  // The post-spine increment means the next call's renderIndexProgress
  // shows the new chapter.
  renderIndexProgress(core);

  auto epubShared = indexProvider_->getEpubShared();
  auto* epub = epubShared.get();
  const Theme& theme = THEME_MANAGER.current();

  // Compute the same viewport the reader will use, so the cache config
  // matches at read time and doesn't trigger "Config mismatch" on first
  // open.
  int marginT, marginR, marginB, marginL;
  renderer_.getOrientedViewableTRBL(&marginT, &marginR, &marginB, &marginL);
  // Reader adds horizontalPadding (per ReaderState::getReaderViewport).
  // Mirror the same constants here so the config matches byte-for-byte.
  constexpr int horizontalPadding = 16;
  constexpr int statusBarMargin = 30;
  marginL += horizontalPadding;
  marginR += horizontalPadding;
  marginB += statusBarMargin;
  const int width = renderer_.getScreenWidth() - marginL - marginR;
  const int height = renderer_.getScreenHeight() - marginT - marginB;

  auto config = core.settings.getRenderConfig(theme, width, height);
  config.allowTallImages = false;

  // Build per-spine cache path matching ReaderState's epubSectionCachePath:
  //   <epubCachePath>/sections/<spineIndex>.bin
  std::string cachePath = epub->getCachePath() + "/sections/" +
                           std::to_string(indexCurrentSpine_) + ".bin";
  std::string imageCachePath =
      core.settings.showImages ? (epub->getCachePath() + "/images") : "";

  auto* parser = new (std::nothrow) EpubChapterParser(
      epubShared, indexCurrentSpine_, renderer_, config, imageCachePath);
  if (!parser) {
    Serial.printf("[INDEX] Spine %u parser alloc failed — skipping\n",
                  (unsigned)indexCurrentSpine_);
    indexCurrentSpine_++;
    needsRender_ = true;
    return;
  }

  PageCache cache(cachePath);
  // maxPages=0 means unlimited — let the parser run to chapter end. The
  // existing safety threshold inside the parser still applies; if heap
  // pressure hits, we get a partial cache for this spine, which is
  // still better than nothing.
  const uint32_t t0 = millis();
  const bool ok = cache.create(*parser, config, /*maxPages=*/0, /*skipPages=*/0, nullptr);
  const uint32_t ms = millis() - t0;
  Serial.printf("[INDEX] Spine %u/%u: %s in %lu ms (%u pages, partial=%d)\n",
                (unsigned)(indexCurrentSpine_ + 1), (unsigned)indexTotalSpines_,
                ok ? "OK" : "FAIL", (unsigned long)ms,
                (unsigned)cache.pageCount(), cache.isPartial() ? 1 : 0);

  delete parser;
  indexCurrentSpine_++;
  // No needsRender_=true: the next update tick's renderIndexProgress
  // call will paint the new chapter number directly. Setting it would
  // just queue a redundant redraw of the SAME (just-rendered) frame.
}

void FileListState::renderIndexProgress(Core& core) {
  (void)core;
  Theme& theme = THEME_MANAGER.mutableCurrent();
  renderer_.clearScreen(theme.backgroundColor);

  const int pageH = renderer_.getScreenHeight();
  const int pageW = renderer_.getScreenWidth();
  const int lineH = renderer_.getLineHeight(theme.menuFontId);
  int y = pageH / 2 - lineH * 4;

  renderer_.drawCenteredText(theme.readerFontId, y, "Indexing...",
                             theme.primaryTextBlack, EpdFontFamily::BOLD);
  y += lineH + 16;

  char shortName[40];
  const char* baseName = indexBookPath_;
  const char* slash = strrchr(indexBookPath_, '/');
  if (slash && *(slash + 1)) baseName = slash + 1;
  if (baseName[0]) {
    utf8SafeCopy(shortName, baseName, sizeof(shortName));
    renderer_.drawCenteredText(theme.menuFontId, y, shortName, theme.primaryTextBlack);
    y += lineH + 18;
  }

  char status[48];
  snprintf(status, sizeof(status), "Chapter %u of %u",
           (unsigned)(indexCurrentSpine_ + 1), (unsigned)indexTotalSpines_);
  renderer_.drawCenteredText(theme.readerFontId, y, status,
                             theme.primaryTextBlack, EpdFontFamily::BOLD);
  y += lineH + 24;

  // Simple progress bar (no time estimate — spine work varies wildly).
  constexpr int barWidth = 320;
  constexpr int barHeight = 16;
  const int barX = (pageW - barWidth) / 2;
  const int barY = y;
  renderer_.drawRect(barX, barY, barWidth, barHeight, theme.primaryTextBlack);
  const int filled = indexTotalSpines_ > 0
                         ? (barWidth * indexCurrentSpine_) / indexTotalSpines_
                         : 0;
  if (filled > 0) {
    renderer_.fillRect(barX + 1, barY + 1, filled - 2 < 0 ? 0 : filled - 2,
                       barHeight - 2, theme.primaryTextBlack);
  }
  y += barHeight + 24;

  renderer_.drawCenteredText(theme.menuFontId, y,
                             "Press Back to cancel",
                             theme.primaryTextBlack);

  renderer_.displayBuffer();
}

void FileListState::renderIndexDone(Core& core) {
  (void)core;
  Theme& theme = THEME_MANAGER.mutableCurrent();
  renderer_.clearScreen(theme.backgroundColor);

  const int pageH = renderer_.getScreenHeight();
  const int lineH = renderer_.getLineHeight(theme.menuFontId);
  int y = pageH / 2 - lineH * 2;

  const char* headline = "Indexing Complete";
  if (indexResult_ == 2) headline = "Indexing Cancelled";
  else if (indexResult_ == 3) headline = "Indexing Failed";
  renderer_.drawCenteredText(theme.readerFontId, y, headline,
                             theme.primaryTextBlack, EpdFontFamily::BOLD);
  y += lineH + 20;

  char summary[64];
  if (indexResult_ == 1) {
    const uint32_t secs = (millis() - indexStartMs_) / 1000;
    snprintf(summary, sizeof(summary), "Built %u chapters in %lus",
             (unsigned)indexTotalSpines_, (unsigned long)secs);
  } else if (indexResult_ == 2) {
    snprintf(summary, sizeof(summary), "Stopped at chapter %u of %u",
             (unsigned)indexCurrentSpine_, (unsigned)indexTotalSpines_);
  } else {
    snprintf(summary, sizeof(summary), "See serial log for details");
  }
  renderer_.drawCenteredText(theme.menuFontId, y, summary, theme.primaryTextBlack);
  y += lineH + 20;

  renderer_.drawCenteredText(theme.menuFontId, y,
                             "Press any button to return",
                             theme.primaryTextBlack);
  renderer_.displayBuffer();
}

void FileListState::finishIndexing(Core& core) {
  (void)core;
  if (indexProvider_) {
    indexProvider_->close();
    indexProvider_.reset();
  }
  if (indexFontWasLoaded_ && indexSavedFont_[0]) {
    Serial.printf("[INDEX] Reloading external font '%s' after indexing\n",
                  indexSavedFont_);
    FONT_MANAGER.loadExternalFont(indexSavedFont_);
  }
  indexFontWasLoaded_ = false;
  indexSavedFont_[0] = '\0';
  indexBleWasInitd_ = false;
  // currentSpine/totalSpines/result kept for IndexDone screen rendering.
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
