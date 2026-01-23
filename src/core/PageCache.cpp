/**
 * @file PageCache.cpp
 * @brief Page Caching System Implementation
 * @version 2.1.16
 *
 * Implements the page caching system that:
 * - Stores pre-computed page layouts to SD card
 * - Enables instant page turns (no re-pagination)
 * - Automatically invalidates when settings change
 * - Manages per-book cache directories
 */

#include <SD.h>
#include <vector>
#include "core/PageCache.h"

// Global instance
PageCache pageCache;

// =============================================================================
// Constructor
// =============================================================================

PageCache::PageCache() 
    : _initialized(false), _metaLoaded(false) {
}

// =============================================================================
// Initialization
// =============================================================================

void PageCache::init(const String& bookPath) {
    _bookPath = bookPath;
    _bookHash = hashPath(bookPath);
    _cachePath = String(PATH_SUMI_CACHE) + "/books/" + _bookHash;
    _initialized = true;
    _metaLoaded = false;
    
    // Ensure cache directories exist
    ensureDirectory(PATH_SUMI_CACHE);
    ensureDirectory(String(PATH_SUMI_CACHE) + "/books");
    ensureDirectory(_cachePath);
    ensureDirectory(_cachePath + "/pages");
    
    // Try to load existing metadata
    _metaLoaded = loadMeta();
    
    Serial.printf("[CACHE] Init: %s -> %s\n", bookPath.c_str(), _bookHash.c_str());
    if (_metaLoaded) {
        Serial.printf("[CACHE] Loaded meta: %d chapters\n", _meta.chapterCount);
    }
}

void PageCache::close() {
    _initialized = false;
    _metaLoaded = false;
}

// =============================================================================
// Cache Validation
// =============================================================================

bool PageCache::hasValidCache(const CacheKey& key) {
    if (!_initialized || !_metaLoaded) {
        Serial.println("[CACHE] Not initialized or meta not loaded");
        return false;
    }
    
    // Log both keys for comparison
    Serial.printf("[CACHE] Checking cache validity:\n");
    Serial.printf("[CACHE]   Current: screen=%dx%d, font=%d, margins=%d, lineSpacing=%d\n",
                  key.screenWidth, key.screenHeight, key.fontSize, key.margins, key.lineSpacing);
    Serial.printf("[CACHE]   Cached:  screen=%dx%d, font=%d, margins=%d, lineSpacing=%d\n",
                  _meta.key.screenWidth, _meta.key.screenHeight, _meta.key.fontSize, 
                  _meta.key.margins, _meta.key.lineSpacing);
    
    // Check if cache key matches current settings
    if (!_meta.key.matches(key)) {
        Serial.println("[CACHE] Key mismatch - cache invalid, will re-index");
        return false;
    }
    
    // Additional validation: screen dimensions must match EXACTLY
    // (not just hash match) to prevent layout issues
    if (_meta.key.screenWidth != key.screenWidth || 
        _meta.key.screenHeight != key.screenHeight) {
        Serial.printf("[CACHE] Screen dimension mismatch! Cached=%dx%d, Current=%dx%d\n",
                      _meta.key.screenWidth, _meta.key.screenHeight,
                      key.screenWidth, key.screenHeight);
        return false;
    }
    
    Serial.println("[CACHE] Cache key matches");
    
    // Verify at least one page exists
    return hasPage(0, 0);
}

bool PageCache::hasPage(int chapter, int page) {
    if (!_initialized) return false;
    
    String pagePath = getPagePath(chapter, page);
    return SD.exists(pagePath);
}

int PageCache::getPageCount(int chapter) {
    if (!_initialized || !_metaLoaded) return -1;
    if (chapter < 0 || chapter >= 32) return -1;
    
    return _meta.pageCount[chapter];
}

// =============================================================================
// Page Operations
// =============================================================================

bool PageCache::loadPage(int chapter, int page, CachedPage& outPage) {
    if (!_initialized) {
        Serial.println("[CACHE] Not initialized");
        return false;
    }
    
    String pagePath = getPagePath(chapter, page);
    File f = SD.open(pagePath, FILE_READ);
    if (!f) {
        Serial.printf("[CACHE] Page not found: %s\n", pagePath.c_str());
        return false;
    }
    
    bool success = outPage.deserialize(f);
    f.close();
    
    if (success) {
        Serial.printf("[CACHE] Loaded page %d_%d (%d lines)\n", chapter, page, outPage.lineCount);
        
        // Validate positions - check if any word has xPos beyond reasonable bounds
        // This catches cache corruption or orientation mismatches
        for (int i = 0; i < outPage.lineCount && i < 3; i++) {  // Check first 3 lines
            const CachedLine& line = outPage.lines[i];
            for (int j = 0; j < line.wordCount; j++) {
                if (line.words[j].xPos > 1000) {  // Sanity check - no e-ink is >1000px wide
                    Serial.printf("[CACHE] WARNING: Word has xPos=%d (line %d, word %d)\n",
                                  line.words[j].xPos, i, j);
                }
            }
            if (line.wordCount > 0) {
                Serial.printf("[CACHE]   Line %d: yPos=%d, firstWord.xPos=%d, text='%.20s...'\n",
                              i, line.yPos, line.words[0].xPos, line.words[0].text);
            }
        }
    } else {
        Serial.printf("[CACHE] Failed to deserialize %s\n", pagePath.c_str());
    }
    
    return success;
}

bool PageCache::savePage(int chapter, int page, const CachedPage& pageData) {
    if (!_initialized) {
        Serial.println("[CACHE] Not initialized");
        return false;
    }
    
    String pagePath = getPagePath(chapter, page);
    File f = SD.open(pagePath, FILE_WRITE);
    if (!f) {
        Serial.printf("[CACHE] Failed to create: %s\n", pagePath.c_str());
        return false;
    }
    
    pageData.serialize(f);
    f.close();
    
    Serial.printf("[CACHE] Saved page %d_%d (%d lines)\n", chapter, page, pageData.lineCount);
    return true;
}

// =============================================================================
// Metadata Operations
// =============================================================================

bool PageCache::saveMeta(const CacheKey& key, int chapterCount) {
    if (!_initialized) return false;
    
    _meta.magic = CACHE_MAGIC;
    _meta.version = CACHE_VERSION;
    _meta.key = key;
    _meta.chapterCount = chapterCount;
    _meta.timestamp = millis();  // In real implementation, use RTC
    
    String metaPath = getMetaPath();
    File f = SD.open(metaPath, FILE_WRITE);
    if (!f) {
        Serial.printf("[CACHE] Failed to save meta: %s\n", metaPath.c_str());
        return false;
    }
    
    _meta.serialize(f);
    f.close();
    
    _metaLoaded = true;
    Serial.printf("[CACHE] Saved meta: %d chapters\n", chapterCount);
    return true;
}

void PageCache::setPageCount(int chapter, int count) {
    if (chapter >= 0 && chapter < 32) {
        _meta.pageCount[chapter] = count;
    }
}

bool PageCache::loadMeta() {
    if (!_initialized) return false;
    
    String metaPath = getMetaPath();
    File f = SD.open(metaPath, FILE_READ);
    if (!f) {
        Serial.println("[CACHE] No meta file found");
        return false;
    }
    
    bool success = _meta.deserialize(f);
    f.close();
    
    return success;
}

// =============================================================================
// Cache Invalidation
// =============================================================================

void PageCache::invalidateBook() {
    if (!_initialized) return;
    
    Serial.printf("[CACHE] Invalidating: %s\n", _cachePath.c_str());
    deleteDirectory(_cachePath);
    
    // Recreate empty directory structure
    ensureDirectory(_cachePath);
    ensureDirectory(_cachePath + "/pages");
    
    _metaLoaded = false;
}

void PageCache::invalidateAll() {
    String basePath = String(PATH_SUMI_CACHE) + "/books";
    Serial.printf("[CACHE] Invalidating all caches: %s\n", basePath.c_str());
    deleteDirectory(basePath);
    
    // Recreate directory
    ensureDirectory(basePath);
    
    _metaLoaded = false;
}

// =============================================================================
// Progress Persistence
// =============================================================================

bool PageCache::saveProgress(int chapter, int page) {
    if (!_initialized) return false;
    
    String progressPath = getProgressPath();
    File f = SD.open(progressPath, FILE_WRITE);
    if (!f) return false;
    
    // Simple format: chapter (2 bytes) + page (2 bytes) + timestamp (4 bytes)
    uint16_t ch = chapter;
    uint16_t pg = page;
    uint32_t ts = millis();
    
    f.write((uint8_t*)&ch, sizeof(ch));
    f.write((uint8_t*)&pg, sizeof(pg));
    f.write((uint8_t*)&ts, sizeof(ts));
    f.close();
    
    return true;
}

bool PageCache::loadProgress(int& outChapter, int& outPage) {
    if (!_initialized) return false;
    
    String progressPath = getProgressPath();
    File f = SD.open(progressPath, FILE_READ);
    if (!f) return false;
    
    uint16_t ch, pg;
    uint32_t ts;
    
    bool ok = true;
    ok = ok && (f.read((uint8_t*)&ch, sizeof(ch)) == sizeof(ch));
    ok = ok && (f.read((uint8_t*)&pg, sizeof(pg)) == sizeof(pg));
    ok = ok && (f.read((uint8_t*)&ts, sizeof(ts)) == sizeof(ts));
    f.close();
    
    if (ok) {
        outChapter = ch;
        outPage = pg;
        Serial.printf("[CACHE] Loaded progress: ch%d pg%d\n", outChapter, outPage);
    }
    
    return ok;
}

// =============================================================================
// Utilities
// =============================================================================

void PageCache::getStats(int& cachedPages, int& totalSize) {
    cachedPages = 0;
    totalSize = 0;
    
    if (!_initialized) return;
    
    String pagesPath = _cachePath + "/pages";
    File dir = SD.open(pagesPath);
    if (!dir) return;
    
    while (File entry = dir.openNextFile()) {
        cachedPages++;
        totalSize += entry.size();
        entry.close();
    }
    dir.close();
}

// =============================================================================
// Private Methods
// =============================================================================

String PageCache::hashPath(const String& path) {
    // Simple hash to 8 hex characters
    uint32_t hash = 5381;
    for (int i = 0; i < path.length(); i++) {
        hash = ((hash << 5) + hash) ^ path[i];
    }
    
    char hex[9];
    snprintf(hex, sizeof(hex), "%08x", hash);
    return String(hex);
}

String PageCache::getPagePath(int chapter, int page) {
    char filename[32];
    snprintf(filename, sizeof(filename), "/pages/%d_%d.bin", chapter, page);
    return _cachePath + filename;
}

String PageCache::getMetaPath() {
    return _cachePath + "/meta.bin";
}

String PageCache::getProgressPath() {
    return _cachePath + "/progress.bin";
}

void PageCache::ensureDirectory(const String& path) {
    if (!SD.exists(path)) {
        SD.mkdir(path);
        Serial.printf("[CACHE] Created dir: %s\n", path.c_str());
    }
}

void PageCache::deleteDirectory(const String& path) {
    if (!SD.exists(path)) return;
    
    // Use iterative approach instead of recursive to avoid stack overflow
    // First, collect all files to delete
    std::vector<String> filesToDelete;
    std::vector<String> dirsToDelete;
    
    // Simple single-level delete (no deep recursion needed for our cache structure)
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        dir.close();
        SD.remove(path);
        return;
    }
    
    // Collect files and subdirs
    while (File entry = dir.openNextFile()) {
        String entryPath = path + "/" + entry.name();
        if (entry.isDirectory()) {
            // For subdirs, collect their files first
            File subdir = SD.open(entryPath);
            if (subdir && subdir.isDirectory()) {
                while (File subentry = subdir.openNextFile()) {
                    String subPath = entryPath + "/" + subentry.name();
                    subentry.close();
                    filesToDelete.push_back(subPath);
                }
                subdir.close();
            }
            dirsToDelete.push_back(entryPath);
        } else {
            filesToDelete.push_back(entryPath);
        }
        entry.close();
    }
    dir.close();
    
    // Delete files first
    for (const String& f : filesToDelete) {
        SD.remove(f);
    }
    
    // Then delete subdirs
    for (const String& d : dirsToDelete) {
        SD.rmdir(d);
    }
    
    // Finally remove the main directory
    SD.rmdir(path);
}
