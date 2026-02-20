#include "HomeState.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <CoverHelpers.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <esp_system.h>

#include <algorithm>
#include <cmath>

#include "../config.h"
#include "../core/BootMode.h"
#include "../core/Core.h"
#include "../content/LibraryIndex.h"
#include "../content/RecentBooks.h"
#include "Battery.h"
#include "FontManager.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"
#include "../assets/sumi_home_bg.h"

namespace sumi {

HomeState::HomeState(GfxRenderer& renderer) : renderer_(renderer) {}

HomeState::~HomeState() = default;

void HomeState::enter(Core& core) {
  Serial.println("[HOME] Entering");
  core_ = &core;  // Store for theme loading

  // Enable sumi-e art background
  view_.useArtBackground = true;

  // Load last book info if content is still open
  loadLastBook(core);
  
  // Load recent books for carousel
  loadRecentBooks(core);

  // Update battery
  updateBattery();

  view_.needsRender = true;
}

void HomeState::exit(Core& core) {
  Serial.println("[HOME] Exiting");
  view_.clear();
}

void HomeState::loadLastBook(Core& core) {
  // Reset cover state
  coverBmpPath_.clear();
  hasCoverImage_ = false;
  coverLoadFailed_ = false;
  currentBookHash_ = 0;

  // If content already open, use it
  if (core.content.isOpen()) {
    const auto& meta = core.content.metadata();
    view_.setBook(meta.title, meta.author, core.buf.path);
    currentBookHash_ = LibraryIndex::hashPath(core.buf.path);

    if (core.settings.showImages) {
      coverBmpPath_ = core.content.getThumbnailPath();
      if (!coverBmpPath_.empty() && SdMan.exists(coverBmpPath_.c_str())) {
        hasCoverImage_ = true;
      }
    }
    view_.hasCoverBmp = hasCoverImage_;
    return;
  }

  // Try to get book info from RecentBooks (avoids opening EPUB just for metadata)
  const char* savedPath = core.settings.lastBookPath;
  if (savedPath[0] == '\0' || !core.storage.exists(savedPath)) {
    view_.clearBook();
    return;
  }

  // Try RecentBooks for title/author (much cheaper than opening EPUB)
  RecentBooks::Entry recentEntry;
  if (RecentBooks::getMostRecent(core, recentEntry) && strcmp(recentEntry.path, savedPath) == 0) {
    view_.setBook(recentEntry.title, recentEntry.author, savedPath);
    strncpy(core.buf.path, savedPath, sizeof(core.buf.path) - 1);
    core.buf.path[sizeof(core.buf.path) - 1] = '\0';
    
    // Get thumbnail path from hash
    uint32_t hash = LibraryIndex::hashPath(savedPath);
    currentBookHash_ = hash;  // Store for flash cache
    char thumbPath[80];
    snprintf(thumbPath, sizeof(thumbPath), SUMI_CACHE_DIR "/epub_%lu/thumb.bmp", (unsigned long)hash);
    if (core.settings.showImages && SdMan.exists(thumbPath)) {
      coverBmpPath_ = thumbPath;
      hasCoverImage_ = true;
    }
    view_.hasCoverBmp = hasCoverImage_;
    
    // Get progress from LibraryIndex - use minimal stack
    LibraryIndex::Entry libEntry;
    if (LibraryIndex::findByHash(core, hash, libEntry)) {
      view_.bookCurrentPage = libEntry.currentPage;
      view_.bookTotalPages = libEntry.totalPages;
      view_.bookProgress = libEntry.progressPercent();
      const char* dot = strrchr(savedPath, '.');
      view_.isChapterBased = dot && (strcasecmp(dot, ".epub") == 0);
    }
    return;
  }
  
  // Fallback: Open content to get metadata (slower, uses more memory)
  auto result = core.content.open(savedPath, SUMI_CACHE_DIR);
  if (result.ok()) {
    const auto& meta = core.content.metadata();
    view_.setBook(meta.title, meta.author, savedPath);
    strncpy(core.buf.path, savedPath, sizeof(core.buf.path) - 1);
    core.buf.path[sizeof(core.buf.path) - 1] = '\0';
    currentBookHash_ = LibraryIndex::hashPath(savedPath);

    if (core.settings.showImages) {
      coverBmpPath_ = core.content.getThumbnailPath();
      if (!coverBmpPath_.empty() && SdMan.exists(coverBmpPath_.c_str())) {
        hasCoverImage_ = true;
      }
    }
    view_.hasCoverBmp = hasCoverImage_;
    core.content.close();
  } else {
    view_.clearBook();
  }
}

void HomeState::updateBattery() {
  int percent = batteryMonitor.readPercentage();
  view_.setBattery(percent);
}

void HomeState::loadRecentBooks(Core& core) {
  view_.clearRecentBooks();
  
  // Load all recent books (current book is shown separately as main card)
  RecentBooks::Entry entries[RecentBooks::MAX_RECENT];
  int count = RecentBooks::loadAll(core, entries, RecentBooks::MAX_RECENT);
  
  // Skip the first one if it matches current book (it's already shown as main)
  int startIdx = 0;
  if (count > 0 && view_.hasBook && strcmp(entries[0].path, view_.bookPath) == 0) {
    startIdx = 1;
  }
  
  // Add remaining recent books (simplified - no thumbnail check to reduce SD operations)
  for (int i = startIdx; i < count && view_.recentBookCount < ui::HomeView::MAX_RECENT_BOOKS; i++) {
    view_.addRecentBook(entries[i].title, entries[i].author, entries[i].path,
                        entries[i].progress, false);
  }
  
  Serial.printf("[HOME] Loaded %d recent books (showing %d)\n", count, view_.recentBookCount);
}

void HomeState::openSelectedBook(Core& core) {
  const char* path = view_.getSelectedPath();
  if (path && path[0] != '\0') {
    showTransitionNotification("Opening book...");
    strncpy(core.buf.path, path, sizeof(core.buf.path) - 1);
    saveTransition(BootMode::READER, core.buf.path, ReturnTo::HOME);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ESP.restart();
  }
}

void HomeState::updateSelectedBook(Core& core) {
  // When user switches to a different book in carousel, update the card display
  
  // Reset cover state - will be reloaded for new selection
  coverLoadFailed_ = false;
  hasCoverImage_ = false;
  coverBmpPath_.clear();
  
  if (view_.selectedBookIndex == 0) {
    // Back to current book - reload from open content or settings
    Serial.println("[HOME] Selected current book - reloading");
    loadLastBook(core);
  } else {
    // Selected a recent book - copy its info
    int recentIdx = view_.selectedBookIndex - 1;
    if (recentIdx < view_.recentBookCount) {
      const auto& recent = view_.recentBooks[recentIdx];
      Serial.printf("[HOME] Selected recent book %d: %s\n", recentIdx, recent.title);
      
      view_.setBook(recent.title, recent.author, recent.path);

      // Compute cover path for this book
      uint32_t hash = LibraryIndex::hashPath(recent.path);
      currentBookHash_ = hash;  // Store for flash cache

      // Get full progress info from LibraryIndex
      LibraryIndex::Entry libEntry;
      if (LibraryIndex::findByHash(core, hash, libEntry)) {
        view_.bookCurrentPage = libEntry.currentPage;
        view_.bookTotalPages = libEntry.totalPages;
        view_.bookProgress = libEntry.progressPercent();
        const char* dot = strrchr(recent.path, '.');
        view_.isChapterBased = dot && (strcasecmp(dot, ".epub") == 0);
      } else {
        view_.bookProgress = recent.progress;
        view_.bookCurrentPage = 0;
        view_.bookTotalPages = 0;
        view_.isChapterBased = true;
      }

      char thumbPath[80];
      snprintf(thumbPath, sizeof(thumbPath), SUMI_CACHE_DIR "/epub_%lu/thumb.bmp", (unsigned long)hash);
      if (core.settings.showImages && SdMan.exists(thumbPath)) {
        coverBmpPath_ = thumbPath;
        hasCoverImage_ = true;
      }
    }
  }
  
  view_.hasCoverBmp = hasCoverImage_;
  view_.needsRender = true;
}

StateTransition HomeState::update(Core& core) {
  Event e;
  while (core.events.pop(e)) {
    switch (e.type) {
      case EventType::ButtonPress:
        switch (e.button) {
          case Button::Back:
            // Back: Continue reading if book is open
            if (view_.hasBook) {
              openSelectedBook(core);
            }
            break;

          case Button::Center:
            // Center opens/resumes selected book
            if (view_.selectedBookIndex == 0 && view_.hasBook) {
              openSelectedBook(core);
            } else if (view_.selectedBookIndex > 0 && view_.selectedBookIndex <= view_.recentBookCount) {
              // Open a recent book
              const char* path = view_.getSelectedPath();
              strncpy(core.buf.path, path, sizeof(core.buf.path) - 1);
              openSelectedBook(core);
            }
            break;

          case Button::Left:
            // btn2: Files
            return StateTransition::to(StateId::FileList);

          case Button::Right:
            // btn4: Settings
            return StateTransition::to(StateId::Settings);

          case Button::Up:
            // Previous book in carousel
            if (view_.recentBookCount > 0) {
              view_.selectPrevBook();
              updateSelectedBook(core);
            }
            break;

          case Button::Down:
            // Next book in carousel
            if (view_.recentBookCount > 0) {
              view_.selectNextBook();
              updateSelectedBook(core);
            }
            break;

          case Button::Power:
            break;
        }
        break;

      case EventType::ButtonLongPress:
        if (e.button == Button::Power) {
          return StateTransition::to(StateId::Sleep);
        }
        break;

      default:
        break;
    }
  }

  return StateTransition::stay(StateId::Home);
}

void HomeState::drawBackground(Core& core) {
  const char* themeName = core.settings.homeArtTheme;
  Serial.printf("[HOME] drawBackground - theme setting: '%s'\n", themeName);
  
  // Check if using default built-in PROGMEM art
  if (strcmp(themeName, "default") == 0 || themeName[0] == '\0') {
    Serial.println("[HOME] Using default PROGMEM theme");
    uint8_t* fb = renderer_.getFrameBuffer();
    if (fb) {
      // Copy sumi-e art directly into framebuffer (native orientation, zero overhead)
      memcpy_P(fb, SumiHomeBg, SUMI_HOME_BG_SIZE);
    }
  } else {
    // Load theme from SD card
    drawBackgroundFromSD(themeName);
  }
}

void HomeState::drawBackgroundFromSD(const char* themeName) {
  char path[64];
  snprintf(path, sizeof(path), "/config/themes/%s.bmp", themeName);
  
  FsFile file;
  if (!SdMan.openFileForRead("THEME", path, file)) {
    Serial.printf("[HOME] Theme not found: %s, using default\n", path);
    // Fall back to default PROGMEM art
    uint8_t* fb = renderer_.getFrameBuffer();
    if (fb) {
      memcpy_P(fb, SumiHomeBg, SUMI_HOME_BG_SIZE);
    }
    return;
  }
  
  // Read BMP header to get pixel data offset
  uint8_t header[62];
  if (file.read(header, 62) != 62) {
    Serial.printf("[HOME] Failed to read BMP header: %s\n", path);
    file.close();
    uint8_t* fb = renderer_.getFrameBuffer();
    if (fb) {
      memcpy_P(fb, SumiHomeBg, SUMI_HOME_BG_SIZE);
    }
    return;
  }
  
  // Get pixel data offset from header (bytes 10-13, little endian)
  uint32_t pixelOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
  
  // Get image dimensions from header (bytes 18-21 = width, 22-25 = height)
  int32_t width = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
  int32_t height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
  
  // Check palette to determine if inversion is needed
  // Palette starts at byte 54 for BITMAPINFOHEADER
  // If palette[0] is black (0,0,0), the BMP uses standard convention
  // Our framebuffer uses: 0=white, 1=black
  // Standard BMP uses: bit 0=palette[0], bit 1=palette[1]
  // If palette[0]=black, palette[1]=white: BMP bit matches FB bit, NO inversion needed
  bool needsInvert = !(header[54] == 0 && header[55] == 0 && header[56] == 0);
  
  Serial.printf("[HOME] BMP: %dx%d, offset: %lu, invert: %s\n", 
                width, height, pixelOffset, needsInvert ? "yes" : "no");
  
  // Verify dimensions - accept 480x800 portrait BMPs
  if (width != 480 || height != 800) {
    Serial.printf("[HOME] BMP dimensions mismatch, expected 480x800, using default\n");
    file.close();
    uint8_t* fb = renderer_.getFrameBuffer();
    if (fb) {
      memcpy_P(fb, SumiHomeBg, SUMI_HOME_BG_SIZE);
    }
    return;
  }
  
  // Seek to pixel data
  file.seek(pixelOffset);
  
  uint8_t* fb = renderer_.getFrameBuffer();
  if (fb) {
    // BMP is 480x800 portrait, framebuffer is 800x480 landscape
    // Rotate 90° CW while loading: BMP(x,y) -> FB(799-y, x)
    // BMP rows are stored bottom-to-top
    constexpr int bmpRowBytes = 60;  // 480 pixels / 8
    constexpr int fbRowBytes = 100;  // 800 pixels / 8
    
    uint8_t rowBuf[bmpRowBytes];
    
    // Read BMP from bottom to top (standard BMP order)
    for (int bmpY = 0; bmpY < 800; bmpY++) {
      if (file.read(rowBuf, bmpRowBytes) != bmpRowBytes) {
        Serial.printf("[HOME] BMP read error at row %d\n", bmpY);
        break;
      }
      
      // This BMP row becomes a vertical column in the framebuffer
      // BMP row bmpY (from bottom) -> FB column (799 - bmpY)
      int fbX = 799 - bmpY;
      int fbByteX = fbX / 8;
      int fbBitX = 7 - (fbX % 8);  // MSB first in framebuffer
      
      // Each pixel in this BMP row goes to a different FB row
      for (int bmpX = 0; bmpX < 480; bmpX++) {
        int bmpByteX = bmpX / 8;
        int bmpBitX = 7 - (bmpX % 8);  // MSB first in BMP
        
        // Get pixel from BMP row
        uint8_t pixel = (rowBuf[bmpByteX] >> bmpBitX) & 1;
        if (needsInvert) pixel = !pixel;
        
        // Write to framebuffer - BMP x becomes FB y
        int fbY = bmpX;
        uint8_t* fbByte = fb + fbY * fbRowBytes + fbByteX;
        if (pixel) {
          *fbByte |= (1 << fbBitX);
        } else {
          *fbByte &= ~(1 << fbBitX);
        }
      }
    }
  }
  
  file.close();
  Serial.printf("[HOME] Loaded theme: %s\n", themeName);
}

void HomeState::render(Core& core) {
  if (!view_.needsRender) {
    return;
  }

  const Theme& theme = THEME;

  // Always draw background first
  drawBackground(core);

  // Load cover from SD card every time (simple, always correct)
  if (hasCoverImage_ && !coverLoadFailed_) {
    renderCoverToCard();
  }

  // Resolve external font for title/author (may trigger SD load on first call)
  view_.titleFontId = (theme.readerFontFamilySmall[0] != '\0')
                          ? FONT_MANAGER.getFontId(theme.readerFontFamilySmall, theme.uiFontId)
                          : theme.uiFontId;

  // Render rest of UI (text boxes will draw on top of cover)
  ui::render(renderer_, theme, view_);

  renderer_.displayBuffer();
  view_.needsRender = false;
  core.display.markDirty();
}

void HomeState::renderCoverToCard() {
  FsFile file;
  if (!SdMan.openFileForRead("HOME", coverBmpPath_, file)) {
    coverLoadFailed_ = true;
    Serial.printf("[%lu] [HOME] Failed to open cover BMP: %s\n", millis(), coverBmpPath_.c_str());
    return;
  }

  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    file.close();
    coverLoadFailed_ = true;
    Serial.printf("[%lu] [HOME] Failed to parse cover BMP: %s\n", millis(), coverBmpPath_.c_str());
    return;
  }

  const auto card = ui::CardDimensions::calculate(renderer_.getScreenWidth(), renderer_.getScreenHeight());
  const auto coverArea = card.getCoverArea();

  // Compute scale (same logic as drawBitmap) to find actual drawn size
  float scale = 1.0f;
  if (bitmap.getWidth() > coverArea.width)
    scale = (float)coverArea.width / (float)bitmap.getWidth();
  if (bitmap.getHeight() > coverArea.height)
    scale = std::min(scale, (float)coverArea.height / (float)bitmap.getHeight());

  int drawnW = (int)(bitmap.getWidth() * scale);
  int drawnH = (int)(bitmap.getHeight() * scale);

  // Anchor to bottom-right of cover area
  int drawX = coverArea.x + coverArea.width - drawnW;
  int drawY = coverArea.y + coverArea.height - drawnH;

  renderer_.drawBitmap(bitmap, drawX, drawY, coverArea.width, coverArea.height);
  file.close();
}


}  // namespace sumi
