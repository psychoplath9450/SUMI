/**
 * @file CoverCache.h
 * @brief Cover image caching for the Library plugin
 * @version 2.1.16
 */

#ifndef SUMI_LIBRARY_COVER_CACHE_H
#define SUMI_LIBRARY_COVER_CACHE_H

#include <Arduino.h>
#include <SD.h>
#include "library/BookEntry.h"

namespace Library {

/**
 * @brief Path constants for cover caching
 */
constexpr const char* COVER_CACHE_DIR = "/.sumi/covers";
constexpr const char* LAST_BOOK_PATH = "/.sumi/lastbook.bin";

/**
 * @brief Manages cover image caching
 * 
 * Cover images are extracted from EPUBs and cached as raw grayscale
 * images for fast display. The cache path is generated from a hash
 * of the book path to ensure uniqueness.
 */
class CoverCache {
public:
    CoverCache() = default;
    
    /**
     * @brief Initialize the cover cache directory
     * @return true if successful
     */
    bool init() {
        if (!SD.exists(COVER_CACHE_DIR)) {
            return SD.mkdir(COVER_CACHE_DIR);
        }
        return true;
    }
    
    /**
     * @brief Generate cache path for a book's cover
     * @param bookPath Full path to the book file
     * @param outPath Output buffer for cache path
     * @param outSize Size of output buffer
     */
    void getCachePath(const char* bookPath, char* outPath, size_t outSize) {
        uint32_t hash = hashPath(bookPath);
        snprintf(outPath, outSize, "%s/%08x.raw", COVER_CACHE_DIR, hash);
    }
    
    /**
     * @brief Check if a cover is cached
     * @param bookPath Full path to the book file
     * @return true if cover exists in cache
     */
    bool hasCachedCover(const char* bookPath) {
        char cachePath[96];
        getCachePath(bookPath, cachePath, sizeof(cachePath));
        return SD.exists(cachePath);
    }
    
    /**
     * @brief Get the cache path for a book
     * @param bookPath Full path to the book file
     * @return String containing cache path
     */
    String getCachePathString(const char* bookPath) {
        char cachePath[96];
        getCachePath(bookPath, cachePath, sizeof(cachePath));
        return String(cachePath);
    }
    
    /**
     * @brief Clear all cached covers
     * @return Number of covers deleted
     */
    int clearCache() {
        int deleted = 0;
        File dir = SD.open(COVER_CACHE_DIR);
        if (!dir) return 0;
        
        while (File entry = dir.openNextFile()) {
            String path = String(COVER_CACHE_DIR) + "/" + entry.name();
            entry.close();
            if (SD.remove(path)) {
                deleted++;
            }
        }
        dir.close();
        
        Serial.printf("[COVER] Cleared %d cached covers\n", deleted);
        return deleted;
    }
    
    /**
     * @brief Check for sufficient memory to extract a cover
     * @return true if memory is sufficient
     */
    bool hasMemoryForExtraction() {
        int freeHeap = ESP.getFreeHeap();
        size_t maxAlloc = ESP.getMaxAllocHeap();
        // Need at least 100KB free with 50KB contiguous for image processing
        return freeHeap >= 100000 && maxAlloc >= 50000;
    }
    
private:
    /**
     * @brief Generate hash from path string
     */
    uint32_t hashPath(const char* path) {
        uint32_t hash = 0;
        for (const char* p = path; *p; p++) {
            hash = hash * 31 + *p;
        }
        return hash;
    }
};

} // namespace Library

#endif // SUMI_LIBRARY_COVER_CACHE_H
