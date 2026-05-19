#include "ReaderState.h"

#include <Arduino.h>
#include <ComicReader.h>
#include <ContentParser.h>
#include <CoverHelpers.h>
#include <Dictionary.h>
#include <Epub/Page.h>
#include <Epub/blocks/ImageBlock.h>
#include <EpubChapterParser.h>
#include <GfxRenderer.h>
#include <LookupHistory.h>
#include <MarkdownParser.h>
#include <PageCache.h>
#include <ReadingStats.h>
#include <Utf8.h>
#include <PlainTextParser.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "../config.h"
#include "../core/MemoryArena.h"
#if FEATURE_BLUETOOTH
#include "../ble/BleHid.h"
#endif

#include "../content/GlobalBookmarkIndex.h"

#include <cstring>
#include <algorithm>

#include "../Battery.h"
#include "../FontManager.h"
#include "../config.h"
#include "../content/ProgressManager.h"
#include "../content/LibraryIndex.h"
#include "../content/ReaderNavigation.h"
#include "../content/RecentBooks.h"
#include "../core/Core.h"
#include "../core/InputDrainGuard.h"
#include "../ui/Elements.h"
#include "ThemeManager.h"

// Global reading stats instance (defined in main.cpp)
extern sumi::ReadingStats readingStats;

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
    SdMan.syncAndClose(file);
    return;
  }
  uint16_t count = static_cast<uint16_t>(anchors.size());
  serialization::writePod(file, count);
  for (const auto& entry : anchors) {
    serialization::writeString(file, entry.first);
    serialization::writePod(file, entry.second);
  }
  SdMan.syncAndClose(file);
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

  // Sanity cap: a corrupted .anchors header with count=65535 would
  // otherwise drive 65535 readString()+emplace() loop iterations which
  // could OOM the 380KB-heap ESP32-C3. Real EPUBs rarely exceed ~500
  // anchors per chapter; 4096 is a safe ceiling that catches garbage
  // without rejecting any plausible content.
  constexpr uint16_t kMaxAnchors = 4096;
  if (count > kMaxAnchors) {
    Serial.printf("[READER] Anchor count %u exceeds max %u, discarding\n", count, kMaxAnchors);
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
    // nothrow + null check (matches loadCacheFromDisk): raw `new` would
    // abort the device on heap exhaustion. The block below null-checks
    // `pageCache_` before dereferencing, so a null result is safe.
    auto* pc = new (std::nothrow) PageCache(cachePath);
    if (!pc) {
      Serial.println("[READER] PageCache alloc failed in createOrExtendCacheImpl");
      return;
    }
    pageCache_.reset(pc);
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

  // Create/load cache (we own pageCache_ while task is running). nothrow:
  // raw `new` would abort the background task with no chance for the
  // main thread to recover the reader.
  auto* pc = new (std::nothrow) PageCache(cachePath);
  if (!pc) {
    Serial.println("[READER] PageCache alloc failed in backgroundCacheImpl");
    return;
  }
  pageCache_.reset(pc);
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
    utf8SafeCopy(contentPath_, path, sizeof(contentPath_));
  } else {
    contentPath_[0] = '\0';
  }
}

void ReaderState::enter(Core& core) {
  // Drain queued button events so a power-button wake doesn't page-turn
  InputDrainGuard::drain(core);

  // Free memory from other states before loading book
  THEME_MANAGER.clearCache();
  renderer_.clearWidthCache();

  // Reader manages its own refresh counter, disable the global one
  renderer_.setPeriodicRefreshInterval(0);

  contentLoaded_ = false;
  loadFailed_ = false;
  needsRender_ = true;
  tocMode_ = false;
  settingsMode_ = false;
  bookmarkListMode_ = false;
  globalBookmarkMode_ = false;
  centerLongPressFired_ = false;
  enterTime_ = millis();
  pagesUntilFullRefresh_ = 1;  // Use HALF_REFRESH on first render (FULL causes 5 flashes)
  stopBackgroundCaching();  // Ensure any previous task is stopped
  parser_.reset();          // Safe - task is stopped
  parserSpineIndex_ = -1;
  pageCache_.reset();
  currentSpineIndex_ = 0;
  currentSectionPage_ = 0;  // Will be set to -1 after progress load if at start

  // Read path from shared buffer if not already set
  if (contentPath_[0] == '\0' && core.buf.path[0] != '\0') {
    utf8SafeCopy(contentPath_, core.buf.path, sizeof(contentPath_));
    core.buf.path[0] = '\0';
  }

  // Determine source state (where to return on exit)
  sourceState_ = (core.settings.transitionReturnTo == 1) ? StateId::FileList : StateId::Home;

  Serial.printf("[READER] Entering with path: %s\n", contentPath_);

  // Crash resilience: record that we are attempting to open a book.
  // If the device reboots before render() clears this, the next boot
  // will skip straight to Home instead of retrying the same crash.
  core.settings.readerLoadAttempts++;
  {
    SdMan.mkdir(SUMI_DIR);
    FsFile guard;
    if (SdMan.openFileForWrite("RDR", SUMI_DIR "/reader_guard.bin", guard)) {
      guard.write(&core.settings.readerLoadAttempts, 1);
      SdMan.syncAndClose(guard);
    }
  }

  if (contentPath_[0] == '\0') {
    Serial.println("[READER] No content path set");
    return;
  }

  // Start reading statistics session
  readingStats.startSession(ReadingStats::hashPath(contentPath_));

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

  // (v1 aliased the framebuffer as a ZIP-dictionary fallback for when
  //  the arena was released for BLE; v2 never releases the arena, so
  //  the alias is gone — Epub.cpp always uses MemoryArena::zipBuffer.)

  // Open content using ContentHandle. ContentHandle::open() and the
  // EPUB parser inside it use new (std::nothrow) plus Result<void>
  // returns for OOM, so we get an Error::OutOfMemory back without an
  // exception. With -fexceptions OFF (see platformio.ini), any rogue
  // throwing-`new` deeper in the stack would abort the chip, and the
  // boot-loop guard handles that on the next boot. The previous
  // try/catch around this call was dead code under -fno-exceptions.
  // Audit #2.
  Result<void> result = core.content.open(contentPath_, SUMI_CACHE_DIR);

  if (!result.ok()) {
    Serial.printf("[READER] Failed to open content: %s\n", errorToString(result.err));
    // Store error message for ErrorState to display
    snprintf(core.buf.text, sizeof(core.buf.text), "Cannot open file:\n%s", errorToString(result.err));
    loadFailed_ = true;  // Mark as failed for update() to transition to error state
    return;
  }

  contentLoaded_ = true;

  // Save last book path to settings. utf8SafeCopy null-terminates at the
  // last full codepoint boundary so CJK book filenames survive saves.
  utf8SafeCopy(core.settings.lastBookPath, contentPath_, sizeof(core.settings.lastBookPath));
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
  const ContentType contentType = core.content.metadata().type;
  landscapeScroll_ = (contentType == ContentType::Comic ||
                      hint == ContentHint::Comic ||
                      hint == ContentHint::ComicRtl ||
                      hint == ContentHint::ComicWebtoon ||
                      hint == ContentHint::BookScanned);
  if (landscapeScroll_) {
    renderer_.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
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

  // Background caching is deferred to the first render() call.
  // Starting it here would compete with cover rendering for heap memory,
  // causing the parser to abort with "Insufficient memory for layout".

  // Drain stale button events that queued during content loading.
  // The center button used to open the book may still be held, generating
  // ButtonLongPress/ButtonRelease events that would trigger settings/TOC overlay.
  {
    Event drain;
    while (core.events.pop(drain)) {
      // Keep system events (battery, USB), discard button events
      if (drain.type != EventType::ButtonPress &&
          drain.type != EventType::ButtonLongPress &&
          drain.type != EventType::ButtonRelease) {
        core.events.push(drain);
      }
    }
  }
  enterTime_ = millis();
}

void ReaderState::exit(Core& core) {
  Serial.println("[READER] Exiting");

  // Clean reader exit — clear the crash guard so next boot resumes
  // normally. This is the real "reader survived" signal (not first-page
  // render, which happens before the background cache task can crash).
  if (core.settings.readerLoadAttempts > 0) {
    core.settings.readerLoadAttempts = 0;
    SdMan.remove(SUMI_DIR "/reader_guard.bin");
  }

  // End reading statistics session (saves accumulated time to SD)
  readingStats.endSession();

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

    // Persist thumbnail path so home screen never loses it
    std::string thumbPath = core.content.getThumbnailPath();
    if (!thumbPath.empty()) {
      RecentBooks::updateThumbPath(core, contentPath_, thumbPath.c_str());
    }

    // Safe to reset - task is stopped, we own pageCache_/parser_
    parser_.reset();
    parserSpineIndex_ = -1;
    pageCache_.reset();
    core.content.close();
  }

  // (v1 cleared MemoryArena::fallbackBuffer here. v2 has no
  //  fallbackBuffer; nothing to clear.)

  // Unload custom reader fonts to free memory
  // Explicit cleanup ensures predictable memory behavior and better logging
  FONT_MANAGER.unloadReaderFonts();

  contentLoaded_ = false;
  contentPath_[0] = '\0';
  landscapeScroll_ = false;
  scrollY_ = 0;
  pageContentHeight_ = 0;

  // Reset orientation to Portrait for UI
  renderer_.setOrientation(GfxRenderer::Orientation::Portrait);

  // Re-enable periodic refresh for non-reader states
  renderer_.setPeriodicRefreshInterval(core.settings.getPagesPerRefreshValue());
  renderer_.resetPeriodicRefreshCounter();
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
    // Any manual input resets auto page turn timer
    autoTurnLastMs_ = 0;

    // Route input to overlay handlers
    if (dictStage_ != DictStage::None) {
      handleDictInput(core, e);
      continue;
    }
    if (bookmarkListMode_) {
      handleBookmarkListInput(core, e);
      continue;
    }
    if (globalBookmarkMode_) {
      handleGlobalBookmarkInput(core, e);
      continue;
    }
    if (historyMode_) {
      handleHistoryInput(core, e);
      continue;
    }
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
          // Landscape scroll: Up/Down scroll within page, Left/Right navigate pages.
          // When sideButtonLayout == NextPrev, the physical top button (Up)
          // means "next", so Up goes FORWARD through the content and Down
          // goes BACKWARD.
          const int screenH = renderer_.getScreenHeight();
          const int maxScroll = pageContentHeight_ - screenH;
          const int scrollStep = (maxScroll > 0) ? ((maxScroll + 1) / 2) : screenH;
          const bool sideNextPrev = core.settings.sideButtonLayout == Settings::NextPrev;
          const auto scrollForward = [&]() {
            if (maxScroll > 0 && scrollY_ < maxScroll) {
              scrollY_ = std::min(scrollY_ + scrollStep, maxScroll);
              needsRender_ = true;
            } else {
              scrollY_ = 0;
              pageContentHeight_ = 0;
              navigateNext(core);
            }
          };
          const auto scrollBackward = [&]() {
            if (scrollY_ > 0) {
              scrollY_ = std::max(0, scrollY_ - scrollStep);
              needsRender_ = true;
            } else {
              scrollY_ = 0;
              pageContentHeight_ = 0;
              navigatePrev(core);
            }
          };
          switch (e.button) {
            case Button::Left:
              scrollForward();
              break;
            case Button::Right:
              scrollBackward();
              break;
            case Button::Down:
              if (sideNextPrev) scrollBackward();
              else              scrollForward();
              break;
            case Button::Up:
              if (sideNextPrev) scrollForward();
              else              scrollBackward();
              break;
            case Button::Center:
              // Defer to ButtonRelease — allows long-press to open settings first
              break;
            case Button::Back:
              exitToUI(core);
              return StateTransition::to(exitTarget_);
            case Button::Power:
              if (core.settings.shortPwrBtn == Settings::PowerPageTurn) {
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
              } else {
                // Default (PowerIgnore): open in-reader settings overlay.
                enterSettingsMode(core);
              }
              break;
          }
        } else {
        // Normal portrait mode. Left/Right are fixed (L=prev, R=next).
        // Up/Down honour sideButtonLayout so "top button → next page"
        // works inside the reader without disturbing menu navigation
        // anywhere else in the UI.
        const bool sideNextPrev = core.settings.sideButtonLayout == Settings::NextPrev;
        switch (e.button) {
          case Button::Right:
            navigateNext(core);
            break;

          case Button::Down:
            if (sideNextPrev) navigatePrev(core);
            else              navigateNext(core);
            break;

          case Button::Left:
            navigatePrev(core);
            break;

          case Button::Up:
            if (sideNextPrev) navigateNext(core);
            else              navigatePrev(core);
            break;

          case Button::Center:
            // Defer to ButtonRelease — allows long-press to open settings first
            break;
          case Button::Back:
            exitToUI(core);
            return StateTransition::to(exitTarget_);
          case Button::Power:
            if (core.settings.shortPwrBtn == Settings::PowerPageTurn) {
              navigateNext(core);
            } else if (core.settings.shortPwrBtn == Settings::PowerRefresh) {
              // Manual screen refresh — clear ghosting
              renderer_.displayBuffer(EInkDisplay::FULL_REFRESH);
            } else {
              // Default (PowerIgnore): open in-reader settings overlay.
              enterSettingsMode(core);
            }
            break;
        }
        }
        break;

      case EventType::ButtonLongPress:
        if (e.button == Button::Center && millis() - enterTime_ > 1500) {
          // Guard: ignore stale long-press from button that opened the book.
          // Event queue is drained in enter(), but InputManager may still fire
          // a new long-press if center is held during the first render cycle.
          centerLongPressFired_ = true;
          enterSettingsMode(core);
        }
        break;

      case EventType::ButtonRelease:
        if (e.button == Button::Center) {
          if (centerLongPressFired_) {
            // Long press already handled — suppress short-press action
            centerLongPressFired_ = false;
          } else if (!settingsMode_ && !tocMode_ && dictStage_ == DictStage::None &&
                     millis() - enterTime_ > 1500) {
            // Short press Center: open TOC (guard prevents stale release from book-open button)
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
      bool isNext = false, isPrev = false;
      switch (bk) {
        case BleKey::PAGE_NEXT:
        case BleKey::KEY_RIGHT:
        case BleKey::KEY_DOWN:
        case BleKey::KEY_CHAR:
          isNext = true;
          break;
        case BleKey::PAGE_PREV:
        case BleKey::KEY_LEFT:
        case BleKey::KEY_UP:
          isPrev = true;
          break;
        default:
          break;
      }
      if (isNext || isPrev) {
        if (landscapeScroll_) {
          // Scroll within page before turning, same as physical buttons
          const int screenH = renderer_.getScreenHeight();
          const int maxScroll = pageContentHeight_ - screenH;
          const int scrollStep = (maxScroll > 0) ? ((maxScroll + 1) / 2) : screenH;
          if (isNext) {
            if (maxScroll > 0 && scrollY_ < maxScroll) {
              scrollY_ = std::min(scrollY_ + scrollStep, maxScroll);
              needsRender_ = true;
            } else {
              scrollY_ = 0;
              pageContentHeight_ = 0;
              navigateNext(core);
            }
          } else {
            if (scrollY_ > 0) {
              scrollY_ = std::max(0, scrollY_ - scrollStep);
              needsRender_ = true;
            } else {
              scrollY_ = 0;
              pageContentHeight_ = 0;
              navigatePrev(core);
            }
          }
        } else {
          if (isNext) navigateNext(core);
          else navigatePrev(core);
        }
      }
    }
  }
#endif

  // Auto page turn (CrossPoint #1219): advance one page on timer expiry.
  // Reset timer on any manual input (handled by event loop setting autoTurnLastMs_=0).
  const uint32_t autoInterval = core.settings.getAutoPageTurnMs();
  if (autoInterval > 0 && !settingsMode_ && !tocMode_ && !bookmarkListMode_ && !globalBookmarkMode_ && dictStage_ == DictStage::None) {
    const uint32_t now = millis();
    if (autoTurnLastMs_ == 0) {
      autoTurnLastMs_ = now;  // Initialize on first check
    } else if (now - autoTurnLastMs_ >= autoInterval) {
      navigateNext(core);
      autoTurnLastMs_ = now;
    }
  } else {
    autoTurnLastMs_ = 0;
  }

  return StateTransition::stay(StateId::Reader);
}

void ReaderState::render(Core& core) {
  if (!needsRender_ || !contentLoaded_) {
    return;
  }

  // Stop the bg cache task before touching the renderer. Two reasons:
  // (1) CONCURRENCY.md C1: GfxRenderer's wordWidthCache is single-owner;
  //     calling clearScreen/drawText/getTextWidth from main while the bg
  //     task is alive races on that cache. The renderer logs warnings
  //     (rate-limited) but doesn't actually serialize the access.
  // (2) Peak heap: with BLE active the heap is ~20 KB free; if both the
  //     bg parser and the main-task render allocate concurrently, the
  //     parser's safety threshold (4 KB) trips inside startElement and
  //     the cache extend fails — user gets stuck at the last cached
  //     page with no way past. Empirically reproduced via [HP] log
  //     timeline. Resume bg caching at the bottom of render() — by then
  //     the heavy allocation is over and the bg task has room to work.
  stopBackgroundCaching();

  // Note: renderCurrentPage allocates during layout (vectors, strings,
  // hash nodes inside parser_). With -fexceptions OFF (see platformio.ini
  // — keeping it off saves ~4 KB of LwIP DRAM), a failed `new` calls
  // std::terminate() → abort. The previous version of this code wrapped
  // the dispatch in a try/catch, but the catch handlers were dead code
  // — uncaught throws can't unwind to a handler when exceptions are
  // disabled. The crash recovery story is now solely: abort →
  // bootloader → next boot increments core.settings.readerLoadAttempts
  // → boot-loop guard skips reader auto-resume after 3 attempts.
  // Audit #2.

  if (dictStage_ != DictStage::None) {
    renderDictOverlay(core);
  } else if (bookmarkListMode_) {
    if (bookmarkView_.needsRender) {
      ui::render(renderer_, THEME_MANAGER.current(), bookmarkView_);
      bookmarkView_.needsRender = false;
    }
  } else if (globalBookmarkMode_) {
    if (globalBookmarkView_.needsRender) {
      ui::render(renderer_, THEME_MANAGER.current(), globalBookmarkView_);
      globalBookmarkView_.needsRender = false;
    }
  } else if (historyMode_) {
    if (historyView_.needsRender) {
      ui::render(renderer_, THEME_MANAGER.current(), historyView_);
      historyView_.needsRender = false;
    }
  } else if (settingsMode_) {
    renderSettingsOverlay(core);
  } else if (tocMode_) {
    renderTocOverlay(core);
  } else {
    renderCurrentPage(core);
    // Track last successfully rendered position for progress saving
    lastRenderedSpineIndex_ = currentSpineIndex_;
    lastRenderedSectionPage_ = currentSectionPage_;

    // Reset guard timer after first render completes. Cover rendering can take 2+ seconds,
    // during which InputManager may fire new long-press events. Without this reset, the
    // 1500ms guard set in enter() expires during render and those events trigger settings/TOC.
    enterTime_ = millis();

    // [HP] post-render heap timeline (diagnostic for BLE page-turner crashes).
    // Pairs with the "[HP] nav" line in applyNavResult to bracket the render's
    // heap delta; if largest drops sharply between nav and render, the render
    // path is the leak/fragmenter.
#if FEATURE_BLUETOOTH
    const int bleOn = ble::isConnected() ? 1 : 0;
#else
    const int bleOn = 0;
#endif
    Serial.printf("[HP] render: spine=%d page=%d flat=%u free=%lu largest=%lu ble=%d\n",
                  currentSpineIndex_, currentSectionPage_, (unsigned)currentPage_,
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMaxAllocHeap(), bleOn);

    // Crash resilience: DO NOT clear reader_guard.bin here. First-page
    // render is not a sufficient signal of reader health — the background
    // page cache task can still crash (seen with War and Peace: cover
    // renders, then cache task std::bad_alloc aborts, boot loop). Guard
    // is now cleared only on clean reader exit() or on the first user-
    // initiated navigation (navigateNext/navigatePrev).
  }

  needsRender_ = false;

  // Resume the bg cache task now that the main-task render is done. This
  // is the paired restart for the stopBackgroundCaching() at the top of
  // render(). The bg task will pick up cache extension if needed.
  startBackgroundCaching(core);
}

void ReaderState::navigateNext(Core& core) {
  // First user navigation is proof the reader is interactive — clear the
  // crash guard so cover + cache init don't keep the guard set forever.
  // If the device can render cover AND accept a button press, it's alive.
  if (core.settings.readerLoadAttempts > 0) {
    core.settings.readerLoadAttempts = 0;
    SdMan.remove(SUMI_DIR "/reader_guard.bin");
  }

  // Stop background task before accessing pageCache_ (ownership model)
  stopBackgroundCaching();

  ContentType type = core.content.metadata().type;

  // XTC/Comic uses flatPage navigation, not spine/section
  if (type == ContentType::Xtc || type == ContentType::Comic) {
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
    readingStats.recordPageTurn();
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

  // XTC/Comic uses flatPage navigation, not spine/section
  if (type == ContentType::Xtc || type == ContentType::Comic) {
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
  if (result.needsRender) {
    readingStats.recordPageTurn();
  }
  scrollY_ = 0;  // Reset scroll for new page
  pageContentHeight_ = 0;
  if (result.needsCacheReset) {
    parser_.reset();  // Safe - task already stopped by caller
    parserSpineIndex_ = -1;
    pageCache_.reset();
  }
  // If a render is coming, defer cache-task restart until after render()
  // completes — otherwise the bg task and the upcoming main-task render
  // contend for heap, blowing the parser's safety threshold under BLE
  // pressure (CONCURRENCY.md C1). render() will resume caching at its end.
  if (!needsRender_) {
    startBackgroundCaching(core);
  }

  // [HP] page-turn heap timeline (diagnostic for BLE page-turner crashes).
  // Grep "[HP] nav" in serial log to see free vs. largest-free across turns;
  // a falling largest with stable free = fragmentation, falling free = leak.
#if FEATURE_BLUETOOTH
  const int bleOn = ble::isConnected() ? 1 : 0;
#else
  const int bleOn = 0;
#endif
  Serial.printf("[HP] nav: spine=%d page=%d flat=%u free=%lu largest=%lu ble=%d\n",
                currentSpineIndex_, currentSectionPage_, (unsigned)currentPage_,
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMaxAllocHeap(), bleOn);
}

void ReaderState::renderCurrentPage(Core& core) {
  ContentType type = core.content.metadata().type;
  const Theme& theme = THEME_MANAGER.current();

  // Set image placeholder mode based on user setting
  ImageBlock::placeholderMode = (core.settings.showImages == sumi::Settings::ImagePlaceholder);

  // Always clear screen first (prevents previous content from showing through)
  renderer_.clearScreen(theme.backgroundColor);

  // Cover page: spineIndex=0, sectionPage=-1 (only when showImages enabled)
  if (currentSpineIndex_ == 0 && currentSectionPage_ == -1) {
    if (core.settings.showImages) {
      if (renderCoverPage(core)) {
        hasCover_ = true;
        // Start background caching now that cover render is done and heap is free
        if (!cacheTask_.isRunning()) {
          startBackgroundCaching(core);
        }
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
    case ContentType::Comic:
      renderComicPage(core);
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

      // Clear overlay and force a clean refresh on the next page render
      renderer_.clearScreen(theme.backgroundColor);
      pagesUntilFullRefresh_ = 1;  // HALF_REFRESH to clear indexing text ghosting
    }

  }

  // Clamp page number (handle negative values and out-of-bounds)
  // Must run outside if(!pageCache_) — background task may have already loaded cache
  if (pageCache_) {
    const int cachedPages = static_cast<int>(pageCache_->pageCount());
    if (currentSectionPage_ < 0) {
      currentSectionPage_ = 0;
    } else if (currentSectionPage_ >= cachedPages) {
      currentSectionPage_ = cachedPages > 0 ? cachedPages - 1 : 0;
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
    // Also skip when textDarkness >= 2 (Extra Dark/Maximum): all gray pixels are
    // already rendered as black in BW mode, so the grayscale pass has nothing to add.
    if (core.settings.textAntiAliasing && core.settings.textDarkness < 2 &&
        !FONT_MANAGER.isUsingCustomReaderFont() &&
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
      const size_t freeTotal = ESP.getFreeHeap();
      if (freeTotal >= 12288) {
        Serial.printf("[READER] Pre-extending cache at page %d\n", pageNum);
        createOrExtendCache(core);
      } else {
        Serial.printf("[READER] Deferring pre-extend — low memory (%zu bytes)\n", freeTotal);
      }
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

  // Caller must have stopped background task (we own pageCache_).
  // nothrow + null check: raw `new` would abort the device under
  // heap pressure; the rest of the function null-checks pageCache_,
  // so a null result is safe as long as we don't dereference here.
  if (!pageCache_) {
    auto* pc = new (std::nothrow) PageCache(cachePath);
    if (!pc) {
      Serial.println("[READER] PageCache alloc failed in loadCacheFromDisk");
      return;
    }
    pageCache_.reset(pc);
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

    // Create parser if we don't have one (or if spine changed). nothrow
    // matches the background-cache-task variant below — raw `new` would
    // abort the device on heap exhaustion mid-page-render. Early return
    // means `createOrExtendCacheImpl(*parser_, ...)` is never called
    // with a null parser.
    if (!parser_ || parserSpineIndex_ != currentSpineIndex_) {
      std::string imageCachePath = core.settings.showImages ? (epub->getCachePath() + "/images") : "";
      auto* p = new (std::nothrow) EpubChapterParser(epub, currentSpineIndex_, renderer_, config, imageCachePath);
      if (!p) {
        Serial.println("[READER] EpubChapterParser alloc failed; skipping cache create/extend");
        return;
      }
      parser_.reset(p);
      parserSpineIndex_ = currentSpineIndex_;
    }
  } else if (type == ContentType::Markdown) {
    cachePath = contentCachePath(core.content.cacheDir(), config.fontId);
    if (!parser_) {
      auto* p = new (std::nothrow) MarkdownParser(contentPath_, renderer_, config);
      if (!p) {
        Serial.println("[READER] MarkdownParser alloc failed; skipping cache create/extend");
        return;
      }
      parser_.reset(p);
      parserSpineIndex_ = 0;
    }
  } else {
    cachePath = contentCachePath(core.content.cacheDir(), config.fontId);
    if (!parser_) {
      auto* p = new (std::nothrow) PlainTextParser(contentPath_, renderer_, config);
      if (!p) {
        Serial.println("[READER] PlainTextParser alloc failed; skipping cache create/extend");
        return;
      }
      parser_.reset(p);
      parserSpineIndex_ = 0;
    }
  }

  // (v1 used to releasePrimary() here when heap was tight or BLE was
  //  active — the comment claimed the parser would use the framebuffer
  //  as the ZIP dict via `fallbackBuffer`. In reality Epub.cpp always
  //  used `zipBuffer` (the primary alias) and the fallback branch was
  //  dead code. v2 keeps the arena permanently in .bss — no
  //  release/reclaim possible, none needed.)
  if (!parser_) return;  // shouldn't happen given the early returns above, defense-in-depth

  // Temporarily release the external (CJK fallback) font during the
  // parse. With BLE active, ~20 KB free heap is the entire budget for
  // the parser, and the external font glyph cache eats several KB
  // that the parser needs to reach the end of a long chapter. The
  // parser doesn't render glyphs — it only measures widths via
  // getTextWidth — and the built-in font handles ASCII fine; CJK
  // widths fall back to the built-in font's missing-glyph metrics
  // for the duration of the parse. Reload immediately after so the
  // next page render has the font back.
  ExternalFont* extFontPtr = FONT_MANAGER.getExternalFont();
  char savedExtFont[32] = {0};
  if (extFontPtr) {
    utf8SafeCopy(savedExtFont, core.settings.readerFont, sizeof(savedExtFont));
    Serial.printf("[READER] Releasing external font '%s' for parse (free=%lu before)\n",
                  savedExtFont, (unsigned long)ESP.getFreeHeap());
    FONT_MANAGER.unloadExternalFont();
    Serial.printf("[READER] External font released (free=%lu after)\n",
                  (unsigned long)ESP.getFreeHeap());
  }

  createOrExtendCacheImpl(*parser_, cachePath, config);

  if (savedExtFont[0] != '\0') {
    Serial.printf("[READER] Reloading external font '%s' after parse\n", savedExtFont);
    FONT_MANAGER.loadExternalFont(savedExtFont);
  }
}

void ReaderState::renderPageContents(Core& core, Page& page, int marginTop, int marginRight, int marginBottom,
                                     int marginLeft) {
  (void)marginRight;
  (void)marginBottom;

  // Detect large images for refresh mode selection (CrossPoint: double FAST_REFRESH fix).
  // Large dithered images wash out on single FAST_REFRESH — e-ink particles don't settle.
  pageHasLargeImage_ = false;
  const int halfViewport = renderer_.getScreenHeight() / 3;
  for (const auto& el : page.elements) {
    if (el->getTag() == TAG_PageImage) {
      if (static_cast<const PageImage*>(el.get())->getImageHeight() > halfViewport) {
        pageHasLargeImage_ = true;
        break;
      }
    }
  }

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
  data.fieldMask = core.settings.statusBarFields;
  data.title = core.content.metadata().title;

  // Battery
  const uint16_t millivolts = batteryMonitor.readMillivolts();
  data.batteryPercent = (millivolts < 100) ? -1 : BatteryMonitor::percentageFromMillivolts(millivolts);

#if FEATURE_BLUETOOTH
  data.bleConnected = ble::isReady() && ble::isConnected();
#endif

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

void ReaderState::renderComicPage(Core& core) {
  auto* provider = core.content.asComic();
  if (!provider) return;

  const Theme& theme = THEME_MANAGER.current();
  auto& reader = provider->getReader();

  if (currentPage_ >= reader.pageCount()) {
    ui::centeredMessage(renderer_, theme, theme.uiFontId, "End of book");
    displayWithRefresh(core);
    return;
  }

  // Set content height for scroll logic
  pageContentHeight_ = reader.pageHeight(currentPage_);
  const int screenH = renderer_.getScreenHeight();
  const int screenW = renderer_.getScreenWidth();

  // Clear screen
  renderer_.clearScreen(theme.backgroundColor);

  // Render BW pass
  renderer_.setRenderMode(GfxRenderer::BW);
  if (!reader.renderPage(currentPage_, renderer_, scrollY_)) {
    ui::centeredMessage(renderer_, theme, theme.uiFontId, "Page load error");
    displayWithRefresh(core);
    return;
  }

  // Draw scroll indicator if content extends beyond viewport
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

  // Show page number
  char pageStr[16];
  snprintf(pageStr, sizeof(pageStr), "%d/%d", currentPage_ + 1, reader.pageCount());
  const int tw = renderer_.getTextWidth(theme.smallFontId, pageStr);
  renderer_.drawText(theme.smallFontId, screenW - tw - 8, screenH - 12, pageStr, true);

  displayWithRefresh(core);

  // Grayscale rendering for 4-level gray (2-bit comics)
  if (renderer_.storeBwBuffer()) {
    // LSB pass (dark gray)
    renderer_.clearScreen(0x00);
    renderer_.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    reader.renderPage(currentPage_, renderer_, scrollY_);
    renderer_.copyGrayscaleLsbBuffers();

    // MSB pass (light gray)
    renderer_.clearScreen(0x00);
    renderer_.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    reader.renderPage(currentPage_, renderer_, scrollY_);
    renderer_.copyGrayscaleMsbBuffers();

    const bool turnOffScreen = core.settings.sunlightFadingFix != 0;
    renderer_.displayGrayBuffer(turnOffScreen);
    renderer_.restoreBwBuffer();
    renderer_.setRenderMode(GfxRenderer::BW);
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
  } else if (pageHasLargeImage_) {
    // Large dithered images wash out on single FAST_REFRESH — use HALF for better quality
    renderer_.displayBuffer(EInkDisplay::HALF_REFRESH, turnOffScreen);
    pagesUntilFullRefresh_--;
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
  // XTC/Comic content uses pre-rendered bitmaps — no page cache or thumbnail support
  if (core.content.metadata().type == ContentType::Xtc ||
      core.content.metadata().type == ContentType::Comic) {
    thumbnailDone_ = true;
    return;
  }

  // BackgroundTask handles safe restart via CAS loop
  if (cacheTask_.isRunning()) {
    Serial.println("[READER] Warning: Previous cache task still running, stopping first");
    stopBackgroundCaching();
  }

  // Skip background task if heap is too low for the 24KB task stack + parser memory.
  // When BLE is connected, arena task stack is unavailable and heap is fragmented.
  // Need ~24KB stack + ~20KB parser working memory = ~44KB minimum.
  if (!sumi::MemoryArena::taskStackRegion && ESP.getFreeHeap() < 50000) {
    Serial.printf("[READER] Skipping background cache — low heap (%zu bytes)\n", ESP.getFreeHeap());
    return;
  }

  Serial.println("[READER] Starting background page cache task");
  coreForCacheTask_ = &core;

  // Snapshot state for the background task
  const int sectionPage = currentSectionPage_;
  const int spineIndex = currentSpineIndex_;
  const bool coverExists = hasCover_;
  const int textStart = textStartIndex_;
  const bool isLandscapeScroll = landscapeScroll_;

  auto taskBody = [this, sectionPage, spineIndex, coverExists, textStart, isLandscapeScroll]() {
        const Theme& theme = THEME_MANAGER.current();
        Serial.println("[READER] Background cache task started");

        // OOM story for the cache task. -fexceptions is off (see
        // platformio.ini) — keeping it off saves ~4 KB of LwIP DRAM. So:
        //   * The `new (std::nothrow) ...Parser(...)` calls below return
        //     nullptr on alloc failure; we check and bail cleanly,
        //     dropping the partial cache.
        //   * Allocations deeper inside the parsers (std::vector,
        //     std::string, hash nodes) still throw std::bad_alloc on
        //     failure. With -fno-exceptions, an uncaught throw calls
        //     std::terminate → abort. Boot-loop guard at next boot
        //     increments core.settings.readerLoadAttempts and skips
        //     reader auto-resume after 3 attempts. War-and-Peace-class
        //     parse failures land here.
        // The previous version of this code wrapped the body in a
        // try/catch, but the catch handlers couldn't fire under
        // -fno-exceptions. Audit #2.

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
                auto* p = new (std::nothrow) EpubChapterParser(
                    provider->getEpubShared(), spineToCache, renderer_,
                    config, imageCachePath);
                if (!p) {
                  Serial.println("[READER] EpubChapterParser alloc failed; "
                                 "dropping partial cache");
                  pageCache_.reset();
                  parser_.reset();
                  return;
                }
                parser_.reset(p);
                parserSpineIndex_ = spineToCache;
              }
            }
          } else if (type == ContentType::Markdown && !cacheTask_.shouldStop()) {
            cachePath = contentCachePath(coreRef.content.cacheDir(), config.fontId);
            if (!parser_) {
              auto* p = new (std::nothrow) MarkdownParser(contentPath_, renderer_, config);
              if (!p) {
                Serial.println("[READER] MarkdownParser alloc failed; "
                               "dropping partial cache");
                pageCache_.reset();
                parser_.reset();
                return;
              }
              parser_.reset(p);
              parserSpineIndex_ = 0;
            }
          } else if (type == ContentType::Txt && !cacheTask_.shouldStop()) {
            cachePath = contentCachePath(coreRef.content.cacheDir(), config.fontId);
            if (!parser_) {
              auto* p = new (std::nothrow) PlainTextParser(contentPath_, renderer_, config);
              if (!p) {
                Serial.println("[READER] PlainTextParser alloc failed; "
                               "dropping partial cache");
                pageCache_.reset();
                parser_.reset();
                return;
              }
              parser_.reset(p);
              parserSpineIndex_ = 0;
            }
          }

          if (parser_ && !cachePath.empty() && !cacheTask_.shouldStop()) {
            backgroundCacheImpl(*parser_, cachePath, config);
          }

          // Silent pre-indexing: after caching current chapter, pre-cache the NEXT chapter
          // so chapter transitions are instant (CrossPoint: Mar 2026).
          // Only for EPUB (multi-chapter); reuses the same task stack and arena buffers.
          if (type == ContentType::Epub && !cacheTask_.shouldStop() && pageCache_) {
            auto* provider = coreRef.content.asEpub();
            if (provider && provider->getEpub()) {
              const auto* epub = provider->getEpub();
              int nextSpine = spineIndex + 1;
              if (sectionPage == -1) {
                nextSpine = calcFirstContentSpine(coverExists, textStart, epub->getSpineItemsCount()) + 1;
              }
              if (nextSpine < epub->getSpineItemsCount()) {
                std::string nextCachePath = epubSectionCachePath(epub->getCachePath(), nextSpine);
                // Only pre-index if cache doesn't already exist
                if (!SdMan.exists(nextCachePath.c_str()) && !cacheTask_.shouldStop()) {
                  Serial.printf("[READER] Pre-indexing next chapter (spine %d)\n", nextSpine);
                  std::string imageCachePath = coreRef.settings.showImages ? (epub->getCachePath() + "/images") : "";
                  auto* p = new (std::nothrow) EpubChapterParser(
                      provider->getEpubShared(), nextSpine, renderer_, config, imageCachePath);
                  if (!p) {
                    Serial.println("[READER] Pre-index parser alloc failed; "
                                   "skipping next-chapter cache");
                  } else {
                    parser_.reset(p);
                    parserSpineIndex_ = nextSpine;
                    // Release current cache — we're done with it, and building a new one
                    pageCache_.reset();
                    backgroundCacheImpl(*parser_, nextCachePath, config);
                    // Don't keep the next chapter's cache in memory — it's saved to SD.
                    // When the user actually navigates there, it loads from SD instantly.
                    pageCache_.reset();
                  }
                }
              }
            }
          }
        }

        // Thumbnail generation moved to ReaderState::exit() to avoid buffer conflicts
        // with concurrent cover/page rendering

        if (!cacheTask_.shouldStop()) {
          Serial.println("[READER] Background cache task completed");
        } else {
          Serial.println("[READER] Background cache task stopped");
        }

        // Log stack high-water-mark so we can size the arena task stack
        // region accurately. On arduino-esp32's IDF build
        // uxTaskGetStackHighWaterMark returns BYTES (not words), despite
        // FreeRTOS spec saying words — so no *4. The previous *4 version
        // produced numbers larger than the stack and a uint-underflow peak.
        TaskHandle_t h = cacheTask_.getHandle();
        if (h) {
          UBaseType_t remainingBytes = uxTaskGetStackHighWaterMark(h);
          const unsigned total = (unsigned)sumi::MemoryArena::TASK_STACK_SIZE;
          const unsigned free = (unsigned)remainingBytes;
          const unsigned peak = (free <= total) ? (total - free) : 0;
          Serial.printf("[MEM] PageCache stack high-water: %u bytes free of %u (peak %u used)\n",
                        free, total, peak);
        }

        // Clear the published handle now that the bg task is exiting under
        // its own power. If we don't, `s_cacheTaskHandle_` stays pointing at
        // a freed task handle and the next main-task renderer call logs a
        // spurious "[GFX] BUG: clearScreen ... cacheTask is still active"
        // (CONCURRENCY.md C1) — false positive seen on Issue #23. The
        // stopBackgroundCaching() path also clears this, but it early-returns
        // when isRunning() is already false, so natural exit must clear here.
        GfxRenderer::s_cacheTaskHandle_ = nullptr;
  };

  // Use arena task stack to avoid heap fragmentation from 24KB xTaskCreate allocation
  if (sumi::MemoryArena::isInitialized() && sumi::MemoryArena::taskStackRegion) {
    cacheTask_.startStatic("PageCache", sumi::MemoryArena::taskStackRegion,
                           sumi::MemoryArena::TASK_STACK_SIZE, std::move(taskBody), 0);
  } else {
    cacheTask_.start("PageCache", kCacheTaskStackSize, std::move(taskBody), 0);
  }

  // Publish the cacheTask handle so GfxRenderer's concurrency check can
  // distinguish "main task forgot to stopBackgroundCaching" (warned) from
  // "cacheTask is calling renderer for measurement" (allowed). The
  // newly-spawned task may not have started running yet — that's fine,
  // the check only matters once both tasks coexist. CONCURRENCY.md C1.
  GfxRenderer::s_cacheTaskHandle_ = cacheTask_.getHandle();
}

void ReaderState::stopBackgroundCaching() {
  if (cacheTask_.isRunning()) {
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

  // Always clear the handle, even on the early-exit path. The bg task body
  // also clears it on natural exit, but a stale handle can survive (a) if
  // the task self-deleted between an earlier isRunning() check and now,
  // or (b) if we never observed the task's last moments before it cleared
  // its own handle. Either way, by the time stopBackgroundCaching() returns
  // there must be no bg task and no handle.
  GfxRenderer::s_cacheTaskHandle_ = nullptr;
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
          // Bound count before allocating — same cap as loadAnchorPage.
          constexpr uint16_t kMaxAnchors = 4096;
          if (count <= kMaxAnchors) {
            anchors.reserve(count);
            for (uint16_t j = 0; j < count; j++) {
              std::string id;
              uint16_t page;
              if (!serialization::readString(file, id) || !serialization::readPodChecked(file, page)) break;
              anchors.emplace_back(std::move(id), page);
            }
          } else {
            Serial.printf("[READER] Anchor count %u exceeds max %u, skipping\n", count, kMaxAnchors);
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
        pagesUntilFullRefresh_ = 1;  // HALF_REFRESH to clear indexing text ghosting
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

void ReaderState::handleHistoryInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) return;

  switch (e.button) {
    case Button::Up:
      historyView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      historyView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Center:
    case Button::Right: {
      // Re-look up the selected word
      const auto* word = historyView_.selected();
      if (word && !word->empty()) {
        historyMode_ = false;
        performDictLookup(core, *word);
      }
      break;
    }

    case Button::Left: {
      // Delete selected word from history
      const auto* word = historyView_.selected();
      const char* cacheDir = core.content.cacheDir();
      if (word && !word->empty() && cacheDir) {
        sumi::LookupHistory::removeWord(cacheDir, *word);
        // Reload
        historyView_.words = sumi::LookupHistory::load(cacheDir);
        if (historyView_.selectedIndex >= static_cast<int>(historyView_.words.size())) {
          historyView_.selectedIndex = std::max(0, static_cast<int>(historyView_.words.size()) - 1);
        }
        historyView_.ensureVisible();
        historyView_.needsRender = true;
        needsRender_ = true;
      }
      break;
    }

    case Button::Back:
      historyMode_ = false;
      settingsMode_ = true;  // return to in-reader settings
      settingsView_.needsRender = true;
      needsRender_ = true;
      break;

    default:
      break;
  }
}

// ============================================================================
// Bookmark List Overlay
// ============================================================================

void ReaderState::enterBookmarkList(Core& core) {
  const char* cacheDir = core.content.cacheDir();
  if (!cacheDir) return;
  bookmarkView_.pages = sumi::Bookmarks::load(cacheDir);
  bookmarkView_.totalBookPages = static_cast<int>(core.content.pageCount());
  bookmarkView_.selectedIndex = 0;
  bookmarkView_.scrollOffset = 0;
  bookmarkView_.needsRender = true;
  settingsMode_ = false;
  bookmarkListMode_ = true;
  needsRender_ = true;
  Serial.printf("[BMK] Opened bookmark list (%d bookmarks)\n", (int)bookmarkView_.pages.size());
}

void ReaderState::handleBookmarkListInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) return;

  switch (e.button) {
    case Button::Up:
      bookmarkView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      bookmarkView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Center:
    case Button::Right: {
      // Jump to bookmarked page
      uint32_t page = bookmarkView_.selectedPage();
      if (page > 0 || (!bookmarkView_.pages.empty() && bookmarkView_.pages[0] == 0)) {
        bookmarkListMode_ = false;
        // Apply the page via the same mechanism as TOC jump for flat-page formats,
        // or via spine navigation for EPUB
        ContentType type = core.content.metadata().type;
        if (type == ContentType::Xtc || type == ContentType::Comic) {
          currentPage_ = page;
        } else {
          // For EPUB/Txt/Markdown, flatPage maps to currentPage_
          currentPage_ = page;
          // Reset section state so renderCurrentPage re-resolves
          currentSectionPage_ = -1;
          currentSpineIndex_ = 0;
          parser_.reset();
          parserSpineIndex_ = -1;
          pageCache_.reset();
        }
        needsRender_ = true;
        Serial.printf("[BMK] Jumped to bookmarked page %u\n", page);
      }
      break;
    }

    case Button::Left: {
      // Delete bookmark
      uint32_t page = bookmarkView_.selectedPage();
      const char* cacheDir = core.content.cacheDir();
      if (cacheDir && !bookmarkView_.pages.empty()) {
        sumi::Bookmarks::toggle(cacheDir, page);
        sumi::GlobalBookmarkIndex::removeBookmark(contentPath_, page);
        bookmarkView_.pages = sumi::Bookmarks::load(cacheDir);
        if (bookmarkView_.selectedIndex >= static_cast<int>(bookmarkView_.pages.size())) {
          bookmarkView_.selectedIndex = std::max(0, static_cast<int>(bookmarkView_.pages.size()) - 1);
        }
        bookmarkView_.ensureVisible();
        bookmarkView_.needsRender = true;
        needsRender_ = true;
      }
      break;
    }

    case Button::Back:
      bookmarkListMode_ = false;
      settingsMode_ = true;
      settingsView_.needsRender = true;
      needsRender_ = true;
      break;

    default:
      break;
  }
}

// ============================================================================
// Global Bookmark Index Overlay
// ============================================================================

void ReaderState::enterGlobalBookmarkList(Core& core) {
  auto entries = sumi::GlobalBookmarkIndex::loadAll();
  globalBookmarkView_.entries.clear();
  globalBookmarkView_.entries.reserve(entries.size());
  for (const auto& e : entries) {
    ui::GlobalBookmarkListView::DisplayEntry de;
    memset(&de, 0, sizeof(de));
    utf8SafeCopy(de.bookTitle, e.bookTitle, sizeof(de.bookTitle));
    utf8SafeCopy(de.snippet, e.snippet, sizeof(de.snippet));
    de.page = e.page;
    globalBookmarkView_.entries.push_back(de);
  }
  globalBookmarkView_.selectedIndex = 0;
  globalBookmarkView_.scrollOffset = 0;
  globalBookmarkView_.needsRender = true;
  settingsMode_ = false;
  globalBookmarkMode_ = true;
  needsRender_ = true;
  Serial.printf("[GBI] Opened global bookmark list (%d entries)\n",
                (int)globalBookmarkView_.entries.size());
}

void ReaderState::handleGlobalBookmarkInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) return;

  switch (e.button) {
    case Button::Up:
      globalBookmarkView_.moveUp();
      needsRender_ = true;
      break;

    case Button::Down:
      globalBookmarkView_.moveDown();
      needsRender_ = true;
      break;

    case Button::Back:
      globalBookmarkMode_ = false;
      settingsMode_ = true;
      settingsView_.needsRender = true;
      needsRender_ = true;
      break;

    default:
      break;
  }
}

void ReaderState::handleSettingsInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) {
    return;
  }

  // Shared helper: when an Action-type row is active, dispatch by label
  // (since Action is a single enum variant used for both Bluetooth and
  // Look up Word). Returns true if an action was dispatched.
  auto tryAction = [&]() -> bool {
    const auto& def = settingsView_.DEFS[settingsView_.selected];
    if (def.type != ui::InReaderSettingsView::SettingType::Action) return false;
    if (def.label && def.label[0] == 'L') {
      // "Lookup History" (label[7]=='H') vs "Look up Word" (label[7]=='W')
      if (def.label[7] == 'H') {
        const char* cacheDir = core.content.cacheDir();
        if (cacheDir) {
          historyView_.words = sumi::LookupHistory::load(cacheDir);
          historyView_.selectedIndex = 0;
          historyView_.scrollOffset = 0;
          historyView_.needsRender = true;
          settingsMode_ = false;
          historyMode_ = true;
          needsRender_ = true;
        }
        return true;
      }
      enterDictWordSelect(core);
      return true;
    }
    if (def.label && def.label[0] == 'T' /* "Toggle Bookmark" */) {
      const char* cacheDir = core.content.cacheDir();
      if (cacheDir) {
        bool added = sumi::Bookmarks::toggle(cacheDir, currentPage_);
        Serial.printf("[BMK] %s bookmark at page %u\n", added ? "Added" : "Removed", currentPage_);
        // Sync to global bookmark index
        if (added) {
          const char* title = core.content.metadata().title;
          sumi::GlobalBookmarkIndex::addBookmark(contentPath_, title, currentPage_);
        } else {
          sumi::GlobalBookmarkIndex::removeBookmark(contentPath_, currentPage_);
        }
        loadInReaderSettings(core);
        settingsView_.needsRender = true;
        needsRender_ = true;
      }
      return true;
    }
    if (def.label && def.label[0] == 'V' /* "View Bookmarks" */) {
      enterBookmarkList(core);
      return true;
    }
    if (def.label && def.label[0] == 'A' /* "All Bookmarks" */) {
      enterGlobalBookmarkList(core);
      return true;
    }
#if FEATURE_BLUETOOTH
    handleBleAction(core);
    return true;
#else
    return false;
#endif
  };

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
      if (tryAction()) break;
      settingsView_.cycleValue(-1);
      applyInReaderSettings(core);
      needsRender_ = true;
      break;

    case Button::Right:
    case Button::Center:
      if (tryAction()) break;
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

  // Mark the view as XTC so reflow-related settings (font, size, line
  // spacing, hyphenation, alignment, show images) are hidden from the
  // overlay — those are baked into the XTC at process time and ignored
  // at read time. Comics are pre-rendered too but don't reach this
  // settings overlay (no chapter list / TOC path), so checking XTC
  // alone is enough here. Selected, scrollOffset, and the move helpers
  // are all isXtc-aware so navigation skips hidden rows cleanly.
  settingsView_.isXtc = (core.content.metadata().type == ContentType::Xtc);
  // If selected lands on a now-hidden row (e.g. user switched orientations
  // mid-session and re-entered), snap to the first visible entry.
  if (!settingsView_.isVisible(settingsView_.selected)) {
    settingsView_.selected = 0;  // "Look up Word" — always visible
    settingsView_.scrollOffset = 0;
  }

  // Index 0: Font (FontSelect) - load available .epdfont families
  settingsView_.fontCount = 0;
  settingsView_.currentFontIndex = 0;
  utf8SafeCopy(settingsView_.fontNames[0], "Default", sizeof(settingsView_.fontNames[0]));
  settingsView_.fontCount = 1;
  auto fonts = FONT_MANAGER.listAvailableFonts();
  for (size_t i = 0; i < fonts.size() && settingsView_.fontCount < ui::InReaderSettingsView::MAX_FONTS; i++) {
    // Include both .epdfont families (full replacement) and .bin fonts
    // (CJK glyph fallback). The picker used to skip .bin entries, which
    // left users with Japanese flashcards unable to select notosansjp
    // and stuck on the built-in font that has no CJK glyphs (rendering
    // every kanji as '?'). FontManager::getReaderFontId handles both
    // formats correctly: a .bin pick keeps the built-in for ASCII and
    // streams CJK glyphs from the .bin file.
    int idx = settingsView_.fontCount;
    utf8SafeCopy(settingsView_.fontNames[idx], fonts[i].c_str(), sizeof(settingsView_.fontNames[idx]));
    if (s.readerFont[0] && strcmp(fonts[i].c_str(), s.readerFont) == 0) {
      settingsView_.currentFontIndex = idx;
    }
    settingsView_.fontCount++;
  }
  settingsView_.values[0] = 0;  // Not used for FontSelect

  // Matches InReaderSettingsView::DEFS order (shifted +1 for Font at index 0)
  settingsView_.values[1] = s.fontSize;
  settingsView_.values[2] = s.textLayout;
  settingsView_.values[3] = s.lineSpacing;
  settingsView_.values[4] = s.paragraphAlignment;
  settingsView_.values[5] = s.hyphenation;
  settingsView_.values[6] = s.textAntiAliasing;
  settingsView_.values[7] = s.showImages;
  settingsView_.values[8] = s.statusBar;
  settingsView_.values[9] = s.textDarkness;

  // Snapshot global defaults before per-book overrides
  globalFontSize_ = s.fontSize;
  globalLineSpacing_ = s.lineSpacing;
  globalHyphenation_ = s.hyphenation;
  globalShowImages_ = s.showImages;
  globalTextDarkness_ = s.textDarkness;

  // Apply per-book overrides (value != -1 means override active)
  const char* cacheDir = core.content.cacheDir();
  if (cacheDir) {
    bookOverrides_ = sumi::BookOverrides::load(cacheDir);
    if (bookOverrides_.fontSize >= 0)
      settingsView_.values[1] = static_cast<uint8_t>(bookOverrides_.fontSize);
    if (bookOverrides_.lineSpacing >= 0)
      settingsView_.values[3] = static_cast<uint8_t>(bookOverrides_.lineSpacing);
    if (bookOverrides_.hyphenation >= 0)
      settingsView_.values[5] = static_cast<uint8_t>(bookOverrides_.hyphenation);
    if (bookOverrides_.showImages >= 0)
      settingsView_.values[7] = static_cast<uint8_t>(bookOverrides_.showImages);
    if (bookOverrides_.textDarkness >= 0)
      settingsView_.values[9] = static_cast<uint8_t>(bookOverrides_.textDarkness);
  }

#if FEATURE_BLUETOOTH
  // Index 10: Bluetooth action — show current connection state
  if (ble::isReady() && ble::isConnected()) {
    strncpy(settingsView_.actionStatus, "Connected", sizeof(settingsView_.actionStatus) - 1);
  } else if (s.blePageTurner[0] != '\0' || s.bleKeyboard[0] != '\0') {
    strncpy(settingsView_.actionStatus, "Connect", sizeof(settingsView_.actionStatus) - 1);
  } else {
    strncpy(settingsView_.actionStatus, "No saved device", sizeof(settingsView_.actionStatus) - 1);
  }
#endif

  // Index 9: Look up Word — show the active dictionary's display name so
  // the user knows which one will get queried. "None" if no dict active.
  if (s.dictionaryName[0] != '\0') {
    // setActive() is cheap when already active, parses .ifo to get display.
    if (sumi::Dictionary::setActive(s.dictionaryName)) {
      utf8SafeCopy(settingsView_.dictActionStatus,
                   sumi::Dictionary::activeDisplayName().c_str(),
                   sizeof(settingsView_.dictActionStatus));
    } else {
      utf8SafeCopy(settingsView_.dictActionStatus, "Missing", sizeof(settingsView_.dictActionStatus));
    }
  } else {
    utf8SafeCopy(settingsView_.dictActionStatus, "None", sizeof(settingsView_.dictActionStatus));
  }

  // Index 10: Lookup History — show word count so the user sees at a glance
  // how many words they've looked up in this book.
  if (cacheDir && sumi::LookupHistory::hasHistory(cacheDir)) {
    auto words = sumi::LookupHistory::load(cacheDir);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d word%s",
             static_cast<int>(words.size()), words.size() == 1 ? "" : "s");
    utf8SafeCopy(settingsView_.historyActionStatus, buf,
                 sizeof(settingsView_.historyActionStatus));
  } else {
    utf8SafeCopy(settingsView_.historyActionStatus, "Empty",
                 sizeof(settingsView_.historyActionStatus));
  }

  // Index 11: Toggle Bookmark — show current page bookmark state
  if (cacheDir && sumi::Bookmarks::isBookmarked(cacheDir, currentPage_)) {
    char buf[24];
    snprintf(buf, sizeof(buf), "\xe2\x98\x85 Page %u", currentPage_);
    utf8SafeCopy(settingsView_.bookmarkToggleStatus, buf,
                 sizeof(settingsView_.bookmarkToggleStatus));
  } else {
    char buf[24];
    snprintf(buf, sizeof(buf), "Page %u", currentPage_);
    utf8SafeCopy(settingsView_.bookmarkToggleStatus, buf,
                 sizeof(settingsView_.bookmarkToggleStatus));
  }

  // Index 12: View Bookmarks — show bookmark count
  if (cacheDir) {
    int bmCount = sumi::Bookmarks::count(cacheDir);
    if (bmCount > 0) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%d bookmark%s", bmCount, bmCount == 1 ? "" : "s");
      utf8SafeCopy(settingsView_.bookmarkListStatus, buf,
                   sizeof(settingsView_.bookmarkListStatus));
    } else {
      utf8SafeCopy(settingsView_.bookmarkListStatus, "Empty",
                   sizeof(settingsView_.bookmarkListStatus));
    }
  } else {
    utf8SafeCopy(settingsView_.bookmarkListStatus, "Empty",
                 sizeof(settingsView_.bookmarkListStatus));
  }

  // Index 13: All Bookmarks — show global bookmark count
  {
    int gCount = sumi::GlobalBookmarkIndex::count();
    if (gCount > 0) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%d across books", gCount);
      utf8SafeCopy(settingsView_.globalBookmarkStatus, buf,
                   sizeof(settingsView_.globalBookmarkStatus));
    } else {
      utf8SafeCopy(settingsView_.globalBookmarkStatus, "Empty",
                   sizeof(settingsView_.globalBookmarkStatus));
    }
  }
}

void ReaderState::applyInReaderSettings(Core& core) {
  auto& s = core.settings;

  // Index 0: Font (FontSelect) - detect font change
  const char* selectedFont = settingsView_.getCurrentFontName();
  bool fontChanged = (strcmp(s.readerFont, selectedFont) != 0);
  strncpy(s.readerFont, selectedFont, sizeof(s.readerFont) - 1);
  s.readerFont[sizeof(s.readerFont) - 1] = '\0';

  // Detect if layout-affecting settings changed (requires cache rebuild)
  bool cacheInvalid = (fontChanged ||
                        s.fontSize != settingsView_.values[1] ||
                        s.textLayout != settingsView_.values[2] ||
                        s.lineSpacing != settingsView_.values[3] ||
                        s.paragraphAlignment != settingsView_.values[4] ||
                        s.hyphenation != settingsView_.values[5] ||
                        s.showImages != settingsView_.values[7]);

  // Apply all values to runtime settings (shifted +1 for Font at index 0)
  s.fontSize = settingsView_.values[1];
  s.textLayout = settingsView_.values[2];
  s.lineSpacing = settingsView_.values[3];
  s.paragraphAlignment = settingsView_.values[4];
  s.hyphenation = settingsView_.values[5];
  s.textAntiAliasing = settingsView_.values[6];
  s.showImages = settingsView_.values[7];
  s.statusBar = settingsView_.values[8];
  s.textDarkness = settingsView_.values[9];

  // Invalidate page cache if layout changed
  if (cacheInvalid) {
    parser_.reset();
    parserSpineIndex_ = -1;
    pageCache_.reset();
  }

  // Save per-book overrides: any field that differs from global default
  // gets stored as an override; fields matching global get -1 (inherit).
  const char* cacheDir = core.content.cacheDir();
  if (cacheDir) {
    bookOverrides_.fontSize =
        (settingsView_.values[1] != globalFontSize_)
            ? static_cast<int8_t>(settingsView_.values[1]) : -1;
    bookOverrides_.lineSpacing =
        (settingsView_.values[3] != globalLineSpacing_)
            ? static_cast<int8_t>(settingsView_.values[3]) : -1;
    bookOverrides_.hyphenation =
        (settingsView_.values[5] != globalHyphenation_)
            ? static_cast<int8_t>(settingsView_.values[5]) : -1;
    bookOverrides_.showImages =
        (settingsView_.values[7] != globalShowImages_)
            ? static_cast<int8_t>(settingsView_.values[7]) : -1;
    bookOverrides_.textDarkness =
        (settingsView_.values[9] != globalTextDarkness_)
            ? static_cast<int8_t>(settingsView_.values[9]) : -1;
    sumi::BookOverrides::save(cacheDir, bookOverrides_);

    // Restore global defaults for the overrideable fields so global settings
    // file stays unchanged. The runtime `s` keeps the per-book values for
    // the current session -- they'll be re-applied from overrides on next open.
    s.fontSize = globalFontSize_;
    s.lineSpacing = globalLineSpacing_;
    s.hyphenation = globalHyphenation_;
    s.showImages = globalShowImages_;
    s.textDarkness = globalTextDarkness_;
  }

  // Persist global settings (non-overrideable fields like textLayout, alignment, etc.)
  s.save(core.storage);

  // Re-apply per-book overrides to runtime settings for continued rendering
  if (cacheDir) {
    s.fontSize = settingsView_.values[1];
    s.lineSpacing = settingsView_.values[3];
    s.hyphenation = settingsView_.values[5];
    s.showImages = settingsView_.values[7];
    s.textDarkness = settingsView_.values[9];
  }
}

#if FEATURE_BLUETOOTH
void ReaderState::handleBleAction(Core& core) {
  if (ble::isConnected()) {
    // Already connected — disconnect
    ble::disconnect();
    strncpy(settingsView_.actionStatus, "Disconnected", sizeof(settingsView_.actionStatus) - 1);
    needsRender_ = true;
    settingsView_.needsRender = true;
    return;
  }

  // Show "Connecting..." while we try
  strncpy(settingsView_.actionStatus, "Connecting...", sizeof(settingsView_.actionStatus) - 1);
  needsRender_ = true;
  settingsView_.needsRender = true;
  renderSettingsOverlay(core);
  renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

  // (v1 released the MemoryArena before BLE init to free heap for the
  //  NimBLE stack. v2's arena is in .bss; NimBLE works from the heap
  //  that remains after boot.)
  ble::init();

  bool connected = false;
  const char* savedPt = core.settings.blePageTurner;
  const char* savedKb = core.settings.bleKeyboard;

  // Fast path: try direct reconnect by saved address
  if (savedPt[0] != '\0') {
    connected = ble::reconnect(savedPt);
  }
  if (!connected && savedKb[0] != '\0') {
    connected = ble::reconnect(savedKb);
  }

  // Slow path: scan and connect (handles re-pairing, address rotation, etc.)
  if (!connected) {
    strncpy(settingsView_.actionStatus, "Scanning...", sizeof(settingsView_.actionStatus) - 1);
    renderSettingsOverlay(core);
    renderer_.displayBuffer(EInkDisplay::FAST_REFRESH);

    ble::startScan(10);

    // Auto-connect to known page turner if found during scan
    int ptIdx = ble::foundPageTurnerIndex();
    if (ptIdx >= 0) {
      const BleDevice* ptDev = ble::scanResult(ptIdx);
      Serial.printf("[READER] Found page turner in scan: %s\n",
                    ptDev ? ptDev->name : "?");
      connected = ble::connectTo(ptIdx);
      if (connected && ptDev) {
        strncpy(core.settings.blePageTurner, ptDev->addr,
                sizeof(core.settings.blePageTurner) - 1);
        core.settings.save(core.storage);
      }
    }

    // Try connecting to any saved device found in scan results
    if (!connected) {
      for (int i = 0; i < ble::scanResultCount() && !connected; i++) {
        const BleDevice* dev = ble::scanResult(i);
        if (!dev) continue;
        if ((savedPt[0] != '\0' && strcmp(dev->addr, savedPt) == 0) ||
            (savedKb[0] != '\0' && strcmp(dev->addr, savedKb) == 0)) {
          Serial.printf("[READER] Found saved device in scan: %s\n", dev->name);
          connected = ble::connectTo(i);
        }
      }
    }

    // Try any HID device as last resort
    if (!connected) {
      for (int i = 0; i < ble::scanResultCount() && !connected; i++) {
        const BleDevice* dev = ble::scanResult(i);
        if (dev && dev->hasHID) {
          Serial.printf("[READER] Connecting to HID device: %s\n", dev->name);
          connected = ble::connectTo(i);
          if (connected) {
            // Save as keyboard by default
            String nameLower = String(dev->name);
            nameLower.toLowerCase();
            bool isPageTurner = nameLower.indexOf("page") >= 0 ||
                                nameLower.indexOf("remote") >= 0 ||
                                nameLower.indexOf("clicker") >= 0 ||
                                nameLower.indexOf("shutter") >= 0 ||
                                nameLower.indexOf("free") >= 0;
            if (isPageTurner) {
              strncpy(core.settings.blePageTurner, dev->addr,
                      sizeof(core.settings.blePageTurner) - 1);
            } else {
              strncpy(core.settings.bleKeyboard, dev->addr,
                      sizeof(core.settings.bleKeyboard) - 1);
            }
            core.settings.save(core.storage);
          }
        }
      }
    }
  }

  if (connected) {
    ble::setInactivityTimeout(core.settings.getBleTimeoutMs());
    strncpy(settingsView_.actionStatus, "Connected", sizeof(settingsView_.actionStatus) - 1);
    Serial.println("[READER] BLE connected from reader settings");
  } else {
    ble::deinit();
    if (savedPt[0] == '\0' && savedKb[0] == '\0') {
      strncpy(settingsView_.actionStatus, "No saved device", sizeof(settingsView_.actionStatus) - 1);
    } else {
      strncpy(settingsView_.actionStatus, "Not found", sizeof(settingsView_.actionStatus) - 1);
    }
    Serial.println("[READER] BLE connect failed");
  }

  // (v1 re-allocated the arena here after BLE connect/deinit. v2's
  //  arena is permanently in .bss — already initialised, never freed.)

  needsRender_ = true;
  settingsView_.needsRender = true;
}
#endif

void ReaderState::exitToUI(Core& core) {
  Serial.println("[READER] Exiting to Home");
  exitTarget_ = StateId::Home;
  // exit() will be called by the state machine during transition,
  // which handles progress saving, thumbnail generation, and cleanup.
}

// ============================================================================
// Dictionary overlay — in-reader lookup flow
// ============================================================================

void ReaderState::enterDictWordSelect(Core& core) {
  // Settings → Look up Word closes the settings overlay and opens the
  // word-select overlay with the current rendered page as the substrate.
  // Auto-discovery: if no dictionary has ever been selected, scan
  // /dictionary/ on the SD and pick the first one. Saves the user a
  // trip to Device Settings → Dictionary just to "enable" something
  // they already installed. Reported by users: "I press Look up Word
  // and nothing happens" — most often because the device-settings dict
  // dropdown was never visited but the dictionary files are on the SD.
  if (core.settings.dictionaryName[0] == '\0') {
    auto avail = sumi::Dictionary::listAvailable();
    if (avail.empty()) {
      Serial.println("[DICT] No dictionaries on /dictionary/ — install one first");
      // Surface the failure to the user instead of silently returning.
      snprintf(core.buf.text, sizeof(core.buf.text),
               "No dictionary installed.\nAdd one to /dictionary/ on SD.");
      // Reuse the error-toast path the bookmark/save flow uses by
      // bouncing back to settings with an overlay flag the next render
      // can show. Simplest: log + Serial print that the user can spot
      // when something feels broken; the toast itself is a follow-up.
      return;
    }
    utf8SafeCopy(core.settings.dictionaryName, avail[0].name.c_str(),
                 sizeof(core.settings.dictionaryName));
    Serial.printf("[DICT] Auto-selected '%s' (first of %u available)\n",
                  core.settings.dictionaryName, (unsigned)avail.size());
    core.settings.save(core.storage);
  }

  // Make sure Dictionary has the right index loaded. setActive is cheap if
  // already set; clearing cached state is a no-op for the active name.
  if (!sumi::Dictionary::setActive(core.settings.dictionaryName)) {
    Serial.printf("[DICT] setActive failed for '%s'\n", core.settings.dictionaryName);
    return;
  }

  // Snapshot the current page. loadPage allocates a new Page — we take
  // ownership for the overlay lifetime so the background cacher can't
  // reclaim the underlying memory while the user is navigating words.
  if (!pageCache_) {
    Serial.println("[DICT] No page cache — skipping word select");
    return;
  }
  dictSelectedPage_ = pageCache_->loadPage(currentSectionPage_);
  if (!dictSelectedPage_) {
    Serial.println("[DICT] loadPage returned null");
    return;
  }

  const Viewport vp = getReaderViewport();
  const int fontId = core.settings.getReaderFontId(THEME_MANAGER.current());

  dictWordSelectView_ = ui::DictWordSelectView{};
  dictWordSelectView_.page = dictSelectedPage_.get();
  dictWordSelectView_.fontId = fontId;
  dictWordSelectView_.marginLeft = vp.marginLeft;
  dictWordSelectView_.marginTop = vp.marginTop;
  dictWordSelectView_.extractWords(renderer_);
  dictWordSelectView_.mergeHyphenatedWords("");  // No cross-page merge for now

  dictStage_ = DictStage::WordSelect;
  settingsMode_ = false;  // Close the settings overlay we came from
  needsRender_ = true;
  Serial.printf("[DICT] Entered word-select: %zu words, %zu rows\n",
                dictWordSelectView_.words.size(), dictWordSelectView_.rows.size());
}

void ReaderState::exitDictOverlay(Core& core) {
  (void)core;
  dictStage_ = DictStage::None;
  dictSelectedPage_.reset();
  // Keep the pre-populated view data around — cheap and lets Back-then-
  // reopen avoid re-extracting. Reset on next enterDictWordSelect.
  needsRender_ = true;
  Serial.println("[DICT] Exited dictionary overlay");
}

void ReaderState::performDictLookup(Core& core, const std::string& cleaned) {
  if (cleaned.empty()) {
    Serial.println("[DICT] Empty cleaned word — bailing");
    return;
  }

  Serial.printf("[DICT] Looking up '%s'\n", cleaned.c_str());

  // Poll raw button state inside the lookup so holding Back during a
  // long first-open scan cancels it. lookupAll() walks every installed
  // dict, building each one's sparse offset cache on first visit; this
  // can add up across many dicts so cancellation is important here.
  // We use isPressed (direct GPIO read) rather than the event queue
  // because the queue only drains in update() which we're not inside.
  auto shouldCancel = [&core]() -> bool {
    return core.input.isPressed(Button::Back);
  };

  // Record every searched word in the book's lookup history even if we
  // don't find a hit — the history view in the reader menu will show
  // them all, matching the Crosspoint fork's behaviour. Use the generic
  // content cacheDir so this works for TXT files too.
  const char* cacheDir = core.content.cacheDir();
  if (cacheDir) {
    sumi::LookupHistory::addWord(cacheDir, cleaned);
  }

  auto results = sumi::Dictionary::lookupAll(cleaned, nullptr, shouldCancel);

  if (!results.empty()) {
    dictDefinitionView_ = ui::DictDefinitionView{};
    dictDefinitionView_.headword = cleaned;
    dictDefinitionView_.fontId = core.settings.getReaderFontId(THEME_MANAGER.current());
    dictDefinitionView_.sections.reserve(results.size());
    for (auto& r : results) {
      ui::DictDefinitionView::Section s;
      s.sourceLabel = r.dictDisplayName;
      if (r.viaStemmer) {
        s.sourceLabel += " — stem of ‘";
        s.sourceLabel += r.headword;
        s.sourceLabel += "’";
        Serial.printf("[DICT] Stem hit in %s: '%s' → '%s'\n",
                      r.dictName.c_str(), cleaned.c_str(), r.headword.c_str());
      } else {
        Serial.printf("[DICT] Hit in %s: '%s'\n",
                      r.dictName.c_str(), cleaned.c_str());
      }
      s.body = std::move(r.definition);
      dictDefinitionView_.sections.push_back(std::move(s));
    }
    dictDefinitionView_.wrapText(renderer_, THEME_MANAGER.current());
    dictStage_ = DictStage::Definition;
    needsRender_ = true;
    return;
  }

  // Fallback: fuzzy suggestions aggregated across every dict.
  auto similar = sumi::Dictionary::findSimilarAll(cleaned, ui::DictSuggestionsView::MAX_SUGGESTIONS);
  if (!similar.empty()) {
    dictSuggestionsView_ = ui::DictSuggestionsView{};
    dictSuggestionsView_.originalWord = cleaned;
    dictSuggestionsView_.suggestions = std::move(similar);
    dictSuggestionsView_.selectedIndex = 0;
    dictStage_ = DictStage::Suggestions;
    needsRender_ = true;
    return;
  }

  // No hit, no stem, no fuzzy match — return to word select with an ugly
  // but truthful log message. A future iteration can flash a toast.
  Serial.printf("[DICT] '%s' not found\n", cleaned.c_str());
  dictStage_ = DictStage::WordSelect;
  needsRender_ = true;
}

void ReaderState::handleDictInput(Core& core, const Event& e) {
  if (e.type != EventType::ButtonPress) return;

  // Orientation-aware mapping: in landscape, up/down becomes word-prev/next
  // so "forward" in reading order always feels natural. The Crosspoint fork
  // has identical logic; we reuse it.
  const uint8_t orientation = core.settings.orientation;
  const bool landscape = orientation == Settings::LandscapeCW || orientation == Settings::LandscapeCCW;
  const bool inverted = orientation == Settings::Inverted;

  Button rowPrev, rowNext, wordPrev, wordNext;
  if (landscape && orientation == Settings::LandscapeCW) {
    rowPrev = Button::Left; rowNext = Button::Right;
    wordPrev = Button::Down; wordNext = Button::Up;
  } else if (landscape) {
    rowPrev = Button::Right; rowNext = Button::Left;
    wordPrev = Button::Up; wordNext = Button::Down;
  } else if (inverted) {
    rowPrev = Button::Down; rowNext = Button::Up;
    wordPrev = Button::Right; wordNext = Button::Left;
  } else {
    rowPrev = Button::Up; rowNext = Button::Down;
    wordPrev = Button::Left; wordNext = Button::Right;
  }

  switch (dictStage_) {
    case DictStage::WordSelect: {
      if (e.button == Button::Back) {
        exitDictOverlay(core);
        return;
      }
      if (e.button == rowPrev)       { dictWordSelectView_.movePrevRow(); needsRender_ = true; return; }
      if (e.button == rowNext)       { dictWordSelectView_.moveNextRow(); needsRender_ = true; return; }
      if (e.button == wordPrev)      { dictWordSelectView_.movePrevWord(); needsRender_ = true; return; }
      if (e.button == wordNext)      { dictWordSelectView_.moveNextWord(); needsRender_ = true; return; }
      if (e.button == Button::Center) {
        const auto* w = dictWordSelectView_.selectedWord();
        if (w == nullptr) return;
        const std::string cleaned = sumi::Dictionary::cleanWord(w->lookupText);
        performDictLookup(core, cleaned);
      }
      return;
    }

    case DictStage::Definition: {
      if (e.button == Button::Back) {
        // Back from definition → word select (so the user can try another
        // word without re-entering from settings).
        dictStage_ = DictStage::WordSelect;
        needsRender_ = true;
        return;
      }
      if (e.button == Button::Center) {
        // Confirm from definition = close the entire overlay, back to reader.
        exitDictOverlay(core);
        return;
      }
      if (e.button == Button::Left) {
        dictDefinitionView_.pageBackward();
        if (dictDefinitionView_.needsRender) needsRender_ = true;
        return;
      }
      if (e.button == Button::Right) {
        dictDefinitionView_.pageForward();
        if (dictDefinitionView_.needsRender) needsRender_ = true;
        return;
      }
      return;
    }

    case DictStage::Suggestions: {
      if (e.button == Button::Back) {
        dictStage_ = DictStage::WordSelect;
        needsRender_ = true;
        return;
      }
      if (e.button == Button::Up) {
        dictSuggestionsView_.moveUp();
        if (dictSuggestionsView_.needsRender) needsRender_ = true;
        return;
      }
      if (e.button == Button::Down) {
        dictSuggestionsView_.moveDown();
        if (dictSuggestionsView_.needsRender) needsRender_ = true;
        return;
      }
      if (e.button == Button::Center) {
        const auto* sel = dictSuggestionsView_.selected();
        if (sel == nullptr) return;
        performDictLookup(core, *sel);
        return;
      }
      return;
    }

    case DictStage::None:
    default:
      return;
  }
}

void ReaderState::renderDictOverlay(Core& core) {
  (void)core;
  const Theme& theme = THEME_MANAGER.current();
  switch (dictStage_) {
    case DictStage::WordSelect:
      ui::render(renderer_, theme, dictWordSelectView_);
      break;
    case DictStage::Definition:
      ui::render(renderer_, theme, dictDefinitionView_);
      break;
    case DictStage::Suggestions:
      ui::render(renderer_, theme, dictSuggestionsView_);
      break;
    case DictStage::None:
    default:
      break;
  }
  core.display.markDirty();
}

}  // namespace sumi
