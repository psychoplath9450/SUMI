#include "ReaderState.h"

#include <Arduino.h>
#include <ContentParser.h>
#include <CoverHelpers.h>
#include <Epub/Page.h>
#include <EpubChapterParser.h>
#include <GfxRenderer.h>
#include <MarkdownParser.h>
#include <PageCache.h>
#include <PlainTextParser.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <esp_system.h>

#include "../config.h"
#if FEATURE_BLUETOOTH
#include "../ble/BleHid.h"
#endif

#include <cstring>
#include <algorithm>

#include "../Battery.h"
#include "../FontManager.h"
#include "../config.h"
#include "../content/ProgressManager.h"
#include "../content/LibraryIndex.h"
#include "../content/ReaderNavigation.h"
#include "../content/RecentBooks.h"
#include "../core/BootMode.h"
#include "../core/Core.h"
#include "../ui/Elements.h"
#include "ThemeManager.h"

namespace sumi {

static constexpr int kCacheTaskStackSize = 24576;  // 24KB - JPEGDEC needs more stack than picojpeg
static constexpr int kCacheTaskStopTimeoutMs = 10000;  // 10s - generous for slow SD operations

namespace {
constexpr int horizontalPadding = 5;
constexpr int statusBarMargin = 19;

// Cache path helpers
inline std::string epubSectionCachePath(const std::string& epubCachePath, int spineIndex) {
  return epubCachePath + "/sections/" + std::to_string(spineIndex) + ".bin";
}

inline std::string contentCachePath(const char* cacheDir, int fontId) {
  return std::string(cacheDir) + "/pages_" + std::to_string(fontId) + ".bin";
}
}  // namespace

void ReaderState::saveAnchorMap(const ContentParser& parser, const std::string& cachePath) {
  const auto& anchors = parser.getAnchorMap();

  std::string anchorPath = cachePath + ".anchors";
  FsFile file;
  if (!SdMan.openFileForWrite("RDR", anchorPath, file)) return;

  if (anchors.size() > UINT16_MAX) {
    uint16_t zero = 0;
    serialization::writePod(file, zero);
    file.close();
    return;
  }
  uint16_t count = static_cast<uint16_t>(anchors.size());
  serialization::writePod(file, count);
  for (const auto& entry : anchors) {
    serialization::writeString(file, entry.first);
    serialization::writePod(file, entry.second);
  }
  file.close();
}

int ReaderState::loadAnchorPage(const std::string& cachePath, const std::string& anchor) {
  std::string anchorPath = cachePath + ".anchors";
  FsFile file;
  if (!SdMan.openFileForRead("RDR", anchorPath, file)) return -1;

  uint16_t count;
  if (!serialization::readPodChecked(file, count)) {
    file.close();
    return -1;
  }

  for (uint16_t i = 0; i < count; i++) {
    std::string anchorId;
    uint16_t page;
    if (!serialization::readString(file, anchorId) || !serialization::readPodChecked(file, page)) {
      file.close();
      return -1;
    }
    if (anchorId == anchor) {
      file.close();
      return page;
    }
  }

  file.close();
  return -1;
}

int ReaderState::calcFirstContentSpine(bool hasCover, int textStartIndex, size_t spineCount) {
  if (hasCover && textStartIndex == 0 && spineCount > 1) {
    return 1;
  }
  return textStartIndex;
}

// Cache creation/extension implementation
// Called from main thread when background task is NOT running (ownership model)
// No mutex needed - main thread owns pageCache_/parser_ when task is stopped
void ReaderState::createOrExtendCacheImpl(ContentParser& parser, const std::string& cachePath,
                                          const RenderConfig& config) {
  bool needsCreate = false;
  bool needsExtend = false;

  if (!pageCache_) {
    pageCache_.reset(new PageCache(cachePath));
    if (pageCache_->load(config)) {
      if (!SdMan.exists((cachePath + ".anchors").c_str())) {
        needsCreate = true;  // Migration: rebuild cache to generate anchor map
      } else {
        needsExtend = pageCache_->isPartial();
      }
    } else {
      needsCreate = true;
    }
  } else {
    if (!SdMan.exists((cachePath + ".anchors").c_str())) {
      needsCreate = true;  // Migration: rebuild cache to generate anchor map
    } else {
      needsExtend = pageCache_->isPartial();
    }
  }

  if (pageCache_) {
    if (needsExtend) {
      pageCache_->extend(parser, PageCache::DEFAULT_CACHE_CHUNK);
      saveAnchorMap(parser, cachePath);
    } else if (needsCreate) {
      parser.reset();  // Ensure clean state for fresh cache creation
      pageCache_->create(parser, config, PageCache::DEFAULT_CACHE_CHUNK);
      saveAnchorMap(parser, cachePath);
    }
  }
}

// Background caching implementation (handles stop request checks)
// Called from background task - uses BackgroundTask's shouldStop() and getAbortCallback()
// Ownership: background task owns pageCache_/parser_ while running
void ReaderState::backgroundCacheImpl(ContentParser& parser, const std::string& cachePath, const RenderConfig& config) {
  auto shouldAbort = cacheTask_.getAbortCallback();

  // Check for early abort before doing anything
  if (cacheTask_.shouldStop()) {
    Serial.println("[READER] Background cache aborted before start");
    return;
  }

  // Create/load cache (we own pageCache_ while task is running)
  pageCache_.reset(new PageCache(cachePath));
  bool loaded = pageCache_->load(config);
  // Migration: rebuild cache to generate anchor map if missing
  if (loaded && !SdMan.exists((cachePath + ".anchors").c_str())) {
    loaded = false;
  }
  bool needsExtend = loaded && pageCache_->needsExtension(currentSectionPage_);

  // Check for abort after setup
  if (cacheTask_.shouldStop()) {
    pageCache_.reset();
    Serial.println("[READER] Background cache aborted after setup");
    return;
  }

  if (!loaded || needsExtend) {
    bool success;
    if (needsExtend) {
      success = pageCache_->extend(parser, PageCache::DEFAULT_CACHE_CHUNK, shouldAbort);
    } else {
      parser.reset();  // Ensure clean state for fresh cache creation
      success = pageCache_->create(parser, config, PageCache::DEFAULT_CACHE_CHUNK, 0, shouldAbort);
    }

    if (success && !cacheTask_.shouldStop()) {
      saveAnchorMap(parser, cachePath);
    }

    if (!success || cacheTask_.shouldStop()) {
      Serial.println("[READER] Cache creation failed or aborted, clearing pageCache");
      pageCache_.reset();
    }
  }
}

ReaderState::ReaderState(GfxRenderer& renderer)
    : renderer_(renderer),
      xtcRenderer_(renderer),
      currentPage_(0),
      needsRender_(true),
      contentLoaded_(false),
      currentSpineIndex_(0),
      currentSectionPage_(0),
      pagesUntilFullRefresh_(1),  // 1 = first render uses HALF_REFRESH (FULL causes 5 flashes)
      tocView_{},
      settingsView_{} {
  contentPath_[0] = '\0';
}

ReaderState::~ReaderState() { stopBackgroundCaching(); }

void ReaderState::setContentPath(const char* path) {
  if (path) {
    strncpy(contentPath_, path, sizeof(contentPath_) - 1);
    contentPath_[sizeof(contentPath_) - 1] = '\0';
  } else {
    contentPath_[0] = '\0';
  }
}

void ReaderState::enter(Core& core) {
  // Free memory from other states before loading book
  THEME_MANAGER.clearCache();
  renderer_.clearWidthCache();

  contentLoaded_ = false;
  loadFailed_ = false;
  needsRender_ = true;
  tocMode_ = false;
  settingsMode_ = false;
  pagesUntilFullRefresh_ = 1;  // Use HALF_REFRESH on first render (FULL causes 5 flashes)
  stopBackgroundCaching();  // Ensure any previous task is stopped
  parser_.reset();          // Safe - task is stopped
  parserSpineIndex_ = -1;
  pageCache_.reset();
  currentSpineIndex_ = 0;
  currentSectionPage_ = 0;  // Will be set to -1 after progress load if at start

  // Read path from shared buffer if not already set
  if (contentPath_[0] == '\0' && core.buf.path[0] != '\0') {
    strncpy(contentPath_, core.buf.path, sizeof(contentPath_) - 1);
    contentPath_[sizeof(contentPath_) - 1] = '\0';
    core.buf.path[0] = '\0';
  }

  // Determine source state from boot transition
  const auto& transition = getTransition();
  sourceState_ =
      (transition.isValid() && transition.returnTo == ReturnTo::FILE_MANAGER) ? StateId::FileList : StateId::Home;

  Serial.printf("[READER] Entering with path: %s\n", contentPath_);

  if (contentPath_[0] == '\0') {
    Serial.println("[READER] No content path set");
    return;
  }

  // Apply orientation setting to renderer
  switch (core.settings.orientation) {
    case Settings::Portrait:
      renderer_.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
    case Settings::LandscapeCW:
      renderer_.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
      break;
    case Settings::Inverted:
      renderer_.setOrientation(GfxRenderer::Orientation::PortraitInverted);
      break;
    case Settings::LandscapeCCW:
      renderer_.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      break;
    default:
      renderer_.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
  }

  // Open content using ContentHandle
  auto result = core.content.open(contentPath_, SUMI_CACHE_DIR);
  if (!result.ok()) {
    Serial.printf("[READER] Failed to open content: %s\n", errorToString(result.err));
    // Store error message for ErrorState to display
    snprintf(core.buf.text, sizeof(core.buf.text), "Cannot open file:\n%s", errorToString(result.err));
    loadFailed_ = true;  // Mark as failed for update() to transition to error state
    return;
  }

  contentLoaded_ = true;

  // Save last book path to settings
  strncpy(core.settings.lastBookPath, contentPath_, sizeof(core.settings.lastBookPath) - 1);
  core.settings.lastBookPath[sizeof(core.settings.lastBookPath) - 1] = '\0';
  core.settings.save(core.storage);
  
  // Record in recent books list
  const auto& meta = core.content.metadata();
  RecentBooks::recordOpen(core, contentPath_, meta.title, meta.author, 0);

  // Setup cache directories for all content types
  // Reset state for new book
  textStartIndex_ = 0;
  hasCover_ = false;
  thumbnailDone_ = false;
  scrollY_ = 0;
  pageContentHeight_ = 0;

  // Detect landscape scroll content (comics, scanned docs)
  // These content types benefit from landscape (800px wide) with vertical scrolling
  const ContentHint hint = core.content.metadata().hint;
  landscapeScroll_ = (hint == ContentHint::Comic ||
                      hint == ContentHint::ComicRtl ||
                      hint == ContentHint::ComicWebtoon ||
                      hint == ContentHint::BookScanned);
  if (landscapeScroll_) {
    renderer_.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
    Serial.printf("[READER] Landscape scroll mode for hint %d\n", static_cast<int>(hint));
  }

  switch (core.content.metadata().type) {
    case ContentType::Epub: {
      auto* provider = core.content.asEpub();
      if (provider && provider->getEpub()) {
        const auto* epub = provider->getEpub();
        epub->setupCacheDir();
        // Get the spine index for the first text content (from <guide> element)
        textStartIndex_ = epub->getSpineIndexForTextReference();
        Serial.printf("[READER] Text starts at spine index %d\n", textStartIndex_);
      }
      break;
    }
    case ContentType::Txt: {
      auto* provider = core.content.asTxt();
      if (provider && provider->getTxt()) {
        provider->getTxt()->setupCacheDir();
      }
      break;
    }
    case ContentType::Markdown: {
      auto* provider = core.content.asMarkdown();
      if (provider && provider->getMarkdown()) {
        provider->getMarkdown()->setupCacheDir();
      }
      break;
    }
    default:
      break;
  }

  // Load saved progress
  ContentType type = core.content.metadata().type;
  auto progress = ProgressManager::load(core, core.content.cacheDir(), type);
  progress = ProgressManager::validate(core, type, progress);
  currentSpineIndex_ = progress.spineIndex;
  currentSectionPage_ = progress.sectionPage;
  currentPage_ = progress.flatPage;

  // If at start of book and showImages enabled, begin at cover
  if (currentSpineIndex_ == 0 && currentSectionPage_ == 0 && core.settings.showImages) {
    currentSectionPage_ = -1;  // Cover page
  }

  // Initialize last rendered to loaded position (until first render)
  lastRenderedSpineIndex_ = currentSpineIndex_;
  lastRenderedSectionPage_ = currentSectionPage_;

  Serial.printf("[READER] Loaded: %s\n", core.content.metadata().title);

  // Start background caching (includes thumbnail generation)
  // This runs once per book open regardless of starting position
  startBackgroundCaching(core);
}

void ReaderState::exit(Core& core) {
  Serial.println("[READER] Exiting");

  // Stop background caching task first - BackgroundTask::stop() waits properly
  stopBackgroundCaching();

  if (contentLoaded_) {
    // Generate thumbnail on exit (not during background task) to avoid buffer conflicts
    // Only generate if not already done and cover exists
    if (!thumbnailDone_) {
      Serial.println("[READER] Generating thumbnail on exit...");
      core.content.generateThumbnail();
      thumbnailDone_ = true;
    }

    // Save progress at last rendered position (not current requested position)
    ProgressManager::Progress progress;
    // If on cover, save as (0, 0) - cover is implicit start
    progress.spineIndex = (lastRenderedSectionPage_ == -1) ? 0 : lastRenderedSpineIndex_;
    progress.sectionPage = (lastRenderedSectionPage_ == -1) ? 0 : lastRenderedSectionPage_;
    progress.flatPage = currentPage_;
    ProgressManager::save(core, core.content.cacheDir(), core.content.metadata().type, progress);

    // Update library index for file browser progress bars
    // Use content-type-appropriate progress metrics
    uint16_t libCurrent = 0, libTotal = 0;
    if (core.content.metadata().type == ContentType::Epub) {
      // EPUB: spine-based progress (chapter index / chapter count)
      libCurrent = static_cast<uint16_t>(lastRenderedSpineIndex_ + 1);
      libTotal = static_cast<uint16_t>(core.content.pageCount());  // = spineCount for epub
    } else {
      // XTC/TXT/MD: flat page progress
      libTotal = static_cast<uint16_t>(std::min(core.content.pageCount(), uint32_t(65535)));
      libCurrent = static_cast<uint16_t>(std::min(currentPage_ + 1, uint32_t(65535)));
    }
    LibraryIndex::updateEntry(core, contentPath_, libCurrent, libTotal,
                              static_cast<uint8_t>(core.content.metadata().hint));
    
    // Update progress in recent books
    uint16_t progressPercent = libTotal > 0 ? (libCurrent * 100 / libTotal) : 0;
    RecentBooks::updateProgress(core, contentPath_, progressPercent);

    // Safe to reset - task is stopped, we own pageCache_/parser_
    parser_.reset();
    parserSpineIndex_ = -1;
    pageCache_.reset();
    core.content.close();
  }

  // Unload custom reader fonts to free memory
  // Note: device may restart after this (dual-boot system), but explicit cleanup
  // ensures predictable memory behavior and better logging
  FONT_MANAGER.unloadReaderFonts();

  contentLoaded_ = false;
  contentPath_[0] = '\0';
  landscapeScroll_ = false;
  scrollY_ = 0;
  pageContentHeight_ = 0;

  // Reset orientation to Portrait for UI
  renderer_.setOrientation(GfxRenderer::Orientation::Portrait);
}

StateTransition ReaderState::update(Core& core) {
  // Handle load failure - transition to error state or back to file list
  if (loadFailed_ || !contentLoaded_) {
    // If error message was set, show ErrorState; otherwise just go back to FileList
    if (core.buf.text[0] != '\0') {
      return StateTransition::to(StateId::Error);
    }
    return StateTransition::to(StateId::FileList);
  }

  Event e;
  while (core.events.pop(e)) {
    // Route input to overlay handlers
    if (settingsMode_) {
      handleSettingsInput(core, e);
      continue;
    }
    if (tocMode_) {
      handleTocInput(core, e);
      continue;
    }

    switch (e.type) {
      case EventType::ButtonPress:
        if (landscapeScroll_) {
          // Landscape scroll: Up/Down scroll within page, Left/Right navigate pages
          const int scrollStep = renderer_.getScreenHeight() * 3 / 4;  // 75% of viewport
          switch (e.button) {
            case Button::Down:
            case Button::Right: {
              const int maxScroll = pageContentHeight_ - renderer_.getScreenHeight();
              if (maxScroll > 0 && scrollY_ < maxScroll) {
                scrollY_ = std::min(scrollY_ + scrollStep, maxScroll);
                needsRender_ = true;
              } else {
                scrollY_ = 0;
                pageContentHeight_ = 0;
                navigateNext(core);
              }
              break;
            }
            case Button::Up:
            case Button::Left:
              if (scrollY_ > 0) {
                scrollY_ = std::max(0, scrollY_ - scrollStep);
                needsRender_ = true;
              } else {
                scrollY_ = 0;
                pageContentHeight_ = 0;
                navigatePrev(core);
              }
              break;
            case Button::Center:
              // Defer to ButtonRelease — allows long-press to open settings first
              break;
            case Button::Back:
              exitToUI(core);
              return StateTransition::stay(StateId::Reader);
            case Button::Power:
              if (core.settings.shortPwrBtn == Settings::PowerPageTurn) {
                const int maxScroll = pageContentHeight_ - renderer_.getScreenHeight();
                if (maxScroll > 0 && scrollY_ < maxScroll) {
                  scrollY_ = std::min(scrollY_ + scrollStep, maxScroll);
                  needsRender_ = true;
                } else {
                  scrollY_ = 0;
                  pageContentHeight_ = 0;
                  navigateNext(core);
                }
              } else if (core.settings.shortPwrBtn == Settings::PowerRefresh) {
                renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
              }
              break;
          }
        } else {
        // Normal portrait mode
        switch (e.button) {
          case Button::Right:
          case Button::Down:
            navigateNext(core);
            break;

          case Button::Left:
          case Button::Up:
            navigatePrev(core);
            break;

          case Button::Center:
            // Defer to ButtonRelease — allows long-press to open settings first
            break;
          case Button::Back:
            exitToUI(core);
            // Won't reach here after restart
            return StateTransition::stay(StateId::Reader);
          case Button::Power:
            if (core.settings.shortPwrBtn == Settings::PowerPageTurn) {
              navigateNext(core);
            } else if (core.settings.shortPwrBtn == Settings::PowerRefresh) {
              // Manual screen refresh — clear ghosting
              renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
            }
            break;
        }
        }
        break;

      case EventType::ButtonLongPress:
        if (e.button == Button::Center) {
          centerLongPressFired_ = true;
          enterSettingsMode(core);
        }
        break;

      case EventType::ButtonRelease:
        if (e.button == Button::Center) {
          if (centerLongPressFired_) {
            // Long press already handled — suppress short-press action
            centerLongPressFired_ = false;
          } else if (!settingsMode_ && !tocMode_) {
            // Short press Center: open TOC
            if (core.content.tocCount() > 0) {
              enterTocMode(core);
            }
          }
        }
        break;

      default:
        break;
    }
  }

#if FEATURE_BLUETOOTH
  // Poll BLE page turner
  if (ble::isReady() && ble::isConnected()) {
    BleKey bk;
    while ((bk = ble::poll()) != BleKey::NONE) {
      switch (bk) {
        case BleKey::PAGE_NEXT:
        case BleKey::KEY_RIGHT:
        case BleKey::KEY_DOWN:
          navigateNext(core);
          break;
        case BleKey::PAGE_PREV:
        case BleKey::KEY_LEFT:
        case BleKey::KEY_UP:
          navigatePrev(core);
          break;
        default:
          break;
      }
    }
  }
#endif

  return StateTransition::stay(StateId::Reader);
}

void ReaderState::render(Core& core) {
  if (!needsRender_ || !contentLoaded_) {
    return;
  }

  if (settingsMode_) {
    renderSettingsOverlay(core);
  } else if (tocMode_) {
    renderTocOverlay(core);
  } else {
    renderCurrentPage(core);
    // Track last successfully rendered position for progress saving
    lastRenderedSpineIndex_ = currentSpineIndex_;
    lastRenderedSectionPage_ = currentSectionPage_;
  }

  needsRender_ = false;
}

void ReaderState::navigateNext(Core& core) {
  // Stop background task before accessing pageCache_ (ownership model)
  stopBackgroundCaching();

  ContentType type = core.content.metadata().type;

  // XTC uses flatPage navigation, not spine/section - skip to navigation logic
  if (type == ContentType::Xtc) {
    ReaderNavigation::Position pos;
    pos.flatPage = currentPage_;
    auto result = ReaderNavigation::next(type, pos, nullptr, core.content.pageCount());
    applyNavResult(result, core);
    return;
  }

  // Spine/section logic for EPUB, TXT, Markdown
  // From cover (-1) -> first text content page
  if (currentSpineIndex_ == 0 && currentSectionPage_ == -1) {
    auto* provider = core.content.asEpub();
    size_t spineCount = 1;
    if (provider && provider->getEpub()) {
      spineCount = provider->getEpub()->getSpineItemsCount();
    }
    int firstContentSpine = calcFirstContentSpine(hasCover_, textStartIndex_, spineCount);

    if (firstContentSpine != currentSpineIndex_) {
      currentSpineIndex_ = firstContentSpine;
      parser_.reset();
      parserSpineIndex_ = -1;
      pageCache_.reset();
    }
    currentSectionPage_ = 0;
    needsRender_ = true;
    startBackgroundCaching(core);
    return;
  }

  ReaderNavigation::Position pos;
  pos.spineIndex = currentSpineIndex_;
  pos.sectionPage = currentSectionPage_;
  pos.flatPage = currentPage_;
  auto result = ReaderNavigation::next(type, pos, pageCache_.get(), core.content.pageCount());
  applyNavResult(result, core);
}

void ReaderState::navigatePrev(Core& core) {
  // Stop background task before accessing pageCache_ (ownership model)
  stopBackgroundCaching();

  ContentType type = core.content.metadata().type;

  // XTC uses flatPage navigation, not spine/section - skip to navigation logic
  if (type == ContentType::Xtc) {
    ReaderNavigation::Position pos;
    pos.flatPage = currentPage_;
    auto result = ReaderNavigation::prev(type, pos, nullptr);
    applyNavResult(result, core);
    return;
  }

  // Spine/section logic for EPUB, TXT, Markdown
  auto* provider = core.content.asEpub();
  size_t spineCount = 1;
  if (provider && provider->getEpub()) {
    spineCount = provider->getEpub()->getSpineItemsCount();
  }
  int firstContentSpine = calcFirstContentSpine(hasCover_, textStartIndex_, spineCount);

  // At first page of text content
  if (currentSpineIndex_ == firstContentSpine && currentSectionPage_ == 0) {
    // Only go to cover if it exists and images enabled
    if (hasCover_ && core.settings.showImages) {
      currentSpineIndex_ = 0;
      currentSectionPage_ = -1;
      parser_.reset();
      parserSpineIndex_ = -1;
      pageCache_.reset();  // Don't need cache for cover
      needsRender_ = true;
    }
    return;  // At start of book either way
  }

  // Prevent going back from cover
  if (currentSpineIndex_ == 0 && currentSectionPage_ == -1) {
    startBackgroundCaching(core);  // Resume task before returning
    return;                        // Already at cover
  }

  ReaderNavigation::Position pos;
  pos.spineIndex = currentSpineIndex_;
  pos.sectionPage = currentSectionPage_;
  pos.flatPage = currentPage_;
  auto result = ReaderNavigation::prev(type, pos, pageCache_.get());
  applyNavResult(result, core);
}

void ReaderState::applyNavResult(const ReaderNavigation::NavResult& result, Core& core) {
  currentSpineIndex_ = result.position.spineIndex;
  currentSectionPage_ = result.position.sectionPage;
  currentPage_ = result.position.flatPage;
  needsRender_ = result.needsRender;
  scrollY_ = 0;  // Reset scroll for new page
  pageContentHeight_ = 0;
  if (result.needsCacheReset) {
    parser_.reset();  // Safe - task already stopped by caller
    parserSpineIndex_ = -1;
    pageCache_.reset();
  }
  startBackgroundCaching(core);  // Resume caching
}

void ReaderState::renderCurrentPage(Core& core) {
  ContentType type = core.content.metadata().type;
  const Theme& theme = THEME_MANAGER.current();

  // Always clear screen first (prevents previous content from showing through)
  renderer_.clearScreen(theme.backgroundColor);

  // Cover page: spineIndex=0, sectionPage=-1 (only when showImages enabled)
  if (currentSpineIndex_ == 0 && currentSectionPage_ == -1) {
    if (core.settings.showImages) {
      if (renderCoverPage(core)) {
        hasCover_ = true;
        core.display.markDirty();
        return;
      }
      // No cover - skip spine 0 if textStartIndex is 0 (likely empty cover document)
      hasCover_ = false;
      currentSectionPage_ = 0;
      if (textStartIndex_ == 0) {
        // Only skip to spine 1 if it exists
        auto* provider = core.content.asEpub();
        if (provider && provider->getEpub()) {
          const auto* epub = provider->getEpub();
          if (epub->getSpineItemsCount() > 1) {
            currentSpineIndex_ = 1;
          }
        }
      }
      // Fall through to render content
    } else {
      currentSectionPage_ = 0;
    }
  }

  switch (type) {
    case ContentType::Epub:
    case ContentType::Txt:
    case ContentType::Markdown:
      renderCachedPage(core);
      break;
    case ContentType::Xtc:
      renderXtcPage(core);
      break;
    default:
      break;
  }

  if (!cacheTask_.isRunning() && (!pageCache_ || !thumbnailDone_)) {
    startBackgroundCaching(core);
  }

  core.display.markDirty();
}

void ReaderState::renderCachedPage(Core& core) {
  Theme& theme = THEME_MANAGER.mutableCurrent();
  ContentType type = core.content.metadata().type;
  const auto vp = getReaderViewport();

  // Handle EPUB bounds
  if (type == ContentType::Epub) {
    auto* provider = core.content.asEpub();
    if (!provider || !provider->getEpub()) return;

    auto epub = provider->getEpubShared();
    if (currentSpineIndex_ < 0) currentSpineIndex_ = 0;
    if (currentSpineIndex_ >= static_cast<int>(epub->getSpineItemsCount())) {
      renderer_.drawCenteredText(core.settings.getReaderFontId(theme), 300, "End of book", theme.primaryTextBlack,
                                 BOLD);
      renderer_.displayBuffer();
      return;
    }
  }

  // Stop background task to ensure we own pageCache_ (ownership model)
  stopBackgroundCaching();

  // Background task may have left parser in inconsistent state
  if (!pageCache_ && parser_ && parserSpineIndex_ == currentSpineIndex_) {
    parser_.reset();
    parserSpineIndex_ = -1;
  }

  // Create or load cache if needed
  if (!pageCache_) {
    // Try to load existing cache silently first
    loadCacheFromDisk(core);

    bool pageIsCached =
        pageCache_ && currentSectionPage_ >= 0 && currentSectionPage_ < static_cast<int>(pageCache_->pageCount());

    if (!pageIsCached) {
      // Current page not cached - show "Indexing..." and create/extend
      renderer_.clearScreen(theme.backgroundColor);
      ui::centeredMessage(renderer_, theme, core.settings.getReaderFontId(theme), "Indexing...");
      renderer_.displayBuffer();

      createOrExtendCache(core);

      // Backward navigation: cache entire chapter to find actual last page
      if (currentSectionPage_ == INT16_MAX) {
        while (pageCache_ && pageCache_->isPartial()) {
          const size_t pagesBefore = pageCache_->pageCount();
          createOrExtendCache(core);
          if (pageCache_ && pageCache_->pageCount() <= pagesBefore) {
            break;  // No progress - avoid infinite loop
          }
        }
      }

      // Clear overlay
      renderer_.clearScreen(theme.backgroundColor);
    }

    // Clamp page number (handle negative values and out-of-bounds)
    if (pageCache_) {
      const int cachedPages = static_cast<int>(pageCache_->pageCount());
      if (currentSectionPage_ < 0) {
        currentSectionPage_ = 0;
      } else if (currentSectionPage_ >= cachedPages) {
        currentSectionPage_ = cachedPages > 0 ? cachedPages - 1 : 0;
      }
    }
  }

  // Check if we need to extend cache
  if (!ensurePageCached(core, currentSectionPage_)) {
    renderer_.drawCenteredText(core.settings.getReaderFontId(theme), 300, "Failed to load page", theme.primaryTextBlack,
                               BOLD);
    renderer_.displayBuffer();
    needsRender_ = false;  // Prevent infinite render loop on cache failure
    return;
  }

  // ensurePageCached may have used the frame buffer as ZIP decompression dictionary
  renderer_.clearScreen(theme.backgroundColor);

  // Load and render page (cache is now guaranteed to exist, we own it)
  size_t pageCount = pageCache_ ? pageCache_->pageCount() : 0;
  auto page = pageCache_ ? pageCache_->loadPage(currentSectionPage_) : nullptr;

  if (!page) {
    Serial.println("[READER] Failed to load page, clearing cache");
    if (pageCache_) {
      pageCache_->clear();
      pageCache_.reset();
    }
    needsRender_ = true;
    return;
  }

  const int fontId = core.settings.getReaderFontId(theme);

  // In landscape scroll mode, compute content height and apply scroll offset
  if (landscapeScroll_) {
    const int lineHeight = renderer_.getLineHeight(fontId);
    pageContentHeight_ = page->contentHeight(lineHeight);

    // Render with scroll offset (drawPixel clips out-of-bounds pixels)
    renderPageContents(core, *page, vp.marginTop - scrollY_, vp.marginRight, vp.marginBottom, vp.marginLeft);

    // Draw scroll indicator if content extends beyond viewport
    const int screenH = renderer_.getScreenHeight();
    const int screenW = renderer_.getScreenWidth();
    if (pageContentHeight_ > screenH) {
      const int barX = screenW - 3;
      const int trackH = screenH - 4;
      const int thumbH = std::max(8, trackH * screenH / pageContentHeight_);
      const int maxScroll = pageContentHeight_ - screenH;
      const int thumbY = 2 + (maxScroll > 0 ? (scrollY_ * (trackH - thumbH) / maxScroll) : 0);
      for (int y = thumbY; y < thumbY + thumbH && y < screenH - 2; y++) {
        renderer_.drawPixel(barX, y);
        renderer_.drawPixel(barX - 1, y);
      }
    }

    // Show page number at bottom-right
    const int statusY = screenH - 12;
    char pageStr[16];
    if (pageCache_) {
      snprintf(pageStr, sizeof(pageStr), "%d/%zu", currentSectionPage_ + 1, pageCache_->pageCount());
    } else {
      snprintf(pageStr, sizeof(pageStr), "%d", currentSectionPage_ + 1);
    }
    const int tw = renderer_.getTextWidth(theme.smallFontId, pageStr);
    renderer_.drawText(theme.smallFontId, screenW - tw - 8, statusY, pageStr, true);

    displayWithRefresh(core);
  } else {
    renderPageContents(core, *page, vp.marginTop, vp.marginRight, vp.marginBottom, vp.marginLeft);
    renderStatusBar(core, vp.marginRight, vp.marginBottom, vp.marginLeft);

    displayWithRefresh(core);

    // Grayscale text rendering (anti-aliasing) - skip for custom fonts (saves ~48KB)
    if (core.settings.textAntiAliasing && !FONT_MANAGER.isUsingCustomReaderFont() &&
        renderer_.fontSupportsGrayscale(fontId) && renderer_.storeBwBuffer()) {
      renderer_.clearScreen(0x00);
      renderer_.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
      page->render(renderer_, fontId, vp.marginLeft, vp.marginTop, theme.primaryTextBlack);
      renderer_.copyGrayscaleLsbBuffers();

      renderer_.clearScreen(0x00);
      renderer_.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
      page->render(renderer_, fontId, vp.marginLeft, vp.marginTop, theme.primaryTextBlack);
      renderer_.copyGrayscaleMsbBuffers();

      const bool turnOffScreen = core.settings.sunlightFadingFix != 0;
      renderer_.displayGrayBuffer(turnOffScreen);
      renderer_.setRenderMode(GfxRenderer::BW);
      renderer_.restoreBwBuffer();
    }
  }

  Serial.printf("[READER] Rendered page %d/%d%s\n", currentSectionPage_ + 1, pageCount,
                landscapeScroll_ ? " (scroll)" : "");
}

bool ReaderState::ensurePageCached(Core& core, uint16_t pageNum) {
  // Caller must have stopped background task (we own pageCache_)
  if (!pageCache_) {
    return false;
  }

  // If page is already cached, we're good
  size_t pageCount = pageCache_->pageCount();
  bool needsExtension = pageCache_->needsExtension(pageNum);
  bool isPartial = pageCache_->isPartial();

  if (pageNum < pageCount) {
    // Check if we should pre-extend (approaching end of partial cache)
    if (needsExtension) {
      Serial.printf("[READER] Pre-extending cache at page %d\n", pageNum);
      createOrExtendCache(core);
    }
    return true;
  }

  // Page not cached yet - need to extend
  if (!isPartial) {
    Serial.printf("[READER] Page %d not available (cache complete at %d pages)\n", pageNum, pageCount);
    return false;
  }

  Serial.printf("[READER] Extending cache for page %d\n", pageNum);

  const Theme& theme = THEME_MANAGER.current();
  ui::centeredMessage(renderer_, theme, core.settings.getReaderFontId(theme), "Loading...");

  createOrExtendCache(core);

  pageCount = pageCache_ ? pageCache_->pageCount() : 0;
  return pageNum < pageCount;
}

void ReaderState::loadCacheFromDisk(Core& core) {
  const Theme& theme = THEME_MANAGER.current();
  ContentType type = core.content.metadata().type;

  const auto vp = getReaderViewport();
  auto config = core.settings.getRenderConfig(theme, vp.width, vp.height);
  config.allowTallImages = landscapeScroll_;

  std::string cachePath;
  if (type == ContentType::Epub) {
    auto* provider = core.content.asEpub();
    if (!provider || !provider->getEpub()) {
      Serial.println("[READER] loadCacheFromDisk: no epub provider");
      return;
    }
    cachePath = epubSectionCachePath(provider->getEpub()->getCachePath(), currentSpineIndex_);
  } else if (type == ContentType::Markdown || type == ContentType::Txt) {
    cachePath = contentCachePath(core.content.cacheDir(), config.fontId);
  } else {
    Serial.printf("[READER] loadCacheFromDisk: unsupported content type %d\n", static_cast<int>(type));
    return;
  }

  // Caller must have stopped background task (we own pageCache_)
  if (!pageCache_) {
    pageCache_.reset(new PageCache(cachePath));
    if (!pageCache_->load(config)) {
      pageCache_.reset();
    }
  }
}

void ReaderState::createOrExtendCache(Core& core) {
  const Theme& theme = THEME_MANAGER.current();
  ContentType type = core.content.metadata().type;

  const auto vp = getReaderViewport();
  auto config = core.settings.getRenderConfig(theme, vp.width, vp.height);
  config.allowTallImages = landscapeScroll_;

  std::string cachePath;
  if (type == ContentType::Epub) {
    auto* provider = core.content.asEpub();
    if (!provider || !provider->getEpub()) return;
    auto epub = provider->getEpubShared();
    cachePath = epubSectionCachePath(epub->getCachePath(), currentSpineIndex_);

    // Create parser if we don't have one (or if spine changed)
    if (!parser_ || parserSpineIndex_ != currentSpineIndex_) {
      std::string imageCachePath = core.settings.showImages ? (epub->getCachePath() + "/images") : "";
      parser_.reset(new EpubChapterParser(epub, currentSpineIndex_, renderer_, config, imageCachePath));
      parserSpineIndex_ = currentSpineIndex_;
    }
  } else if (type == ContentType::Markdown) {
    cachePath = contentCachePath(core.content.cacheDir(), config.fontId);
    if (!parser_) {
      parser_.reset(new MarkdownParser(contentPath_, renderer_, config));
      parserSpineIndex_ = 0;
    }
  } else {
    cachePath = contentCachePath(core.content.cacheDir(), config.fontId);
    if (!parser_) {
      parser_.reset(new PlainTextParser(contentPath_, renderer_, config));
      parserSpineIndex_ = 0;
    }
  }

  createOrExtendCacheImpl(*parser_, cachePath, config);
}

void ReaderState::renderPageContents(Core& core, Page& page, int marginTop, int marginRight, int marginBottom,
                                     int marginLeft) {
  (void)marginRight;
  (void)marginBottom;

  const Theme& theme = THEME_MANAGER.current();
  const int fontId = core.settings.getReaderFontId(theme);
  page.render(renderer_, fontId, marginLeft, marginTop, theme.primaryTextBlack);
}

void ReaderState::renderStatusBar(Core& core, int marginRight, int marginBottom, int marginLeft) {
  const Theme& theme = THEME_MANAGER.current();
  ContentType type = core.content.metadata().type;

  // Build status bar data
  ui::ReaderStatusBarData data{};
  data.mode = core.settings.statusBar;
  data.title = core.content.metadata().title;

  // Battery
  const uint16_t millivolts = batteryMonitor.readMillivolts();
  data.batteryPercent = (millivolts < 100) ? -1 : BatteryMonitor::percentageFromMillivolts(millivolts);

  // Page info
  // Note: renderCachedPage() already stopped the task, so we own pageCache_
  if (type == ContentType::Epub) {
    auto* provider = core.content.asEpub();
    if (provider && provider->getEpub()) {
      data.currentPage = currentSectionPage_ + 1;
      if (pageCache_) {
        data.totalPages = pageCache_->pageCount();
        data.isPartial = pageCache_->isPartial();
      } else {
        data.isPartial = true;
      }
    } else {
      return;
    }
  } else {
    data.currentPage = currentSectionPage_ + 1;
    data.totalPages = core.content.pageCount();
  }

  ui::readerStatusBar(renderer_, theme, marginLeft, marginRight, marginBottom, data);
}

void ReaderState::renderXtcPage(Core& core) {
  auto* provider = core.content.asXtc();
  if (!provider) {
    return;
  }

  const Theme& theme = THEME_MANAGER.current();

  auto result = xtcRenderer_.render(provider->getParser(), currentPage_, [this, &core]() { displayWithRefresh(core); });

  switch (result) {
    case XtcPageRenderer::RenderResult::Success:
      break;
    case XtcPageRenderer::RenderResult::EndOfBook:
      ui::centeredMessage(renderer_, theme, theme.uiFontId, "End of book");
      break;
    case XtcPageRenderer::RenderResult::InvalidDimensions:
      ui::centeredMessage(renderer_, theme, theme.uiFontId, "Invalid file");
      break;
    case XtcPageRenderer::RenderResult::AllocationFailed:
      ui::centeredMessage(renderer_, theme, theme.uiFontId, "Memory error");
      break;
    case XtcPageRenderer::RenderResult::PageLoadFailed:
      ui::centeredMessage(renderer_, theme, theme.uiFontId, "Page load error");
      break;
  }
}

void ReaderState::displayWithRefresh(Core& core) {
  const bool turnOffScreen = core.settings.sunlightFadingFix != 0;
  const int ppr = core.settings.getPagesPerRefreshValue();
  if (ppr == 0) {
    // "Never" — always fast refresh, no automatic half refresh
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH, turnOffScreen);
  } else if (pagesUntilFullRefresh_ <= 0) {
    // First render - use FULL_REFRESH for clean display
    renderer_.displayBuffer(EInkDisplay::FULL_REFRESH, turnOffScreen);
    pagesUntilFullRefresh_ = ppr;
  } else if (pagesUntilFullRefresh_ == 1) {
    renderer_.displayBuffer(EInkDisplay::HALF_REFRESH, turnOffScreen);
    pagesUntilFullRefresh_ = ppr;
  } else {
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH, turnOffScreen);
    pagesUntilFullRefresh_--;
  }
}

ReaderState::Viewport ReaderState::getReaderViewport() const {
  Viewport vp{};
  renderer_.getOrientedViewableTRBL(&vp.marginTop, &vp.marginRight, &vp.marginBottom, &vp.marginLeft);
  vp.marginLeft += horizontalPadding;
  vp.marginRight += horizontalPadding;
  vp.marginBottom += statusBarMargin;
  vp.width = renderer_.getScreenWidth() - vp.marginLeft - vp.marginRight;
  vp.height = renderer_.getScreenHeight() - vp.marginTop - vp.marginBottom;
  return vp;
}

bool ReaderState::renderCoverPage(Core& core) {
  Serial.printf("[%lu] [RDR] Generating cover for reader...\n", millis());
  std::string coverPath = core.content.generateCover(true);  // Always 1-bit in reader (saves ~48KB grayscale buffer)
  if (coverPath.empty()) {
    Serial.printf("[%lu] [RDR] No cover available, skipping cover page\n", millis());
    return false;
  }

  Serial.printf("[%lu] [RDR] Rendering cover page from: %s\n", millis(), coverPath.c_str());
  const auto vp = getReaderViewport();
  int pagesUntilRefresh = pagesUntilFullRefresh_;
  const bool turnOffScreen = core.settings.sunlightFadingFix != 0;

  bool rendered = CoverHelpers::renderCoverFromBmp(renderer_, coverPath, vp.marginTop, vp.marginRight, vp.marginBottom,
                                                   vp.marginLeft, pagesUntilRefresh,
                                                   core.settings.getPagesPerRefreshValue(), turnOffScreen);

  // Force half refresh on next page to fully clear the cover image
  pagesUntilFullRefresh_ = 1;
  return rendered;
}

void ReaderState::startBackgroundCaching(Core& core) {
  // XTC content uses pre-rendered bitmaps — no page cache or thumbnail support
  if (core.content.metadata().type == ContentType::Xtc) {
    thumbnailDone_ = true;
    return;
  }

  // BackgroundTask handles safe restart via CAS loop
  if (cacheTask_.isRunning()) {
    Serial.println("[READER] Warning: Previous cache task still running, stopping first");
    stopBackgroundCaching();
  }

  Serial.println("[READER] Starting background page cache task");
  coreForCacheTask_ = &core;

  // Snapshot state for the background task
  const int sectionPage = currentSectionPage_;
  const int spineIndex = currentSpineIndex_;
  const bool coverExists = hasCover_;
  const int textStart = textStartIndex_;
  const bool isLandscapeScroll = landscapeScroll_;

  cacheTask_.start(
      "PageCache", kCacheTaskStackSize,
      [this, sectionPage, spineIndex, coverExists, textStart, isLandscapeScroll]() {
        const Theme& theme = THEME_MANAGER.current();
        Serial.println("[READER] Background cache task started");

        if (cacheTask_.shouldStop()) {
          Serial.println("[READER] Background cache task aborted (stop requested)");
          return;
        }

        Core* corePtr = coreForCacheTask_;
        if (!corePtr) {
          Serial.println("[READER] Background cache task aborted (no core)");
          return;
        }

        Core& coreRef = *corePtr;
        ContentType type = coreRef.content.metadata().type;

        // Build cache if it doesn't exist
        if (!pageCache_ && !cacheTask_.shouldStop()) {
          const auto vp = getReaderViewport();
          auto config = coreRef.settings.getRenderConfig(theme, vp.width, vp.height);
          config.allowTallImages = isLandscapeScroll;
          std::string cachePath;

          if (type == ContentType::Epub) {
            auto* provider = coreRef.content.asEpub();
            if (provider && provider->getEpub() && !cacheTask_.shouldStop()) {
              const auto* epub = provider->getEpub();
              std::string imageCachePath = coreRef.settings.showImages ? (epub->getCachePath() + "/images") : "";
              // When on cover page (sectionPage=-1), cache the first content spine
              int spineToCache = spineIndex;
              if (sectionPage == -1) {
                spineToCache = calcFirstContentSpine(coverExists, textStart, epub->getSpineItemsCount());
              }
              cachePath = epubSectionCachePath(epub->getCachePath(), spineToCache);

              if (!parser_ || parserSpineIndex_ != spineToCache) {
                parser_.reset(
                    new EpubChapterParser(provider->getEpubShared(), spineToCache, renderer_, config, imageCachePath));
                parserSpineIndex_ = spineToCache;
              }
            }
          } else if (type == ContentType::Markdown && !cacheTask_.shouldStop()) {
            cachePath = contentCachePath(coreRef.content.cacheDir(), config.fontId);
            if (!parser_) {
              parser_.reset(new MarkdownParser(contentPath_, renderer_, config));
              parserSpineIndex_ = 0;
            }
          } else if (type == ContentType::Txt && !cacheTask_.shouldStop()) {
            cachePath = contentCachePath(coreRef.content.cacheDir(), config.fontId);
            if (!parser_) {
              parser_.reset(new PlainTextParser(contentPath_, renderer_, config));
              parserSpineIndex_ = 0;
            }
          }

          if (parser_ && !cachePath.empty() && !cacheTask_.shouldStop()) {
            backgroundCacheImpl(*parser_, cachePath, config);
          }
        }

        // Thumbnail generation moved to ReaderState::exit() to avoid buffer conflicts
        // with concurrent cover/page rendering

        if (!cacheTask_.shouldStop()) {
          Serial.println("[READER] Background cache task completed");
        } else {
          Serial.println("[READER] Background cache task stopped");
        }
      },
      0);  // priority 0 (idle)
}

void ReaderState::stopBackgroundCaching() {
  if (!cacheTask_.isRunning()) {
    return;
  }

  // BackgroundTask::stop() uses event-based waiting (no polling)
  // and NEVER force-deletes the task
  if (!cacheTask_.stop(kCacheTaskStopTimeoutMs)) {
    Serial.println("[READER] WARNING: Cache task did not stop within timeout");
    Serial.println("[READER] Task may be blocked on SD card I/O");
  }

  // Yield to allow FreeRTOS idle task to clean up the deleted task's TCB.
  // The background task self-deletes via vTaskDelete(NULL), but the idle task
  // must run to free its resources. Without this, parser_.reset() or
  // pageCache_.reset() can trigger mutex ownership violations
  // (assert failed: xQueueGenericSend queue.c:832).
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

// ============================================================================
// TOC Overlay Mode
// ============================================================================

void ReaderState::enterTocMode(Core& core) {
  if (core.content.tocCount() == 0) {
    return;
  }

  // Stop background task before TOC overlay — both SD card I/O (thumbnail)
  // and e-ink display update share the same SPI bus
  stopBackgroundCaching();

  populateTocView(core);
  int currentIdx = findCurrentTocEntry(core);
  if (currentIdx >= 0) {
    tocView_.setCurrentChapter(static_cast<uint8_t>(currentIdx));
  }

  tocMode_ = true;
  needsRender_ = true;
  Serial.println("[READER] Entered TOC mode");
}

void ReaderState::exitTocMode() {
  tocMode_ = false;
  centerLongPressFired_ = false;  // Reset so next short-press works normally
  needsRender_ = true;
  Serial.println("[READER] Exited TOC mode");
}

void ReaderState::handleTocInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) {
    return;
  }

  switch (e.button) {
    case Button::Up:
      tocView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      tocView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Left:
      tocView_.movePageUp(tocVisibleCount());
      needsRender_ = true;
      break;

    case Button::Right:
      tocView_.movePageDown(tocVisibleCount());
      needsRender_ = true;
      break;

    case Button::Center:
      jumpToTocEntry(core, tocView_.selected);
      exitTocMode();
      startBackgroundCaching(core);
      break;

    case Button::Back:
      exitTocMode();
      startBackgroundCaching(core);
      break;

    case Button::Power:
      if (core.settings.shortPwrBtn == Settings::PowerPageTurn) {
        tocView_.moveDown();
        needsRender_ = true;
      } else if (core.settings.shortPwrBtn == Settings::PowerRefresh) {
        renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
      }
      break;
  }
}

void ReaderState::populateTocView(Core& core) {
  tocView_.clear();
  const uint16_t count = core.content.tocCount();

  for (uint16_t i = 0; i < count && i < ui::ChapterListView::MAX_CHAPTERS; i++) {
    auto result = core.content.getTocEntry(i);
    if (result.ok()) {
      const TocEntry& entry = result.value;
      tocView_.addChapter(entry.title, static_cast<uint16_t>(entry.pageIndex), entry.depth);
    }
  }
}

int ReaderState::findCurrentTocEntry(Core& core) {
  ContentType type = core.content.metadata().type;

  if (type == ContentType::Epub) {
    auto* provider = core.content.asEpub();
    if (!provider || !provider->getEpub()) return -1;
    auto epub = provider->getEpubShared();

    // Start with spine-level match as fallback
    int bestMatch = epub->getTocIndexForSpineIndex(currentSpineIndex_);
    int bestMatchPage = -1;

    // Load anchor map once from disk (avoids reopening file per TOC entry)
    std::string cachePath = epubSectionCachePath(epub->getCachePath(), currentSpineIndex_);
    std::string anchorPath = cachePath + ".anchors";
    std::vector<std::pair<std::string, uint16_t>> anchors;
    {
      FsFile file;
      if (SdMan.openFileForRead("RDR", anchorPath, file)) {
        uint16_t count;
        if (serialization::readPodChecked(file, count)) {
          for (uint16_t j = 0; j < count; j++) {
            std::string id;
            uint16_t page;
            if (!serialization::readString(file, id) || !serialization::readPodChecked(file, page)) break;
            anchors.emplace_back(std::move(id), page);
          }
        }
        file.close();
      }
    }

    // Refine: find the latest TOC entry whose anchor page <= current page
    const int tocCount = epub->getTocItemsCount();

    for (int i = 0; i < tocCount; i++) {
      auto tocItem = epub->getTocItem(i);
      if (tocItem.spineIndex != currentSpineIndex_) continue;

      int entryPage = 0;  // No anchor = start of spine
      if (!tocItem.anchor.empty()) {
        int anchorPage = -1;
        for (const auto& a : anchors) {
          if (a.first == tocItem.anchor) {
            anchorPage = a.second;
            break;
          }
        }
        if (anchorPage < 0) continue;  // Anchor not resolved yet
        entryPage = anchorPage;
      }

      if (entryPage <= currentSectionPage_ && entryPage >= bestMatchPage) {
        bestMatch = i;
        bestMatchPage = entryPage;
      }
    }

    return bestMatch;
  } else if (type == ContentType::Xtc) {
    // For XTC, find chapter containing current page
    const uint16_t count = core.content.tocCount();
    int lastMatch = -1;
    for (uint16_t i = 0; i < count; i++) {
      auto result = core.content.getTocEntry(i);
      if (result.ok() && result.value.pageIndex <= static_cast<uint32_t>(currentPage_)) {
        lastMatch = i;
      }
    }
    return lastMatch;
  }

  return -1;
}

void ReaderState::jumpToTocEntry(Core& core, int tocIndex) {
  if (tocIndex < 0 || tocIndex >= tocView_.chapterCount) {
    return;
  }

  const auto& chapter = tocView_.chapters[tocIndex];
  ContentType type = core.content.metadata().type;

  if (type == ContentType::Epub) {
    auto* provider = core.content.asEpub();
    if (!provider || !provider->getEpub()) return;
    auto epub = provider->getEpubShared();

    if (static_cast<int>(chapter.pageNum) != currentSpineIndex_) {
      // Different spine — full reset
      // Task already stopped by enterTocMode(); caller restarts after exitTocMode()
      currentSpineIndex_ = chapter.pageNum;
      parser_.reset();
      parserSpineIndex_ = -1;
      pageCache_.reset();
      currentSectionPage_ = 0;
    } else {
      // Same spine — navigate using anchor (default to page 0)
      currentSectionPage_ = 0;
    }

    // Try anchor-based navigation for precise positioning
    auto tocItem = epub->getTocItem(tocIndex);
    if (!tocItem.anchor.empty()) {
      std::string cachePath = epubSectionCachePath(epub->getCachePath(), chapter.pageNum);
      int page = loadAnchorPage(cachePath, tocItem.anchor);

      // Anchor not resolved — build cache until found or chapter fully parsed
      if (page < 0) {
        const Theme& theme = THEME_MANAGER.current();
        renderer_.clearScreen(theme.backgroundColor);
        ui::centeredMessage(renderer_, theme, core.settings.getReaderFontId(theme), "Indexing...");
        renderer_.displayBuffer();

        createOrExtendCache(core);
        page = loadAnchorPage(cachePath, tocItem.anchor);

        while (page < 0 && pageCache_ && pageCache_->isPartial()) {
          const size_t pagesBefore = pageCache_->pageCount();
          createOrExtendCache(core);
          if (!pageCache_ || pageCache_->pageCount() <= pagesBefore) break;
          page = loadAnchorPage(cachePath, tocItem.anchor);
        }
      }

      if (page >= 0) {
        currentSectionPage_ = page;
      }
    }
  } else if (type == ContentType::Xtc) {
    // For XTC, pageNum is page index
    currentPage_ = chapter.pageNum;
  }

  needsRender_ = true;
  Serial.printf("[READER] Jumped to TOC entry %d (spine/page %d)\n", tocIndex, chapter.pageNum);
}

int ReaderState::tocVisibleCount() const {
  constexpr int startY = 60;
  constexpr int bottomMargin = 70;
  const Theme& theme = THEME_MANAGER.current();
  const int itemHeight = theme.itemHeight + theme.itemSpacing;
  return (renderer_.getScreenHeight() - startY - bottomMargin) / itemHeight;
}

void ReaderState::renderTocOverlay(Core& core) {
  const Theme& theme = THEME_MANAGER.current();
  constexpr int startY = 60;
  const int visibleCount = tocVisibleCount();

  // Adjust scroll to keep selection visible
  tocView_.ensureVisible(visibleCount);

  renderer_.clearScreen(theme.backgroundColor);
  renderer_.drawCenteredText(theme.uiFontId, 15, "Chapters", theme.primaryTextBlack, BOLD);

  // Use reader font only when external font is selected (for VN/Thai/CJK support),
  // otherwise use smaller UI font for better chapter list readability
  const ContentType type = core.content.metadata().type;
  const bool hasExternalFont = core.settings.hasExternalReaderFont(theme);
  const int tocFontId =
      (type == ContentType::Xtc || !hasExternalFont) ? theme.uiFontId : core.settings.getReaderFontId(theme);

  const int itemHeight = theme.itemHeight + theme.itemSpacing;
  const int end = std::min(tocView_.scrollOffset + visibleCount, static_cast<int>(tocView_.chapterCount));
  for (int i = tocView_.scrollOffset; i < end; i++) {
    const int y = startY + (i - tocView_.scrollOffset) * itemHeight;
    ui::chapterItem(renderer_, theme, tocFontId, y, tocView_.chapters[i].title, tocView_.chapters[i].depth,
                    i == tocView_.selected, i == tocView_.currentChapter);
  }

  renderer_.displayBuffer();
  core.display.markDirty();
}

// ============================================================================
// In-Reader Settings Overlay (long-press Select)
// ============================================================================

void ReaderState::enterSettingsMode(Core& core) {
  stopBackgroundCaching();
  loadInReaderSettings(core);
  settingsView_.selected = 0;
  settingsView_.scrollOffset = 0;
  settingsMode_ = true;
  needsRender_ = true;
  Serial.println("[READER] Entered settings mode");
}

void ReaderState::exitSettingsMode(Core& core) {
  settingsMode_ = false;
  centerLongPressFired_ = false;  // Reset so next short-press works normally
  needsRender_ = true;
  startBackgroundCaching(core);
  Serial.println("[READER] Exited settings mode");
}

void ReaderState::handleSettingsInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) {
    return;
  }

  switch (e.button) {
    case Button::Up:
      settingsView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      settingsView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Left:
      settingsView_.cycleValue(-1);
      applyInReaderSettings(core);
      needsRender_ = true;
      break;

    case Button::Right:
    case Button::Center:
      settingsView_.cycleValue(1);
      applyInReaderSettings(core);
      needsRender_ = true;
      break;

    case Button::Back:
      exitSettingsMode(core);
      break;

    case Button::Power:
      if (core.settings.shortPwrBtn == Settings::PowerRefresh) {
        renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
      }
      break;
  }
}

void ReaderState::renderSettingsOverlay(Core& core) {
  ui::render(renderer_, THEME_MANAGER.current(), settingsView_);
  core.display.markDirty();
}

void ReaderState::loadInReaderSettings(Core& core) {
  const auto& s = core.settings;
  // Matches InReaderSettingsView::DEFS order
  settingsView_.values[0] = s.fontSize;
  settingsView_.values[1] = s.textLayout;
  settingsView_.values[2] = s.lineSpacing;
  settingsView_.values[3] = s.paragraphAlignment;
  settingsView_.values[4] = s.hyphenation;
  settingsView_.values[5] = s.textAntiAliasing;
  settingsView_.values[6] = s.showImages;
  settingsView_.values[7] = s.statusBar;
}

void ReaderState::applyInReaderSettings(Core& core) {
  auto& s = core.settings;

  // Detect if layout-affecting settings changed (requires cache rebuild)
  bool cacheInvalid = (s.fontSize != settingsView_.values[0] ||
                        s.textLayout != settingsView_.values[1] ||
                        s.lineSpacing != settingsView_.values[2] ||
                        s.paragraphAlignment != settingsView_.values[3] ||
                        s.hyphenation != settingsView_.values[4] ||
                        s.showImages != settingsView_.values[6]);

  // Apply all values
  s.fontSize = settingsView_.values[0];
  s.textLayout = settingsView_.values[1];
  s.lineSpacing = settingsView_.values[2];
  s.paragraphAlignment = settingsView_.values[3];
  s.hyphenation = settingsView_.values[4];
  s.textAntiAliasing = settingsView_.values[5];
  s.showImages = settingsView_.values[6];
  s.statusBar = settingsView_.values[7];

  // Invalidate page cache if layout changed
  if (cacheInvalid) {
    parser_.reset();
    parserSpineIndex_ = -1;
    pageCache_.reset();
  }

  // Persist to disk
  s.save(core.storage);
}

void ReaderState::exitToUI(Core& core) {
  Serial.println("[READER] Exiting to UI mode via restart");

  // Stop background caching first - BackgroundTask::stop() waits properly
  stopBackgroundCaching();

  // Save progress at last rendered position
  if (contentLoaded_) {
    // Generate thumbnail on exit (not during background task) to avoid buffer conflicts
    if (!thumbnailDone_) {
      Serial.println("[READER] Generating thumbnail on exit...");
      core.content.generateThumbnail();
      thumbnailDone_ = true;
    }

    ProgressManager::Progress progress;
    progress.spineIndex = (lastRenderedSectionPage_ == -1) ? 0 : lastRenderedSpineIndex_;
    progress.sectionPage = (lastRenderedSectionPage_ == -1) ? 0 : lastRenderedSectionPage_;
    progress.flatPage = currentPage_;
    ProgressManager::save(core, core.content.cacheDir(), core.content.metadata().type, progress);

    // Update library index for file browser progress bars
    uint16_t libCurrent = 0, libTotal = 0;
    if (core.content.metadata().type == ContentType::Epub) {
      libCurrent = static_cast<uint16_t>(lastRenderedSpineIndex_ + 1);
      libTotal = static_cast<uint16_t>(core.content.pageCount());
    } else {
      libTotal = static_cast<uint16_t>(std::min(core.content.pageCount(), uint32_t(65535)));
      libCurrent = static_cast<uint16_t>(std::min(currentPage_ + 1, uint32_t(65535)));
    }
    LibraryIndex::updateEntry(core, contentPath_, libCurrent, libTotal,
                              static_cast<uint8_t>(core.content.metadata().hint));

    // Skip pageCache_.reset() and content.close() — ESP.restart() follows,
    // and if stopBackgroundCaching() timed out the task still uses them.
  }

  // Determine return destination from cached transition or fall back to sourceState_
  ReturnTo returnTo = ReturnTo::HOME;
  const auto& transition = getTransition();
  if (transition.isValid()) {
    returnTo = transition.returnTo;
  } else if (sourceState_ == StateId::FileList) {
    returnTo = ReturnTo::FILE_MANAGER;
  }

  // Show notification and restart
  showTransitionNotification("Returning to library...");
  saveTransition(BootMode::UI, nullptr, returnTo);

  // Brief delay to ensure SD writes complete before restart
  vTaskDelay(50 / portTICK_PERIOD_MS);
  ESP.restart();
}

}  // namespace sumi
