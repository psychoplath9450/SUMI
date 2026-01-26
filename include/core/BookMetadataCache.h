/**
 * @file BookMetadataCache.h
 * @brief Two-Tier Book Metadata Caching System
 * @version 1.4.2
 *
 * Two-tier caching system:
 * 
 * TIER 1 (book.bin) - Loaded to RAM:
 * - Title, author, language
 * - Spine entries (reading order)
 * - TOC entries (chapter names)
 * - Cover image path
 * - Total ~2-5KB for most books
 *
 * TIER 2 (sections/*.bin) - Streamed from SD:
 * - Pre-rendered page layouts
 * - Never fully loaded to RAM
 * - Includes validation header for auto-invalidation
 *
 * This separates small, frequently-accessed metadata from
 * large page data, dramatically reducing RAM usage.
 */

#ifndef SUMI_BOOK_METADATA_CACHE_H
#define SUMI_BOOK_METADATA_CACHE_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

// =============================================================================
// Constants
// =============================================================================
#define BOOK_CACHE_MAGIC    0x53554D49  // "SUMI"
#define BOOK_CACHE_VERSION  3
#define MAX_SPINE_ENTRIES   200
#define MAX_TOC_ENTRIES     100
#define MAX_TITLE_LEN       128
#define MAX_AUTHOR_LEN      64
#define MAX_HREF_LEN        128

// =============================================================================
// Spine Entry - Single item in reading order
// =============================================================================
struct SpineEntry {
    char href[MAX_HREF_LEN];    // Path to content file within EPUB
    uint32_t size;              // Uncompressed size in bytes
    uint16_t tocIndex;          // Corresponding TOC entry (-1 if none)
    
    void clear() {
        memset(href, 0, sizeof(href));
        size = 0;
        tocIndex = 0xFFFF;
    }
    
    void serialize(File& f) const {
        f.write((uint8_t*)href, sizeof(href));
        f.write((uint8_t*)&size, sizeof(size));
        f.write((uint8_t*)&tocIndex, sizeof(tocIndex));
    }
    
    bool deserialize(File& f) {
        if (f.read((uint8_t*)href, sizeof(href)) != sizeof(href)) return false;
        if (f.read((uint8_t*)&size, sizeof(size)) != sizeof(size)) return false;
        if (f.read((uint8_t*)&tocIndex, sizeof(tocIndex)) != sizeof(tocIndex)) return false;
        return true;
    }
};

// =============================================================================
// TOC Entry - Table of contents item
// =============================================================================
struct TocEntry {
    char title[64];             // Chapter title
    char href[MAX_HREF_LEN];    // Path to content
    char anchor[32];            // Fragment identifier (#anchor)
    uint8_t level;              // Nesting level (0 = top)
    uint16_t spineIndex;        // Corresponding spine item
    
    void clear() {
        memset(title, 0, sizeof(title));
        memset(href, 0, sizeof(href));
        memset(anchor, 0, sizeof(anchor));
        level = 0;
        spineIndex = 0;
    }
    
    void serialize(File& f) const {
        f.write((uint8_t*)title, sizeof(title));
        f.write((uint8_t*)href, sizeof(href));
        f.write((uint8_t*)anchor, sizeof(anchor));
        f.write((uint8_t*)&level, sizeof(level));
        f.write((uint8_t*)&spineIndex, sizeof(spineIndex));
    }
    
    bool deserialize(File& f) {
        if (f.read((uint8_t*)title, sizeof(title)) != sizeof(title)) return false;
        if (f.read((uint8_t*)href, sizeof(href)) != sizeof(href)) return false;
        if (f.read((uint8_t*)anchor, sizeof(anchor)) != sizeof(anchor)) return false;
        if (f.read((uint8_t*)&level, sizeof(level)) != sizeof(level)) return false;
        if (f.read((uint8_t*)&spineIndex, sizeof(spineIndex)) != sizeof(spineIndex)) return false;
        return true;
    }
};

// =============================================================================
// Section Validation Header - Stored at start of each section file
// =============================================================================
struct SectionHeader {
    uint32_t magic;             // BOOK_CACHE_MAGIC
    uint16_t version;           // BOOK_CACHE_VERSION
    uint16_t screenWidth;       // Display width when cached
    uint16_t screenHeight;      // Display height when cached
    uint8_t fontSize;           // Font size setting
    uint8_t margins;            // Margin setting
    uint8_t lineSpacing;        // Line spacing setting
    uint8_t justify;            // Justification on/off
    uint16_t pageCount;         // Number of pages in this section
    uint32_t settingsHash;      // Hash of all reader settings
    
    bool matches(const SectionHeader& other) const {
        return magic == BOOK_CACHE_MAGIC &&
               version == BOOK_CACHE_VERSION &&
               screenWidth == other.screenWidth &&
               screenHeight == other.screenHeight &&
               fontSize == other.fontSize &&
               margins == other.margins &&
               lineSpacing == other.lineSpacing &&
               justify == other.justify;
    }
    
    void serialize(File& f) const {
        f.write((uint8_t*)this, sizeof(SectionHeader));
    }
    
    bool deserialize(File& f) {
        return f.read((uint8_t*)this, sizeof(SectionHeader)) == sizeof(SectionHeader);
    }
};

// =============================================================================
// Book Metadata - Tier 1 cache (loaded to RAM)
// =============================================================================
class BookMetadataCache {
public:
    // Header
    uint32_t magic;
    uint16_t version;
    
    // Metadata
    char title[MAX_TITLE_LEN];
    char author[MAX_AUTHOR_LEN];
    char language[8];
    char coverHref[MAX_HREF_LEN];
    char contentBasePath[MAX_HREF_LEN];
    
    // Spine (reading order)
    uint16_t spineCount;
    SpineEntry spine[MAX_SPINE_ENTRIES];
    
    // TOC
    uint16_t tocCount;
    TocEntry toc[MAX_TOC_ENTRIES];
    
    // Computed values
    uint32_t totalSize;         // Sum of all spine entry sizes
    
    BookMetadataCache() {
        clear();
    }
    
    void clear() {
        magic = BOOK_CACHE_MAGIC;
        version = BOOK_CACHE_VERSION;
        memset(title, 0, sizeof(title));
        memset(author, 0, sizeof(author));
        memset(language, 0, sizeof(language));
        memset(coverHref, 0, sizeof(coverHref));
        memset(contentBasePath, 0, sizeof(contentBasePath));
        spineCount = 0;
        tocCount = 0;
        totalSize = 0;
        
        for (int i = 0; i < MAX_SPINE_ENTRIES; i++) {
            spine[i].clear();
        }
        for (int i = 0; i < MAX_TOC_ENTRIES; i++) {
            toc[i].clear();
        }
    }
    
    // Add a spine entry
    bool addSpineEntry(const char* href, uint32_t size) {
        if (spineCount >= MAX_SPINE_ENTRIES) return false;
        
        strncpy(spine[spineCount].href, href, MAX_HREF_LEN - 1);
        spine[spineCount].size = size;
        spine[spineCount].tocIndex = 0xFFFF;
        totalSize += size;
        spineCount++;
        return true;
    }
    
    // Add a TOC entry
    bool addTocEntry(const char* entryTitle, const char* href, const char* anchor, uint8_t level) {
        if (tocCount >= MAX_TOC_ENTRIES) return false;
        
        strncpy(toc[tocCount].title, entryTitle, 63);
        strncpy(toc[tocCount].href, href, MAX_HREF_LEN - 1);
        if (anchor) {
            strncpy(toc[tocCount].anchor, anchor, 31);
        }
        toc[tocCount].level = level;
        toc[tocCount].spineIndex = findSpineIndexForHref(href);
        tocCount++;
        return true;
    }
    
    // Find spine index for a given href
    uint16_t findSpineIndexForHref(const char* href) const {
        for (uint16_t i = 0; i < spineCount; i++) {
            if (strcmp(spine[i].href, href) == 0) {
                return i;
            }
            // Also check without anchor
            const char* hashPos = strchr(href, '#');
            if (hashPos) {
                size_t len = hashPos - href;
                if (strncmp(spine[i].href, href, len) == 0 && 
                    strlen(spine[i].href) == len) {
                    return i;
                }
            }
        }
        return 0;
    }
    
    // Link TOC entries to spine entries
    void linkTocToSpine() {
        for (uint16_t i = 0; i < tocCount; i++) {
            toc[i].spineIndex = findSpineIndexForHref(toc[i].href);
            
            // Update spine's tocIndex if not already set
            if (toc[i].spineIndex < spineCount && 
                spine[toc[i].spineIndex].tocIndex == 0xFFFF) {
                spine[toc[i].spineIndex].tocIndex = i;
            }
        }
    }
    
    // Get TOC index for a spine index
    int getTocIndexForSpine(uint16_t spineIndex) const {
        if (spineIndex >= spineCount) return -1;
        if (spine[spineIndex].tocIndex == 0xFFFF) return -1;
        return spine[spineIndex].tocIndex;
    }
    
    // Calculate reading progress (0.0 - 1.0)
    float calculateProgress(uint16_t spineIndex, float spineProgress) const {
        if (totalSize == 0 || spineCount == 0) return 0.0f;
        
        // Sum sizes up to current spine
        uint32_t readSize = 0;
        for (uint16_t i = 0; i < spineIndex && i < spineCount; i++) {
            readSize += spine[i].size;
        }
        
        // Add partial progress in current spine
        if (spineIndex < spineCount) {
            readSize += (uint32_t)(spine[spineIndex].size * spineProgress);
        }
        
        return (float)readSize / (float)totalSize;
    }
    
    // Save to file
    bool save(const String& path) {
        File f = SD.open(path, FILE_WRITE);
        if (!f) {
            Serial.printf("[CACHE] Failed to save: %s\n", path.c_str());
            return false;
        }
        
        // Write header
        f.write((uint8_t*)&magic, sizeof(magic));
        f.write((uint8_t*)&version, sizeof(version));
        
        // Write metadata
        f.write((uint8_t*)title, sizeof(title));
        f.write((uint8_t*)author, sizeof(author));
        f.write((uint8_t*)language, sizeof(language));
        f.write((uint8_t*)coverHref, sizeof(coverHref));
        f.write((uint8_t*)contentBasePath, sizeof(contentBasePath));
        
        // Write spine
        f.write((uint8_t*)&spineCount, sizeof(spineCount));
        for (uint16_t i = 0; i < spineCount; i++) {
            spine[i].serialize(f);
        }
        
        // Write TOC
        f.write((uint8_t*)&tocCount, sizeof(tocCount));
        for (uint16_t i = 0; i < tocCount; i++) {
            toc[i].serialize(f);
        }
        
        // Write computed values
        f.write((uint8_t*)&totalSize, sizeof(totalSize));
        
        f.close();
        
        Serial.printf("[CACHE] Saved book.bin: %d spine, %d toc, %d bytes total\n",
                      spineCount, tocCount, totalSize);
        return true;
    }
    
    // Load from file
    bool load(const String& path) {
        File f = SD.open(path, FILE_READ);
        if (!f) {
            return false;
        }
        
        clear();
        
        // Read and validate header
        if (f.read((uint8_t*)&magic, sizeof(magic)) != sizeof(magic) ||
            f.read((uint8_t*)&version, sizeof(version)) != sizeof(version)) {
            f.close();
            return false;
        }
        
        if (magic != BOOK_CACHE_MAGIC || version != BOOK_CACHE_VERSION) {
            Serial.printf("[CACHE] Version mismatch: %08X/%d vs %08X/%d\n",
                          magic, version, BOOK_CACHE_MAGIC, BOOK_CACHE_VERSION);
            f.close();
            return false;
        }
        
        // Read metadata
        f.read((uint8_t*)title, sizeof(title));
        f.read((uint8_t*)author, sizeof(author));
        f.read((uint8_t*)language, sizeof(language));
        f.read((uint8_t*)coverHref, sizeof(coverHref));
        f.read((uint8_t*)contentBasePath, sizeof(contentBasePath));
        
        // Read spine
        f.read((uint8_t*)&spineCount, sizeof(spineCount));
        if (spineCount > MAX_SPINE_ENTRIES) {
            f.close();
            clear();
            return false;
        }
        for (uint16_t i = 0; i < spineCount; i++) {
            if (!spine[i].deserialize(f)) {
                f.close();
                clear();
                return false;
            }
        }
        
        // Read TOC
        f.read((uint8_t*)&tocCount, sizeof(tocCount));
        if (tocCount > MAX_TOC_ENTRIES) {
            f.close();
            clear();
            return false;
        }
        for (uint16_t i = 0; i < tocCount; i++) {
            if (!toc[i].deserialize(f)) {
                f.close();
                clear();
                return false;
            }
        }
        
        // Read computed values
        f.read((uint8_t*)&totalSize, sizeof(totalSize));
        
        f.close();
        
        Serial.printf("[CACHE] Loaded book.bin: \"%s\" by %s, %d chapters\n",
                      title, author, spineCount);
        return true;
    }
};

// =============================================================================
// Book Cache Manager - Handles cache directory structure
// =============================================================================
class BookCacheManager {
public:
    static String getCachePath(const String& bookPath) {
        // Generate hash of book path for unique cache directory
        uint32_t hash = 5381;
        for (size_t i = 0; i < bookPath.length(); i++) {
            hash = ((hash << 5) + hash) ^ bookPath[i];
        }
        
        char hex[16];
        snprintf(hex, sizeof(hex), "%08x", hash);
        
        return String("/.sumi/books/") + hex;
    }
    
    static void ensureCacheDir(const String& cachePath) {
        if (!SD.exists("/.sumi")) SD.mkdir("/.sumi");
        if (!SD.exists("/.sumi/books")) SD.mkdir("/.sumi/books");
        if (!SD.exists(cachePath)) SD.mkdir(cachePath);
        
        String sectionsPath = cachePath + "/sections";
        if (!SD.exists(sectionsPath)) SD.mkdir(sectionsPath);
    }
    
    static String getBookBinPath(const String& cachePath) {
        return cachePath + "/book.bin";
    }
    
    static String getSectionPath(const String& cachePath, int spineIndex) {
        char filename[32];
        snprintf(filename, sizeof(filename), "/sections/%d.bin", spineIndex);
        return cachePath + filename;
    }
    
    static String getProgressPath(const String& cachePath) {
        return cachePath + "/progress.bin";
    }
    
    static String getCoverPath(const String& cachePath) {
        return cachePath + "/cover.bmp";
    }
    
    // Save reading progress (4 bytes: spine + page)
    static bool saveProgress(const String& cachePath, uint16_t spineIndex, uint16_t page) {
        String path = getProgressPath(cachePath);
        File f = SD.open(path, FILE_WRITE);
        if (!f) return false;
        
        f.write((uint8_t*)&spineIndex, sizeof(spineIndex));
        f.write((uint8_t*)&page, sizeof(page));
        f.close();
        return true;
    }
    
    // Load reading progress
    static bool loadProgress(const String& cachePath, uint16_t& spineIndex, uint16_t& page) {
        String path = getProgressPath(cachePath);
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        
        bool ok = true;
        ok = ok && (f.read((uint8_t*)&spineIndex, sizeof(spineIndex)) == sizeof(spineIndex));
        ok = ok && (f.read((uint8_t*)&page, sizeof(page)) == sizeof(page));
        f.close();
        return ok;
    }
    
    // Check if section cache is valid for current settings
    static bool isSectionValid(const String& sectionPath, const SectionHeader& currentSettings) {
        File f = SD.open(sectionPath, FILE_READ);
        if (!f) return false;
        
        SectionHeader cached;
        if (!cached.deserialize(f)) {
            f.close();
            return false;
        }
        f.close();
        
        return cached.matches(currentSettings);
    }
    
    // Delete all cache for a book
    static void clearBookCache(const String& cachePath) {
        // Delete section files
        String sectionsPath = cachePath + "/sections";
        File dir = SD.open(sectionsPath);
        if (dir && dir.isDirectory()) {
            while (File entry = dir.openNextFile()) {
                String path = sectionsPath + "/" + entry.name();
                entry.close();
                SD.remove(path);
            }
            dir.close();
        }
        SD.rmdir(sectionsPath);
        
        // Delete other files
        SD.remove(cachePath + "/book.bin");
        SD.remove(cachePath + "/progress.bin");
        SD.remove(cachePath + "/cover.bmp");
        
        // Remove directory
        SD.rmdir(cachePath);
        
        Serial.printf("[CACHE] Cleared: %s\n", cachePath.c_str());
    }
};

#endif // SUMI_BOOK_METADATA_CACHE_H
