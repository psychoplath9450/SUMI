/**
 * @file Library.h
 * @brief Book Library & Reader - Header Declarations Only
 * @version 1.5.0
 *
 * Header declarations only - implementation in Library_*.cpp
 * 
 * Notes:
 * - All books must be preprocessed via portal before reading
 * - Book list stored in SD-backed binary index
 * - Chapter titles use fixed-size array
 * - Extends Activity base class
 * - Page preloading for instant page turns
 */

#ifndef SUMI_PLUGIN_LIBRARY_H
#define SUMI_PLUGIN_LIBRARY_H

#include "config.h"
#if FEATURE_READER

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "core/PageCache.h"
#include "core/TextLayout.h"
#include "core/ReaderSettings.h"
#include "core/PluginHelpers.h"
#include "core/BatteryMonitor.h"
#include "core/SettingsManager.h"
#include "core/KOSync.h"
#include "core/PowerManager.h"
#include "core/Activity.h"
#include <TJpg_Decoder.h>

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern SettingsManager settingsManager;

// =============================================================================
// Constants (LIBRARY_MAX_BOOKS defined in config.h)
// =============================================================================
#define LIBRARY_INDEX_PATH      "/.sumi/library.idx"
#define COVER_CACHE_DIR         "/.sumi/covers"
#define LAST_BOOK_PATH          "/.sumi/lastbook.bin"
#define MAX_CHAPTER_TITLES      50
#define MAX_BOOKMARKS           20

// =============================================================================
// Bookmark Structure
// =============================================================================
struct Bookmark {
    int chapter;
    int page;
    uint32_t timestamp;
    char label[32];
    
    Bookmark();
    void serialize(File& f) const;
    bool deserialize(File& f);
};

// =============================================================================
// BookmarkList - Fixed size, no std::vector
// =============================================================================
struct BookmarkList {
    Bookmark bookmarks[MAX_BOOKMARKS];
    int count;
    
    BookmarkList();
    bool add(int chapter, int page, const char* label = nullptr);
    bool remove(int index);
    int find(int chapter, int page) const;
    void clear();
    void save(const String& path);
    void load(const String& path);
};

// =============================================================================
// Reading Statistics
// =============================================================================
struct ReadingStats {
    uint32_t totalPagesRead;
    uint32_t totalMinutesRead;
    uint32_t booksFinished;
    uint32_t sessionStartTime;
    uint32_t sessionPagesRead;
    
    ReadingStats();
    void startSession();
    void recordPageTurn();
    void endSession();
    uint32_t getSessionMinutes() const;
    void save();
    void load();
};

// =============================================================================
// KOSync Progress
// =============================================================================
struct KOSyncProgress {
    float progress;
    int page;
};

// =============================================================================
// Book Type Detection
// =============================================================================
enum class BookType {
    UNKNOWN,
    TXT,
    EPUB_FILE,
    EPUB_FOLDER
};

BookType detectBookType(const char* path);

// =============================================================================
// Last Book Info (for resume)
// =============================================================================
struct LastBookInfo {
    uint32_t magic;          // 0x4C415354 "LAST"
    char title[64];
    char author[48];
    char coverPath[96];
    char bookPath[128];
    int chapter;
    int page;
    int totalPages;
    float progress;
};

// =============================================================================
// Book Entry - Fixed size for binary storage
// =============================================================================
struct BookEntry {
    char filename[64];
    char title[48];
    char author[32];
    char coverPath[96];
    uint32_t size;
    uint32_t totalWords;     // Word count (from meta.json)
    uint32_t cacheHash;      // Hash for /.sumi/books/{hash}/ directory
    uint16_t estimatedPages; // Estimated page count
    uint16_t pubYear;        // Publication year (0 if unknown)
    bool isDirectory;
    bool isRegularDir;
    BookType bookType;
    bool hasCover;
    bool hasCache;          // True if pre-processed cache exists
    int lastChapter;
    int lastPage;
    int totalChapters;      // Total chapters in book (from meta.json)
    float progress;
    
    void clear();
    void serialize(File& f) const;
    bool deserialize(File& f);
};

// =============================================================================
// Library Index Header
// =============================================================================
#define LIBRARY_INDEX_MAGIC     0x4C494258  // "LIBX"
#define LIBRARY_INDEX_VERSION   4

struct LibraryIndexHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t bookCount;
    uint32_t timestamp;
    char currentPath[128];
    
    LibraryIndexHeader();
    bool isValid() const;
};

// =============================================================================
// Chapter Title Entry - Fixed size
// =============================================================================
struct ChapterTitle {
    char title[64];
};

// =============================================================================
// View States
// =============================================================================
enum class ViewState {
    MAIN_MENU,
    BROWSER,
    BROWSER_LIST,
    READING,
    CHAPTER_SELECT,
    BOOKMARK_SELECT,
    READING_STATS,
    SETTINGS_MENU,
    INFO,
    INDEXING
};

// =============================================================================
// Settings Menu Items
// =============================================================================
enum class SettingsItem {
    FONT_SIZE = 0,
    MARGINS,
    LINE_SPACING,
    JUSTIFY,
    REFRESH_FREQ,   // Full refresh frequency
    CHAPTERS,
    BOOKMARKS,
    ADD_BOOKMARK,
    STATS,
    CLEAR_CACHE,
    BACK,
    COUNT
};

// =============================================================================
// LibraryApp Class - Declaration Only
// =============================================================================
class LibraryApp : public Activity {
public:
    // Physical display dimensions
    static const int PHYSICAL_WIDTH = 800;
    static const int PHYSICAL_HEIGHT = 480;
    
    // Constructor/Destructor
    LibraryApp();
    ~LibraryApp();
    
    // Activity Lifecycle
    void onEnter() override;
    void onExit() override;
    void loop() override;
    bool preventAutoSleep() override;
    
    // Initialization
    void init(int w, int h, bool autoResume = false);
    
    // Layout helpers
    int getLayoutWidth() const;
    const GFXfont* getReaderFont();
    const GFXfont* getReaderBoldFont();
    void applyFontSettings();
    
    // Directory/Book scanning
    void scanDirectory();
    void sortBooks(int sortMode = 0);  // 0=A-Z, 1=Z-A, 2=by type
    void checkForCovers();
    bool isValidCoverFile(const char* path);
    void loadBookMetadata(BookEntry& book, const char* fullPath);
    void updateBooksFromPortal();  // Quick update without full scan
    String getCoverCachePath(const char* bookPath);
    String getCoverCachePath(const char* bookPath, bool forWidget);
    
    // Input handling
    bool handleKeyboardKey(uint8_t keyCode, bool pressed);
    bool isReading() const;
    bool handleInput(Button btn);
    bool handleButtonPress(Button btn);
    bool handleMainMenuInput(Button btn);
    bool handleBrowserInput(Button btn);
    bool handleFlipBrowserInput(Button btn);
    bool handleListBrowserInput(Button btn);
    bool handleReadingInput(Button btn);
    bool handleChapterSelectInput(Button btn);
    bool handleSettingsInput(Button btn);
    bool handleBookmarkSelectInput(Button btn);
    bool handleReadingStatsInput(Button btn);
    
    // Bookmarks
    String getBookmarkPath();
    void loadBookmarks();
    void saveBookmarks();
    
    // Progress & Sync
    String getDocumentHash();
    float getReadingProgress();
    void syncProgressToKOSync();
    void syncProgressFromKOSync();
    
    // Page management
    void preloadAdjacentPages();
    void clearAllCache();
    
    // Rendering
    static void renderTaskTrampoline(void* param);
    void showLoadingScreen(const char* message);
    void showErrorScreen(const char* message);
    
    // Book operations
    void openBook(int index);
    void openTxtMetadata(const char* path);
    bool openPreprocessedMetadata(uint32_t hash);
    void renderTaskLoop();
    void renderCurrentPage();
    void closeBook();
    String getPreprocessedChapterPath(int chapter);
    bool loadChapterSync(int chapter);
    void loadChapter(int chapter);
    void saveProgress();
    void syncToKOReader();
    void syncFromKOReader();
    
    // Page preloading for instant turns
    void preloadNextPage();
    void preloadPrevPage();
    bool usePreloadedPage(int chapter, int page, CachedPage& outPage);
    
    // Drawing
    void draw();
    void drawPartial();           // Partial refresh for menu navigation
    void drawFullScreen();        // Force full refresh
    bool update();                // Called each frame - returns true if needs redraw
    bool needsFullRefresh();
    bool needsRedraw();           // Returns true if draw() should be called
    void requestFullRefresh();
    void drawMainMenu();
    void drawBrowser();
    void drawFlipBrowser();
    void drawCoverImage(const char* path, int x, int y, int maxW, int maxH);
    void drawCoverPlaceholder(int x, int y, int maxW, int maxH, const char* label);
    void drawListBrowser();
    void drawReadingPage();
    void drawStatusBarInPage();
    void saveLastBookInfo();
    static bool getLastBookInfo(LastBookInfo& info);
    bool resumeLastBook();
    static void drawSleepCover(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& disp, int w, int h);
    void drawIndexingScreen();
    void drawChapterSelect();
    void drawSettingsMenu();
    void drawBookmarkSelect();
    void drawReadingStatsScreen();
    void drawInfo();
    
    // Book access (SD-backed, no std::vector)
    int getBookCount() const;
    bool getBook(int index, BookEntry& out) const;
    bool updateBook(int index, const BookEntry& book);
    void clearBooks();
    
private:
    // State
    ViewState state;
    int bookCount;
    int cursor;
    int scrollOffset;
    int screenW, screenH;
    bool landscape;
    int itemsPerPage;
    int mainMenuCursor;
    char currentPath[128];
    char currentBook[64];
    char currentBookPath[128];
    char bookCacheDir[64];      // /.sumi/books/{hash} - for images
    char chapterTitle[48];
    uint32_t currentBookHash;   // Hash of full filename (from scan, before truncation)
    int currentPage;
    int totalPages;
    int currentChapter;
    int totalChapters;
    int chapterCursor;
    int chapterScrollOffset;
    int settingsCursor;
    int pagesUntilFullRefresh;
    int pagesUntilHalfRefresh;
    
public:
    bool needsFullRedraw;       // Flag for partial vs full refresh
    bool _pendingRedraw;        // Flag indicating draw() should be called
    
    // JPG decoding state (static for TJpgDec callback)
    static int _coverOffsetX;
    static int _coverOffsetY;
    static int _coverMaxX;
    static int _coverMaxY;
    static int _jpgCallbackCount;
    static float _coverScale;
    static bool _jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    
    // FreeRTOS task
    volatile bool updateRequired;
    TaskHandle_t renderTaskHandle;
    SemaphoreHandle_t renderMutex;
    
    // Button tracking
    uint32_t buttonHoldStart;
    Button lastButtonState;
    
    // Cache state
    bool cacheValid;
    int indexingProgress;
    int preloadedPage;
    
    // EPUB state (metadata only - parsing done by portal)
    bool isEpub;
    ChapterTitle chapterTitles[MAX_CHAPTER_TITLES];  // Fixed array
    int chapterTitleCount;
    
    // Page preloading for instant page turns
    CachedPage _preloadedPageData;
    int _preloadedChapter;
    int _preloadedPageNum;
    
    // Reading stats & bookmarks
    ReadingStats stats;
    BookmarkList bookmarks;
    int bookmarkCursor;
    int bookmarkScrollOffset;
    
    // Browser mode
    bool useFlipBrowser;
    
    // Book state
    bool bookIsOpen;
    bool pendingChapterLoad;
    int pendingChapterToLoad;
    bool firstRenderAfterOpen;
    
    // SD-backed book list helpers
    bool saveLibraryIndex();
    bool loadLibraryIndex();
    bool loadPortalLibraryIndex();  // Load from portal-generated /.sumi/library.json
    bool addBookToIndex(const BookEntry& book);
    
    // UI Drawing helpers
    void drawHeader(const char* title, const char* subtitle = nullptr);
    void drawFooter(const char* text);
    void drawToggle(int x, int y, bool enabled);
    void centerText(const char* text, int x, int y);
    void drawMiniCover(int x, int y, int w, int h, const char* coverPath, const char* title);
};

// =============================================================================
// Global Instance
// =============================================================================

// =============================================================================
// Sleep Screen API
// =============================================================================
inline void drawLibrarySleepScreen(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& disp, int w, int h) {
    LibraryApp::drawSleepCover(disp, w, h);
}

inline bool hasLastBookForSleep() {
    LastBookInfo info;
    return LibraryApp::getLastBookInfo(info);
}

#endif // FEATURE_READER
#endif // SUMI_PLUGIN_LIBRARY_H
