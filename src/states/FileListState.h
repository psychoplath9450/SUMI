#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../config.h"
#include "../ui/views/SettingsViews.h"
#include "State.h"

class GfxRenderer;

namespace sumi {

#if FEATURE_PLUGINS
class PluginHostState;
#endif

// FileListState - browse and select files
// Uses dynamic vector for unlimited file support with pagination
class FileListState : public State {
  enum class Screen : uint8_t {
    Browse,
    ConfirmDelete,
    ConvertInfo,     // Friendly "needs conversion" message
  };

 public:
  explicit FileListState(GfxRenderer& renderer);
  ~FileListState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::FileList; }

  // Get selected file path after state exits
  const char* selectedPath() const { return selectedPath_; }

  // Set initial directory before entering
  void setDirectory(const char* dir);

#if FEATURE_PLUGINS
  // Set plugin host for launching apps (e.g., Flashcards) from file browser
  void setHostState(PluginHostState* host) { pluginHost_ = host; }
#endif

 private:
  GfxRenderer& renderer_;
  char currentDir_[256];
  char selectedPath_[256];

  // File entries - dynamic vector for unlimited files
  struct FileEntry {
    std::string name;
    bool isDir;
    int8_t progressPercent;  // -1 = never opened, 0-100 = reading progress
    bool unsupported;        // true = known format but needs conversion via sumi.page
    uint8_t contentHint;     // ContentHint from LibraryIndex (0 = generic/unknown)
  };
  std::vector<FileEntry> files_;

  size_t selectedIndex_;
  bool needsRender_;
  bool hasSelection_;
  bool goHome_;       // Return to Home state
  bool firstRender_;  // Use HALF_REFRESH on first render to clear ghosting
  Screen currentScreen_;
  ui::ConfirmDialogView confirmView_;

#if FEATURE_PLUGINS
  PluginHostState* pluginHost_ = nullptr;
  bool launchPlugin_ = false;
#endif

  void loadFiles(Core& core);
  void promptDelete(Core& core);
  void navigateUp(Core& core);
  void navigateDown(Core& core);
  void openSelected(Core& core);
  void goBack(Core& core);
  void showConvertMessage(Core& core, const char* filename);

  // Pagination helpers
  int getPageItems() const;
  int getTotalPages() const;
  int getCurrentPage() const;
  int getPageStartIndex() const;

  bool isHidden(const char* name) const;
  bool isSupportedFile(const char* name) const;
  bool isImageFile(const char* name) const;
  bool isFlashcardFile(const char* name) const;
  bool isConvertibleFile(const char* name) const;
  bool isAtRoot() const { return strcmp(currentDir_, "/") == 0; }
};

}  // namespace sumi
