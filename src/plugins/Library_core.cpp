/**
 * @file Library.cpp
 * @brief Book Library & Reader - Complete Implementation
 * @version 1.5.0
 *
 * All EPUB parsing now done via portal preprocessing.
 * Device loads preprocessed text files for fast, reliable reading.
 * - SD-backed book index (no std::vector<BookEntry>)
 * - Fixed-size chapter title array (no std::vector<String>)
 * - Proper header/implementation separation
 * - Activity lifecycle integration
 * - Page preloading for instant page turns
 */

#include "plugins/Library.h"

#if FEATURE_READER

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <algorithm>
#include <new>  // For std::nothrow

// =============================================================================
// JPEG Decoder Globals
// =============================================================================
static int _coverOffsetX = 0;
static int _coverOffsetY = 0;
static int _coverMaxX = 9999;
static int _coverMaxY = 9999;
static int _jpgCallbackCount = 0;

static bool _jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    _jpgCallbackCount++;
    if (_jpgCallbackCount % 500 == 0) yield();
    
    int drawX = x + _coverOffsetX;
    int drawY = y + _coverOffsetY;
    
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = drawX + i;
            int py = drawY + j;
            
            if (px < _coverOffsetX || px >= _coverMaxX) continue;
            if (py < _coverOffsetY || py >= _coverMaxY) continue;
            
            uint16_t color = bitmap[j * w + i];
            uint8_t r = ((color >> 11) & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t b = (color & 0x1F) << 3;
            int gray = (r * 77 + g * 150 + b * 29) >> 8;
            
            int threshold[4] = {64, 192, 240, 128};
            int ditherIdx = ((px & 1) + (py & 1) * 2);
            
            display.drawPixel(px, py, (gray > threshold[ditherIdx]) ? GxEPD_WHITE : GxEPD_BLACK);
        }
    }
    return true;
}

// =============================================================================
// Global Instance
// =============================================================================

// =============================================================================
// Bookmark Implementation
// =============================================================================
Bookmark::Bookmark() : chapter(0), page(0), timestamp(0) {
    memset(label, 0, sizeof(label));
}

void Bookmark::serialize(File& f) const {
    f.write((uint8_t*)&chapter, sizeof(chapter));
    f.write((uint8_t*)&page, sizeof(page));
    f.write((uint8_t*)&timestamp, sizeof(timestamp));
    f.write((uint8_t*)label, sizeof(label));
}

bool Bookmark::deserialize(File& f) {
    if (f.read((uint8_t*)&chapter, sizeof(chapter)) != sizeof(chapter)) return false;
    if (f.read((uint8_t*)&page, sizeof(page)) != sizeof(page)) return false;
    if (f.read((uint8_t*)&timestamp, sizeof(timestamp)) != sizeof(timestamp)) return false;
    if (f.read((uint8_t*)label, sizeof(label)) != sizeof(label)) return false;
    return true;
}

// =============================================================================
// BookmarkList Implementation
// =============================================================================
BookmarkList::BookmarkList() : count(0) {}

void BookmarkList::clear() { count = 0; }

bool BookmarkList::add(int chapter, int page, const char* label) {
    if (count >= MAX_BOOKMARKS) return false;
    if (find(chapter, page) >= 0) return false;
    
    Bookmark& bm = bookmarks[count];
    bm.chapter = chapter;
    bm.page = page;
    bm.timestamp = millis();
    
    if (label && strlen(label) > 0) {
        strncpy(bm.label, label, sizeof(bm.label) - 1);
    } else {
        snprintf(bm.label, sizeof(bm.label), "Ch%d Pg%d", chapter + 1, page + 1);
    }
    count++;
    return true;
}

bool BookmarkList::remove(int index) {
    if (index < 0 || index >= count) return false;
    for (int i = index; i < count - 1; i++) {
        bookmarks[i] = bookmarks[i + 1];
    }
    count--;
    return true;
}

int BookmarkList::find(int chapter, int page) const {
    for (int i = 0; i < count; i++) {
        if (bookmarks[i].chapter == chapter && bookmarks[i].page == page) return i;
    }
    return -1;
}

void BookmarkList::save(const String& path) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) return;
    f.write((uint8_t*)&count, sizeof(count));
    for (int i = 0; i < count; i++) bookmarks[i].serialize(f);
    f.close();
}

void BookmarkList::load(const String& path) {
    clear();
    File f = SD.open(path, FILE_READ);
    if (!f) return;
    f.read((uint8_t*)&count, sizeof(count));
    if (count > MAX_BOOKMARKS) count = MAX_BOOKMARKS;
    for (int i = 0; i < count; i++) {
        if (!bookmarks[i].deserialize(f)) { count = i; break; }
    }
    f.close();
}

// =============================================================================
// ReadingStats Implementation
// =============================================================================
ReadingStats::ReadingStats() : totalPagesRead(0), totalMinutesRead(0), booksFinished(0), sessionStartTime(0), sessionPagesRead(0) {}

void ReadingStats::startSession() { sessionStartTime = millis(); sessionPagesRead = 0; }
void ReadingStats::recordPageTurn() { sessionPagesRead++; totalPagesRead++; }
void ReadingStats::endSession() { totalMinutesRead += getSessionMinutes(); }
uint32_t ReadingStats::getSessionMinutes() const { return (millis() - sessionStartTime) / 60000; }

void ReadingStats::save() {
    File f = SD.open("/.sumi/stats.bin", FILE_WRITE);
    if (f) { f.write((uint8_t*)this, sizeof(ReadingStats)); f.close(); }
}

void ReadingStats::load() {
    File f = SD.open("/.sumi/stats.bin", FILE_READ);
    if (f) { f.read((uint8_t*)this, sizeof(ReadingStats)); f.close(); }
}

// =============================================================================
// BookEntry Implementation
// =============================================================================
void BookEntry::clear() { memset(this, 0, sizeof(BookEntry)); }

void BookEntry::serialize(File& f) const {
    f.write((const uint8_t*)this, sizeof(BookEntry));
}

bool BookEntry::deserialize(File& f) {
    return f.read((uint8_t*)this, sizeof(BookEntry)) == sizeof(BookEntry);
}

// =============================================================================
// LibraryIndexHeader Implementation
// =============================================================================
LibraryIndexHeader::LibraryIndexHeader() : magic(LIBRARY_INDEX_MAGIC), version(LIBRARY_INDEX_VERSION), bookCount(0), timestamp(0) {
    memset(currentPath, 0, sizeof(currentPath));
}

bool LibraryIndexHeader::isValid() const {
    return magic == LIBRARY_INDEX_MAGIC && version == LIBRARY_INDEX_VERSION;
}

// =============================================================================
// BookType Detection
// =============================================================================
BookType detectBookType(const char* path) {
    String p = String(path);
    String pLower = p;
    pLower.toLowerCase();
    
    if (pLower.endsWith(".txt")) return BookType::TXT;
    if (pLower.endsWith(".epub")) return BookType::EPUB_FILE;
    if (SD.exists(p + "/META-INF/container.xml")) return BookType::EPUB_FOLDER;
    return BookType::UNKNOWN;
}

// =============================================================================
// LibraryApp Constructor/Destructor
// =============================================================================
LibraryApp::LibraryApp() 
    : Activity("Library")
    , state(ViewState::MAIN_MENU)
    , bookCount(0), cursor(0), scrollOffset(0)
    , screenW(800), screenH(480), landscape(true), itemsPerPage(8)
    , mainMenuCursor(0)
    , currentPage(0), totalPages(0), currentChapter(0), totalChapters(1)
    , currentBookHash(0)
    , chapterCursor(0), chapterScrollOffset(0), settingsCursor(0)
    , pagesUntilFullRefresh(30), pagesUntilHalfRefresh(10)
    , updateRequired(false), renderTaskHandle(nullptr), renderMutex(nullptr)
    , buttonHoldStart(0), lastButtonState(BTN_NONE)
    , cacheValid(false), indexingProgress(0), preloadedPage(-1)
    , isEpub(false), chapterTitleCount(0)
    , _preloadedChapter(-1), _preloadedPageNum(-1)
    , bookmarkCursor(0), bookmarkScrollOffset(0)
    , useFlipBrowser(true), bookIsOpen(false)
    , pendingChapterLoad(false), pendingChapterToLoad(-1), firstRenderAfterOpen(false)
    , needsFullRedraw(true), _pendingRedraw(true)
{
    strcpy(currentPath, "/books");
    memset(chapterTitle, 0, sizeof(chapterTitle));
    memset(currentBook, 0, sizeof(currentBook));
    memset(currentBookPath, 0, sizeof(currentBookPath));
    memset(bookCacheDir, 0, sizeof(bookCacheDir));
    memset(chapterTitles, 0, sizeof(chapterTitles));
    
    // Allocate reader subsystems on-demand with error checking
    // All EPUB parsing now happens in portal - device just loads preprocessed text
    Serial.println("[LIBRARY] Allocating reader subsystems...");
    Serial.printf("[LIBRARY] Heap before: %d, largest block: %d\n", 
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    
    Serial.println("[LIBRARY] Allocating TextLayout...");
    textLayout = new (std::nothrow) TextLayout();
    if (!textLayout) {
        Serial.println("[LIBRARY] ERROR: TextLayout allocation failed!");
        return;
    }
    // Set display for accurate text measurement
    extern void TextLayout_setDisplay(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>* disp);
    TextLayout_setDisplay(&display);
    Serial.printf("[LIBRARY] TextLayout OK, heap: %d\n", ESP.getFreeHeap());
    
    Serial.println("[LIBRARY] Allocating PageCache...");
    pageCache = new (std::nothrow) PageCache();
    if (!pageCache) {
        Serial.println("[LIBRARY] ERROR: PageCache allocation failed!");
        delete textLayout;
        textLayout = nullptr;
        return;
    }
    Serial.printf("[LIBRARY] PageCache OK, heap: %d\n", ESP.getFreeHeap());
    
    Serial.printf("[LIBRARY] Reader subsystems allocated. Final heap: %d\n", ESP.getFreeHeap());
}

LibraryApp::~LibraryApp() {
    if (renderTaskHandle) { vTaskDelete(renderTaskHandle); renderTaskHandle = nullptr; }
    if (renderMutex) { vSemaphoreDelete(renderMutex); renderMutex = nullptr; }
    
    // Free reader subsystems
    Serial.println("[LIBRARY] Freeing reader subsystems...");
    if (textLayout) { delete textLayout; textLayout = nullptr; }
    if (pageCache) { delete pageCache; pageCache = nullptr; }
    Serial.printf("[LIBRARY] Reader subsystems freed. Free heap: %d\n", ESP.getFreeHeap());
}

// =============================================================================
// Activity Lifecycle
// =============================================================================
void LibraryApp::onEnter() {
    Activity::onEnter();
    MEM_LOG("library_onEnter");
    SD.mkdir("/.sumi");
    SD.mkdir(COVER_CACHE_DIR);
    stats.load();
}

void LibraryApp::onExit() {
    MEM_LOG("library_onExit");
    if (bookIsOpen) { saveProgress(); stats.endSession(); }
    stats.save();
    closeBook();
    Activity::onExit();
}

void LibraryApp::loop() { /* Main loop handled externally */ }
bool LibraryApp::preventAutoSleep() { return state == ViewState::INDEXING; }

// =============================================================================
// SD-Backed Book Index Operations
// =============================================================================
int LibraryApp::getBookCount() const { return bookCount; }

bool LibraryApp::getBook(int index, BookEntry& out) const {
    if (index < 0 || index >= bookCount) return false;
    
    File f = SD.open(LIBRARY_INDEX_PATH, FILE_READ);
    if (!f) return false;
    
    f.seek(sizeof(LibraryIndexHeader) + (index * sizeof(BookEntry)));
    bool ok = out.deserialize(f);
    f.close();
    return ok;
}

bool LibraryApp::updateBook(int index, const BookEntry& book) {
    if (index < 0 || index >= bookCount) return false;
    
    Serial.printf("[UPDATE] Book %d: hasCover=%d, coverPath=%s\n", 
                  index, book.hasCover, book.coverPath);
    
    // Read entire file, modify, write back (SD doesn't support random write well)
    File f = SD.open(LIBRARY_INDEX_PATH, FILE_READ);
    if (!f) return false;
    
    size_t fileSize = f.size();
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) { f.close(); return false; }
    
    f.read(buffer, fileSize);
    f.close();
    
    // Modify the entry in buffer
    size_t offset = sizeof(LibraryIndexHeader) + (index * sizeof(BookEntry));
    memcpy(buffer + offset, &book, sizeof(BookEntry));
    
    // Write back
    f = SD.open(LIBRARY_INDEX_PATH, FILE_WRITE);
    if (!f) { free(buffer); return false; }
    f.write(buffer, fileSize);
    f.close();
    free(buffer);
    return true;
}

void LibraryApp::clearBooks() {
    SD.remove(LIBRARY_INDEX_PATH);
    bookCount = 0;
}

bool LibraryApp::saveLibraryIndex() { return true; }

bool LibraryApp::loadLibraryIndex() {
    File f = SD.open(LIBRARY_INDEX_PATH, FILE_READ);
    if (!f) return false;
    
    LibraryIndexHeader header;
    if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) { f.close(); return false; }
    if (!header.isValid()) { f.close(); return false; }
    
    bookCount = header.bookCount;
    strncpy(currentPath, header.currentPath, sizeof(currentPath) - 1);
    f.close();
    return true;
}

bool LibraryApp::addBookToIndex(const BookEntry& book) {
    File f = SD.open(LIBRARY_INDEX_PATH, FILE_WRITE);
    if (!f) return false;
    
    if (f.size() == 0) {
        LibraryIndexHeader header;
        strncpy(header.currentPath, currentPath, sizeof(header.currentPath) - 1);
        header.timestamp = millis();
        f.write((uint8_t*)&header, sizeof(header));
    }
    
    f.seek(f.size());
    book.serialize(f);
    
    // Update count in header
    f.seek(offsetof(LibraryIndexHeader, bookCount));
    uint16_t newCount = bookCount + 1;
    f.write((uint8_t*)&newCount, sizeof(newCount));
    f.close();
    
    bookCount++;
    return true;
}

// =============================================================================
// Initialization
// =============================================================================
void LibraryApp::init(int w, int h, bool autoResume) {
    screenW = w; screenH = h;
    landscape = (w > h);
    itemsPerPage = (screenH - 100) / 50;
    setScreenSize(w, h);
    
    Serial.printf("[LIBRARY] Init: %dx%d, landscape=%d\n", w, h, landscape);
    MEM_LOG("library_init");
    
    // Check if subsystems were allocated
    if (!textLayout || !pageCache) {
        Serial.println("[LIBRARY] ERROR: Reader subsystems not allocated!");
        showErrorScreen("Memory Error\n\nNot enough RAM.\nTry rebooting.");
        return;
    }
    
    readerSettings.load();
    textLayout->setPageSize(getLayoutWidth(), screenH);
    applyFontSettings();
    
    // Ensure directories exist before scanning
    SD.mkdir("/.sumi");
    SD.mkdir(COVER_CACHE_DIR);
    
    if (autoResume && SD.exists(LAST_BOOK_PATH)) {
        LastBookInfo info;
        if (getLastBookInfo(info) && strlen(info.bookPath) > 0 && SD.exists(info.bookPath)) {
            Serial.printf("[LIBRARY] Quick-open: %s\n", info.title);
            if (resumeLastBook()) return;
        }
    }
    
    // Try to load portal-generated library index first (fastest)
    if (loadPortalLibraryIndex()) {
        Serial.printf("[LIBRARY] Using portal index: %d books\n", bookCount);
        state = ViewState::BROWSER;
        return;
    }
    
    // Try to load existing binary library index
    if (loadLibraryIndex() && bookCount > 0) {
        Serial.printf("[LIBRARY] Using cached index: %d books\n", bookCount);
        // Validate that current path matches
        if (strcmp(currentPath, "/books") == 0) {
            state = ViewState::BROWSER;
            // Still need to update covers from portal-processed books
            updateBooksFromPortal();
            return;
        }
    }
    
    // No valid cache or path changed - full scan needed
    scanDirectory();
}

int LibraryApp::getLayoutWidth() const { return screenW; }

const GFXfont* LibraryApp::getReaderFont() {
    LibReaderSettings& settings = readerSettings.get();
    switch (settings.fontSize) {
        case FontSize::SMALL:       return &FreeSans9pt7b;
        case FontSize::MEDIUM:      return &FreeSans12pt7b;
        case FontSize::LARGE:       return &FreeSans18pt7b;
        case FontSize::EXTRA_LARGE: return &FreeSans24pt7b;
        default:                    return &FreeSans12pt7b;
    }
}

const GFXfont* LibraryApp::getReaderBoldFont() {
    LibReaderSettings& settings = readerSettings.get();
    switch (settings.fontSize) {
        case FontSize::SMALL:       return &FreeSansBold9pt7b;
        case FontSize::MEDIUM:      return &FreeSansBold12pt7b;
        case FontSize::LARGE:       return &FreeSansBold18pt7b;
        case FontSize::EXTRA_LARGE: return &FreeSansBold24pt7b;
        default:                    return &FreeSansBold12pt7b;
    }
}

void LibraryApp::applyFontSettings() {
    const GFXfont* font = getReaderFont();
    const GFXfont* boldFont = getReaderBoldFont();
    
    textLayout->setFont(font);
    textLayout->setBoldFont(boldFont);
    textLayout->setItalicFont(font);        // Use regular for italic (no italic font available)
    textLayout->setBoldItalicFont(boldFont); // Use bold for bold-italic
    display.setFont(font);
    
    LibReaderSettings& settings = readerSettings.get();
    
    // Get layout settings
    int lineHeight = settings.getBaseFontHeight();
    float lineCompression = settings.getLineCompression();
    int paraSpacing = settings.getParagraphSpacing();
    bool useIndent = !settings.extraParagraphSpacing;
    
    // Apply layout settings
    textLayout->setLineHeight(lineHeight);
    textLayout->setLineHeightMultiplier(lineCompression);
    textLayout->setMargins(
        settings.getMarginLeft(),
        settings.getMarginRight(),
        settings.getMarginTop(),
        settings.getMarginBottom()
    );
    textLayout->setParaSpacing(paraSpacing);
    textLayout->setUseParaIndent(useIndent);
    textLayout->setJustify(settings.justifyText());
}

#endif // FEATURE_READER
