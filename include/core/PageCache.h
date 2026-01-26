/**
 * @file PageCache.h
 * @brief Page Caching System for Sumi E-Reader
 * @version 1.3.0
 *
 * Key Features:
 * - Pre-computed word positions stored to SD
 * - Instant page loads (no re-pagination)
 * - Cache invalidation when settings change
 * - Per-book cache directories
 *
 * Cache Structure on SD:
 * /.sumi/books/
 *   ├── HASH/                 # Hash of book path (8 hex chars)
 *   │   ├── meta.bin          # Book metadata + cache key
 *   │   ├── progress.bin      # Reading position
 *   │   └── pages/
 *   │       ├── 0_0.bin       # Chapter 0, Page 0
 *   │       ├── 0_1.bin       # Chapter 0, Page 1
 *   │       └── ...
 *   └── ...
 */

#ifndef SUMI_PAGE_CACHE_H
#define SUMI_PAGE_CACHE_H

#include <Arduino.h>
#include <SD.h>
#include <vector>
#include "config.h"

// =============================================================================
// Cache Configuration - Compact for ESP32-C3 memory constraints
// Target: Keep CachedPage under 8KB to reduce stack/heap pressure
// =============================================================================

// Maximum word length (longer words get truncated)
#define CACHE_MAX_WORD_LEN      20

// Maximum words per line
#define CACHE_MAX_WORDS_LINE    10

// Maximum lines per page
#define CACHE_MAX_LINES_PAGE    20

// Cache file magic number (for validation)
#define CACHE_MAGIC             0x53554D49  // "SUMI"

// Cache version (increment when format changes)
#define CACHE_VERSION           5

// =============================================================================
// Cache Key - Determines when cache is valid
// =============================================================================
struct CacheKey {
    uint8_t fontSize;       // Font size setting (0-4)
    uint8_t margins;        // Margin size in pixels
    uint8_t lineSpacing;    // Line spacing (10-20 for 1.0x-2.0x)
    uint8_t flags;          // Bit flags: justify, etc.
    uint16_t screenWidth;   // Display width
    uint16_t screenHeight;  // Display height
    
    CacheKey() : fontSize(2), margins(1), lineSpacing(14), 
                 flags(0x01), screenWidth(800), screenHeight(480) {}  // flags=1 means justify=true
    
    // Generate hash for comparison
    uint32_t hash() const {
        uint32_t h = 5381;
        h = ((h << 5) + h) ^ fontSize;
        h = ((h << 5) + h) ^ margins;
        h = ((h << 5) + h) ^ lineSpacing;
        h = ((h << 5) + h) ^ flags;
        h = ((h << 5) + h) ^ screenWidth;
        h = ((h << 5) + h) ^ screenHeight;
        return h;
    }
    
    bool matches(const CacheKey& other) const {
        return hash() == other.hash();
    }
    
    // Set justify flag
    void setJustify(bool j) { 
        if (j) flags |= 0x01; 
        else flags &= ~0x01; 
    }
    bool getJustify() const { return flags & 0x01; }
};

// =============================================================================
// Cached Word - Single word with pre-computed position
// =============================================================================
struct CachedWord {
    uint16_t xPos;                      // X position on line (pixels)
    uint8_t style;                      // 0=regular, 1=bold, 2=italic, 3=bold-italic
    uint8_t length;                     // Text length
    char text[CACHE_MAX_WORD_LEN];      // Word text
    
    CachedWord() : xPos(0), style(0), length(0) {
        memset(text, 0, sizeof(text));
    }
    
    CachedWord(uint16_t x, const char* t, uint8_t s = 0) : xPos(x), style(s) {
        length = strlen(t);
        if (length > CACHE_MAX_WORD_LEN - 1) length = CACHE_MAX_WORD_LEN - 1;
        strncpy(text, t, length);
        text[length] = '\0';
    }
    
    // Binary serialization (fixed size for simplicity)
    void serialize(File& f) const {
        f.write((uint8_t*)&xPos, sizeof(xPos));
        f.write(&style, 1);
        f.write(&length, 1);
        f.write((uint8_t*)text, CACHE_MAX_WORD_LEN);
    }
    
    bool deserialize(File& f) {
        if (f.read((uint8_t*)&xPos, sizeof(xPos)) != sizeof(xPos)) return false;
        if (f.read(&style, 1) != 1) return false;
        if (f.read(&length, 1) != 1) return false;
        if (f.read((uint8_t*)text, CACHE_MAX_WORD_LEN) != CACHE_MAX_WORD_LEN) return false;
        text[CACHE_MAX_WORD_LEN - 1] = '\0';  // Safety
        return true;
    }
    
    // Size in bytes when serialized
    static constexpr size_t serializedSize() {
        return sizeof(uint16_t) + 1 + 1 + CACHE_MAX_WORD_LEN;
    }
};

// =============================================================================
// Cached Line - Single line with words OR image placeholder
// =============================================================================
struct CachedLine {
    uint16_t yPos;              // Y position on page (pixels)
    uint8_t wordCount;          // Number of words (0 if this is an image)
    uint8_t flags;              // Bit 0: isLastInPara, Bit 1: isImage
    CachedWord words[CACHE_MAX_WORDS_LINE];
    
    // Image data (used when isImage() is true)
    struct ImageInfo {
        char path[64];          // Path to extracted image file
        uint16_t width;         // Display width
        uint16_t height;        // Display height
        
        ImageInfo() : width(0), height(0) {
            memset(path, 0, sizeof(path));
        }
    };
    
    CachedLine() : yPos(0), wordCount(0), flags(0) {}
    
    bool isLastInPara() const { return flags & 0x01; }
    void setLastInPara(bool v) { if (v) flags |= 0x01; else flags &= ~0x01; }
    
    bool isImage() const { return flags & 0x02; }
    void setIsImage(bool v) { if (v) flags |= 0x02; else flags &= ~0x02; }
    
    // Get image info (stored in words array space)
    ImageInfo* getImageInfo() {
        return reinterpret_cast<ImageInfo*>(&words[0]);
    }
    const ImageInfo* getImageInfo() const {
        return reinterpret_cast<const ImageInfo*>(&words[0]);
    }
    
    void setImage(const char* path, uint16_t w, uint16_t h, uint16_t y) {
        setIsImage(true);
        wordCount = 0;
        yPos = y;
        ImageInfo* info = getImageInfo();
        strncpy(info->path, path, sizeof(info->path) - 1);
        info->width = w;
        info->height = h;
    }
    
    void addWord(const CachedWord& w) {
        if (wordCount < CACHE_MAX_WORDS_LINE) {
            words[wordCount++] = w;
        }
    }
    
    void serialize(File& f) const {
        f.write((uint8_t*)&yPos, sizeof(yPos));
        f.write(&wordCount, 1);
        f.write(&flags, 1);
        if (isImage()) {
            // Serialize image info
            const ImageInfo* info = getImageInfo();
            f.write((uint8_t*)info->path, sizeof(info->path));
            f.write((uint8_t*)&info->width, sizeof(info->width));
            f.write((uint8_t*)&info->height, sizeof(info->height));
        } else {
            // Serialize words
            for (int i = 0; i < wordCount; i++) {
                words[i].serialize(f);
            }
        }
    }
    
    bool deserialize(File& f) {
        if (f.read((uint8_t*)&yPos, sizeof(yPos)) != sizeof(yPos)) return false;
        if (f.read(&wordCount, 1) != 1) return false;
        if (f.read(&flags, 1) != 1) return false;
        
        if (isImage()) {
            // Deserialize image info
            ImageInfo* info = getImageInfo();
            if (f.read((uint8_t*)info->path, sizeof(info->path)) != sizeof(info->path)) return false;
            if (f.read((uint8_t*)&info->width, sizeof(info->width)) != sizeof(info->width)) return false;
            if (f.read((uint8_t*)&info->height, sizeof(info->height)) != sizeof(info->height)) return false;
        } else {
            // Deserialize words
            if (wordCount > CACHE_MAX_WORDS_LINE) wordCount = CACHE_MAX_WORDS_LINE;
            for (int i = 0; i < wordCount; i++) {
                if (!words[i].deserialize(f)) return false;
            }
        }
        return true;
    }
};

// =============================================================================
// Cached Page - Single page with lines
// =============================================================================
struct CachedPage {
    uint8_t lineCount;          // Number of lines
    uint8_t reserved;           // Padding
    uint16_t startOffset;       // Start position in source text
    uint16_t endOffset;         // End position in source text
    CachedLine lines[CACHE_MAX_LINES_PAGE];
    
    CachedPage() : lineCount(0), reserved(0), startOffset(0), endOffset(0) {}
    
    void addLine(const CachedLine& l) {
        if (lineCount < CACHE_MAX_LINES_PAGE) {
            lines[lineCount++] = l;
        }
    }
    
    void clear() {
        lineCount = 0;
        startOffset = 0;
        endOffset = 0;
    }
    
    void serialize(File& f) const {
        f.write(&lineCount, 1);
        f.write(&reserved, 1);
        f.write((uint8_t*)&startOffset, sizeof(startOffset));
        f.write((uint8_t*)&endOffset, sizeof(endOffset));
        for (int i = 0; i < lineCount; i++) {
            lines[i].serialize(f);
        }
    }
    
    bool deserialize(File& f) {
        if (f.read(&lineCount, 1) != 1) return false;
        if (f.read(&reserved, 1) != 1) return false;
        if (f.read((uint8_t*)&startOffset, sizeof(startOffset)) != sizeof(startOffset)) return false;
        if (f.read((uint8_t*)&endOffset, sizeof(endOffset)) != sizeof(endOffset)) return false;
        if (lineCount > CACHE_MAX_LINES_PAGE) lineCount = CACHE_MAX_LINES_PAGE;
        for (int i = 0; i < lineCount; i++) {
            if (!lines[i].deserialize(f)) return false;
        }
        return true;
    }
};

// =============================================================================
// Book Cache Metadata
// =============================================================================
struct BookCacheMeta {
    uint32_t magic;             // CACHE_MAGIC
    uint8_t version;            // CACHE_VERSION
    uint8_t chapterCount;       // Total chapters
    uint16_t reserved;          // Padding
    CacheKey key;               // Settings when cached
    uint16_t pageCount[32];     // Page counts per chapter (max 32 chapters)
    uint32_t timestamp;         // When cache was created
    
    BookCacheMeta() : magic(CACHE_MAGIC), version(CACHE_VERSION),
                      chapterCount(1), reserved(0), timestamp(0) {
        memset(pageCount, 0, sizeof(pageCount));
    }
    
    bool isValid() const {
        return magic == CACHE_MAGIC && version == CACHE_VERSION;
    }
    
    void serialize(File& f) const {
        f.write((uint8_t*)&magic, sizeof(magic));
        f.write(&version, 1);
        f.write(&chapterCount, 1);
        f.write((uint8_t*)&reserved, sizeof(reserved));
        f.write((uint8_t*)&key, sizeof(key));
        f.write((uint8_t*)pageCount, sizeof(pageCount));
        f.write((uint8_t*)&timestamp, sizeof(timestamp));
    }
    
    bool deserialize(File& f) {
        if (f.read((uint8_t*)&magic, sizeof(magic)) != sizeof(magic)) return false;
        if (f.read(&version, 1) != 1) return false;
        if (f.read(&chapterCount, 1) != 1) return false;
        if (f.read((uint8_t*)&reserved, sizeof(reserved)) != sizeof(reserved)) return false;
        if (f.read((uint8_t*)&key, sizeof(key)) != sizeof(key)) return false;
        if (f.read((uint8_t*)pageCount, sizeof(pageCount)) != sizeof(pageCount)) return false;
        if (f.read((uint8_t*)&timestamp, sizeof(timestamp)) != sizeof(timestamp)) return false;
        return isValid();
    }
};

// =============================================================================
// Page Cache Manager Class
// =============================================================================
class PageCache {
public:
    PageCache();
    
    // =========================================================================
    // Initialization
    // =========================================================================
    
    /**
     * Initialize cache for a book
     * Creates cache directory if needed
     * @param bookPath Full path to book file
     */
    void init(const String& bookPath);
    
    /**
     * Close current cache (flush any pending writes)
     */
    void close();
    
    // =========================================================================
    // Cache Validation
    // =========================================================================
    
    /**
     * Check if cache exists and matches current settings
     * @param key Current cache key (from settings)
     * @return true if cache is valid and can be used
     */
    bool hasValidCache(const CacheKey& key);
    
    /**
     * Check if specific page exists in cache
     */
    bool hasPage(int chapter, int page);
    
    /**
     * Get page count for a chapter
     * @return -1 if not cached, otherwise page count
     */
    int getPageCount(int chapter);
    
    /**
     * Get total chapter count
     */
    int getChapterCount() const { return _meta.chapterCount; }
    
    // =========================================================================
    // Page Operations
    // =========================================================================
    
    /**
     * Load a page from cache
     * @param chapter Chapter index
     * @param page Page index within chapter
     * @param outPage Output page data
     * @return true if loaded successfully
     */
    bool loadPage(int chapter, int page, CachedPage& outPage);
    
    /**
     * Save a page to cache
     */
    bool savePage(int chapter, int page, const CachedPage& pageData);
    
    // =========================================================================
    // Metadata Operations
    // =========================================================================
    
    /**
     * Save metadata (call after all pages are cached)
     */
    bool saveMeta(const CacheKey& key, int chapterCount);
    
    /**
     * Set page count for a chapter
     */
    void setPageCount(int chapter, int count);
    
    /**
     * Load existing metadata
     */
    bool loadMeta();
    
    // =========================================================================
    // Cache Invalidation
    // =========================================================================
    
    /**
     * Invalidate cache for current book
     */
    void invalidateBook();
    
    /**
     * Invalidate all caches (clear /.sumi/books/)
     */
    void invalidateAll();
    
    // =========================================================================
    // Progress Persistence (per-book)
    // =========================================================================
    
    /**
     * Save reading progress for current book
     */
    bool saveProgress(int chapter, int page);
    
    /**
     * Load reading progress for current book
     * @param outChapter Output chapter index
     * @param outPage Output page index
     * @return true if progress found
     */
    bool loadProgress(int& outChapter, int& outPage);
    
    // =========================================================================
    // Utilities
    // =========================================================================
    
    /**
     * Get cache directory path for current book
     */
    const String& getCachePath() const { return _cachePath; }
    
    /**
     * Check if cache is initialized
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * Get cache statistics
     */
    void getStats(int& cachedPages, int& totalSize);

private:
    String _bookPath;       // Original book path
    String _cachePath;      // Cache directory (/.sumi/books/HASH/)
    String _bookHash;       // 8-char hex hash of book path
    BookCacheMeta _meta;    // Cached metadata
    bool _initialized;      // Whether init() was called
    bool _metaLoaded;       // Whether metadata is loaded
    
    // Generate hash of book path
    String hashPath(const String& path);
    
    // Get path to page file
    String getPagePath(int chapter, int page);
    
    // Get path to metadata file
    String getMetaPath();
    
    // Get path to progress file
    String getProgressPath();
    
    // Ensure directory exists
    void ensureDirectory(const String& path);
    
    // Recursive directory delete
    void deleteDirectory(const String& path);
};

// Global instance
extern PageCache* pageCache;

#endif // SUMI_PAGE_CACHE_H
