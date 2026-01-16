/**
 * @file BookEntry.h
 * @brief Book data structures for the Library plugin
 * @version 2.1.16
 */

#ifndef SUMI_LIBRARY_BOOK_ENTRY_H
#define SUMI_LIBRARY_BOOK_ENTRY_H

#include <Arduino.h>
#include "config.h"

namespace Library {

// Memory limits based on device capabilities
#if SUMI_LOW_MEMORY
    constexpr int MAX_BOOKS = 50;
    constexpr int TEXT_BUFFER_SIZE = 4096;
#else
    constexpr int MAX_BOOKS = 200;
    constexpr int TEXT_BUFFER_SIZE = 16384;
#endif

/**
 * @brief Type of book file
 */
enum class BookType {
    UNKNOWN,
    TXT,
    EPUB_FILE,
    EPUB_FOLDER
};

/**
 * @brief Detect book type from file path
 */
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

/**
 * @brief Information about a book in the library
 */
struct BookEntry {
    char filename[64];      // File or folder name
    char title[48];         // Display title
    char author[32];        // Author name (if available)
    char coverPath[96];     // Path to cached cover image
    uint32_t size;          // File size in bytes
    bool isDirectory;       // True if this is a directory
    bool isRegularDir;      // True for non-book directories
    BookType bookType;      // Type of book
    bool hasCover;          // True if cover image is available
    int lastChapter;        // Last read chapter
    int lastPage;           // Last read page in chapter
    float progress;         // Reading progress 0.0 - 1.0
    
    BookEntry() {
        memset(this, 0, sizeof(BookEntry));
    }
    
    bool isBook() const {
        return bookType == BookType::TXT || 
               bookType == BookType::EPUB_FILE || 
               bookType == BookType::EPUB_FOLDER;
    }
    
    bool isEpub() const {
        return bookType == BookType::EPUB_FILE || 
               bookType == BookType::EPUB_FOLDER;
    }
};

} // namespace Library

#endif // SUMI_LIBRARY_BOOK_ENTRY_H
