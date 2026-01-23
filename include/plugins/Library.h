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
#include "core/ZipReader.h"
#include "core/ReaderSettings.h"
#include "core/PluginHelpers.h"
#include "core/BatteryMonitor.h"
#include "core/SettingsManager.h"
#include "core/KOSync.h"
#include "core/PowerManager.h"
#include <TJpg_Decoder.h>

// External display instance
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern SettingsManager settingsManager;

// JPEG cover rendering globals
static int _coverOffsetX = 0;
static int _coverOffsetY = 0;
static int _coverMaxX = 9999;  // Right edge for clipping
static int _coverMaxY = 9999;  // Bottom edge for clipping

// TJpg_Decoder callback - draws pixels to e-ink display with dithering
static int _jpgCallbackCount = 0;
static bool _jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    _jpgCallbackCount++;
    // Only log first block for debugging (reduced verbosity)
    // if (_jpgCallbackCount == 1) {
    //     Serial.printf("[JPG] Decoding: x=%d y=%d w=%d h=%d\n", x, y, w, h);
    // }
    
    // Yield occasionally to prevent watchdog and allow stack to unwind
    if (_jpgCallbackCount % 500 == 0) {
        yield();
    }
    
    // Apply offset
    int drawX = x + _coverOffsetX;
    int drawY = y + _coverOffsetY;
    
    // Draw each pixel, converting RGB565 to black/white with ordered dithering
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = drawX + i;
            int py = drawY + j;
            
            // Clip to bounds
            if (px < _coverOffsetX || px >= _coverMaxX) continue;
            if (py < _coverOffsetY || py >= _coverMaxY) continue;
            
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
#define LIBRARY_MAX_BOOKS 200
#define TEXT_BUFFER_SIZE 16384

// =============================================================================
// Reading Statistics
// =============================================================================
// Bookmark Structure
// =============================================================================
#define MAX_BOOKMARKS_PER_BOOK 20
#define BOOKMARKS_PATH "/.sumi/books"

struct Bookmark {
    int chapter;
    int page;
    uint32_t timestamp;        // When created (millis at save time, later RTC)
    char label[32];            // User label or auto "Page X"
    
    Bookmark() : chapter(0), page(0), timestamp(0) {
        memset(label, 0, sizeof(label));
    }
    
    void serialize(File& f) const {
        f.write((uint8_t*)&chapter, sizeof(chapter));
        f.write((uint8_t*)&page, sizeof(page));
        f.write((uint8_t*)&timestamp, sizeof(timestamp));
        f.write((uint8_t*)label, sizeof(label));
    }
    
    bool deserialize(File& f) {
        if (f.read((uint8_t*)&chapter, sizeof(chapter)) != sizeof(chapter)) return false;
        if (f.read((uint8_t*)&page, sizeof(page)) != sizeof(page)) return false;
        if (f.read((uint8_t*)&timestamp, sizeof(timestamp)) != sizeof(timestamp)) return false;
        if (f.read((uint8_t*)label, sizeof(label)) != sizeof(label)) return false;
        return true;
    }
};

struct BookmarkList {
    uint32_t magic = 0x424D4152;  // "BMAR"
    uint8_t count = 0;
    Bookmark bookmarks[MAX_BOOKMARKS_PER_BOOK];
    
    bool add(int chapter, int page, const char* label = nullptr) {
        if (count >= MAX_BOOKMARKS_PER_BOOK) return false;
        
        // Check for duplicate
        for (int i = 0; i < count; i++) {
            if (bookmarks[i].chapter == chapter && bookmarks[i].page == page) {
                return false;  // Already bookmarked
            }
        }
        
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
    
    bool remove(int index) {
        if (index < 0 || index >= count) return false;
        
        // Shift remaining bookmarks
        for (int i = index; i < count - 1; i++) {
            bookmarks[i] = bookmarks[i + 1];
        }
        count--;
        return true;
    }
    
    int find(int chapter, int page) const {
        for (int i = 0; i < count; i++) {
            if (bookmarks[i].chapter == chapter && bookmarks[i].page == page) {
                return i;
            }
        }
        return -1;
    }
};

// =============================================================================
// Enhanced Reading Statistics (persistent)
// =============================================================================
#define STATS_PATH "/.sumi/reading_stats.bin"

struct ReadingStats {
    uint32_t magic = 0x53544154;  // "STAT"
    uint32_t totalPagesRead = 0;
    uint32_t totalMinutesRead = 0;
    uint32_t totalBooksFinished = 0;
    uint32_t longestStreak = 0;        // Consecutive days
    uint32_t currentStreak = 0;
    uint32_t lastReadDate = 0;         // Days since epoch (for streak tracking)
    uint32_t sessionPagesRead = 0;
    uint32_t sessionStartTime = 0;
    
    // Per-book stats (stored in book's cache folder)
    uint32_t bookPagesRead = 0;
    uint32_t bookMinutesRead = 0;
    uint32_t bookStartDate = 0;
    
    void startSession() {
        sessionPagesRead = 0;
        sessionStartTime = millis();
    }
    
    void recordPageTurn() {
        totalPagesRead++;
        sessionPagesRead++;
        bookPagesRead++;
    }
    
    void endSession() {
        uint32_t sessionMins = getSessionMinutes();
        totalMinutesRead += sessionMins;
        bookMinutesRead += sessionMins;
    }
    
    uint32_t getSessionMinutes() const {
        return (millis() - sessionStartTime) / 60000;
    }
    
    void save() {
        File f = SD.open(STATS_PATH, FILE_WRITE);
        if (f) {
            endSession();  // Update totals before saving
            f.write((uint8_t*)this, sizeof(ReadingStats));
            f.close();
            startSession();  // Restart session timer
        }
    }
    
    void load() {
        File f = SD.open(STATS_PATH, FILE_READ);
        if (f) {
            f.read((uint8_t*)this, sizeof(ReadingStats));
            f.close();
            if (magic != 0x53544154) {
                // Invalid file, reset
                *this = ReadingStats();
            }
        }
        startSession();
    }
};

// =============================================================================
// KOReader Sync Structure
// =============================================================================
struct KOSyncProgress {
    char document[128];        // Book identifier (usually filename hash)
    float progress;            // 0.0 - 1.0
    int page;
    uint32_t timestamp;        // Unix timestamp
    char device[32];           // Device name
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

// Last book info for sleep screen and boot-to-last-book
struct LastBookInfo {
    uint32_t magic = 0x4C415354;  // "LAST"
    char title[64];
    char author[48];
    char coverPath[96];
    char bookPath[128];    // NEW: Full path to the book file for resume
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
        BOOKMARK_SELECT, // NEW: View/manage bookmarks
        READING_STATS,   // NEW: View reading statistics
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
        SET_BOOKMARKS,      // NEW: View bookmarks
        SET_ADD_BOOKMARK,   // NEW: Add bookmark at current position
        SET_STATS,          // NEW: View reading stats
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
    
    // Get the appropriate font for reading based on settings
    const GFXfont* getReaderFont() {
        LibReaderSettings& settings = readerSettings.get();
        switch (settings.fontSize) {
            case FontSize::SMALL:  return &FreeSans9pt7b;
            case FontSize::MEDIUM: return &FreeSans12pt7b;
            case FontSize::LARGE:  return &FreeSans12pt7b;  // Use 12pt for large too
            default:               return &FreeSans12pt7b;
        }
    }
    
    // Apply font settings to text layout
    void applyFontSettings() {
        const GFXfont* font = getReaderFont();
        textLayout.setFont(font);
        display.setFont(font);
        
        LibReaderSettings& settings = readerSettings.get();
        int lineHeight = settings.getLineHeight();
        int margins = settings.getMarginPx();
        int paraSpacing = settings.getParaSpacing();
        
        // Status bar takes 30px at bottom, add small padding
        const int statusBarHeight = 35;
        // Top margin = side margin + small offset for aesthetics
        int topMargin = margins + 5;
        // Bottom margin = status bar + side margin
        int bottomMargin = statusBarHeight + margins;
        
        textLayout.setLineHeight(lineHeight);
        textLayout.setMargins(margins, margins, topMargin, bottomMargin);
        textLayout.setParaSpacing(paraSpacing);
        textLayout.setJustify(settings.justifyText);
        
        Serial.printf("[LIBRARY] Font: %s, LineHeight: %d, Margins: L/R=%d T=%d B=%d\n",
                      LibReaderSettings::getFontSizeName(settings.fontSize), lineHeight, 
                      margins, topMargin, bottomMargin);
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
                   firstRenderAfterOpen(false), pendingChapterLoad(false),
                   pendingChapterToLoad(-1) {
        strcpy(currentPath, "/books");
        memset(chapterTitle, 0, sizeof(chapterTitle));
        memset(currentBook, 0, sizeof(currentBook));
        memset(currentBookPath, 0, sizeof(currentBookPath));
        books.reserve(LIBRARY_MAX_BOOKS);
    }
    
    void init(int w, int h, bool autoResume = true) {
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
        applyFontSettings();  // Apply font, line height, margins from settings
        
        // Ensure cover cache directory exists
        SD.mkdir(COVER_CACHE_DIR);
        
        // Check if we should auto-resume last book (quick-open feature)
        if (autoResume && SD.exists(LAST_BOOK_PATH)) {
            LastBookInfo info;
            if (getLastBookInfo(info) && strlen(info.bookPath) > 0 && SD.exists(info.bookPath)) {
                Serial.printf("[LIBRARY] Quick-open: Resuming last book: %s\n", info.title);
                if (resumeLastBook()) {
                    return;  // Successfully resumed, don't scan directory
                }
            }
        }
        
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
                // Lowered threshold to allow book registration with tight memory
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 8000) {
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
        
        // Extract covers using lightweight method (no full EPUB parsing)
        extractAllCoversLightweight();
    }
    
    // =========================================================================
    // Lightweight Cover Extraction (minimal memory usage)
    // =========================================================================
    
    // Find cover image path by scanning ZIP file names
    String findCoverInZip(ZipReader& zip) {
        int fileCount = zip.getFileCount();
        String coverPath = "";
        
        // First pass: look for files with "cover" in the name
        for (int i = 0; i < fileCount; i++) {
            String name = zip.getFileName(i);
            String nameLower = name;
            nameLower.toLowerCase();
            
            // Common cover patterns
            if (nameLower.endsWith(".jpg") || nameLower.endsWith(".jpeg") || nameLower.endsWith(".png")) {
                if (nameLower.indexOf("cover") >= 0) {
                    coverPath = name;
                    Serial.printf("[COVER] Found by name: %s\n", name.c_str());
                    break;
                }
            }
        }
        
        // If not found, try reading container.xml -> content.opf to find cover
        if (coverPath.length() == 0) {
            // Read container.xml to find OPF path
            String container = zip.readFile("META-INF/container.xml");
            if (container.length() > 0) {
                // Simple parse: find rootfile full-path
                int rfPos = container.indexOf("rootfile");
                if (rfPos > 0) {
                    int fpPos = container.indexOf("full-path=\"", rfPos);
                    if (fpPos > 0) {
                        fpPos += 11;
                        int fpEnd = container.indexOf("\"", fpPos);
                        if (fpEnd > fpPos) {
                            String opfPath = container.substring(fpPos, fpEnd);
                            
                            // Read OPF to find cover
                            String opf = zip.readFile(opfPath);
                            if (opf.length() > 0) {
                                // Get base path for OPF
                                String basePath = "";
                                int lastSlash = opfPath.lastIndexOf('/');
                                if (lastSlash > 0) {
                                    basePath = opfPath.substring(0, lastSlash + 1);
                                }
                                
                                // Look for cover in meta (name="cover" content="id")
                                int metaPos = opf.indexOf("name=\"cover\"");
                                if (metaPos > 0) {
                                    int contentPos = opf.indexOf("content=\"", metaPos);
                                    if (contentPos > 0) {
                                        contentPos += 9;
                                        int contentEnd = opf.indexOf("\"", contentPos);
                                        if (contentEnd > contentPos) {
                                            String coverId = opf.substring(contentPos, contentEnd);
                                            
                                            // Find item with this id
                                            String idSearch = "id=\"" + coverId + "\"";
                                            int itemPos = opf.indexOf(idSearch);
                                            if (itemPos > 0) {
                                                int hrefPos = opf.indexOf("href=\"", itemPos);
                                                if (hrefPos > 0) {
                                                    hrefPos += 6;
                                                    int hrefEnd = opf.indexOf("\"", hrefPos);
                                                    if (hrefEnd > hrefPos) {
                                                        coverPath = basePath + opf.substring(hrefPos, hrefEnd);
                                                        Serial.printf("[COVER] Found in OPF meta: %s\n", coverPath.c_str());
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                
                                // Also check for properties="cover-image"
                                if (coverPath.length() == 0) {
                                    int propPos = opf.indexOf("properties=\"cover-image\"");
                                    if (propPos > 0) {
                                        // Search backwards for href
                                        int hrefPos = opf.lastIndexOf("href=\"", propPos);
                                        if (hrefPos > 0 && propPos - hrefPos < 200) {
                                            hrefPos += 6;
                                            int hrefEnd = opf.indexOf("\"", hrefPos);
                                            if (hrefEnd > hrefPos) {
                                                coverPath = basePath + opf.substring(hrefPos, hrefEnd);
                                                Serial.printf("[COVER] Found by properties: %s\n", coverPath.c_str());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        return coverPath;
    }
    
    void extractAllCoversLightweight() {
        Serial.println("[LIBRARY] ===== Extracting covers (lightweight) =====");
        
        int extracted = 0;
        int skipped = 0;
        
        for (int i = 0; i < (int)books.size(); i++) {
            BookEntry& book = books[i];
            
            // Skip non-EPUBs
            if (book.bookType != BookType::EPUB_FILE) {
                continue;
            }
            
            // Build paths
            char fullPath[128];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
            
            uint32_t hash = 0;
            for (const char* p = fullPath; *p; p++) {
                hash = hash * 31 + *p;
            }
            char coverCachePath[96];
            snprintf(coverCachePath, sizeof(coverCachePath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
            
            // Check if already cached
            if (isValidCoverFile(coverCachePath)) {
                strncpy(book.coverPath, coverCachePath, sizeof(book.coverPath) - 1);
                book.hasCover = true;
                skipped++;
                continue;
            }
            
            Serial.printf("[LIBRARY] Cover %d/%d: %s (heap=%d)\n", 
                         i+1, (int)books.size(), book.filename, ESP.getFreeHeap());
            
            // Open ZIP directly (not EpubParser)
            ZipReader zip;
            if (!zip.open(fullPath)) {
                Serial.printf("[LIBRARY] Failed to open ZIP: %s\n", zip.getError().c_str());
                continue;
            }
            
            // Find cover image path
            String coverInZip = findCoverInZip(zip);
            
            if (coverInZip.length() == 0) {
                Serial.println("[LIBRARY] No cover found in EPUB");
                zip.close();
                continue;
            }
            
            // Ensure output directory exists
            SD.mkdir(COVER_CACHE_DIR);
            
            // Extract cover to file
            File outFile = SD.open(coverCachePath, FILE_WRITE);
            if (!outFile) {
                Serial.printf("[LIBRARY] Failed to create cover file: %s\n", coverCachePath);
                zip.close();
                continue;
            }
            
            bool success = zip.streamFileTo(coverInZip, outFile, 1024);
            outFile.close();
            zip.close();
            
            if (success) {
                strncpy(book.coverPath, coverCachePath, sizeof(book.coverPath) - 1);
                book.hasCover = true;
                extracted++;
                Serial.printf("[LIBRARY] Cover extracted: %s\n", coverCachePath);
            } else {
                SD.remove(coverCachePath);
                Serial.printf("[LIBRARY] Cover extraction failed\n");
            }
            
            // Small delay to let memory settle
            yield();
        }
        
        Serial.printf("[LIBRARY] Covers: %d extracted, %d cached\n", extracted, skipped);
    }
    
    // =========================================================================
    // Book Metadata Loading (for covers and progress)
    // =========================================================================
    
    // Helper to check if cover file is valid (exists and has content)
    bool isValidCoverFile(const char* path) {
        if (!SD.exists(path)) return false;
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        size_t sz = f.size();
        f.close();
        if (sz < 100) {  // Cover should be at least 100 bytes
            // Delete empty/corrupt cover file
            SD.remove(path);
            Serial.printf("[LIBRARY] Deleted invalid cover file: %s (size=%d)\n", path, (int)sz);
            return false;
        }
        return true;
    }
    
    void loadBookMetadata(BookEntry& book, const char* fullPath) {
        // Generate cover cache path from book filename hash
        uint32_t hash = 0;
        for (const char* p = fullPath; *p; p++) {
            hash = hash * 31 + *p;
        }
        
        // Check for cached cover (try .jpg first, then .raw)
        // Also verify file has actual content (not empty)
        char jpgPath[96];
        char rawPath[96];
        snprintf(jpgPath, sizeof(jpgPath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
        snprintf(rawPath, sizeof(rawPath), "%s/%08x.raw", COVER_CACHE_DIR, hash);
        
        if (isValidCoverFile(jpgPath)) {
            strncpy(book.coverPath, jpgPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.printf("[LIBRARY] Cached cover found: %s\n", jpgPath);
        } else if (isValidCoverFile(rawPath)) {
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
        snprintf(path, sizeof(path), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
        return String(path);
    }
    
    // =========================================================================
    // Bluetooth Page Turner Support
    // =========================================================================
    
    /**
     * Handle keyboard key for page turning
     * Common page turner keys:
     * - Left Arrow (0x50), Page Up (0x4B), Space (0x2C) = Previous page
     * - Right Arrow (0x4F), Page Down (0x4E), Enter (0x28) = Next page
     * - Up Arrow (0x52) = Chapter menu
     * - Escape (0x29) = Back/Exit
     * 
     * Returns true if key was handled
     */
    bool handleKeyboardKey(uint8_t keyCode, bool pressed) {
        if (!pressed) return false;  // Only handle key press, not release
        if (state != READING) return false;  // Only handle when reading
        
        Button btn = BTN_NONE;
        
        switch (keyCode) {
            // Previous page keys
            case 0x50:  // Left arrow
            case 0x4B:  // Page Up
            case 0x2C:  // Space (configurable)
                btn = BTN_LEFT;
                break;
                
            // Next page keys
            case 0x4F:  // Right arrow
            case 0x4E:  // Page Down
            case 0x28:  // Enter
                btn = BTN_RIGHT;
                break;
                
            // Menu/Chapter select
            case 0x52:  // Up arrow
                btn = BTN_UP;
                break;
                
            // Settings
            case 0x51:  // Down arrow
                btn = BTN_DOWN;
                break;
                
            // Back/Exit
            case 0x29:  // Escape
                btn = BTN_BACK;
                break;
                
            default:
                return false;
        }
        
        if (btn != BTN_NONE) {
            Serial.printf("[LIBRARY] BT Page turner: keyCode=0x%02X -> btn=%d\n", keyCode, btn);
            return handleInput(btn);
        }
        
        return false;
    }
    
    // Check if currently reading (for BT page turner context)
    bool isReading() const { return state == READING && bookIsOpen; }
    
    // =========================================================================
    // Input Handling
    // =========================================================================
    
    bool handleInput(Button btn) {
        // DON'T remap buttons - use raw physical buttons
        // This matches the help text: "< > Flip | OK: Read | UP: List | DOWN: Settings"
        
        // Ignore BTN_NONE
        if (btn == BTN_NONE) {
            return false;
        }
        
        // Handle button immediately (no debounce needed - runPluginSimple handles it)
        return handleButtonPress(btn);
    }
    
    bool handleButtonPress(Button btn) {
        switch (state) {
            case BROWSER:
                return handleBrowserInput(btn);
            case READING:
                return handleReadingInput(btn);
            case CHAPTER_SELECT:
                return handleChapterSelectInput(btn);
            case BOOKMARK_SELECT:
                return handleBookmarkSelectInput(btn);
            case READING_STATS:
                return handleReadingStatsInput(btn);
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
                }
                // At root - return false to exit library
                return false;
                
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
                    // Defer chapter loading to main loop (fresh stack)
                    showLoadingScreen("Loading...");
                    currentChapter--;
                    currentPage = 999;  // Will go to last page
                    pendingChapterToLoad = currentChapter;
                    pendingChapterLoad = true;
                    cacheValid = false;
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
                    // Defer chapter loading to main loop (fresh stack)
                    showLoadingScreen("Loading...");
                    currentChapter++;
                    currentPage = 0;
                    pendingChapterToLoad = currentChapter;
                    pendingChapterLoad = true;
                    cacheValid = false;
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
                // closeBook() handles saving progress internally
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
                    // Defer chapter loading to main loop (fresh stack)
                    showLoadingScreen("Loading...");
                    currentChapter = chapterCursor;
                    currentPage = 0;
                    pendingChapterToLoad = currentChapter;
                    pendingChapterLoad = true;
                    cacheValid = false;
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
        
        // Calculate menu structure to match drawSettingsMenu
        int numReadingItems = 4;  // Font family, font size, line spacing, alignment
        int numDisplayItems = 4;  // Sleep screen, status bar, anti-aliasing, refresh
        int baseItems = numReadingItems + numDisplayItems;
        
        // Dynamic action buttons
        bool showChapters = bookIsOpen && totalChapters > 1;
        int chaptersIdx = showChapters ? baseItems : -1;
        int bookmarksIdx = bookIsOpen ? (showChapters ? baseItems + 1 : baseItems) : -1;
        int addBookmarkIdx = bookIsOpen ? (bookmarksIdx + 1) : -1;
        int statsIdx = bookIsOpen ? (addBookmarkIdx + 1) : baseItems;
        int backIdx = statsIdx + 1;
        int maxItems = backIdx + 1;
        
        switch (btn) {
            case BTN_UP:
                if (settingsCursor > 0) {
                    settingsCursor--;
                    return true;
                }
                break;
                
            case BTN_DOWN:
                if (settingsCursor < maxItems - 1) {
                    settingsCursor++;
                    return true;
                }
                break;
                
            case BTN_LEFT:
            case BTN_RIGHT:
            case BTN_CONFIRM:
                // Reading items (0-3)
                if (settingsCursor < numReadingItems) {
                    switch (settingsCursor) {
                        case 0: // Font family - no action for now
                            break;
                        case 1: // Font size
                            settings.fontSize = (FontSize)(((int)settings.fontSize + 1) % 3);
                            break;
                        case 2: // Line spacing
                            settings.lineSpacing = (LineSpacing)(((int)settings.lineSpacing + 1) % 3);
                            break;
                        case 3: // Alignment
                            settings.justifyText = !settings.justifyText;
                            break;
                    }
                    readerSettings.save();
                    return true;
                }
                
                // Display items (4-7)
                if (settingsCursor < baseItems) {
                    // Display settings - most are placeholder for now
                    return true;
                }
                
                // Action buttons
                if (settingsCursor == chaptersIdx) {
                    chapterCursor = currentChapter;
                    chapterScrollOffset = (currentChapter > 3) ? currentChapter - 3 : 0;
                    state = CHAPTER_SELECT;
                    return true;
                }
                
                if (settingsCursor == bookmarksIdx) {
                    loadBookmarks();
                    bookmarkCursor = 0;
                    bookmarkScrollOffset = 0;
                    state = BOOKMARK_SELECT;
                    return true;
                }
                
                if (settingsCursor == addBookmarkIdx) {
                    loadBookmarks();  // Make sure we have latest
                    int existingIdx = bookmarks.find(currentChapter, currentPage);
                    if (existingIdx >= 0) {
                        // Remove existing bookmark
                        bookmarks.remove(existingIdx);
                        saveBookmarks();
                        Serial.printf("[LIBRARY] Bookmark removed: ch%d pg%d\n", currentChapter, currentPage);
                    } else {
                        // Add new bookmark
                        if (bookmarks.add(currentChapter, currentPage, nullptr)) {
                            saveBookmarks();
                            Serial.printf("[LIBRARY] Bookmark added: ch%d pg%d\n", currentChapter, currentPage);
                        }
                    }
                    return true;
                }
                
                if (settingsCursor == statsIdx) {
                    state = READING_STATS;
                    return true;
                }
                
                if (settingsCursor == backIdx) {
                    // Check if settings changed requiring cache rebuild
                    if (bookIsOpen && readerSettings.requiresCacheRebuild(oldSettings)) {
                        readerSettings.save();
                        // IMPORTANT: Invalidate old cache before rebuilding with new settings
                        pageCache.invalidateBook();
                        textLayout.setPageSize(getLayoutWidth(), screenH);
                        readerSettings.applyToLayout(textLayout);
                        showLoadingScreen("Reformatting...");
                        // Defer to main loop (fresh stack)
                        pendingChapterToLoad = currentChapter;
                        pendingChapterLoad = true;
                        cacheValid = false;
                        updateRequired = true;
                    }
                    state = bookIsOpen ? READING : BROWSER;
                    return true;
                }
                break;
                
            case BTN_BACK:
                if (bookIsOpen && readerSettings.requiresCacheRebuild(oldSettings)) {
                    readerSettings.save();
                    // IMPORTANT: Invalidate old cache before rebuilding with new settings
                    pageCache.invalidateBook();
                    textLayout.setPageSize(getLayoutWidth(), screenH);
                    readerSettings.applyToLayout(textLayout);
                    showLoadingScreen("Reformatting...");
                    // Defer to main loop (fresh stack)
                    pendingChapterToLoad = currentChapter;
                    pendingChapterLoad = true;
                    cacheValid = false;
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
    // Bookmark Handlers
    // =========================================================================
    
    bool handleBookmarkSelectInput(Button btn) {
        int maxVisible = 8;
        
        switch (btn) {
            case BTN_UP:
            case BTN_LEFT:
                if (bookmarkCursor > 0) {
                    bookmarkCursor--;
                    if (bookmarkCursor < bookmarkScrollOffset) {
                        bookmarkScrollOffset = bookmarkCursor;
                    }
                    return true;
                }
                break;
                
            case BTN_DOWN:
            case BTN_RIGHT:
                if (bookmarkCursor < bookmarks.count - 1) {
                    bookmarkCursor++;
                    if (bookmarkCursor >= bookmarkScrollOffset + maxVisible) {
                        bookmarkScrollOffset = bookmarkCursor - maxVisible + 1;
                    }
                    return true;
                }
                break;
                
            case BTN_CONFIRM:
                if (bookmarks.count > 0 && bookmarkCursor < bookmarks.count) {
                    // Jump to bookmarked position
                    Bookmark& bm = bookmarks.bookmarks[bookmarkCursor];
                    
                    if (bm.chapter != currentChapter) {
                        showLoadingScreen("Loading...");
                        currentChapter = bm.chapter;
                        currentPage = bm.page;
                        // Defer to main loop (fresh stack)
                        pendingChapterToLoad = currentChapter;
                        pendingChapterLoad = true;
                        cacheValid = false;
                    } else {
                        currentPage = bm.page;
                        // Validate page
                        if (currentPage >= totalPages && totalPages > 0) {
                            currentPage = totalPages - 1;
                        }
                    }
                    
                    saveProgress();
                    updateRequired = true;
                    state = READING;
                    return true;
                }
                break;
                
            case BTN_BACK:
                state = SETTINGS_MENU;
                return true;
                
            default:
                break;
        }
        return false;
    }
    
    bool handleReadingStatsInput(Button btn) {
        switch (btn) {
            case BTN_BACK:
            case BTN_CONFIRM:
                state = SETTINGS_MENU;
                return true;
            default:
                break;
        }
        return false;
    }
    
    // =========================================================================
    // Bookmark Persistence
    // =========================================================================
    
    String getBookmarkPath() {
        // Get bookmark file path for current book
        return pageCache.getCachePath() + "/bookmarks.bin";
    }
    
    void loadBookmarks() {
        bookmarks = BookmarkList();  // Reset
        
        String path = getBookmarkPath();
        File f = SD.open(path, FILE_READ);
        if (!f) return;
        
        // Read magic
        uint32_t magic;
        if (f.read((uint8_t*)&magic, sizeof(magic)) != sizeof(magic) || magic != 0x424D4152) {
            f.close();
            return;
        }
        
        // Read count
        f.read(&bookmarks.count, 1);
        if (bookmarks.count > MAX_BOOKMARKS_PER_BOOK) {
            bookmarks.count = MAX_BOOKMARKS_PER_BOOK;
        }
        
        // Read bookmarks
        for (int i = 0; i < bookmarks.count; i++) {
            bookmarks.bookmarks[i].deserialize(f);
        }
        
        f.close();
        Serial.printf("[LIBRARY] Loaded %d bookmarks\n", bookmarks.count);
    }
    
    void saveBookmarks() {
        String path = getBookmarkPath();
        File f = SD.open(path, FILE_WRITE);
        if (!f) return;
        
        f.write((uint8_t*)&bookmarks.magic, sizeof(bookmarks.magic));
        f.write(&bookmarks.count, 1);
        
        for (int i = 0; i < bookmarks.count; i++) {
            bookmarks.bookmarks[i].serialize(f);
        }
        
        f.close();
        Serial.printf("[LIBRARY] Saved %d bookmarks\n", bookmarks.count);
    }
    
    // =========================================================================
    // KOReader Sync Integration
    // =========================================================================
    
    String getDocumentHash() {
        // Generate a hash of the book for KOReader sync
        // KOReader uses MD5 of the filename typically
        String filename = currentBookPath;
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) {
            filename = filename.substring(lastSlash + 1);
        }
        
        // Simple hash for compatibility
        uint32_t hash = 5381;
        for (int i = 0; i < (int)filename.length(); i++) {
            hash = ((hash << 5) + hash) + filename.charAt(i);
        }
        
        char hexHash[17];
        snprintf(hexHash, sizeof(hexHash), "%08lx%08lx", hash, hash ^ 0xDEADBEEF);
        return String(hexHash);
    }
    
    float getReadingProgress() {
        // Calculate overall progress through book
        if (!bookIsOpen || totalChapters == 0) return 0.0f;
        
        float chapterProgress = totalPages > 0 ? (float)currentPage / totalPages : 0.0f;
        float overallProgress = ((float)currentChapter + chapterProgress) / totalChapters;
        return overallProgress;
    }
    
    void syncProgressToKOSync() {
        if (!settingsManager.sync.kosyncEnabled) return;
        if (strlen(settingsManager.sync.kosyncUrl) == 0) return;
        if (!bookIsOpen) return;
        
        Serial.println("[KOSYNC] Syncing progress to server...");
        
        // Build JSON payload
        JsonDocument doc;
        doc["document"] = getDocumentHash();
        doc["progress"] = String(getReadingProgress(), 4);
        doc["percentage"] = getReadingProgress() * 100;
        doc["device"] = "SUMI";
        doc["device_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
        
        String payload;
        serializeJson(doc, payload);
        
        // Send to KOSync server
        String url = String(settingsManager.sync.kosyncUrl) + "/syncs/progress";
        
        Serial.printf("[KOSYNC] PUT %s\n", url.c_str());
        Serial.printf("[KOSYNC] Payload: %s\n", payload.c_str());
        
        // Note: Actual HTTP request would go here
        // For now, just log that we would sync
        // This requires WiFiClient which should be done asynchronously
    }
    
    void syncProgressFromKOSync() {
        if (!settingsManager.sync.kosyncEnabled) return;
        if (strlen(settingsManager.sync.kosyncUrl) == 0) return;
        if (!bookIsOpen) return;
        
        Serial.println("[KOSYNC] Fetching progress from server...");
        
        String url = String(settingsManager.sync.kosyncUrl) + "/syncs/progress/" + getDocumentHash();
        
        Serial.printf("[KOSYNC] GET %s\n", url.c_str());
        
        // Note: Actual HTTP request would go here
        // Parse response and update position if server progress is newer
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
        
        // Reload current chapter if a book is open (defer to main loop)
        if (bookIsOpen && (state == SETTINGS_MENU || state == READING)) {
            showLoadingScreen("Rebuilding...");
            pendingChapterToLoad = currentChapter;
            pendingChapterLoad = true;
            cacheValid = false;
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
        
        // Check stack space at entry
        UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
        Serial.printf("[LIBRARY] Stack high water: %d words (%d bytes)\n", 
                      (int)stackHighWater, (int)(stackHighWater * 4));
        
        if (index >= (int)books.size()) {
            Serial.printf("[LIBRARY] ERROR: index %d >= books.size() %d\n", index, (int)books.size());
            return;
        }
        
        // Show loading screen IMMEDIATELY
        showLoadingScreen("Loading...");
        
        // === CRITICAL: Free up RAM by suspending WiFi/WebServer ===
        suspendForReading();
        Serial.printf("[LIBRARY] After suspend, heap: %d\n", ESP.getFreeHeap());
        
        // Ensure ZIP buffers are allocated (may have been freed for portal mode)
        // This also resets in-use flags to ensure clean state
        ZipReader_preallocateBuffer();
        
        // Load global reading stats
        stats.load();
        
        BookEntry& book = books[index];
        // Build path directly into member variable to save stack space
        snprintf(currentBookPath, sizeof(currentBookPath), "%s/%s", currentPath, book.filename);
        strncpy(currentBook, book.title, sizeof(currentBook) - 1);
        
        Serial.printf("[LIBRARY] Book: '%s'\n", currentBook);
        Serial.printf("[LIBRARY] Path: '%s'\n", currentBookPath);
        Serial.printf("[LIBRARY] Type: %d\n", (int)book.bookType);
        
        // Detect book type
        isEpub = (book.bookType == BookType::EPUB_FOLDER || 
                  book.bookType == BookType::EPUB_FILE);
        
        // =====================================================
        // Open EPUB metadata
        // =====================================================
        if (isEpub) {
            Serial.println("[LIBRARY] Opening EPUB metadata...");
            if (!openEpubMetadata(currentBookPath)) {
                showErrorScreen("Failed to open EPUB");
                state = BROWSER;
                return;
            }
        } else {
            openTxtMetadata(currentBookPath);
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
            Serial.println("[LIBRARY] Cache invalidated, configuring layout...");
        }
        
        // Configure text layout PROPERLY - page size first, then settings
        Serial.printf("[LIBRARY] Setting page size: %dx%d\n", getLayoutWidth(), screenH);
        textLayout.setPageSize(getLayoutWidth(), screenH);
        Serial.println("[LIBRARY] Applying font settings...");
        applyFontSettings();
        Serial.println("[LIBRARY] Layout configured.");
        
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
        // DON'T load chapter here - stack is too deep
        // Create render task with its own 16KB stack to handle loading
        // =====================================================
        pendingChapterLoad = true;
        pendingChapterToLoad = currentChapter;
        cacheValid = false;
        
        // Create render task if not already running
        if (!renderTaskHandle) {
            renderMutex = xSemaphoreCreateMutex();
            xTaskCreate(
                renderTaskTrampoline,   // Task function
                "ReaderRender",         // Task name
                24576,                  // Stack size (24KB - enough for deep EPUB parsing)
                this,                   // Parameter
                1,                      // Priority
                &renderTaskHandle       // Task handle
            );
            Serial.printf("[LIBRARY] Created render task with 24KB stack\n");
        }
        
        // Start reading stats
        stats.startSession();
        
        // NO blank screen clear - go directly to first page render
        bookIsOpen = true;
        firstRenderAfterOpen = true;  // Will trigger display reset on first render
        state = READING;
        pagesUntilFullRefresh = readerSettings.get().pagesPerFullRefresh;
        pagesUntilHalfRefresh = readerSettings.get().pagesPerHalfRefresh;
        
        // Load bookmarks for this book
        loadBookmarks();
        
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
    
    // Extract cover on-demand when browsing (lightweight method)
    void extractCoverOnDemand(BookEntry& book, const char* fullPath) {
        // Generate cover cache path
        uint32_t hash = 0;
        for (const char* p = fullPath; *p; p++) {
            hash = hash * 31 + *p;
        }
        char coverPath[96];
        snprintf(coverPath, sizeof(coverPath), "%s/%08x.jpg", COVER_CACHE_DIR, hash);
        
        // Check if already cached
        if (isValidCoverFile(coverPath)) {
            strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.println("[LIBRARY] Cover found in cache");
            return;
        }
        
        Serial.printf("[LIBRARY] Extracting cover: %s (heap=%d)\n", book.filename, ESP.getFreeHeap());
        
        // Open ZIP directly (lightweight, no EpubParser)
        ZipReader zip;
        if (!zip.open(fullPath)) {
            Serial.printf("[LIBRARY] Failed to open ZIP\n");
            strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
            book.hasCover = false;
            return;
        }
        
        // Find cover image path
        String coverInZip = findCoverInZip(zip);
        
        if (coverInZip.length() == 0) {
            Serial.println("[LIBRARY] No cover found");
            zip.close();
            strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
            book.hasCover = false;
            return;
        }
        
        // Extract to file
        SD.mkdir(COVER_CACHE_DIR);
        File outFile = SD.open(coverPath, FILE_WRITE);
        if (!outFile) {
            zip.close();
            return;
        }
        
        bool success = zip.streamFileTo(coverInZip, outFile, 1024);
        outFile.close();
        zip.close();
        
        strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
        book.hasCover = success;
        
        if (success) {
            Serial.println("[LIBRARY] Cover extracted successfully");
        }
    }
    
    // =========================================================================
    // FreeRTOS Render Task Loop
    // =========================================================================
    
    void renderTaskLoop() {
        Serial.println("[LIBRARY] Render task started");
        Serial.printf("[MEM:RENDER_TASK] Initial Free: %d, Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
        
        static unsigned long lastMemLog = 0;
        
        while (true) {
            // Periodic memory logging in render task
            if (millis() - lastMemLog > 15000) {
                lastMemLog = millis();
                UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
                Serial.printf("[MEM:RENDER] Free: %d | Min: %d | Task Stack: %d words\n", 
                              ESP.getFreeHeap(), ESP.getMinFreeHeap(), (int)stackHighWater);
            }
            
            // Handle deferred chapter loading (runs in render task with 16KB stack)
            if (pendingChapterLoad) {
                pendingChapterLoad = false;
                
                Serial.printf("[MEM:CH_LOAD_START] Free: %d, Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
                
                // Use specified chapter or currentChapter
                int chapterToLoad = (pendingChapterToLoad >= 0) ? pendingChapterToLoad : currentChapter;
                pendingChapterToLoad = -1;
                
                Serial.printf("[LIBRARY] Loading chapter %d in render task...\n", chapterToLoad);
                currentChapter = chapterToLoad;
                
                // Try to load chapter, auto-advance if empty
                int maxAttempts = 5;
                int attempt = 0;
                while (attempt < maxAttempts && currentChapter < totalChapters) {
                    if (loadChapterSync(currentChapter)) {
                        break;
                    }
                    Serial.printf("[LIBRARY] Chapter %d empty, trying next...\n", currentChapter);
                    currentChapter++;
                    attempt++;
                }
                
                Serial.printf("[MEM:CH_LOAD_END] Free: %d, Min: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
                
                if (totalPages == 0 || currentChapter >= totalChapters) {
                    Serial.println("[LIBRARY] No readable content found");
                } else {
                    Serial.printf("[LIBRARY] Chapter loaded: %d pages\n", totalPages);
                    cacheValid = true;
                    
                    // Validate page number
                    if (currentPage >= totalPages) {
                        currentPage = totalPages > 0 ? totalPages - 1 : 0;
                    }
                    saveProgress();
                }
                updateRequired = true;
            }
            
            if (updateRequired) {
                updateRequired = false;
                
                // Take mutex to prevent conflicts with input handling
                if (xSemaphoreTake(renderMutex, portMAX_DELAY) == pdTRUE) {
                    // CRITICAL: Must use GxEPD2's firstPage/nextPage pattern
                    // Without this, only part of the display buffer gets rendered
                    display.setFullWindow();
                    display.firstPage();
                    do {
                        display.fillScreen(GxEPD_WHITE);
                        renderCurrentPage();
                    } while (display.nextPage());
                    
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
        stats.save();  // Save reading statistics
        
        // Stop render task with timeout to avoid blocking
        if (renderTaskHandle) {
            // Try to get mutex with 100ms timeout
            if (renderMutex && xSemaphoreTake(renderMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                vTaskDelete(renderTaskHandle);
                renderTaskHandle = nullptr;
                xSemaphoreGive(renderMutex);
                vSemaphoreDelete(renderMutex);
                renderMutex = nullptr;
            } else {
                // Mutex timeout - just delete the task
                vTaskDelete(renderTaskHandle);
                renderTaskHandle = nullptr;
                if (renderMutex) {
                    vSemaphoreDelete(renderMutex);
                    renderMutex = nullptr;
                }
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
        
        // Services stay suspended for battery savings
        Serial.printf("[LIBRARY] Book closed, heap: %d\n", ESP.getFreeHeap());
        
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
        Serial.printf("[LOAD] Chapter %d (heap=%d)\n", chapter, ESP.getFreeHeap());
        
        // Verify sufficient memory available to parse
        if (ESP.getFreeHeap() < 10000) {
            Serial.printf("[LOAD] ERROR: Low heap! %d bytes\n", ESP.getFreeHeap());
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
        
        // Check cache first
        if (pageCache.hasValidCache(key)) {
            int cachedCount = pageCache.getPageCount(chapter);
            if (cachedCount > 0) {
                totalPages = cachedCount;
                cacheValid = true;
                preloadedPage = -1;
                return true;
            }
        }
        
        // Cache miss or invalid - need to parse
        Serial.println("[LOAD] Cache miss - parsing...");
        
        // IMPORTANT: Always set page size BEFORE applying settings
        // This ensures contentWidth is calculated correctly
        textLayout.setPageSize(getLayoutWidth(), screenH);
        applyFontSettings();
        textLayout.beginLayout();
        
        int pageCount = 0;
        bool success = true;
        
        if (isEpub) {
            SD.mkdir("/.sumi");
            String tempPath = getTempFilePath(chapter);
            
            // Stream from EPUB to temp file
            bool streamOk = epub.streamChapterToFile(chapter, tempPath);
            if (!streamOk) {
                Serial.println("[LOAD] Stream failed");
                textLayout.addParagraph("Chapter unavailable", true);
                success = false;
            } else {
                // Check memory again before parsing
                // Use simpler parsing when memory is below 15KB
                if (ESP.getFreeHeap() < 15000) {
                    Serial.println("[LOAD] Low mem, simple parse");
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
                    
                    if (!parseOk) {
                        Serial.println("[LOAD] Parse failed");
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
        textLayout.setPageSize(getLayoutWidth(), screenH);
        applyFontSettings();
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
        
        // Sync to KOReader server (non-blocking, only on WiFi)
        if (koSync.isEnabled() && WiFi.status() == WL_CONNECTED) {
            syncToKOReader();
        }
    }
    
    void syncToKOReader() {
        if (!bookIsOpen || !koSync.isEnabled()) return;
        
        String docHash = KOSync::hashDocument(currentBookPath);
        String progress = KOSync::formatProgress(currentChapter, currentPage, totalChapters);
        float percentage = totalPages > 0 ? (float)currentPage / totalPages : 0.0f;
        
        // Do sync in background - don't block UI
        koSync.updateProgress(docHash.c_str(), progress.c_str(), percentage);
    }
    
    void syncFromKOReader() {
        if (!bookIsOpen || !koSync.isEnabled()) return;
        
        String docHash = KOSync::hashDocument(currentBookPath);
        KOSync::Progress serverProgress;
        
        if (koSync.getProgress(docHash.c_str(), serverProgress)) {
            int serverChapter, serverPage;
            if (KOSync::parseProgress(serverProgress.progress, serverChapter, serverPage)) {
                // Check if server has newer progress
                float serverPct = serverProgress.percentage.toFloat();
                float localPct = totalPages > 0 ? (float)currentPage / totalPages : 0.0f;
                
                if (serverPct > localPct + 0.01f) {  // Server is ahead by more than 1%
                    Serial.printf("[KOSYNC] Server progress ahead: %s (%.1f%% vs %.1f%%)\n",
                                  serverProgress.progress.c_str(), serverPct * 100, localPct * 100);
                    
                    // Optionally jump to server position
                    // For now, just log it - user can manually sync
                }
            }
        }
    }
    
    // =========================================================================
    // Drawing
    // =========================================================================
    
    void draw() {
        // =====================================================
        // Handle deferred chapter loading
        // If render task exists, let IT handle loading (has 16KB stack)
        // Only do it here if no render task (fallback for compatibility)
        // =====================================================
        if (pendingChapterLoad && state == READING && !renderTaskHandle) {
            pendingChapterLoad = false;
            
            int chapterToLoad = (pendingChapterToLoad >= 0) ? pendingChapterToLoad : currentChapter;
            pendingChapterToLoad = -1;
            
            Serial.printf("[LIBRARY] Deferred load (no render task): chapter %d\n", chapterToLoad);
            currentChapter = chapterToLoad;
            
            // Try to load chapter, auto-advance if empty
            int maxAttempts = 5;
            int attempt = 0;
            while (attempt < maxAttempts && currentChapter < totalChapters) {
                if (loadChapterSync(currentChapter)) {
                    break;
                }
                Serial.printf("[LIBRARY] Chapter %d empty, trying next...\n", currentChapter);
                currentChapter++;
                attempt++;
            }
            
            if (totalPages == 0 || currentChapter >= totalChapters) {
                Serial.println("[LIBRARY] No readable content found");
                showErrorScreen("No readable content");
                state = BROWSER;
                return;
            }
            
            Serial.printf("[LIBRARY] Chapter loaded: %d pages\n", totalPages);
            cacheValid = true;
            
            // Validate page number
            if (currentPage >= totalPages) {
                currentPage = totalPages > 0 ? totalPages - 1 : 0;
            }
            saveProgress();
            
            // Don't draw yet - let the display update happen on next cycle
            // This keeps stack usage minimal
            return;
        }
        
        // If render task exists and chapter loading is pending, just wait
        if (pendingChapterLoad && state == READING && renderTaskHandle) {
            // Show loading indicator while waiting for render task
            return;
        }
        
        // Normal drawing
        switch (state) {
            case BROWSER:
                drawBrowser();
                break;
            case READING:
                // Draw reading page directly when state is READING
                if (updateRequired || cacheValid) {
                    drawReadingPage();
                    updateRequired = false;
                }
                break;
            case CHAPTER_SELECT:
                drawChapterSelect();
                break;
            case BOOKMARK_SELECT:
                drawBookmarkSelect();
                break;
            case READING_STATS:
                drawReadingStatsScreen();
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
        // Only do full refresh when reading - menus should always use partial
        if (state == READING) {
            pagesUntilFullRefresh--;
            if (pagesUntilFullRefresh <= 0) {
                pagesUntilFullRefresh = readerSettings.get().pagesPerFullRefresh;
                return true;
            }
        }
        // Settings menu, chapter select, bookmarks, stats - always partial
        return false;
    }
    
    // Force next refresh to be full (e.g., when entering/leaving reading mode)
    void requestFullRefresh() {
        pagesUntilFullRefresh = 0;  // Next check will trigger full refresh
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
        // Verbose logging disabled - uncomment for debugging
        // Serial.printf("[LIBRARY] drawFlipBrowser: cursor=%d, books=%d\n", cursor, (int)books.size());
        
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
        
        // Reserve space for bottom bars
        int bottomBarHeight = 70;  // Book X of Y + help bar
        int usableHeight = screenH - bottomBarHeight;
        
        // Calculate layout - cover on left, info on right (landscape)
        // or cover fills most of screen, info at bottom (portrait)
        int coverX, coverY, coverW, coverH;
        int infoX, infoY, infoW;
        
        if (landscape) {
            // Landscape: cover on left half
            coverW = screenW / 3;
            coverH = usableHeight - 40;
            coverX = 40;
            coverY = 30;
            infoX = coverX + coverW + 40;
            infoY = 50;
            infoW = screenW - infoX - 40;
        } else {
            // Portrait: cover fills top portion, info at bottom above bars
            coverW = screenW - 80;  // Leave room for nav arrows
            coverH = usableHeight - 120;  // Leave room for title + info below
            coverX = 40;
            coverY = 20;
            infoX = 30;
            infoY = coverY + coverH + 15;  // Position just below cover
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
        bool coverExists = book.hasCover && SD.exists(book.coverPath);
        
        if (coverExists) {
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
        
        // File type and size on same line (portrait) or next line (landscape)
        display.setFont(&FreeSans9pt7b);
        if (landscape) {
            if (strlen(book.author) > 0) {
                display.setCursor(infoX, infoY + 30);
                display.print(book.author);
            }
            display.setCursor(infoX, infoY + 60);
        } else {
            display.setCursor(infoX, infoY + 25);
        }
        
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
        
        // Book counter - positioned above help bar
        display.setCursor(infoX, screenH - 60);
        display.printf("Book %d of %d", cursor + 1, (int)books.size());
        
        // Bottom help bar
        display.drawLine(0, screenH - 35, screenW, screenH - 35, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, screenH - 12);
        display.print("< > Flip | OK: Read | UP: List | DOWN: Settings");
    }
    
    void drawCoverImage(const char* path, int x, int y, int maxW, int maxH) {
        Serial.printf("[COVER] drawCoverImage: %s\n", path);
        
        // Check file format by reading magic bytes
        File coverFile = SD.open(path, FILE_READ);
        if (!coverFile) {
            Serial.printf("[COVER] File open failed\n");
            drawCoverPlaceholder(x, y, maxW, maxH, "No Cover");
            return;
        }
        
        size_t fileSize = coverFile.size();
        Serial.printf("[COVER] File size: %d bytes\n", (int)fileSize);
        
        if (fileSize < 100) {
            Serial.printf("[COVER] File too small, deleting invalid cache\n");
            coverFile.close();
            SD.remove(path);  // Delete invalid cache file
            drawCoverPlaceholder(x, y, maxW, maxH, "No Cover");
            return;
        }
        
        uint8_t magic[4];
        coverFile.read(magic, 4);
        coverFile.close();
        
        // Check for JPEG (FFD8FF) or PNG (89504E47)
        bool isJpeg = (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF);
        bool isPng = (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47);
        
        Serial.printf("[COVER] Magic: %02X %02X %02X %02X (jpg=%d png=%d)\n",
                      magic[0], magic[1], magic[2], magic[3], isJpeg, isPng);
        
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
        
        // Calculate scale to fit within bounds (TJpgDec supports 1, 2, 4, 8)
        int scale = 1;
        while ((jpgW / scale > (uint16_t)maxW || jpgH / scale > (uint16_t)maxH) && scale < 8) {
            scale *= 2;
        }
        
        TJpgDec.setJpgScale(scale);
        
        int scaledW = jpgW / scale;
        int scaledH = jpgH / scale;
        
        // If still too big after max scale, we'll clip
        if (scaledW > maxW) scaledW = maxW;
        if (scaledH > maxH) scaledH = maxH;
        
        // Center the image within bounds
        _coverOffsetX = x + (maxW - scaledW) / 2;
        _coverOffsetY = y + (maxH - scaledH) / 2;
        if (_coverOffsetX < x) _coverOffsetX = x;
        if (_coverOffsetY < y) _coverOffsetY = y;
        
        // Set clipping bounds
        _coverMaxX = x + maxW;
        _coverMaxY = y + maxH;
        
        // Clear area and decode
        display.fillRect(x, y, maxW, maxH, GxEPD_WHITE);
        
        _jpgCallbackCount = 0;  // Reset debug counter
        JRESULT result = TJpgDec.drawFsJpg(0, 0, path, SD);
        
        if (result != JDR_OK) {
            Serial.printf("[LIBRARY] Cover decode failed: %d\n", result);
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
        // Clear first render flag
        if (firstRenderAfterOpen) {
            firstRenderAfterOpen = false;
        }
        
        if (!cacheValid) {
            // Simple minimal loading indicator
            display.setFont(&FreeSansBold12pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(screenW/2 - 15, screenH/2);
            display.print("...");
            return;
        }
        
        // Validate page number after chapter load
        if (currentPage >= totalPages || currentPage < 0) {
            currentPage = totalPages > 0 ? 0 : 0;
        }
        
        // Use heap allocation to avoid stack overflow (CachedPage is ~14KB)
        CachedPage* page = new CachedPage();
        if (!page) {
            Serial.println("[READER] ERROR: Failed to allocate CachedPage");
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(20, 100);
            display.print("Memory error");
            return;
        }
        
        if (!pageCache.loadPage(currentChapter, currentPage, *page)) {
            delete page;
            Serial.println("[READER] ERROR: Failed to load page from cache");
            display.setFont(&FreeSans9pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(20, 100);
            display.print("Error loading page");
            return;
        }
        
        // Validate page data
        if (page->lineCount > CACHE_MAX_LINES_PAGE) {
            page->lineCount = CACHE_MAX_LINES_PAGE;
        }
        
        // Draw all text - GxEPD2 handles clipping automatically
        // Use the correct font based on settings
        display.setFont(getReaderFont());
        display.setTextColor(GxEPD_BLACK);
        
        for (int i = 0; i < page->lineCount; i++) {
            const CachedLine& line = page->lines[i];
            
            // Check if this is an image line
            if (line.isImage()) {
                const CachedLine::ImageInfo* imgInfo = line.getImageInfo();
                if (strlen(imgInfo->path) > 0 && SD.exists(imgInfo->path)) {
                    // Draw the image using TJpgDec
                    int imgX = (screenW - imgInfo->width) / 2;  // Center image
                    int imgY = line.yPos;
                    
                    // Set up JPEG decoder
                    TJpgDec.setJpgScale(1);
                    TJpgDec.setCallback(_jpgDrawCallback);
                    
                    _coverOffsetX = imgX;
                    _coverOffsetY = imgY;
                    _coverMaxX = screenW;  // Clip to screen bounds
                    _coverMaxY = screenH - 30;  // Leave room for status bar
                    
                    JRESULT result = TJpgDec.drawFsJpg(0, 0, imgInfo->path, SD);
                    if (result != JDR_OK) {
                        // Draw placeholder on error
                        display.drawRect(imgX, imgY, imgInfo->width, imgInfo->height, GxEPD_BLACK);
                        display.setCursor(imgX + 5, imgY + imgInfo->height / 2);
                        display.print("[Image]");
                    }
                }
                continue;  // Don't try to draw words on image line
            }
            
            // Draw all words on this line
            for (int j = 0; j < line.wordCount; j++) {
                const CachedWord& word = line.words[j];
                display.setCursor(word.xPos, line.yPos);
                display.print(word.text);
            }
        }
        
        // Draw status bar
        drawStatusBarInPage();
        
        delete page;  // Free after rendering complete
        
        Serial.printf("[READER] Page %d/%d rendered\n", currentPage + 1, totalPages);
        
        // Save last book info for sleep screen
        saveLastBookInfo();
    }
    
    // Draw status bar during page render loop
    void drawStatusBarInPage() {
        int barY = screenH - 25;
        
        display.drawLine(0, barY - 5, screenW, barY - 5, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Chapter name on left (truncated if needed)
        String chapterName;
        if (isEpub && currentChapter < totalChapters) {
            chapterName = epub.getChapter(currentChapter).title;
        }
        if (chapterName.length() == 0) {
            chapterName = String("Chapter ") + String(currentChapter + 1);
        }
        int maxChapterLen = landscape ? 40 : 20;
        if ((int)chapterName.length() > maxChapterLen) {
            chapterName = chapterName.substring(0, maxChapterLen - 3) + "...";
        }
        display.setCursor(15, barY + 10);
        display.print(chapterName);
        
        // Page number on right
        char pageStr[24];
        snprintf(pageStr, sizeof(pageStr), "%d / %d", currentPage + 1, totalPages);
        
        int16_t x1, y1;
        uint16_t tw, th;
        display.getTextBounds(pageStr, 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(screenW - tw - 15, barY + 10);
        display.print(pageStr);
    }
    
    // =========================================================================
    // Last Book Info (for sleep screen)
    // =========================================================================
    
    void saveLastBookInfo() {
        LastBookInfo info;
        info.magic = 0x4C415354;  // "LAST"
        strncpy(info.title, currentBook, sizeof(info.title) - 1);
        strncpy(info.bookPath, currentBookPath, sizeof(info.bookPath) - 1);  // Save book path for resume
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
            Serial.printf("[LIBRARY] Saved last book info: %s at %s\n", info.title, info.bookPath);
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
    
    // Resume last book (for boot-to-last-book feature)
    bool resumeLastBook() {
        LastBookInfo info;
        if (!getLastBookInfo(info)) {
            Serial.println("[LIBRARY] No last book to resume");
            return false;
        }
        
        if (strlen(info.bookPath) == 0 || !SD.exists(info.bookPath)) {
            Serial.printf("[LIBRARY] Last book path invalid: %s\n", info.bookPath);
            return false;
        }
        
        Serial.printf("[LIBRARY] Resuming: %s at chapter %d, page %d\n", 
                      info.title, info.chapter, info.page);
        
        // Set current book info
        strncpy(currentBookPath, info.bookPath, sizeof(currentBookPath) - 1);
        strncpy(currentBook, info.title, sizeof(currentBook) - 1);
        
        // Extract directory path from book path
        String pathStr = String(info.bookPath);
        int lastSlash = pathStr.lastIndexOf('/');
        if (lastSlash > 0) {
            strncpy(currentPath, pathStr.substring(0, lastSlash).c_str(), sizeof(currentPath) - 1);
        }
        
        // Detect book type from extension
        String ext = pathStr.substring(pathStr.lastIndexOf('.'));
        ext.toLowerCase();
        isEpub = (ext == ".epub");
        
        // Show loading screen
        showLoadingScreen("Resuming...");
        
        // Open book metadata
        if (isEpub) {
            if (!openEpubMetadata(currentBookPath)) {
                showErrorScreen("Failed to open book");
                state = BROWSER;
                return false;
            }
        } else {
            openTxtMetadata(currentBookPath);
        }
        
        // Initialize cache and restore position
        pageCache.init(currentBookPath);
        
        // Configure text layout
        textLayout.setPageSize(getLayoutWidth(), screenH);
        applyFontSettings();
        
        // Restore position from saved info
        currentChapter = info.chapter;
        currentPage = info.page;
        
        // Load chapter
        if (!loadChapterSync(currentChapter)) {
            // Try chapter 0 if saved chapter fails
            currentChapter = 0;
            currentPage = 0;
            if (!loadChapterSync(0)) {
                showErrorScreen("Failed to load chapter");
                state = BROWSER;
                return false;
            }
        }
        
        // Validate page
        if (currentPage >= totalPages) {
            currentPage = totalPages > 0 ? totalPages - 1 : 0;
        }
        
        // Start reading stats
        stats.startSession();
        
        // Create render task
        if (!renderMutex) {
            renderMutex = xSemaphoreCreateMutex();
        }
        
        state = READING;
        bookIsOpen = true;
        firstRenderAfterOpen = true;
        updateRequired = true;
        pagesUntilFullRefresh = 30;
        
        // Load bookmarks for the resumed book
        loadBookmarks();
        
        // Load reading stats
        stats.load();
        stats.startSession();
        
        Serial.printf("[LIBRARY] Resumed book successfully\n");
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
        
        // Clean header
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Center the title
        const char* title = "Settings";
        int16_t tx, ty;
        uint16_t tw, th;
        display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 35);
        display.print(title);
        
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        // Section header - Reading
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, 75);
        display.setTextColor(GxEPD_BLACK);
        display.print("Reading");
        
        display.drawLine(20, 82, screenW - 20, 82, GxEPD_BLACK);
        
        // Settings items with cleaner layout
        int startY = 95;
        int itemHeight = landscape ? 38 : 44;
        int leftMargin = 25;
        int rightMargin = 25;
        int valueBoxWidth = 100;
        
        // Define menu structure: label, value getter
        struct MenuItem {
            const char* label;
            const char* value;
            bool hasToggle;  // Simple on/off vs value selector
        };
        
        // Get current values
        const char* fontFamilyVal = "Bookerly";  // Default font family
        const char* fontSizeVal = LibReaderSettings::getFontSizeName(settings.fontSize);
        const char* lineSpacingVal = LibReaderSettings::getSpacingName(settings.lineSpacing);
        const char* alignVal = settings.justifyText ? "Justify" : "Normal";
        
        MenuItem readingItems[] = {
            {"Font Family", fontFamilyVal, false},
            {"Font Size", fontSizeVal, false},
            {"Line Spacing", lineSpacingVal, false},
            {"Paragraph Alignment", alignVal, false},
        };
        int numReadingItems = 4;
        
        // Draw Reading section items
        int y = startY;
        for (int i = 0; i < numReadingItems; i++) {
            bool selected = (settingsCursor == i);
            
            // Draw item row
            if (selected) {
                display.fillRect(15, y - 5, screenW - 30, itemHeight - 4, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Label on left
            display.setCursor(leftMargin, y + 18);
            display.print(readingItems[i].label);
            
            // Value on right with box
            int valueX = screenW - rightMargin - valueBoxWidth;
            
            if (!selected) {
                display.drawRoundRect(valueX, y - 2, valueBoxWidth, itemHeight - 8, 4, GxEPD_BLACK);
            }
            
            // Center value text in box
            display.getTextBounds(readingItems[i].value, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(valueX + (valueBoxWidth - tw) / 2, y + 18);
            display.print(readingItems[i].value);
            
            // Draw right arrow indicator
            if (!selected) {
                display.setCursor(screenW - rightMargin - 15, y + 18);
                display.print(">");
            }
            
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight;
        }
        
        // Section header - Display
        y += 10;
        display.setCursor(20, y + 10);
        display.print("Display");
        display.drawLine(20, y + 17, screenW - 20, y + 17, GxEPD_BLACK);
        y += 30;
        
        MenuItem displayItems[] = {
            {"Sleep Screen", "Custom", false},
            {"Status Bar", settings.showProgress ? "Full" : "Off", false},
            {"Anti Aliasing", "On", true},
            {"Refresh Frequency", "15 pages", false},
        };
        int numDisplayItems = 4;
        
        for (int i = 0; i < numDisplayItems; i++) {
            int menuIdx = numReadingItems + i;
            bool selected = (settingsCursor == menuIdx);
            
            if (selected) {
                display.fillRect(15, y - 5, screenW - 30, itemHeight - 4, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setCursor(leftMargin, y + 18);
            display.print(displayItems[i].label);
            
            int valueX = screenW - rightMargin - valueBoxWidth;
            
            if (displayItems[i].hasToggle) {
                // Draw toggle switch style
                int toggleW = 50;
                int toggleH = 24;
                int toggleX = screenW - rightMargin - toggleW;
                int toggleY = y + 2;
                
                if (!selected) {
                    display.drawRoundRect(toggleX, toggleY, toggleW, toggleH, toggleH/2, GxEPD_BLACK);
                    // Draw filled circle on right (ON position)
                    display.fillCircle(toggleX + toggleW - toggleH/2 - 2, toggleY + toggleH/2, toggleH/2 - 4, GxEPD_BLACK);
                } else {
                    display.drawRoundRect(toggleX, toggleY, toggleW, toggleH, toggleH/2, GxEPD_WHITE);
                    display.fillCircle(toggleX + toggleW - toggleH/2 - 2, toggleY + toggleH/2, toggleH/2 - 4, GxEPD_WHITE);
                }
            } else {
                if (!selected) {
                    display.drawRoundRect(valueX, y - 2, valueBoxWidth, itemHeight - 8, 4, GxEPD_BLACK);
                }
                
                display.getTextBounds(displayItems[i].value, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(valueX + (valueBoxWidth - tw) / 2, y + 18);
                display.print(displayItems[i].value);
                
                if (!selected) {
                    display.setCursor(screenW - rightMargin - 15, y + 18);
                    display.print(">");
                }
            }
            
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight;
        }
        
        // Action buttons at bottom
        y += 15;
        
        // Chapters button (if reading a book)
        bool showChapters = bookIsOpen && totalChapters > 1;
        int actionIdx = numReadingItems + numDisplayItems;
        
        if (showChapters) {
            bool selected = (settingsCursor == actionIdx);
            if (selected) {
                display.fillRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            display.setCursor(leftMargin, y + 18);
            display.print("Go to Chapter...");
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight + 5;
            actionIdx++;
        }
        
        // Bookmarks button (if reading a book)
        if (bookIsOpen) {
            bool selected = (settingsCursor == actionIdx);
            if (selected) {
                display.fillRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            display.setCursor(leftMargin, y + 18);
            display.print("View Bookmarks");
            
            // Show bookmark count
            char countStr[16];
            snprintf(countStr, sizeof(countStr), "(%d)", bookmarks.count);
            display.setCursor(screenW - rightMargin - 60, y + 18);
            display.print(countStr);
            
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight + 5;
            actionIdx++;
            
            // Add bookmark button
            selected = (settingsCursor == actionIdx);
            if (selected) {
                display.fillRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            display.setCursor(leftMargin, y + 18);
            
            // Check if current page is bookmarked
            bool isBookmarked = (bookmarks.find(currentChapter, currentPage) >= 0);
            display.print(isBookmarked ? "Remove Bookmark" : "Add Bookmark Here");
            
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight + 5;
            actionIdx++;
        }
        
        // Reading Stats button
        {
            bool selected = (settingsCursor == actionIdx);
            if (selected) {
                display.fillRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            display.setCursor(leftMargin, y + 18);
            display.print("Reading Statistics");
            display.setTextColor(GxEPD_BLACK);
            y += itemHeight + 5;
            actionIdx++;
        }
        
        // Back button
        bool backSelected = (settingsCursor == actionIdx);
        if (backSelected) {
            display.fillRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.drawRoundRect(15, y - 3, screenW - 30, itemHeight - 2, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
        }
        
        const char* backText = "Back";
        display.getTextBounds(backText, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, y + 18);
        display.print(backText);
        
        // Footer hints
        display.setTextColor(GxEPD_BLACK);
        display.drawLine(0, screenH - 38, screenW, screenH - 38, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        
        // Bottom navigation hints - minimal
        display.setCursor(leftMargin, screenH - 14);
        display.print("Save");
        
        display.setCursor(screenW/2 - 30, screenH - 14);
        display.print("Toggle");
        
        display.setCursor(screenW - 80, screenH - 14);
        display.print("Up");
        
        display.setCursor(screenW - 45, screenH - 14);
        display.print("Down");
    }
    
    void drawBookmarkSelect() {
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Title
        const char* title = "Bookmarks";
        int16_t tx, ty;
        uint16_t tw, th;
        display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 35);
        display.print(title);
        
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        
        if (bookmarks.count == 0) {
            display.setCursor(screenW/2 - 80, screenH/2);
            display.print("No bookmarks yet");
            display.setCursor(screenW/2 - 100, screenH/2 + 25);
            display.print("Use Settings > Add Bookmark");
            return;
        }
        
        // Draw bookmark list
        int maxVisible = 8;
        int itemHeight = 45;
        int startY = 70;
        
        for (int i = 0; i < maxVisible && (bookmarkScrollOffset + i) < bookmarks.count; i++) {
            int idx = bookmarkScrollOffset + i;
            Bookmark& bm = bookmarks.bookmarks[idx];
            
            int y = startY + i * itemHeight;
            bool selected = (idx == bookmarkCursor);
            
            if (selected) {
                display.fillRoundRect(15, y - 5, screenW - 30, itemHeight - 5, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(15, y - 5, screenW - 30, itemHeight - 5, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Bookmark label
            display.setCursor(25, y + 15);
            display.print(bm.label);
            
            // Chapter/page info
            char info[32];
            snprintf(info, sizeof(info), "Ch%d, Pg%d", bm.chapter + 1, bm.page + 1);
            display.setCursor(screenW - 120, y + 15);
            display.print(info);
            
            display.setTextColor(GxEPD_BLACK);
        }
        
        // Scroll indicator
        if (bookmarks.count > maxVisible) {
            int scrollH = (screenH - 120) * maxVisible / bookmarks.count;
            int scrollY = 70 + ((screenH - 120 - scrollH) * bookmarkScrollOffset / (bookmarks.count - maxVisible));
            display.fillRoundRect(screenW - 8, scrollY, 4, scrollH, 2, GxEPD_BLACK);
        }
        
        // Footer
        display.drawLine(0, screenH - 38, screenW, screenH - 38, GxEPD_BLACK);
        display.setCursor(20, screenH - 14);
        display.print("OK: Jump to | BACK: Cancel");
    }
    
    void drawReadingStatsScreen() {
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Title
        const char* title = "Reading Statistics";
        int16_t tx, ty;
        uint16_t tw, th;
        display.getTextBounds(title, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 35);
        display.print(title);
        
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        display.setFont(&FreeSans9pt7b);
        
        int y = 80;
        int labelX = 30;
        int valueX = screenW - 150;
        int lineH = 35;
        
        // All-time stats
        display.setCursor(labelX, y);
        display.print("Lifetime Stats");
        display.drawLine(labelX, y + 5, screenW - 30, y + 5, GxEPD_BLACK);
        y += 25;
        
        // Total pages
        display.setCursor(labelX + 10, y);
        display.print("Pages Read:");
        display.setCursor(valueX, y);
        display.print(stats.totalPagesRead);
        y += lineH;
        
        // Total time
        display.setCursor(labelX + 10, y);
        display.print("Time Reading:");
        char timeStr[32];
        int hours = stats.totalMinutesRead / 60;
        int mins = stats.totalMinutesRead % 60;
        if (hours > 0) {
            snprintf(timeStr, sizeof(timeStr), "%dh %dm", hours, mins);
        } else {
            snprintf(timeStr, sizeof(timeStr), "%d min", mins);
        }
        display.setCursor(valueX, y);
        display.print(timeStr);
        y += lineH;
        
        // Books finished
        display.setCursor(labelX + 10, y);
        display.print("Books Finished:");
        display.setCursor(valueX, y);
        display.print(stats.totalBooksFinished);
        y += lineH + 15;
        
        // Current session
        display.setCursor(labelX, y);
        display.print("This Session");
        display.drawLine(labelX, y + 5, screenW - 30, y + 5, GxEPD_BLACK);
        y += 25;
        
        display.setCursor(labelX + 10, y);
        display.print("Pages:");
        display.setCursor(valueX, y);
        display.print(stats.sessionPagesRead);
        y += lineH;
        
        display.setCursor(labelX + 10, y);
        display.print("Time:");
        int sessionMins = stats.getSessionMinutes();
        if (sessionMins >= 60) {
            snprintf(timeStr, sizeof(timeStr), "%dh %dm", sessionMins / 60, sessionMins % 60);
        } else {
            snprintf(timeStr, sizeof(timeStr), "%d min", sessionMins);
        }
        display.setCursor(valueX, y);
        display.print(timeStr);
        
        // Footer
        display.drawLine(0, screenH - 38, screenW, screenH - 38, GxEPD_BLACK);
        display.setCursor(20, screenH - 14);
        display.print("Press OK or BACK to return");
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
    
    // Bookmarks for current book
    BookmarkList bookmarks;
    int bookmarkCursor;
    int bookmarkScrollOffset;
    
    // Flippable browser mode
    bool useFlipBrowser;
    
    // Track if a book is currently open
    bool bookIsOpen;
    bool pendingChapterLoad;  // Deferred chapter load for stack safety
    int pendingChapterToLoad; // Which chapter to load (-1 = use currentChapter)
    
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
