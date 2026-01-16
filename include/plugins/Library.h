#ifndef SUMI_PLUGIN_LIBRARY_H
#define SUMI_PLUGIN_LIBRARY_H

/**
 * @file Library.h
 * @brief Enhanced Book Library & Reader for Sumi e-reader
 * @version 2.1.16
 *
 * Features:
 * - Full EPUB/TXT reading with chapter navigation
 * - Direct .epub ZIP file reading (miniz)
 * - Expat XML parser for robust HTML parsing
 * - FreeRTOS task with 8KB stack for rendering
 * - Page caching for instant page turns
 * - Text justification
 * - Reader settings (font size, margins, spacing)
 * - Page preloading for smooth navigation
 * - Reading statistics
 * - Flippable cover browser
 * - Three-level display refresh
 */

#include "config.h"
#if FEATURE_READER

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include <vector>
#include <climits>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "core/PageCache.h"
#include "core/TextLayout.h"
#include "core/ExpatHtmlParser.h"
#include "core/EpubParser.h"
#include "core/ReaderSettings.h"
#include "core/PluginHelpers.h"
#include "core/BatteryMonitor.h"
#include "core/SettingsManager.h"
#include <TJpg_Decoder.h>

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern SettingsManager settingsManager;

// JPEG cover rendering globals
static int _coverOffsetX = 0;
static int _coverOffsetY = 0;

// TJpg_Decoder callback - draws pixels to e-ink display with dithering
static bool _jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // Apply offset
    int drawX = x + _coverOffsetX;
    int drawY = y + _coverOffsetY;
    
    // Draw each pixel, converting RGB565 to black/white with ordered dithering
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = drawX + i;
            int py = drawY + j;
            
            uint16_t color = bitmap[j * w + i];
            // Extract RGB from RGB565
            uint8_t r = ((color >> 11) & 0x1F) << 3;  // 5 bits -> 8 bits
            uint8_t g = ((color >> 5) & 0x3F) << 2;   // 6 bits -> 8 bits  
            uint8_t b = (color & 0x1F) << 3;          // 5 bits -> 8 bits
            
            // Convert to grayscale using luminance formula
            int gray = (r * 77 + g * 150 + b * 29) >> 8;  // 0-255
            
            // Ordered dithering (2x2 Bayer matrix)
            int threshold[4] = {64, 192, 240, 128};
            int ditherIdx = ((px & 1) + (py & 1) * 2);
            
            display.drawPixel(px, py, (gray > threshold[ditherIdx]) ? GxEPD_WHITE : GxEPD_BLACK);
        }
    }
    return true;  // Continue decoding
}

// Memory limits
#if SUMI_LOW_MEMORY
    #define LIBRARY_MAX_BOOKS 50
    #define TEXT_BUFFER_SIZE 4096
#else
    #define LIBRARY_MAX_BOOKS 200
    #define TEXT_BUFFER_SIZE 16384
#endif

// =============================================================================
// Reading Statistics
// =============================================================================
struct ReadingStats {
    uint32_t magic = 0x53544154;  // "STAT"
    uint32_t totalPagesRead = 0;
    uint32_t totalMinutesRead = 0;
    uint32_t sessionPagesRead = 0;
    uint32_t sessionStartTime = 0;
    
    void startSession() {
        sessionPagesRead = 0;
        sessionStartTime = millis();
    }
    
    void recordPageTurn() {
        totalPagesRead++;
        sessionPagesRead++;
    }
    
    uint32_t getSessionMinutes() const {
        return (millis() - sessionStartTime) / 60000;
    }
};

// =============================================================================
// Book Type Detection
// =============================================================================
enum class BookType {
    TXT,
    EPUB_FILE,
    EPUB_FOLDER,
    UNKNOWN
};

inline BookType detectBookType(const char* path) {
    String p = String(path);
    String pLower = p;
    pLower.toLowerCase();
    
    if (pLower.endsWith(".txt")) return BookType::TXT;
    if (pLower.endsWith(".epub")) return BookType::EPUB_FILE;
    
    // Check for extracted EPUB folder
    if (SD.exists(p + "/META-INF/container.xml")) {
        return BookType::EPUB_FOLDER;
    }
    
    return BookType::UNKNOWN;
}

// =============================================================================
// Library Browser & Reader with Full Polish
// =============================================================================

// Path to store last book info for sleep screen
#define LAST_BOOK_PATH "/.sumi/lastbook.bin"
#define COVER_CACHE_DIR "/.sumi/covers"

// Last book info for sleep screen
struct LastBookInfo {
    uint32_t magic = 0x4C415354;  // "LAST"
    char title[64];
    char author[48];
    char coverPath[96];
    int chapter;
    int page;
    int totalPages;
    float progress;  // 0.0 - 1.0
};

class LibraryApp {
public:
    struct BookEntry {
        char filename[64];
        char title[48];
        char author[32];
        char coverPath[96];  // Cached cover image path
        uint32_t size;
        bool isDirectory;
        bool isRegularDir;   // True for non-book directories
        BookType bookType;
        bool hasCover;
        int lastChapter;     // Saved progress
        int lastPage;
        float progress;      // 0.0 - 1.0
    };
    
    enum ViewState { 
        BROWSER,         // Flippable cover view
        BROWSER_LIST,    // Traditional list view
        READING, 
        CHAPTER_SELECT, 
        SETTINGS_MENU,
        INFO, 
        INDEXING 
    };
    
    // Settings menu items
    enum SettingsItem {
        SET_ORIENTATION = 0,  // Portrait/Landscape toggle
        SET_FONT_SIZE,
        SET_MARGINS,
        SET_LINE_SPACING,
        SET_JUSTIFY,
        SET_CHAPTERS,       // Jump to chapter select
        SET_CLEAR_CACHE,
        SET_BACK,
        SET_COUNT
    };
    
    // Physical display dimensions
    static const int PHYSICAL_WIDTH = 800;
    static const int PHYSICAL_HEIGHT = 480;
    
    // Helper to get layout width
    // In portrait mode, we use full width since GxEPD2 handles rotation
    int getLayoutWidth() const {
        return screenW;
    }
    
    LibraryApp() : state(BROWSER), cursor(0), scrollOffset(0),
                   screenW(800), screenH(480), landscape(true), itemsPerPage(8),
                   currentPage(0), totalPages(0), currentChapter(0), totalChapters(1),
                   pagesUntilFullRefresh(30), pagesUntilHalfRefresh(10),
                   updateRequired(false), renderTaskHandle(nullptr), renderMutex(nullptr),
                   buttonHoldStart(0), lastButtonState(BTN_NONE),
                   chapterCursor(0), chapterScrollOffset(0),
                   settingsCursor(0),
                   cacheValid(false), indexingProgress(0),
                   isEpub(false), preloadedPage(-1),
                   useFlipBrowser(true), bookIsOpen(false),
                   firstRenderAfterOpen(false) {
        strcpy(currentPath, "/books");
        memset(chapterTitle, 0, sizeof(chapterTitle));
        memset(currentBook, 0, sizeof(currentBook));
        memset(currentBookPath, 0, sizeof(currentBookPath));
        books.reserve(LIBRARY_MAX_BOOKS);
    }
    
    void init(int w, int h) {
        // Use the dimensions passed by the system (GxEPD2 handles rotation)
        screenW = w;
        screenH = h;
        landscape = isLandscapeMode(w, h);
        itemsPerPage = (screenH - 100) / 50;
        
        Serial.printf("[LIBRARY] ==========================================\n");
        Serial.printf("[LIBRARY] init called with w=%d, h=%d\n", w, h);
        Serial.printf("[LIBRARY] landscape=%d\n", landscape);
        Serial.printf("[LIBRARY] screenW=%d, screenH=%d\n", screenW, screenH);
        Serial.printf("[LIBRARY] layoutWidth=%d (for text)\n", getLayoutWidth());
        Serial.printf("[LIBRARY] GxEPD2 handles rotation - no manual transform\n");
        Serial.printf("[LIBRARY] ==========================================\n");
        
        // Load reader settings (now syncs with portal)
        readerSettings.load();
        
        // Initialize text layout engine - GxEPD2 handles coordinate rotation
        textLayout.setPageSize(getLayoutWidth(), screenH);
        readerSettings.applyToLayout(textLayout);
        textLayout.setFont(&FreeSans9pt7b);
        
        // Ensure cover cache directory exists
        SD.mkdir(COVER_CACHE_DIR);
        
        scanDirectory();
    }
    
    void scanDirectory() {
        Serial.printf("[LIBRARY] ===== scanDirectory: %s =====\n", currentPath);
        books.clear();
        
        File dir = SD.open(currentPath);
        if (!dir) {
            Serial.printf("[LIBRARY] ERROR: Failed to open directory: %s\n", currentPath);
            return;
        }
        
        Serial.println("[LIBRARY] Scanning for books...");
        int bookCount = 0;
        while (File entry = dir.openNextFile()) {
            if (bookCount >= LIBRARY_MAX_BOOKS) {
                Serial.println("[LIBRARY] Max books limit reached");
                entry.close();
                break;
            }
            
            const char* name = entry.name();
            if (name[0] == '.') {
                entry.close();
                continue;
            }
            
            BookEntry book;
            memset(&book, 0, sizeof(BookEntry));
            
            strncpy(book.filename, name, 63);
            book.filename[63] = '\0';
            book.size = entry.size();
            book.isDirectory = entry.isDirectory();
            book.isRegularDir = false;  // Will be set later
            book.hasCover = false;
            book.lastChapter = 0;
            book.lastPage = 0;
            book.progress = 0.0f;
            
            strncpy(book.title, book.filename, 47);
            book.title[47] = '\0';
            char* dot = strrchr(book.title, '.');
            if (dot && !book.isDirectory) *dot = '\0';
            
            // Detect book type
            char fullPath[128];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, name);
            book.bookType = detectBookType(fullPath);
            
            Serial.printf("[LIBRARY] Found: '%s' dir=%d type=%d size=%lu\n", 
                          name, book.isDirectory, (int)book.bookType, book.size);
            
            // Include ONLY books (not regular directories) for cleaner flip browser
            // Regular directories are only shown in list view for navigation
            bool isBook = (book.bookType == BookType::TXT ||
                          book.bookType == BookType::EPUB_FILE ||
                          book.bookType == BookType::EPUB_FOLDER);
            bool isRegularDir = book.isDirectory && book.bookType == BookType::UNKNOWN;
            
            if (isBook || isRegularDir) {
                // Check if book limit reached
                if (books.size() >= LIBRARY_MAX_BOOKS) {
                    Serial.println("[LIBRARY] Book limit reached, stopping scan");
                    entry.close();
                    break;
                }
                
                // Check memory before processing
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 30000) {
                    Serial.printf("[LIBRARY] Memory critical (%d bytes), stopping scan\n", freeHeap);
                    entry.close();
                    break;
                }
                
                // For EPUBs, try to load metadata and check for cached cover
                if (book.bookType == BookType::EPUB_FILE || 
                    book.bookType == BookType::EPUB_FOLDER) {
                    loadBookMetadata(book, fullPath);
                }
                
                // Mark regular directories separately so flip browser can skip them
                book.isRegularDir = isRegularDir;
                
                books.push_back(book);
                bookCount++;
                Serial.printf("[LIBRARY] Added to list as #%d (isBook=%d, isRegularDir=%d, heap=%d)\n", 
                             bookCount, isBook, isRegularDir, ESP.getFreeHeap());
            }
            
            entry.close();
        }
        dir.close();
        
        // Sort: BOOKS first (not regular directories), then alphabetically
        // Flip browser displays books before subdirectories
        std::sort(books.begin(), books.end(), [](const BookEntry& a, const BookEntry& b) {
            // Books (not regular dirs) come first
            bool aIsBook = !a.isRegularDir;
            bool bIsBook = !b.isRegularDir;
            if (aIsBook != bIsBook) return aIsBook > bIsBook;
            // Then directories before files
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            // Then alphabetically
            return strcasecmp(a.title, b.title) < 0;
        });
        
        // Find first actual book for cursor (skip directories)
        cursor = 0;
        for (int i = 0; i < (int)books.size(); i++) {
            if (!books[i].isRegularDir) {
                cursor = i;
                break;
            }
        }
        scrollOffset = 0;
        Serial.printf("[LIBRARY] Scan complete: %d items total, cursor at %d\n", (int)books.size(), cursor);
    }
    
    // =========================================================================
    // Book Metadata Loading (for covers and progress)
    // =========================================================================
    
    void loadBookMetadata(BookEntry& book, const char* fullPath) {
        // Generate cover cache path from book filename hash
        uint32_t hash = 0;
        for (const char* p = fullPath; *p; p++) {
            hash = hash * 31 + *p;
        }
        
        // Check for cached cover (try .jpg first, then .raw)
        char jpgPath[96];
        char rawPath[96];
        snprintf(jpgPath, sizeof(jpgPath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
        snprintf(rawPath, sizeof(rawPath), "%s/%08x.raw", COVER_CACHE_DIR, hash);
        
        if (SD.exists(jpgPath)) {
            strncpy(book.coverPath, jpgPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.printf("[LIBRARY] Cached cover found: %s\n", jpgPath);
        } else if (SD.exists(rawPath)) {
            strncpy(book.coverPath, rawPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.printf("[LIBRARY] Cached cover found: %s\n", rawPath);
        } else {
            snprintf(book.coverPath, sizeof(book.coverPath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
            book.hasCover = false;
            Serial.printf("[LIBRARY] No cached cover - will extract when opened\n");
        }
        
        // Try to load saved progress (this is lightweight)
        PageCache tempCache;
        tempCache.init(fullPath);
        int savedChapter, savedPage;
        if (tempCache.loadProgress(savedChapter, savedPage)) {
            book.lastChapter = savedChapter;
            book.lastPage = savedPage;
            // Calculate approximate progress
            book.progress = (float)savedChapter / 10.0f;  // Rough estimate
            if (book.progress > 1.0f) book.progress = 1.0f;
        }
        tempCache.close();
    }
    
    // Get cover cache path for a book
    String getCoverCachePath(const char* bookPath) {
        uint32_t hash = 0;
        for (const char* p = bookPath; *p; p++) {
            hash = hash * 31 + *p;
        }
        char path[96];
        snprintf(path, sizeof(path), "%s/%08x.raw", COVER_CACHE_DIR, hash);
        return String(path);
    }
    
    // =========================================================================
    // Input Handling
    // =========================================================================
    
    bool handleInput(Button btn) {
        Serial.printf("[LIBRARY] handleInput: raw_btn=%d, landscape=%d\n", btn, landscape);
        
        // DON'T remap buttons - use raw physical buttons
        // This matches the help text: "< > Flip | OK: Read | UP: List | DOWN: Settings"
        // Button labels remain consistent regardless of screen orientation
        
        Serial.printf("[LIBRARY] handleInput: using raw btn=%d\n", btn);
        
        // Ignore BTN_NONE
        if (btn == BTN_NONE) {
            return false;
        }
        
        // Handle button immediately (no debounce needed - runPluginSimple handles it)
        return handleButtonPress(btn);
    }
    
    bool handleButtonPress(Button btn) {
        Serial.printf("[LIBRARY] handleButtonPress: btn=%d, state=%d\n", btn, state);
        
        switch (state) {
            case BROWSER:
                return handleBrowserInput(btn);
            case READING:
                return handleReadingInput(btn);
            case CHAPTER_SELECT:
                return handleChapterSelectInput(btn);
            case SETTINGS_MENU:
                return handleSettingsInput(btn);
            case INFO:
                if (btn == BTN_BACK || btn == BTN_CONFIRM) {
                    state = BROWSER;
                    return true;
                }
                break;
            case INDEXING:
                break;
        }
        return false;
    }
    
    bool handleBrowserInput(Button btn) {
        Serial.printf("[LIBRARY] handleBrowserInput: btn=%d, cursor=%d, books=%d, flipMode=%d\n", 
                      btn, cursor, (int)books.size(), useFlipBrowser);
        
        if (useFlipBrowser) {
            // Flippable cover browser mode
            return handleFlipBrowserInput(btn);
        } else {
            // Traditional list browser mode
            return handleListBrowserInput(btn);
        }
    }
    
    bool handleFlipBrowserInput(Button btn) {
        // Helper to find next/prev book (skipping directories)
        auto findNextBook = [this](int from, int dir) -> int {
            int idx = from + dir;
            while (idx >= 0 && idx < (int)books.size()) {
                if (!books[idx].isRegularDir) return idx;
                idx += dir;
            }
            return from;  // No book found, stay in place
        };
        
        switch (btn) {
            case BTN_LEFT:
                // Previous book (skip directories)
                {
                    int newCursor = findNextBook(cursor, -1);
                    if (newCursor != cursor) {
                        cursor = newCursor;
                        Serial.printf("[LIBRARY] Flip to previous book: %d\n", cursor);
                        return true;
                    }
                }
                break;
                
            case BTN_RIGHT:
                // Next book (skip directories)
                {
                    int newCursor = findNextBook(cursor, 1);
                    if (newCursor != cursor) {
                        cursor = newCursor;
                        Serial.printf("[LIBRARY] Flip to next book: %d\n", cursor);
                        return true;
                    }
                }
                break;
                
            case BTN_UP:
                // Switch to list view
                useFlipBrowser = false;
                scrollOffset = cursor > 3 ? cursor - 3 : 0;
                Serial.println("[LIBRARY] Switching to list view");
                return true;
                
            case BTN_DOWN:
                // Open reader settings
                settingsCursor = 0;
                state = SETTINGS_MENU;
                return true;
                
            case BTN_CONFIRM:
                if (books.size() > 0) {
                    BookEntry& book = books[cursor];
                    if (book.isDirectory && book.bookType != BookType::EPUB_FOLDER) {
                        // Navigate into directory
                        if (strlen(currentPath) + strlen(book.filename) + 2 < sizeof(currentPath)) {
                            strcat(currentPath, "/");
                            strcat(currentPath, book.filename);
                            scanDirectory();
                        }
                    } else {
                        // Open book
                        openBook(cursor);
                    }
                    return true;
                }
                break;
                
            case BTN_BACK:
                if (strcmp(currentPath, "/books") != 0) {
                    char* lastSlash = strrchr(currentPath, '/');
                    if (lastSlash && lastSlash != currentPath) {
                        *lastSlash = '\0';
                    }
                    scanDirectory();
                    return true;
                }
                break;
                
            default:
                break;
        }
        return false;
    }
    
    bool handleListBrowserInput(Button btn) {
        // Traditional list browser
        
        switch (btn) {
            case BTN_UP:
                if (cursor > 0) {
                    cursor--;
                    if (cursor < scrollOffset) scrollOffset = cursor;
                    Serial.printf("[LIBRARY] Cursor moved up to %d\n", cursor);
                    return true;
                }
                break;
            
            case BTN_LEFT:
                // Switch back to flip view
                useFlipBrowser = true;
                Serial.println("[LIBRARY] Switching to flip view");
                return true;
                
            case BTN_DOWN:
            case BTN_RIGHT:
                if (cursor < (int)books.size() - 1) {
                    cursor++;
                    if (cursor >= scrollOffset + itemsPerPage) {
                        scrollOffset = cursor - itemsPerPage + 1;
                    }
                    Serial.printf("[LIBRARY] Cursor moved down to %d\n", cursor);
                    return true;
                }
                break;
                
            case BTN_CONFIRM:
                Serial.printf("[LIBRARY] CONFIRM pressed, books.size()=%d\n", (int)books.size());
                if (books.size() > 0) {
                    BookEntry& book = books[cursor];
                    Serial.printf("[LIBRARY] Selected: '%s', isDir=%d, type=%d\n", 
                                  book.filename, book.isDirectory, (int)book.bookType);
                    
                    if (book.isDirectory && book.bookType != BookType::EPUB_FOLDER) {
                        // Regular directory - navigate into
                        Serial.printf("[LIBRARY] Navigating into directory: %s\n", book.filename);
                        if (strlen(currentPath) + strlen(book.filename) + 2 < sizeof(currentPath)) {
                            strcat(currentPath, "/");
                            strcat(currentPath, book.filename);
                            scanDirectory();
                        }
                    } else {
                        // Book file or EPUB folder - open it
                        Serial.printf("[LIBRARY] Opening book at index %d\n", cursor);
                        openBook(cursor);
                    }
                    return true;
                } else {
                    Serial.println("[LIBRARY] No books to select!");
                }
                break;
                
            case BTN_BACK:
                if (strcmp(currentPath, "/books") != 0) {
                    char* lastSlash = strrchr(currentPath, '/');
                    if (lastSlash && lastSlash != currentPath) {
                        *lastSlash = '\0';
                    } else {
                        strcpy(currentPath, "/books");
                    }
                    scanDirectory();
                    return true;
                } else {
                    // At root, switch to flip view
                    useFlipBrowser = true;
                    Serial.println("[LIBRARY] At root, switching to flip view");
                    return true;
                }
                break;
                
            default:
                break;
        }
        return false;
    }
    
    bool handleReadingInput(Button btn) {
        Serial.printf("[LIBRARY] handleReadingInput: btn=%d, page=%d/%d, ch=%d/%d\n",
                      btn, currentPage, totalPages, currentChapter, totalChapters);
        
        // Take mutex to safely modify state
        if (renderMutex && xSemaphoreTake(renderMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            Serial.println("[LIBRARY] Failed to get render mutex");
            return false;  // Couldn't get mutex, skip this input
        }
        
        bool needsUpdate = false;
        bool result = false;
        
        // Sanity check state
        if (totalPages <= 0) {
            Serial.println("[LIBRARY] ERROR: totalPages <= 0");
            if (renderMutex) xSemaphoreGive(renderMutex);
            return false;
        }
        
        switch (btn) {
            case BTN_LEFT:
                // Previous page
                if (currentPage > 0) {
                    currentPage--;
                    stats.recordPageTurn();
                    saveProgress();
                    preloadAdjacentPages();
                    needsUpdate = true;
                    result = true;
                } else if (currentChapter > 0) {
                    // Load previous chapter on MAIN THREAD
                    if (renderMutex) xSemaphoreGive(renderMutex);
                    
                    showLoadingScreen("Loading...");
                    
                    // Save current position in case all previous chapters are empty
                    int originalChapter = currentChapter;
                    int originalPage = currentPage;
                    
                    currentChapter--;
                    
                    // Skip empty chapters (cover pages, etc.)
                    int attempts = 0;
                    bool foundValid = false;
                    while (attempts < 5 && currentChapter >= 0) {
                        if (loadChapterSync(currentChapter)) {
                            currentPage = totalPages > 0 ? totalPages - 1 : 0;  // Go to last page
                            saveProgress();
                            cacheValid = true;
                            foundValid = true;
                            break;
                        }
                        currentChapter--;
                        attempts++;
                    }
                    
                    // If no valid previous chapter found, stay on current
                    if (!foundValid) {
                        Serial.println("[LIBRARY] No valid previous chapter, staying put");
                        currentChapter = originalChapter;
                        currentPage = originalPage;
                        loadChapterSync(currentChapter);  // Reload current
                        cacheValid = true;
                    }
                    
                    if (renderMutex) xSemaphoreTake(renderMutex, portMAX_DELAY);
                    needsUpdate = true;
                    result = true;
                }
                break;
                
            case BTN_RIGHT:
                // Next page
                if (currentPage < totalPages - 1) {
                    currentPage++;
                    stats.recordPageTurn();
                    saveProgress();
                    preloadAdjacentPages();
                    needsUpdate = true;
                    result = true;
                } else if (currentChapter < totalChapters - 1) {
                    // Load next chapter on MAIN THREAD
                    if (renderMutex) xSemaphoreGive(renderMutex);
                    
                    showLoadingScreen("Loading...");
                    
                    // Save current position in case all next chapters are empty
                    int originalChapter = currentChapter;
                    int originalPage = currentPage;
                    
                    currentChapter++;
                    
                    // Skip empty chapters (cover pages, etc.)
                    int attempts = 0;
                    bool foundValid = false;
                    while (attempts < 5 && currentChapter < totalChapters) {
                        if (loadChapterSync(currentChapter)) {
                            currentPage = 0;
                            saveProgress();
                            cacheValid = true;
                            foundValid = true;
                            break;
                        }
                        currentChapter++;
                        attempts++;
                    }
                    
                    // If no valid next chapter found, stay on current
                    if (!foundValid) {
                        Serial.println("[LIBRARY] No valid next chapter, staying put");
                        currentChapter = originalChapter;
                        currentPage = originalPage;
                        loadChapterSync(currentChapter);  // Reload current
                        cacheValid = true;
                    }
                    
                    if (renderMutex) xSemaphoreTake(renderMutex, portMAX_DELAY);
                    needsUpdate = true;
                    result = true;
                }
                break;
                
            case BTN_UP:
                // Open chapter select
                if (totalChapters > 1) {
                    chapterCursor = currentChapter;
                    chapterScrollOffset = (currentChapter > 3) ? currentChapter - 3 : 0;
                    state = CHAPTER_SELECT;
                    result = true;
                }
                break;
                
            case BTN_DOWN:
            case BTN_CONFIRM:
                // Open settings menu
                settingsCursor = 0;
                state = SETTINGS_MENU;
                result = true;
                break;
                
            case BTN_BACK:
                saveProgress();
                if (renderMutex) {
                    xSemaphoreGive(renderMutex);
                }
                closeBook();
                return true;
                
            default:
                break;
        }
        
        if (needsUpdate) {
            updateRequired = true;
        }
        
        if (renderMutex) {
            xSemaphoreGive(renderMutex);
        }
        
        return result;
    }
    
    bool handleChapterSelectInput(Button btn) {
        int maxVisible = 8;
        
        switch (btn) {
            case BTN_UP:
            case BTN_LEFT:
                if (chapterCursor > 0) {
                    chapterCursor--;
                    if (chapterCursor < chapterScrollOffset) {
                        chapterScrollOffset = chapterCursor;
                    }
                    return true;
                }
                break;
                
            case BTN_DOWN:
            case BTN_RIGHT:
                if (chapterCursor < totalChapters - 1) {
                    chapterCursor++;
                    if (chapterCursor >= chapterScrollOffset + maxVisible) {
                        chapterScrollOffset = chapterCursor - maxVisible + 1;
                    }
                    return true;
                }
                break;
                
            case BTN_CONFIRM:
                if (chapterCursor != currentChapter) {
                    // Load selected chapter on main thread
                    showLoadingScreen("Loading...");
                    
                    // Save original position
                    int originalChapter = currentChapter;
                    int originalPage = currentPage;
                    
                    currentChapter = chapterCursor;
                    
                    // Skip empty chapters if needed
                    int attempts = 0;
                    bool found = false;
                    while (attempts < 5 && currentChapter < totalChapters) {
                        if (loadChapterSync(currentChapter)) {
                            currentPage = 0;
                            cacheValid = true;
                            saveProgress();
                            found = true;
                            break;
                        }
                        currentChapter++;
                        attempts++;
                    }
                    
                    if (!found) {
                        // Couldn't find readable chapter, go back to original
                        Serial.println("[LIBRARY] Selected chapter empty, returning to original");
                        currentChapter = originalChapter;
                        currentPage = originalPage;
                        loadChapterSync(currentChapter);
                        cacheValid = true;
                    }
                    updateRequired = true;
                }
                state = READING;
                return true;
                
            case BTN_BACK:
                state = READING;
                updateRequired = true;
                return true;
                
            default:
                break;
        }
        return false;
    }
    
    bool handleSettingsInput(Button btn) {
        LibReaderSettings& settings = readerSettings.get();
        LibReaderSettings oldSettings = settings;
        
        switch (btn) {
            case BTN_UP:
                if (settingsCursor > 0) {
                    settingsCursor--;
                    // Skip chapters if not applicable
                    if (settingsCursor == SET_CHAPTERS && (!bookIsOpen || totalChapters <= 1)) {
                        settingsCursor--;
                    }
                    return true;
                }
                break;
                
            case BTN_DOWN:
                if (settingsCursor < SET_COUNT - 1) {
                    settingsCursor++;
                    // Skip chapters if not applicable
                    if (settingsCursor == SET_CHAPTERS && (!bookIsOpen || totalChapters <= 1)) {
                        settingsCursor++;
                    }
                    return true;
                }
                break;
                
            case BTN_LEFT:
            case BTN_RIGHT:
            case BTN_CONFIRM:
                switch (settingsCursor) {
                    case SET_ORIENTATION: {
                        // Toggle orientation
                        bool isLandscape = (settingsManager.display.orientation == 0);
                        settingsManager.display.orientation = isLandscape ? 1 : 0;
                        settingsManager.save();
                        
                        // Apply new rotation
                        display.setRotation(settingsManager.display.orientation == 0 ? 0 : 3);
                        
                        // Update screen dimensions
                        if (settingsManager.display.orientation == 0) {
                            screenW = PHYSICAL_WIDTH;
                            screenH = PHYSICAL_HEIGHT;
                            landscape = true;
                        } else {
                            screenW = PHYSICAL_HEIGHT;
                            screenH = PHYSICAL_WIDTH;
                            landscape = false;
                        }
                        
                        // Recalculate items per page
                        itemsPerPage = (screenH - 100) / 50;
                        
                        // If reading, need to reformat
                        if (bookIsOpen) {
                            textLayout.setPageSize(getLayoutWidth(), screenH);
                            readerSettings.applyToLayout(textLayout);
                            showLoadingScreen("Reformatting...");
                            if (loadChapterSync(currentChapter)) {
                                cacheValid = true;
                            }
                            updateRequired = true;
                        }
                        return true;
                    }
                    case SET_FONT_SIZE:
                        settings.fontSize = (FontSize)(((int)settings.fontSize + 1) % 3);
                        break;
                    case SET_MARGINS:
                        settings.margins = (MarginSize)(((int)settings.margins + 1) % 3);
                        break;
                    case SET_LINE_SPACING:
                        settings.lineSpacing = (LineSpacing)(((int)settings.lineSpacing + 1) % 3);
                        break;
                    case SET_JUSTIFY:
                        settings.justifyText = !settings.justifyText;
                        break;
                    case SET_CHAPTERS:
                        // Go to chapter select
                        if (bookIsOpen && totalChapters > 1) {
                            chapterCursor = currentChapter;
                            chapterScrollOffset = (currentChapter > 3) ? currentChapter - 3 : 0;
                            state = CHAPTER_SELECT;
                        }
                        return true;
                    case SET_CLEAR_CACHE:
                        clearAllCache();
                        return true;
                    case SET_BACK:
                        // Check if settings changed requiring cache rebuild
                        if (bookIsOpen && readerSettings.requiresCacheRebuild(oldSettings)) {
                            readerSettings.save();
                            textLayout.setPageSize(getLayoutWidth(), screenH);
                            readerSettings.applyToLayout(textLayout);
                            showLoadingScreen("Reformatting...");
                            if (loadChapterSync(currentChapter)) {
                                cacheValid = true;
                            }
                            updateRequired = true;
                        }
                        state = bookIsOpen ? READING : BROWSER;
                        return true;
                }
                readerSettings.save();
                return true;
                
            case BTN_BACK:
                if (bookIsOpen && readerSettings.requiresCacheRebuild(oldSettings)) {
                    readerSettings.save();
                    textLayout.setPageSize(getLayoutWidth(), screenH);
                    readerSettings.applyToLayout(textLayout);
                    showLoadingScreen("Reformatting...");
                    if (loadChapterSync(currentChapter)) {
                        cacheValid = true;
                    }
                    updateRequired = true;
                }
                state = bookIsOpen ? READING : BROWSER;
                return true;
                
            default:
                break;
        }
        return false;
    }
    
    // =========================================================================
    // Page Preloading
    // =========================================================================
    
    void preloadAdjacentPages() {
        // Mark next page in cache for preloading
        int nextPage = currentPage + 1;
        if (nextPage < totalPages && preloadedPage != nextPage) {
            CachedPage* page = new CachedPage();
            if (pageCache.loadPage(currentChapter, nextPage, *page)) {
                preloadedPage = nextPage;
            }
            delete page;
        }
    }
    
    // =========================================================================
    // Cache Management
    // =========================================================================
    
    void clearAllCache() {
        // Clear book caches
        pageCache.close();
        
        // Remove cache directory
        File dir = SD.open("/.sumi/books");
        if (dir) {
            while (File entry = dir.openNextFile()) {
                String path = String("/.sumi/books/") + entry.name();
                if (entry.isDirectory()) {
                    // Remove files in directory
                    File subdir = SD.open(path);
                    while (File f = subdir.openNextFile()) {
                        String fpath = path + "/" + f.name();
                        SD.remove(fpath);
                        f.close();
                    }
                    subdir.close();
                    SD.rmdir(path);
                }
                entry.close();
            }
            dir.close();
        }
        
        Serial.println("[LIBRARY] Cache cleared");
        
        // Reload current chapter on main thread only if a book is open
        if (bookIsOpen && (state == SETTINGS_MENU || state == READING)) {
            showLoadingScreen("Rebuilding...");
            if (loadChapterSync(currentChapter)) {
                cacheValid = true;
            }
            state = READING;
        }
    }
    
    // =========================================================================
    // Book Opening - Load on main thread for stability
    // =========================================================================
    
    // Static trampoline for FreeRTOS task
    static void renderTaskTrampoline(void* param) {
        LibraryApp* self = static_cast<LibraryApp*>(param);
        self->renderTaskLoop();
    }
    
    void showLoadingScreen(const char* message) {
        // Simple minimal loading indicator - just dots
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(screenW/2 - 15, screenH/2);
            display.print("...");
        } while (display.nextPage());
    }
    
    void showErrorScreen(const char* message) {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(40, screenH / 2 - 20);
            display.print("Error");
            display.setFont(&FreeSans9pt7b);
            display.setCursor(40, screenH / 2 + 20);
            display.print(message);
        } while (display.nextPage());
        delay(2000);
    }
    
    void openBook(int index) {
        Serial.printf("[LIBRARY] ===== openBook(%d) =====\n", index);
        Serial.printf("[LIBRARY] Free heap: %d\n", ESP.getFreeHeap());
        
        if (index >= (int)books.size()) {
            Serial.printf("[LIBRARY] ERROR: index %d >= books.size() %d\n", index, (int)books.size());
            return;
        }
        
        // Show loading screen IMMEDIATELY
        showLoadingScreen("Loading...");
        
        BookEntry& book = books[index];
        char fullPath[128];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
        strncpy(currentBookPath, fullPath, sizeof(currentBookPath) - 1);
        strncpy(currentBook, book.title, sizeof(currentBook) - 1);
        
        Serial.printf("[LIBRARY] Book: '%s'\n", currentBook);
        Serial.printf("[LIBRARY] Path: '%s'\n", fullPath);
        Serial.printf("[LIBRARY] Type: %d\n", (int)book.bookType);
        
        // Detect book type
        isEpub = (book.bookType == BookType::EPUB_FOLDER || 
                  book.bookType == BookType::EPUB_FILE);
        
        // =====================================================
        // Open EPUB metadata
        // =====================================================
        if (isEpub) {
            Serial.println("[LIBRARY] Opening EPUB metadata...");
            if (!openEpubMetadata(fullPath)) {
                showErrorScreen("Failed to open EPUB");
                state = BROWSER;
                return;
            }
        } else {
            openTxtMetadata(fullPath);
        }
        
        Serial.printf("[LIBRARY] Metadata loaded: %d chapters\n", totalChapters);
        
        // =====================================================
        // Initialize cache (no extra loading screen - we already showed one)
        // =====================================================
        pageCache.init(currentBookPath);
        
        // Check if cached dimensions match current screen
        // If not, we need to invalidate and rebuild
        CacheKey checkKey;
        LibReaderSettings& settings = readerSettings.get();
        checkKey.fontSize = (uint8_t)settings.fontSize;
        checkKey.margins = (uint8_t)settings.margins;
        checkKey.lineSpacing = (uint8_t)settings.lineSpacing;
        checkKey.setJustify(settings.justifyText);
        checkKey.screenWidth = getLayoutWidth();
        checkKey.screenHeight = screenH;
        
        Serial.printf("[LIBRARY] Current screen: %dx%d (landscape=%d)\n", screenW, screenH, landscape);
        Serial.printf("[LIBRARY] Settings: font=%d, margins=%d, lineSpacing=%d, justify=%d\n",
                      (int)settings.fontSize, (int)settings.margins, 
                      (int)settings.lineSpacing, settings.justifyText);
        
        // If cache exists but was created for different dimensions, invalidate it
        if (!pageCache.hasValidCache(checkKey)) {
            Serial.println("[LIBRARY] Cache invalid for current settings - will rebuild");
            pageCache.invalidateBook();
        }
        
        // Configure text layout PROPERLY - page size first, then settings
        textLayout.setPageSize(getLayoutWidth(), screenH);
        readerSettings.applyToLayout(textLayout);
        textLayout.setFont(&FreeSans9pt7b);
        
        // Restore progress
        int savedChapter = 0, savedPage = 0;
        if (pageCache.loadProgress(savedChapter, savedPage)) {
            Serial.printf("[LIBRARY] Restoring progress: ch=%d, pg=%d\n", savedChapter, savedPage);
            if (savedChapter < totalChapters) {
                currentChapter = savedChapter;
            }
            currentPage = savedPage;
        } else {
            currentChapter = 0;
            currentPage = 0;
        }
        
        // =====================================================
        // Load first chapter
        // Some EPUBs have empty chapters (cover pages, etc.) - skip them
        // =====================================================
        Serial.printf("[LIBRARY] Free heap before indexing: %d\n", ESP.getFreeHeap());
        
        // Try to load current chapter, auto-advance if empty (cover pages, etc.)
        int maxAttempts = 5;  // Don't loop forever
        int attempt = 0;
        while (attempt < maxAttempts && currentChapter < totalChapters) {
            if (loadChapterSync(currentChapter)) {
                break;  // Found a chapter with content
            }
            
            // Chapter was empty (like a cover page) - try next
            Serial.printf("[LIBRARY] Chapter %d empty, trying next...\n", currentChapter);
            currentChapter++;
            attempt++;
        }
        
        if (totalPages == 0 || currentChapter >= totalChapters) {
            showErrorScreen("No readable content");
            if (isEpub) epub.close();
            state = BROWSER;
            return;
        }
        
        Serial.printf("[LIBRARY] Chapter loaded: %d pages\n", totalPages);
        Serial.printf("[LIBRARY] Free heap after indexing: %d\n", ESP.getFreeHeap());
        
        // Validate page number
        if (currentPage >= totalPages) {
            currentPage = totalPages > 0 ? totalPages - 1 : 0;
        }
        
        // Start reading stats
        stats.startSession();
        
        // =====================================================
        // Create render task
        // =====================================================
        if (!renderMutex) {
            renderMutex = xSemaphoreCreateMutex();
        }
        
        cacheValid = true;  // Data loaded
        updateRequired = true;
        
        if (!renderTaskHandle) {
            Serial.println("[LIBRARY] Creating render task (8KB - display only)...");
            BaseType_t result = xTaskCreate(
                renderTaskTrampoline,
                "LibRenderTask",
                8192,  // Only 8KB needed for display
                this,
                1,
                &renderTaskHandle
            );
            
            if (result != pdPASS) {
                Serial.printf("[LIBRARY] ERROR: Task creation failed: %d\n", result);
                showErrorScreen("Memory error");
                if (isEpub) epub.close();
                state = BROWSER;
                return;
            }
        }
        
        // NO blank screen clear - go directly to first page render
        bookIsOpen = true;
        firstRenderAfterOpen = true;  // Will trigger display reset on first render
        state = READING;
        pagesUntilFullRefresh = readerSettings.get().pagesPerFullRefresh;
        pagesUntilHalfRefresh = readerSettings.get().pagesPerHalfRefresh;
        Serial.printf("[LIBRARY] Ready! page=%d/%d, ch=%d/%d\n", 
                     currentPage+1, totalPages, currentChapter+1, totalChapters);
    }
    
    void openTxtMetadata(const char* path) {
        Serial.printf("[LIBRARY] openTxtMetadata: %s\n", path);
        totalChapters = 1;
        currentChapter = 0;
        strncpy(chapterTitle, currentBook, sizeof(chapterTitle) - 1);
        chapterTitles.clear();
        chapterTitles.push_back(String(currentBook));
    }
    
    bool openEpubMetadata(const char* path) {
        Serial.printf("[LIBRARY] openEpubMetadata: %s\n", path);
        if (!epub.open(path)) {
            Serial.printf("[LIBRARY] ERROR: Failed to open EPUB: %s\n", epub.getError().c_str());
            state = BROWSER;
            return false;
        }
        
        // EPUB opened successfully
        totalChapters = epub.getChapterCount();
        strncpy(currentBook, epub.getTitle().c_str(), sizeof(currentBook) - 1);
        currentChapter = 0;
        
        // Load chapter titles from TOC
        chapterTitles.clear();
        if (epub.getTocCount() > 0) {
            for (int i = 0; i < epub.getChapterCount(); i++) {
                chapterTitles.push_back(epub.getChapter(i).title);
            }
        } else {
            for (int i = 0; i < totalChapters; i++) {
                chapterTitles.push_back("Chapter " + String(i + 1));
            }
        }
        
        // Extract and cache cover if not already cached
        cacheBookCover(path);
        
        return true;
    }
    
    // Extract cover from EPUB and save to cache
    void cacheBookCover(const char* bookPath) {
        // Generate cover cache path
        uint32_t hash = 0;
        for (const char* p = bookPath; *p; p++) {
            hash = hash * 31 + *p;
        }
        char coverPath[96];
        snprintf(coverPath, sizeof(coverPath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
        
        // Skip if already cached
        if (SD.exists(coverPath)) {
            Serial.printf("[LIBRARY] Cover already cached: %s\n", coverPath);
            // Update book entry if in list
            for (auto& book : books) {
                char fullPath[128];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
                if (strcmp(fullPath, bookPath) == 0) {
                    strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
                    book.hasCover = true;
                    break;
                }
            }
            return;
        }
        
        // Check if EPUB has a cover
        if (!epub.hasCover()) {
            Serial.println("[LIBRARY] EPUB has no cover image");
            return;
        }
        
        Serial.printf("[LIBRARY] Extracting cover to: %s\n", coverPath);
        
        // Extract cover image from EPUB
        if (epub.extractCoverImage(coverPath)) {
            Serial.println("[LIBRARY] Cover extracted successfully");
            // Update book entry
            for (auto& book : books) {
                char fullPath[128];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
                if (strcmp(fullPath, bookPath) == 0) {
                    strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
                    book.hasCover = true;
                    break;
                }
            }
        } else {
            Serial.printf("[LIBRARY] Cover extraction failed: %s\n", epub.getError().c_str());
        }
    }
    
    // Extract cover on-demand when browsing (doesn't keep EPUB open)
    void extractCoverOnDemand(BookEntry& book, const char* fullPath) {
        Serial.printf("[LIBRARY] Extracting cover on-demand: %s\n", book.filename);
        
        // Generate cover cache path
        uint32_t hash = 0;
        for (const char* p = fullPath; *p; p++) {
            hash = hash * 31 + *p;
        }
        char coverPath[96];
        snprintf(coverPath, sizeof(coverPath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
        
        // Double-check it's not already cached
        if (SD.exists(coverPath)) {
            strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.println("[LIBRARY] Cover was already cached");
            return;
        }
        
        // Open EPUB temporarily just to extract cover
        EpubParser tempEpub;
        if (!tempEpub.open(fullPath)) {
            Serial.printf("[LIBRARY] Could not open EPUB for cover: %s\n", tempEpub.getError().c_str());
            return;
        }
        
        // Check if it has a cover
        if (!tempEpub.hasCover()) {
            Serial.println("[LIBRARY] EPUB has no cover");
            tempEpub.close();
            return;
        }
        
        // Extract the cover
        if (tempEpub.extractCoverImage(coverPath)) {
            strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.println("[LIBRARY] Cover extracted successfully");
        } else {
            Serial.printf("[LIBRARY] Cover extraction failed: %s\n", tempEpub.getError().c_str());
        }
        
        tempEpub.close();
    }
    
    // =========================================================================
    // FreeRTOS Render Task Loop
    // =========================================================================
    
    void renderTaskLoop() {
        Serial.println("[LIBRARY] Render task started");
        
        while (true) {
            if (updateRequired) {
                updateRequired = false;
                
                // Take mutex to prevent conflicts with input handling
                if (xSemaphoreTake(renderMutex, portMAX_DELAY) == pdTRUE) {
                    renderCurrentPage();
                    xSemaphoreGive(renderMutex);
                }
            }
            
            // Small delay to prevent CPU hogging
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    
    void renderCurrentPage() {
        // Only display cached pages - loading happens on main thread
        if (!cacheValid) {
            Serial.println("[LIBRARY] renderCurrentPage: cache not valid, waiting...");
            return;  // Don't try to load here - main thread handles it
        }
        
        // Validate current page
        if (currentPage >= totalPages) {
            currentPage = totalPages > 0 ? totalPages - 1 : 0;
        }
        if (currentPage < 0) {
            currentPage = 0;
        }
        
        // Draw the page
        if (state == READING) {
            drawReadingPage();
        }
    }
    
    void closeBook() {
        Serial.println("[LIBRARY] Closing book...");
        
        // Save progress before closing
        saveProgress();
        
        // Stop render task
        if (renderTaskHandle) {
            if (renderMutex) {
                xSemaphoreTake(renderMutex, portMAX_DELAY);
            }
            vTaskDelete(renderTaskHandle);
            renderTaskHandle = nullptr;
            if (renderMutex) {
                xSemaphoreGive(renderMutex);
                vSemaphoreDelete(renderMutex);
                renderMutex = nullptr;
            }
        }
        
        // Close EPUB if open
        if (isEpub) {
            epub.close();
        }
        
        // Clear state
        bookIsOpen = false;
        cacheValid = false;
        totalPages = 0;
        currentPage = 0;
        
        state = BROWSER;
    }
    
    // =========================================================================
    // Chapter Loading (with per-chapter temp files)
    // =========================================================================
    
    String getTempFilePath(int chapter) {
        // Per-chapter temp files
        return "/.sumi/.tmp_" + String(chapter) + ".html";
    }
    
    // Synchronous chapter loading for main thread
    bool loadChapterSync(int chapter) {
        Serial.printf("[LIBRARY] ===== loadChapterSync(%d) =====\n", chapter);
        Serial.printf("[LIBRARY] Free heap: %d\n", ESP.getFreeHeap());
        Serial.printf("[LIBRARY] Screen size: %dx%d (landscape=%d)\n", screenW, screenH, landscape);
        
        // Verify sufficient memory available to parse
        int freeHeap = ESP.getFreeHeap();
        if (freeHeap < 50000) {
            Serial.printf("[LIBRARY] ERROR: Not enough memory to parse! Need 50KB, have %d\n", freeHeap);
            return false;
        }
        
        // Update chapter title
        if (isEpub && chapter < (int)chapterTitles.size()) {
            strncpy(chapterTitle, chapterTitles[chapter].c_str(), sizeof(chapterTitle) - 1);
        } else if (!isEpub) {
            strncpy(chapterTitle, currentBook, sizeof(chapterTitle) - 1);
        } else {
            snprintf(chapterTitle, sizeof(chapterTitle), "Chapter %d", chapter + 1);
        }
        
        // Build cache key - MUST match current screen dimensions
        LibReaderSettings& settings = readerSettings.get();
        CacheKey key;
        key.fontSize = (uint8_t)settings.fontSize;
        key.margins = (uint8_t)settings.margins;
        key.lineSpacing = (uint8_t)settings.lineSpacing;
        key.setJustify(settings.justifyText);
        key.screenWidth = getLayoutWidth();
        key.screenHeight = screenH;
        
        Serial.printf("[LIBRARY] Cache key: font=%d, margins=%d, lineSpacing=%d, screen=%dx%d\n",
                      key.fontSize, key.margins, key.lineSpacing, key.screenWidth, key.screenHeight);
        
        // Check cache first
        if (pageCache.hasValidCache(key)) {
            int cachedCount = pageCache.getPageCount(chapter);
            if (cachedCount > 0) {
                totalPages = cachedCount;
                cacheValid = true;
                preloadedPage = -1;
                Serial.printf("[LIBRARY] Cache hit: ch%d has %d pages\n", chapter, totalPages);
                return true;
            }
        }
        
        // Cache miss or invalid - need to parse
        Serial.println("[LIBRARY] Cache miss/invalid, re-parsing chapter...");
        Serial.printf("[LIBRARY] Free heap before layout: %d\n", ESP.getFreeHeap());
        
        // IMPORTANT: Always set page size BEFORE applying settings
        // This ensures contentWidth is calculated correctly
        textLayout.setPageSize(getLayoutWidth(), screenH);
        readerSettings.applyToLayout(textLayout);
        textLayout.setFont(&FreeSans9pt7b);
        textLayout.beginLayout();
        
        Serial.printf("[LIBRARY] Layout configured, starting parse...\n");
        
        int pageCount = 0;
        bool success = true;
        
        if (isEpub) {
            SD.mkdir("/.sumi");
            String tempPath = getTempFilePath(chapter);
            
            Serial.printf("[LIBRARY] Free heap before stream: %d\n", ESP.getFreeHeap());
            
            // Stream from EPUB to temp file
            bool streamOk = epub.streamChapterToFile(chapter, tempPath);
            if (!streamOk) {
                Serial.printf("[LIBRARY] Stream failed: %s\n", epub.getError().c_str());
                textLayout.addParagraph("Chapter unavailable", true);
                success = false;
            } else {
                Serial.printf("[LIBRARY] Free heap before parse: %d\n", ESP.getFreeHeap());
                
                // Check memory again before parsing
                if (ESP.getFreeHeap() < 40000) {
                    Serial.println("[LIBRARY] Low memory, using simple parser");
                    // Fall back to simple line-by-line parsing
                    File tempFile = SD.open(tempPath, FILE_READ);
                    if (tempFile) {
                        String content = tempFile.readString();
                        tempFile.close();
                        
                        // Very simple HTML stripping
                        content.replace("<p>", "\n");
                        content.replace("</p>", "\n");
                        content.replace("<br>", "\n");
                        content.replace("<br/>", "\n");
                        content.replace("<br />", "\n");
                        
                        // Remove all other tags
                        int start = 0;
                        while ((start = content.indexOf('<', start)) >= 0) {
                            int end = content.indexOf('>', start);
                            if (end > start) {
                                content = content.substring(0, start) + content.substring(end + 1);
                            } else {
                                break;
                            }
                        }
                        
                        // Split into paragraphs and add
                        int pos = 0;
                        while (pos < (int)content.length()) {
                            int nextNL = content.indexOf('\n', pos);
                            if (nextNL < 0) nextNL = content.length();
                            
                            String para = content.substring(pos, nextNL);
                            para.trim();
                            if (para.length() > 0) {
                                textLayout.addParagraph(para, false);
                                std::vector<CachedPage> completed = std::move(textLayout.getCompletedPages());
                                for (size_t i = 0; i < completed.size(); i++) {
                                    pageCache.savePage(chapter, pageCount++, completed[i]);
                                }
                                completed.clear();
                                completed.shrink_to_fit();
                            }
                            pos = nextNL + 1;
                        }
                    }
                } else {
                    // Parse with Expat - use heap for completed pages to avoid stack overflow
                    int chapterNum = chapter;
                    int* pCount = &pageCount;
                    
                    Serial.printf("[LIBRARY] Starting Expat parse, free heap: %d\n", ESP.getFreeHeap());
                    
                    bool parseOk = expatParser.parseFile(tempPath, [this, chapterNum, pCount](const String& text, bool isHeader) {
                        if (text.length() > 0) {
                            textLayout.addParagraph(text, isHeader);
                            
                            // Get completed pages and save immediately to free memory
                            std::vector<CachedPage> completed = std::move(textLayout.getCompletedPages());
                            for (size_t i = 0; i < completed.size(); i++) {
                                pageCache.savePage(chapterNum, (*pCount)++, completed[i]);
                            }
                            completed.clear();  // Explicitly free
                            completed.shrink_to_fit();
                        }
                    });
                    
                    Serial.printf("[LIBRARY] Expat parse done, free heap: %d\n", ESP.getFreeHeap());
                    
                    if (!parseOk) {
                        Serial.printf("[LIBRARY] Parse failed: %s\n", expatParser.getError().c_str());
                    }
                }
                SD.remove(tempPath.c_str());
            }
        } else {
            // TXT file
            File file = SD.open(currentBookPath);
            if (file) {
                int fileSize = file.size();
                int readSize = (fileSize < TEXT_BUFFER_SIZE) ? fileSize : TEXT_BUFFER_SIZE;
                
                char* buffer = (char*)malloc(readSize + 1);
                if (buffer) {
                    file.readBytes(buffer, readSize);
                    buffer[readSize] = '\0';
                    
                    String text(buffer);
                    free(buffer);
                    
                    // Process incrementally to avoid large vector
                    textLayout.beginLayout();
                    int start = 0;
                    int len = text.length();
                    while (start < len) {
                        int end = text.indexOf('\n', start);
                        if (end < 0) end = len;
                        
                        String para = text.substring(start, end);
                        para.trim();
                        if (para.length() > 0) {
                            textLayout.addParagraph(para, false);
                            std::vector<CachedPage> completed = std::move(textLayout.getCompletedPages());
                            for (size_t i = 0; i < completed.size(); i++) {
                                pageCache.savePage(chapter, pageCount++, completed[i]);
                            }
                            completed.clear();
                            completed.shrink_to_fit();
                        }
                        start = end + 1;
                    }
                } else {
                    success = false;
                }
                file.close();
            } else {
                success = false;
            }
        }
        
        // Finish layout - use pointer to avoid stack overflow
        CachedPage* lastPage = new CachedPage();
        *lastPage = textLayout.finishLayout();
        if (lastPage->lineCount > 0) {
            pageCache.savePage(chapter, pageCount++, *lastPage);
        }
        delete lastPage;
        
        totalPages = pageCount;
        pageCache.setPageCount(chapter, totalPages);
        pageCache.saveMeta(key, totalChapters);
        cacheValid = totalPages > 0;
        preloadedPage = -1;
        
        Serial.printf("[LIBRARY] Parsed: %d pages, success=%d\n", totalPages, success);
        return success && totalPages > 0;
    }
    
    void loadChapter(int chapter) {
        Serial.printf("[LIBRARY] ===== loadChapter(%d) =====\n", chapter);
        Serial.printf("[LIBRARY] Free heap: %d\n", ESP.getFreeHeap());
        
        // Update chapter title
        if (isEpub && chapter < (int)chapterTitles.size()) {
            strncpy(chapterTitle, chapterTitles[chapter].c_str(), sizeof(chapterTitle) - 1);
        } else if (!isEpub) {
            strncpy(chapterTitle, currentBook, sizeof(chapterTitle) - 1);
        } else {
            snprintf(chapterTitle, sizeof(chapterTitle), "Chapter %d", chapter + 1);
        }
        
        // Build cache key from current settings
        LibReaderSettings& settings = readerSettings.get();
        CacheKey key;
        key.fontSize = (uint8_t)settings.fontSize;
        key.margins = (uint8_t)settings.margins;
        key.lineSpacing = (uint8_t)settings.lineSpacing;
        key.setJustify(settings.justifyText);
        key.screenWidth = getLayoutWidth();
        key.screenHeight = screenH;
        
        // Check cache first
        if (pageCache.hasValidCache(key)) {
            int cachedCount = pageCache.getPageCount(chapter);
            if (cachedCount > 0) {
                totalPages = cachedCount;
                cacheValid = true;
                preloadedPage = -1;
                Serial.printf("[LIBRARY] Cache found, skipping build: ch%d, %d pages\n", chapter, totalPages);
                return;
            }
        }
        
        // Cache miss - need to index
        Serial.printf("[LIBRARY] Cache not found, building...\n");
        cacheValid = false;
        
        // Show indexing screen
        indexingProgress = 0;
        drawIndexingScreen();
        display.display();
        
        // Apply settings to layout
        readerSettings.applyToLayout(textLayout);
        textLayout.setPageSize(getLayoutWidth(), screenH);
        textLayout.setFont(&FreeSans9pt7b);
        textLayout.beginLayout();
        
        int pageCount = 0;
        
        if (isEpub) {
            // ========================================================
            // STREAMING PIPELINE:
            // 1. Stream chapter from ZIP to per-chapter temp file
            // 2. Parse temp file with Expat XML parser
            // 3. Delete temp file
            // ========================================================
            
            // Ensure temp directory exists
            SD.mkdir("/.sumi");
            
            // Per-chapter temp file
            String tempPath = getTempFilePath(chapter);
            Serial.printf("[LIBRARY] Streaming chapter to: %s\n", tempPath.c_str());
            
            // Stream chapter content to temp file
            bool streamOk = epub.streamChapterToFile(chapter, tempPath);
            
            if (!streamOk) {
                Serial.printf("[LIBRARY] Failed to stream chapter: %s\n", epub.getError().c_str());
                textLayout.addParagraph("Chapter content unavailable", true);
                textLayout.addParagraph("Failed to extract chapter from EPUB.");
            } else {
                Serial.printf("[LIBRARY] Streamed temp HTML to %s\n", tempPath.c_str());
                
                indexingProgress = 30;
                drawIndexingScreen();
                display.display(true);
                
                // ========================================================
                // Parse with Expat XML parser
                // ========================================================
                int chapterNum = chapter;
                int* pCount = &pageCount;
                
                bool parseOk = expatParser.parseFile(tempPath, [this, chapterNum, pCount](const String& text, bool isHeader) {
                    if (text.length() > 0) {
                        textLayout.addParagraph(text, isHeader);
                        
                        // Save completed pages immediately to free memory
                        std::vector<CachedPage> completed = std::move(textLayout.getCompletedPages());
                        for (size_t i = 0; i < completed.size(); i++) {
                            pageCache.savePage(chapterNum, (*pCount)++, completed[i]);
                        }
                        completed.clear();
                        completed.shrink_to_fit();
                    }
                });
                
                if (!parseOk) {
                    Serial.printf("[LIBRARY] Parse failed: %s\n", expatParser.getError().c_str());
                } else {
                    Serial.printf("[LIBRARY] Parsed %d paragraphs, %d chars\n", 
                                  expatParser.getParagraphCount(), expatParser.getCharacterCount());
                }
                
                // Clean up temp file
                SD.remove(tempPath.c_str());
            }
        } else {
            // TXT: Load and parse plain text
            File file = SD.open(currentBookPath);
            if (file) {
                int fileSize = file.size();
                int readSize = (fileSize < TEXT_BUFFER_SIZE) ? fileSize : TEXT_BUFFER_SIZE;
                
                char* buffer = (char*)malloc(readSize + 1);
                if (buffer) {
                    file.readBytes(buffer, readSize);
                    buffer[readSize] = '\0';
                    
                    indexingProgress = 30;
                    drawIndexingScreen();
                    display.display(true);
                    
                    String text(buffer);
                    free(buffer);
                    
                    // Process incrementally to avoid large vector on heap
                    textLayout.beginLayout();
                    
                    // Split text into paragraphs and process one at a time
                    int start = 0;
                    int len = text.length();
                    while (start < len) {
                        int end = text.indexOf('\n', start);
                        if (end < 0) end = len;
                        
                        String para = text.substring(start, end);
                        para.trim();
                        if (para.length() > 0) {
                            textLayout.addParagraph(para, false);
                            
                            // Save completed pages immediately
                            std::vector<CachedPage> completed = std::move(textLayout.getCompletedPages());
                            for (size_t i = 0; i < completed.size(); i++) {
                                pageCache.savePage(chapter, pageCount++, completed[i]);
                            }
                            completed.clear();
                            completed.shrink_to_fit();
                        }
                        start = end + 1;
                        
                        if (pageCount % 10 == 0) {
                            indexingProgress = 50 + (start * 40 / len);
                            drawIndexingScreen();
                            display.display(true);
                        }
                    }
                }
                file.close();
            }
        }
        
        // Finish layout and save remaining pages - use heap to avoid stack overflow
        CachedPage* lastPage = new CachedPage();
        *lastPage = textLayout.finishLayout();
        if (lastPage->lineCount > 0) {
            pageCache.savePage(chapter, pageCount++, *lastPage);
        }
        delete lastPage;
        
        // Save metadata
        totalPages = pageCount;
        pageCache.setPageCount(chapter, totalPages);
        pageCache.saveMeta(key, totalChapters);
        
        cacheValid = true;
        preloadedPage = -1;
        
        Serial.printf("[LIBRARY] Indexed %d pages for chapter %d\n", totalPages, chapter);
    }
    
    void saveProgress() {
        pageCache.saveProgress(currentChapter, currentPage);
    }
    
    // =========================================================================
    // Drawing
    // =========================================================================
    
    void draw() {
        // FreeRTOS task handles rendering for READING state
        // Other states are drawn directly here
        switch (state) {
            case BROWSER:
                drawBrowser();
                break;
            case READING:
                // DO NOT set updateRequired here - it causes infinite refresh loop
                // The render task handles page display, triggered by:
                // - Initial openBook() setting updateRequired=true
                // - Page turns in handleInput setting updateRequired=true  
                // - Chapter changes setting updateRequired=true
                break;
            case CHAPTER_SELECT:
                drawChapterSelect();
                break;
            case SETTINGS_MENU:
                drawSettingsMenu();
                break;
            case INFO:
                drawInfo();
                break;
            case INDEXING:
                drawIndexingScreen();
                break;
        }
    }
    
    bool needsFullRefresh() {
        if (state == READING) {
            pagesUntilFullRefresh--;
            if (pagesUntilFullRefresh <= 0) {
                pagesUntilFullRefresh = readerSettings.get().pagesPerFullRefresh;
                return true;
            }
        }
        return false;
    }
    
    void drawBrowser() {
        if (useFlipBrowser) {
            drawFlipBrowser();
        } else {
            drawListBrowser();
        }
    }
    
    // =========================================================================
    // Flippable Cover Browser
    // =========================================================================
    
    void drawFlipBrowser() {
        Serial.printf("[LIBRARY] drawFlipBrowser: cursor=%d, books=%d\n", cursor, (int)books.size());
        
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        
        if (books.size() == 0) {
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(screenW/2 - 80, screenH/2 - 20);
            display.print("No Books");
            display.setFont(&FreeSans9pt7b);
            display.setCursor(screenW/2 - 120, screenH/2 + 20);
            display.print("Add .epub files to /books/");
            return;
        }
        
        BookEntry& book = books[cursor];
        
        // Calculate layout - cover on left, info on right (landscape)
        // or cover on top, info below (portrait)
        int coverX, coverY, coverW, coverH;
        int infoX, infoY, infoW;
        
        if (landscape) {
            // Landscape: cover on left half
            coverW = screenW / 3;
            coverH = screenH - 100;
            coverX = 40;
            coverY = 30;
            infoX = coverX + coverW + 40;
            infoY = 50;
            infoW = screenW - infoX - 40;
        } else {
            // Portrait: cover on top ~40% of screen, info below
            coverW = screenW - 100;  // Leave room for nav arrows
            coverH = screenH * 2 / 5 - 20;  // 40% of height minus margin
            coverX = 50;
            coverY = 20;
            infoX = 30;
            infoY = coverY + coverH + 30;  // More space below cover
            infoW = screenW - 60;
        }
        
        // Draw cover placeholder/frame
        display.drawRect(coverX - 2, coverY - 2, coverW + 4, coverH + 4, GxEPD_BLACK);
        
        // Extract cover on-demand if not cached yet
        if (!book.hasCover && (book.bookType == BookType::EPUB_FILE || book.bookType == BookType::EPUB_FOLDER)) {
            char fullPath[128];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
            extractCoverOnDemand(book, fullPath);
        }
        
        // Try to load and display cover image
        if (book.hasCover && SD.exists(book.coverPath)) {
            drawCoverImage(book.coverPath, coverX, coverY, coverW, coverH);
        } else {
            // Draw nice placeholder with book icon and title
            display.fillRect(coverX, coverY, coverW, coverH, GxEPD_WHITE);
            
            // Draw a simple book icon at top
            int iconX = coverX + coverW/2;
            int iconY = coverY + 50;
            int iconW = 60;
            int iconH = 80;
            
            // Book shape
            display.drawRect(iconX - iconW/2, iconY, iconW, iconH, GxEPD_BLACK);
            display.drawRect(iconX - iconW/2 + 2, iconY + 2, iconW - 4, iconH - 4, GxEPD_BLACK);
            // Spine
            display.drawLine(iconX - iconW/2 + 8, iconY, iconX - iconW/2 + 8, iconY + iconH, GxEPD_BLACK);
            // Page lines
            for (int i = 1; i < 5; i++) {
                display.drawLine(iconX - iconW/2 + 15, iconY + 10 + i*12, 
                                iconX + iconW/2 - 8, iconY + 10 + i*12, GxEPD_BLACK);
            }
            
            // Title text below icon, centered and wrapped
            display.setFont(&FreeSans9pt7b);
            String title = book.title;
            int textY = iconY + iconH + 30;
            int maxCharsPerLine = (coverW - 20) / 9;
            
            // Simple word wrap
            if ((int)title.length() <= maxCharsPerLine) {
                // Single line - center it
                int16_t tx, ty;
                uint16_t tw, th;
                display.getTextBounds(title.c_str(), 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(coverX + (coverW - tw) / 2, textY);
                display.print(title);
            } else {
                // Multi-line
                int line1End = maxCharsPerLine;
                // Find last space before limit
                while (line1End > 0 && title[line1End] != ' ') line1End--;
                if (line1End == 0) line1End = maxCharsPerLine;
                
                String line1 = title.substring(0, line1End);
                String line2 = title.substring(line1End + 1);
                if (line2.length() > maxCharsPerLine) {
                    line2 = line2.substring(0, maxCharsPerLine - 3) + "...";
                }
                
                int16_t tx, ty;
                uint16_t tw, th;
                display.getTextBounds(line1.c_str(), 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(coverX + (coverW - tw) / 2, textY);
                display.print(line1);
                
                display.getTextBounds(line2.c_str(), 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(coverX + (coverW - tw) / 2, textY + 22);
                display.print(line2);
            }
        }
        
        // Draw navigation arrows
        display.setFont(&FreeSansBold12pt7b);
        if (cursor > 0) {
            // Left arrow
            display.setCursor(10, coverY + coverH/2);
            display.print("<");
        }
        if (cursor < (int)books.size() - 1) {
            // Right arrow  
            if (landscape) {
                display.setCursor(coverX + coverW + 15, coverY + coverH/2);
            } else {
                display.setCursor(screenW - 25, coverY + coverH/2);
            }
            display.print(">");
        }
        
        // Draw book info
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(infoX, infoY);
        
        // Truncate title if needed
        String displayTitle = book.title;
        int maxTitleLen = infoW / 10;
        if (displayTitle.length() > maxTitleLen) {
            displayTitle = displayTitle.substring(0, maxTitleLen - 3) + "...";
        }
        display.print(displayTitle);
        
        // Author
        display.setFont(&FreeSans9pt7b);
        if (strlen(book.author) > 0) {
            display.setCursor(infoX, infoY + 30);
            display.print(book.author);
        }
        
        // File info
        display.setCursor(infoX, infoY + 60);
        if (book.bookType == BookType::EPUB_FILE || book.bookType == BookType::EPUB_FOLDER) {
            display.print("EPUB");
        } else if (book.bookType == BookType::TXT) {
            display.print("TXT");
        } else {
            display.print("Folder");
        }
        
        // Size
        char sizeStr[24];
        if (book.size > 1048576) {
            snprintf(sizeStr, sizeof(sizeStr), " - %.1f MB", book.size / 1048576.0f);
        } else if (book.size > 1024) {
            snprintf(sizeStr, sizeof(sizeStr), " - %.0f KB", book.size / 1024.0f);
        } else {
            snprintf(sizeStr, sizeof(sizeStr), " - %lu B", (unsigned long)book.size);
        }
        display.print(sizeStr);
        
        // Reading progress
        if (book.lastChapter > 0 || book.lastPage > 0) {
            display.setCursor(infoX, infoY + 90);
            display.printf("Progress: Ch %d, Pg %d", book.lastChapter + 1, book.lastPage + 1);
            
            // Progress bar
            int barW = infoW - 20;
            int barH = 8;
            int barY = infoY + 105;
            display.drawRect(infoX, barY, barW, barH, GxEPD_BLACK);
            int fillW = barW * book.progress;
            if (fillW > 0) {
                display.fillRect(infoX + 1, barY + 1, fillW - 2, barH - 2, GxEPD_BLACK);
            }
        }
        
        // Book counter
        display.setCursor(infoX, screenH - 60);
        display.printf("Book %d of %d", cursor + 1, (int)books.size());
        
        // Bottom help bar
        display.drawLine(0, screenH - 35, screenW, screenH - 35, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, screenH - 12);
        display.print("< > Flip | OK: Read | UP: List | DOWN: Settings");
    }
    
    void drawCoverImage(const char* path, int x, int y, int maxW, int maxH) {
        // Check file format by reading magic bytes
        File coverFile = SD.open(path, FILE_READ);
        if (!coverFile || coverFile.size() < 4) {
            if (coverFile) coverFile.close();
            drawCoverPlaceholder(x, y, maxW, maxH, "No Cover");
            return;
        }
        
        uint8_t magic[4];
        coverFile.read(magic, 4);
        coverFile.close();
        
        // Check for JPEG (FFD8FF) or PNG (89504E47)
        bool isJpeg = (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF);
        bool isPng = (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47);
        
        if (!isJpeg && !isPng) {
            Serial.printf("[LIBRARY] Unknown cover format: %02X %02X %02X %02X\n", 
                         magic[0], magic[1], magic[2], magic[3]);
            drawCoverPlaceholder(x, y, maxW, maxH, "Unknown");
            return;
        }
        
        if (isPng) {
            // PNG not supported yet - show placeholder
            Serial.println("[LIBRARY] PNG cover - not decoded");
            drawCoverPlaceholder(x, y, maxW, maxH, "PNG");
            return;
        }
        
        // JPEG decoding
        TJpgDec.setJpgScale(1);
        TJpgDec.setCallback(_jpgDrawCallback);
        
        // Get JPEG dimensions
        uint16_t jpgW = 0, jpgH = 0;
        if (TJpgDec.getFsJpgSize(&jpgW, &jpgH, path, SD) != JDR_OK) {
            Serial.printf("[LIBRARY] Failed to get JPEG size: %s\n", path);
            drawCoverPlaceholder(x, y, maxW, maxH, "Error");
            return;
        }
        
        Serial.printf("[LIBRARY] Cover JPEG: %dx%d -> %dx%d\n", jpgW, jpgH, maxW, maxH);
        
        // Calculate scale (TJpgDec supports 1, 2, 4, 8)
        int scale = 1;
        if (jpgW > (uint16_t)maxW * 2 || jpgH > (uint16_t)maxH * 2) scale = 2;
        if (jpgW > (uint16_t)maxW * 4 || jpgH > (uint16_t)maxH * 4) scale = 4;
        if (jpgW > (uint16_t)maxW * 8 || jpgH > (uint16_t)maxH * 8) scale = 8;
        
        TJpgDec.setJpgScale(scale);
        
        int scaledW = jpgW / scale;
        int scaledH = jpgH / scale;
        
        // Center the image
        _coverOffsetX = x + (maxW - scaledW) / 2;
        _coverOffsetY = y + (maxH - scaledH) / 2;
        if (_coverOffsetX < x) _coverOffsetX = x;
        if (_coverOffsetY < y) _coverOffsetY = y;
        
        // Clear area and decode
        display.fillRect(x, y, maxW, maxH, GxEPD_WHITE);
        
        JRESULT result = TJpgDec.drawFsJpg(0, 0, path, SD);
        
        if (result != JDR_OK) {
            Serial.printf("[LIBRARY] JPEG decode failed: %d\n", result);
            drawCoverPlaceholder(x, y, maxW, maxH, "Error");
        }
    }
    
    void drawCoverPlaceholder(int x, int y, int maxW, int maxH, const char* label) {
        // Draw decorative book cover placeholder
        display.fillRect(x, y, maxW, maxH, GxEPD_WHITE);
        display.drawRect(x, y, maxW, maxH, GxEPD_BLACK);
        display.drawRect(x + 1, y + 1, maxW - 2, maxH - 2, GxEPD_BLACK);
        
        // Spine line
        display.drawLine(x + 15, y, x + 15, y + maxH, GxEPD_BLACK);
        
        // Book icon in center
        int iconSize = (maxW < maxH) ? maxW / 4 : maxH / 4;
        int iconX = x + (maxW - iconSize) / 2;
        int iconY = y + maxH / 3;
        
        display.fillRect(iconX, iconY, iconSize, iconSize * 1.3, GxEPD_BLACK);
        display.fillRect(iconX + 3, iconY + 3, iconSize - 6, iconSize * 1.3 - 6, GxEPD_WHITE);
        
        for (int i = 0; i < 4; i++) {
            int lineY = iconY + 8 + i * (iconSize / 5);
            display.drawLine(iconX + 6, lineY, iconX + iconSize - 6, lineY, GxEPD_BLACK);
        }
        
        if (label && strlen(label) > 0) {
            display.setFont(&FreeSans9pt7b);
            display.setCursor(x + maxW/2 - 20, y + maxH - 20);
            display.print(label);
        }
    }
    
    // =========================================================================
    // Traditional List Browser
    // =========================================================================
    
    void drawListBrowser() {
        Serial.printf("[LIBRARY] drawListBrowser: cursor=%d, scrollOffset=%d, books=%d\n", 
                      cursor, scrollOffset, (int)books.size());
        
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Library");
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(120, 35);
        String pathDisplay = String(currentPath);
        if (pathDisplay.length() > 40) {
            pathDisplay = "..." + pathDisplay.substring(pathDisplay.length() - 37);
        }
        display.print(pathDisplay);
        
        display.drawLine(0, 48, screenW, 48, GxEPD_BLACK);
        
        if (books.size() == 0) {
            display.setCursor(20, 100);
            display.print("No books found.");
            display.setCursor(20, 130);
            display.print("Add .txt or .epub files to /books/");
            return;
        }
        
        int y = 75;
        for (int i = scrollOffset; i < (int)books.size() && i < scrollOffset + itemsPerPage; i++) {
            bool selected = (i == cursor);
            
            if (selected) {
                display.fillRect(10, y - 18, screenW - 20, 48, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            }
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(20, y + 5);
            
            // Show type indicator
            if (books[i].isDirectory && books[i].bookType != BookType::EPUB_FOLDER) {
                display.print("[DIR]");
            } else if (books[i].bookType == BookType::EPUB_FOLDER || 
                       books[i].bookType == BookType::EPUB_FILE) {
                display.print("[EPB]");
            } else {
                display.print("[TXT]");
            }
            
            display.setCursor(85, y + 5);
            char truncTitle[40];
            strncpy(truncTitle, books[i].title, 35);
            truncTitle[35] = '\0';
            if (strlen(books[i].title) > 35) strcat(truncTitle, "...");
            display.print(truncTitle);
            
            if (!books[i].isDirectory || books[i].bookType == BookType::EPUB_FOLDER) {
                char sizeStr[16];
                if (books[i].size > 1048576) {
                    snprintf(sizeStr, sizeof(sizeStr), "%.1fMB", books[i].size / 1048576.0f);
                } else if (books[i].size > 1024) {
                    snprintf(sizeStr, sizeof(sizeStr), "%.1fKB", books[i].size / 1024.0f);
                } else {
                    snprintf(sizeStr, sizeof(sizeStr), "%luB", (unsigned long)books[i].size);
                }
                display.setCursor(screenW - 90, y + 5);
                display.print(sizeStr);
            }
            
            display.setTextColor(GxEPD_BLACK);
            y += 50;
        }
        
        // Scrollbar
        if ((int)books.size() > itemsPerPage) {
            int sbHeight = screenH - 120;
            int sbY = 60;
            int thumbHeight = sbHeight * itemsPerPage / (int)books.size();
            if (thumbHeight < 20) thumbHeight = 20;
            int maxScroll = (int)books.size() - itemsPerPage;
            if (maxScroll < 1) maxScroll = 1;
            int thumbY = sbY + (sbHeight - thumbHeight) * scrollOffset / maxScroll;
            
            display.drawRect(screenW - 10, sbY, 8, sbHeight, GxEPD_BLACK);
            display.fillRect(screenW - 9, thumbY, 6, thumbHeight, GxEPD_BLACK);
        }
        
        display.drawLine(0, screenH - 45, screenW, screenH - 45, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, screenH - 18);
        display.printf("%d items | UP/DOWN: Nav | OK: Open | DOWN: Flip view", (int)books.size());
    }
    
    void drawReadingPage() {
        Serial.printf("[READER] drawReadingPage: page=%d/%d, ch=%d, cacheValid=%d\n",
                      currentPage, totalPages, currentChapter, cacheValid);
        
        // Clear first render flag (no blank screen needed - just render directly)
        if (firstRenderAfterOpen) {
            Serial.println("[READER] First render after open");
            firstRenderAfterOpen = false;
        }
        
        if (!cacheValid) {
            // Simple minimal loading - just dots
            display.setFullWindow();
            display.firstPage();
            do {
                display.fillScreen(GxEPD_WHITE);
                display.setFont(&FreeSansBold12pt7b);
                display.setTextColor(GxEPD_BLACK);
                display.setCursor(screenW/2 - 15, screenH/2);
                display.print("...");
            } while (display.nextPage());
            return;
        }
        
        // Validate page number after chapter load
        if (currentPage >= totalPages || currentPage < 0) {
            currentPage = totalPages > 0 ? 0 : 0;
            Serial.printf("[READER] Corrected page to %d\n", currentPage);
        }
        
        // Use heap allocation to avoid stack overflow (CachedPage is ~14KB)
        CachedPage* page = new CachedPage();
        if (!page) {
            Serial.println("[READER] ERROR: Failed to allocate CachedPage");
            display.setFullWindow();
            display.firstPage();
            do {
                display.fillScreen(GxEPD_WHITE);
                display.setFont(&FreeSans9pt7b);
                display.setTextColor(GxEPD_BLACK);
                display.setCursor(20, 100);
                display.print("Memory error");
            } while (display.nextPage());
            return;
        }
        
        if (!pageCache.loadPage(currentChapter, currentPage, *page)) {
            delete page;
            Serial.println("[READER] ERROR: Failed to load page from cache");
            display.setFullWindow();
            display.firstPage();
            do {
                display.fillScreen(GxEPD_WHITE);
                display.setFont(&FreeSans9pt7b);
                display.setTextColor(GxEPD_BLACK);
                display.setCursor(20, 100);
                display.print("Error loading page");
            } while (display.nextPage());
            return;
        }
        
        // Validate page data
        if (page->lineCount > CACHE_MAX_LINES_PAGE) {
            Serial.printf("[READER] WARNING: lineCount=%d exceeds max, clamping\n", page->lineCount);
            page->lineCount = CACHE_MAX_LINES_PAGE;
        }
        
        Serial.printf("[READER] Page loaded: %d lines\n", page->lineCount);
        
        // Log first line position for debugging
        if (page->lineCount > 0 && page->lines[0].wordCount > 0) {
            Serial.printf("[READER] First word: xPos=%d, yPos=%d, text='%.15s'\n",
                          page->lines[0].words[0].xPos, page->lines[0].yPos,
                          page->lines[0].words[0].text);
        }
        
        // Determine refresh mode
        RefreshMode mode = readerSettings.getRefreshMode(pagesUntilHalfRefresh, pagesUntilFullRefresh);
        
        Serial.printf("[READER] Drawing page with screenW=%d, screenH=%d, landscape=%d\n", 
                      screenW, screenH, landscape);
        
        // Calculate page band info for paged rendering
        // With rotation 3 and 480 buffer: page 0 = Y 0-479, page 1 = Y 480-799
        const int pageHeight = DISPLAY_BUFFER_HEIGHT;  // 480
        const int totalLogicalHeight = screenH;  // 800 in portrait
        const int numPages = (totalLogicalHeight + pageHeight - 1) / pageHeight;  // ceil division
        
        Serial.printf("[READER] Paged rendering: %d pages (buffer=%d, screen=%d)\n", 
                      numPages, pageHeight, totalLogicalHeight);
        
        // Always use full window for reading to prevent ghosting
        display.setFullWindow();
        int pageNum = 0;
        display.firstPage();
        do {
            // Calculate Y range for current page band
            int bandYStart = pageNum * pageHeight;
            int bandYEnd = bandYStart + pageHeight;
            
            // Always fill screen white (GxEPD2 clips to current band)
            display.fillScreen(GxEPD_WHITE);
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(GxEPD_BLACK);
            
            // Draw only text lines within this page band
            for (int i = 0; i < page->lineCount; i++) {
                const CachedLine& line = page->lines[i];
                
                // Check if line is within current page band (with some margin for glyph height)
                int lineY = line.yPos;
                int lineTop = lineY - 25;  // Approximate glyph ascent
                int lineBottom = lineY + 5;  // Approximate glyph descent
                
                // Skip lines completely outside this band
                if (lineBottom < bandYStart || lineTop >= bandYEnd) {
                    continue;
                }
                
                // Draw words on this line
                for (int j = 0; j < line.wordCount; j++) {
                    const CachedWord& word = line.words[j];
                    display.setCursor(word.xPos, lineY);
                    display.print(word.text);
                }
            }
            
            // Draw status bar (only on the band that contains it)
            int statusBarY = screenH - 28;
            if (statusBarY >= bandYStart && statusBarY < bandYEnd) {
                drawStatusBarInPage();
            }
            
            pageNum++;
        } while (display.nextPage());
        
        delete page;  // Free after rendering complete
        
        // Note: With full buffer mode, we don't need double refresh tricks
        // The display handles refresh properly
        
        Serial.printf("[READER] Page %d/%d rendered (mode=%d)\n", currentPage + 1, totalPages, (int)mode);
        
        // Save last book info for sleep screen
        saveLastBookInfo();
    }
    
    // Draw status bar during page render loop
    void drawStatusBarInPage() {
        LibReaderSettings& settings = readerSettings.get();
        int barY = screenH - 28;
        
        display.drawLine(0, barY - 7, screenW, barY - 7, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        
        // Page info (centered)
        if (settings.showProgress) {
            char pageStr[32];
            snprintf(pageStr, sizeof(pageStr), "%d/%d", currentPage + 1, totalPages);
            
            int16_t x1, y1;
            uint16_t tw, th;
            display.getTextBounds(pageStr, 0, 0, &x1, &y1, &tw, &th);
            display.setCursor((screenW - tw) / 2, barY + 9);
            display.print(pageStr);
        }
    }
    
    // =========================================================================
    // Last Book Info (for sleep screen)
    // =========================================================================
    
    void saveLastBookInfo() {
        LastBookInfo info;
        info.magic = 0x4C415354;  // "LAST"
        strncpy(info.title, currentBook, sizeof(info.title) - 1);
        info.chapter = currentChapter;
        info.page = currentPage;
        info.totalPages = totalPages;
        info.progress = totalPages > 0 ? (float)currentPage / totalPages : 0.0f;
        
        // Get author and cover from current epub
        if (isEpub) {
            strncpy(info.author, epub.getAuthor().c_str(), sizeof(info.author) - 1);
            
            // Get cover path
            String coverPath = getCoverCachePath(currentBookPath);
            if (SD.exists(coverPath)) {
                strncpy(info.coverPath, coverPath.c_str(), sizeof(info.coverPath) - 1);
            } else {
                info.coverPath[0] = '\0';
            }
        } else {
            info.author[0] = '\0';
            info.coverPath[0] = '\0';
        }
        
        // Save to file
        File file = SD.open(LAST_BOOK_PATH, FILE_WRITE);
        if (file) {
            file.write((uint8_t*)&info, sizeof(LastBookInfo));
            file.close();
            Serial.printf("[LIBRARY] Saved last book info: %s\n", info.title);
        }
    }
    
    // Static function to get last book info (for sleep screen)
    static bool getLastBookInfo(LastBookInfo& info) {
        if (!SD.exists(LAST_BOOK_PATH)) {
            return false;
        }
        
        File file = SD.open(LAST_BOOK_PATH, FILE_READ);
        if (!file) {
            return false;
        }
        
        size_t read = file.read((uint8_t*)&info, sizeof(LastBookInfo));
        file.close();
        
        if (read != sizeof(LastBookInfo) || info.magic != 0x4C415354) {
            return false;
        }
        
        return true;
    }
    
    // Static function to draw cover on sleep screen
    static void drawSleepCover(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& disp, int w, int h) {
        LastBookInfo info;
        if (!getLastBookInfo(info)) {
            // No last book, draw default sleep
            disp.fillScreen(GxEPD_WHITE);
            disp.setFont(&FreeSansBold12pt7b);
            disp.setTextColor(GxEPD_BLACK);
            disp.setCursor(w/2 - 50, h/2);
            disp.print("ZZZ...");
            return;
        }
        
        disp.fillScreen(GxEPD_WHITE);
        disp.setTextColor(GxEPD_BLACK);
        
        // Draw cover frame
        int coverW = w / 3;
        int coverH = h - 80;
        int coverX = (w - coverW) / 2;
        int coverY = 20;
        
        disp.drawRect(coverX - 2, coverY - 2, coverW + 4, coverH + 4, GxEPD_BLACK);
        
        // Try to load cover
        if (strlen(info.coverPath) > 0 && SD.exists(info.coverPath)) {
            // JPEG decoder not implemented - show placeholder
            disp.fillRect(coverX, coverY, coverW, coverH, GxEPD_WHITE);
            disp.setFont(&FreeSans9pt7b);
            disp.setCursor(coverX + 10, coverY + coverH/2);
            disp.print("[Cover]");
        } else {
            // Draw title in cover area
            disp.fillRect(coverX, coverY, coverW, coverH, GxEPD_WHITE);
            disp.setFont(&FreeSans9pt7b);
            disp.setCursor(coverX + 10, coverY + coverH/2);
            
            // Word wrap
            String title = info.title;
            int maxChars = (coverW - 20) / 8;
            if (title.length() > maxChars) {
                disp.print(title.substring(0, maxChars));
                disp.setCursor(coverX + 10, coverY + coverH/2 + 20);
                disp.print(title.substring(maxChars, maxChars * 2));
            } else {
                disp.print(title);
            }
        }
        
        // Draw reading progress
        int infoY = coverY + coverH + 15;
        disp.setFont(&FreeSans9pt7b);
        disp.setCursor(coverX, infoY);
        disp.printf("Reading: %s", info.title);
        
        if (strlen(info.author) > 0) {
            disp.setCursor(coverX, infoY + 22);
            disp.printf("by %s", info.author);
        }
        
        // Progress bar
        int barW = coverW;
        int barH = 8;
        int barY = h - 30;
        disp.drawRect(coverX, barY, barW, barH, GxEPD_BLACK);
        int fillW = (int)(barW * info.progress);
        if (fillW > 2) {
            disp.fillRect(coverX + 1, barY + 1, fillW - 2, barH - 2, GxEPD_BLACK);
        }
        
        disp.setCursor(coverX, barY + 20);
        disp.printf("%d%% complete", (int)(info.progress * 100));
    }
    
    void drawIndexingScreen() {
        display.fillScreen(GxEPD_WHITE);
        
        int centerY = screenH / 2;
        
        display.setFont(&FreeSansBold12pt7b);
        const char* title = "Indexing Book...";
        int16_t tx, ty;
        uint16_t tw, th;
        display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, centerY - 40);
        display.print(title);
        
        // Progress bar
        int barW = 400, barH = 20;
        int barX = (screenW - barW) / 2;
        int barY = centerY;
        
        display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
        
        int fillW = barW * indexingProgress / 100;
        if (fillW > 4) {
            display.fillRect(barX + 2, barY + 2, fillW - 4, barH - 4, GxEPD_BLACK);
        }
        
        // Percentage
        display.setFont(&FreeSans9pt7b);
        char pctText[16];
        snprintf(pctText, sizeof(pctText), "%d%%", indexingProgress);
        display.getTextBounds(pctText, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, centerY + 50);
        display.print(pctText);
        
        // Info
        const char* info = "This only happens once per book";
        display.getTextBounds(info, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, centerY + 80);
        display.print(info);
    }
    
    void drawChapterSelect() {
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Chapters");
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(screenW - 140, 35);
        display.printf("%d of %d chapters", currentChapter + 1, totalChapters);
        
        display.drawLine(0, 48, screenW, 48, GxEPD_BLACK);
        
        if (totalChapters <= 1) {
            display.setCursor(20, 100);
            display.print("This book has only one chapter.");
            display.setCursor(20, 140);
            display.print("Press OK for reader settings.");
            display.setCursor(20, 180);
            display.print("Press BACK to return to reading.");
            return;
        }
        
        int maxVisible = 8;
        int y = 75;
        
        for (int i = chapterScrollOffset; i < totalChapters && i < chapterScrollOffset + maxVisible; i++) {
            bool selected = (i == chapterCursor);
            bool isCurrent = (i == currentChapter);
            
            if (selected) {
                display.fillRect(10, y - 18, screenW - 20, 48, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            }
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(20, y + 5);
            
            // Chapter number
            display.printf("%2d. ", i + 1);
            
            // Get chapter title
            String title = (i < (int)chapterTitles.size()) ? chapterTitles[i] : "Chapter " + String(i + 1);
            if (title.length() > 45) {
                title = title.substring(0, 42) + "...";
            }
            
            if (isCurrent) {
                display.print("> ");
            }
            display.print(title);
            
            display.setTextColor(GxEPD_BLACK);
            y += 50;
        }
        
        // Scrollbar for long TOCs
        if (totalChapters > maxVisible) {
            int sbHeight = screenH - 120;
            int sbY = 60;
            int thumbHeight = sbHeight * maxVisible / totalChapters;
            if (thumbHeight < 20) thumbHeight = 20;
            int maxScroll = totalChapters - maxVisible;
            if (maxScroll < 1) maxScroll = 1;
            int thumbY = sbY + (sbHeight - thumbHeight) * chapterScrollOffset / maxScroll;
            
            display.drawRect(screenW - 10, sbY, 8, sbHeight, GxEPD_BLACK);
            display.fillRect(screenW - 9, thumbY, 6, thumbHeight, GxEPD_BLACK);
        }
        
        display.drawLine(0, screenH - 45, screenW, screenH - 45, GxEPD_BLACK);
        display.setCursor(20, screenH - 18);
        display.print("UP/DOWN: Select | OK: Jump to chapter | BACK: Cancel");
    }
    
    void drawSettingsMenu() {
        LibReaderSettings& settings = readerSettings.get();
        bool isLandscape = (settingsManager.display.orientation == 0);
        
        // Header
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Reader Settings");
        
        display.drawLine(0, 48, screenW, 48, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        
        // Show current book info if reading
        int startY = 75;
        if (bookIsOpen && strlen(currentBook) > 0) {
            display.setCursor(20, 68);
            char truncTitle[30];
            strncpy(truncTitle, currentBook, 29);
            truncTitle[29] = '\0';
            display.printf("Reading: %s", truncTitle);
            display.setCursor(20, 88);
            display.printf("Page %d/%d | Ch %d/%d", 
                           currentPage + 1, totalPages, currentChapter + 1, totalChapters);
            display.drawLine(0, 100, screenW, 100, GxEPD_BLACK);
            startY = 110;
        }
        
        // Menu items
        const char* items[] = {
            "Orientation",
            "Font Size",
            "Margins", 
            "Line Spacing",
            "Justify Text",
            "Go to Chapter...",
            "Clear Cache",
            "< Back"
        };
        
        // Values for each item
        const char* orientVal = isLandscape ? "Landscape" : "Portrait";
        const char* fontVal = LibReaderSettings::getFontSizeName(settings.fontSize);
        const char* marginVal = LibReaderSettings::getMarginName(settings.margins);
        const char* spacingVal = LibReaderSettings::getSpacingName(settings.lineSpacing);
        const char* justifyVal = settings.justifyText ? "On" : "Off";
        
        const char* values[SET_COUNT];
        values[SET_ORIENTATION] = orientVal;
        values[SET_FONT_SIZE] = fontVal;
        values[SET_MARGINS] = marginVal;
        values[SET_LINE_SPACING] = spacingVal;
        values[SET_JUSTIFY] = justifyVal;
        values[SET_CHAPTERS] = "";
        values[SET_CLEAR_CACHE] = "";
        values[SET_BACK] = "";
        
        int y = startY;
        int itemHeight = landscape ? 42 : 48;
        
        // Hide "Go to Chapter" if not reading a book with chapters
        bool showChapters = bookIsOpen && totalChapters > 1;
        
        for (int i = 0; i < SET_COUNT; i++) {
            // Skip chapters option if not applicable
            if (i == SET_CHAPTERS && !showChapters) continue;
            
            bool selected = (i == settingsCursor);
            
            if (selected) {
                display.fillRoundRect(12, y - 2, screenW - 24, itemHeight - 6, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(12, y - 2, screenW - 24, itemHeight - 6, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setCursor(25, y + 20);
            display.print(items[i]);
            
            if (strlen(values[i]) > 0) {
                // Right-align the value with arrows
                char valStr[24];
                snprintf(valStr, sizeof(valStr), "< %s >", values[i]);
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(valStr, 0, 0, &x1, &y1, &w, &h);
                display.setCursor(screenW - 30 - w, y + 20);
                display.print(valStr);
            }
            
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight;
        }
        
        // Footer
        display.drawLine(0, screenH - 38, screenW, screenH - 38, GxEPD_BLACK);
        display.setCursor(20, screenH - 14);
        display.print("OK: Change | BACK: Return");
    }
    
    void drawInfo() {
        if (cursor >= (int)books.size()) return;
        
        const BookEntry& book = books[cursor];
        
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Book Info");
        display.drawLine(0, 48, screenW, 48, GxEPD_BLACK);
        
        int cardX = 30, cardY = 70;
        int cardW = screenW - 60, cardH = 280;
        
        display.drawRoundRect(cardX, cardY, cardW, cardH, 10, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(cardX + 20, cardY + 40);
        display.print(book.title);
        
        display.setFont(&FreeSans9pt7b);
        
        display.setCursor(cardX + 20, cardY + 80);
        display.printf("Filename: %s", book.filename);
        
        display.setCursor(cardX + 20, cardY + 110);
        if (book.size > 1048576) {
            display.printf("Size: %.2f MB", book.size / 1048576.0f);
        } else {
            display.printf("Size: %.2f KB", book.size / 1024.0f);
        }
        
        display.setCursor(cardX + 20, cardY + 140);
        switch (book.bookType) {
            case BookType::TXT:
                display.print("Format: Plain Text");
                break;
            case BookType::EPUB_FILE:
                display.print("Format: EPUB (ZIP)");
                break;
            case BookType::EPUB_FOLDER:
                display.print("Format: EPUB (Extracted)");
                break;
            default:
                display.print("Format: Unknown");
        }
        
        display.setCursor(cardX + 20, cardY + 200);
        display.print("Press OK or BACK to return");
    }
    
    ViewState getState() const { return state; }
    
private:
    ViewState state;
    std::vector<BookEntry> books;
    int cursor;
    int scrollOffset;
    int screenW, screenH;
    bool landscape;
    int itemsPerPage;
    char currentPath[128];
    char currentBook[64];
    char currentBookPath[128];
    char chapterTitle[48];
    int currentPage;
    int totalPages;
    int currentChapter;
    int totalChapters;
    int chapterCursor;
    int chapterScrollOffset;
    int settingsCursor;
    int pagesUntilFullRefresh;
    
    // FreeRTOS task for rendering
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
    
    // EPUB state
    bool isEpub;
    EpubParser epub;
    std::vector<String> chapterTitles;
    ExpatHtmlParser expatParser;  // Expat-based HTML parser
    
    // Reading stats
    ReadingStats stats;
    
    // Flippable browser mode
    bool useFlipBrowser;
    
    // Track if a book is currently open
    bool bookIsOpen;
    
    // Track first render after opening book (needs display reset)
    bool firstRenderAfterOpen;
    
    // Half-refresh tracking (three-level refresh)
    int pagesUntilHalfRefresh;
};

// Global instance

// =========================================================================
// Public API for Sleep Screen Cover Display
// =========================================================================

/**
 * Draw current book cover on sleep screen
 * Call this from main.cpp when going to sleep to show reading progress
 */
inline void drawLibrarySleepScreen(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& disp, int w, int h) {
    LibraryApp::drawSleepCover(disp, w, h);
}

/**
 * Check for last book to show on sleep
 */
inline bool hasLastBookForSleep() {
    LastBookInfo info;
    return LibraryApp::getLastBookInfo(info);
}


extern LibraryApp libraryApp;

#endif // FEATURE_READER
#endif // SUMI_PLUGIN_LIBRARY_H
