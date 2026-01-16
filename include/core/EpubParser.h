/**
 * @file EpubParser.h
 * @brief Enhanced EPUB Parser for Sumi E-Reader
 * @version 2.1.16
 *
 * Features:
 * - Reads .epub ZIP files directly (no extraction needed)
 * - Supports extracted EPUB folders (for large files)
 * - NCX table of contents parsing
 * - Comprehensive HTML entity decoding
 * - Better text extraction from XHTML
 */

#ifndef SUMI_EPUB_PARSER_H
#define SUMI_EPUB_PARSER_H

#include <Arduino.h>
#include <vector>
#include <SD.h>
#include "config.h"

// =============================================================================
// EPUB Source Type
// =============================================================================
enum class EpubSourceType {
    ZIP_FILE,       // Standard .epub file (ZIP archive)
    FOLDER          // Extracted EPUB folder
};

// =============================================================================
// Chapter Info
// =============================================================================
struct Chapter {
    String title;           // Chapter title from TOC
    String href;            // Path within EPUB (relative to OPF)
    String anchor;          // Optional anchor within file (after #)
    uint32_t size;          // File size in bytes
    int spineIndex;         // Position in reading order
};

// =============================================================================
// TOC Entry (from NCX)
// =============================================================================
struct TocEntry {
    String title;
    String href;
    String anchor;
    int level;              // Nesting level (0 = top)
    int chapterIndex;       // Corresponding chapter index
};

// =============================================================================
// EPUB Parser Class
// =============================================================================
class EpubParser {
public:
    EpubParser();
    ~EpubParser();
    
    // ==========================================================================
    // Open/Close
    // ==========================================================================
    
    /**
     * Open an EPUB file or folder
     * @param path Path to .epub file or extracted EPUB folder
     * @return true if successful
     */
    bool open(const String& path);
    
    /**
     * Close current EPUB
     */
    void close();
    
    // ==========================================================================
    // Metadata
    // ==========================================================================
    String getTitle() const { return _title; }
    String getAuthor() const { return _author; }
    String getLanguage() const { return _language; }
    String getPublisher() const { return _publisher; }
    
    // ==========================================================================
    // Chapters (Spine Items)
    // ==========================================================================
    int getChapterCount() const { return _chapters.size(); }
    const Chapter& getChapter(int index) const;
    
    /**
     * Get chapter text content
     * @param chapterIndex Index of chapter
     * @return Plain text content
     */
    String getChapterText(int chapterIndex);
    
    /**
     * Get raw HTML content of chapter
     * @param chapterIndex Index of chapter  
     * @return Raw XHTML content
     */
    String getChapterHTML(int chapterIndex);
    
    /**
     * Stream chapter HTML content to a file (memory-efficient)
     * @param chapterIndex Index of chapter
     * @param outputPath Path to write temp file
     * @return true if successful
     */
    bool streamChapterToFile(int chapterIndex, const String& outputPath);
    
    /**
     * Get cover image path (relative to EPUB root)
     * @return Path to cover image, empty if none
     */
    String getCoverImagePath() const { return _coverImagePath; }
    
    /**
     * Check if EPUB has a cover image
     */
    bool hasCover() const { return _coverImagePath.length() > 0; }
    
    /**
     * Extract cover image to file
     * @param outputPath Where to save the cover (e.g., "/.sumi/cover.jpg")
     * @return true if successful
     */
    bool extractCoverImage(const String& outputPath);
    
    /**
     * Get all text (concatenated chapters)
     * @return Entire book as plain text
     */
    String getAllText();
    
    // ==========================================================================
    // Table of Contents
    // ==========================================================================
    int getTocCount() const { return _toc.size(); }
    const TocEntry& getTocEntry(int index) const;
    
    /**
     * Find chapter index for a TOC entry
     */
    int getChapterForToc(int tocIndex) const;
    
    // ==========================================================================
    // Status
    // ==========================================================================
    bool isOpen() const { return _isOpen; }
    bool isFolder() const { return _sourceType == EpubSourceType::FOLDER; }
    const String& getError() const { return _error; }
    const String& getPath() const { return _path; }

private:
    // State
    String _path;
    String _error;
    bool _isOpen;
    EpubSourceType _sourceType;
    
    // Metadata
    String _title;
    String _author;
    String _language;
    String _publisher;
    
    // Content
    std::vector<Chapter> _chapters;
    std::vector<TocEntry> _toc;
    String _contentBasePath;    // Base path for content files relative to EPUB root
    String _ncxPath;            // Path to NCX file (table of contents)
    String _coverImagePath;     // Path to cover image
    
    // Static empty chapter for bounds checking
    static Chapter _emptyChapter;
    static TocEntry _emptyTocEntry;
    
    // ==========================================================================
    // Parsing Methods
    // ==========================================================================
    bool parseContainer();
    bool parseOPF(const String& opfPath);
    bool parseNCX(const String& ncxPath);
    
    // ==========================================================================
    // File Reading
    // ==========================================================================
    
    /**
     * Read file from ZIP or folder
     */
    String readFile(const String& relativePath);
    
    /**
     * Read file from ZIP archive
     */
    String readFileFromZip(const String& innerPath);
    
    /**
     * Read file from folder
     */
    String readFileFromFolder(const String& relativePath);
    
    /**
     * Check if path exists (works for both ZIP and folder)
     */
    bool fileExists(const String& relativePath);
    
    // ==========================================================================
    // HTML Processing
    // ==========================================================================
    
    /**
     * Extract plain text from HTML/XHTML
     */
    String extractTextFromHTML(const String& html);
    
    /**
     * Extract body content from full HTML document
     */
    String extractBody(const String& html);
    
    /**
     * Strip all HTML tags
     */
    String stripTags(const String& html);
    
    /**
     * Decode HTML entities
     */
    String decodeEntities(const String& text);
    
    /**
     * Normalize whitespace
     */
    String normalizeWhitespace(const String& text);
    
    // ==========================================================================
    // Path Helpers
    // ==========================================================================
    
    /**
     * Resolve relative path against base
     */
    String resolvePath(const String& base, const String& relative);
    
    /**
     * Normalize path (resolve . and ..)
     */
    String normalizePath(const String& path);
    
    /**
     * URL decode path
     */
    String urlDecode(const String& path);
    
    /**
     * Extract attribute value from XML tag string
     */
    String extractAttribute(const String& tag, const char* attrName);
};

// =============================================================================
// Folder EPUB Detection
// =============================================================================

/**
 * Check if path is a valid extracted EPUB folder
 * (contains META-INF/container.xml)
 */
bool isValidEpubFolder(const String& path);

/**
 * Check if path is an EPUB file
 */
bool isEpubFile(const String& path);

#endif // SUMI_EPUB_PARSER_H
