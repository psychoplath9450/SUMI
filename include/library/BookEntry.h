/**
 * @file BookEntry.h
 * @brief Book data structures for the Library plugin
 * @version 1.3.0
 */

#ifndef SUMI_LIBRARY_BOOK_ENTRY_H
#define SUMI_LIBRARY_BOOK_ENTRY_H

#include <Arduino.h>
#include "config.h"

namespace Library {

// Memory limits
constexpr int MAX_BOOKS = 200;
constexpr int TEXT_BUFFER_SIZE = 16384;

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
    bool hasCache;          // True if pre-processed cache exists
    int lastChapter;        // Last read chapter
    int lastPage;           // Last read page in chapter
    int totalChapters;      // Total chapters in book (from meta.json)
    float progress;         // Reading progress 0.0 - 1.0
    
    BookEntry() {
        memset(this, 0, sizeof(BookEntry));
    }
    
    void clear();
    void serialize(File& f) const;
    bool deserialize(File& f);
    
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
