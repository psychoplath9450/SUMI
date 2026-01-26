/**
 * @file EpubParser.h
 * @brief Streaming EPUB Parser - Memory-Efficient Implementation
 * @version 1.4.2
 *
 * COMPLETELY REWRITTEN for 380KB RAM constraint:
 * - NEVER loads entire files into memory
 * - Uses Expat streaming XML parser with 1KB chunks
 * - Two-tier caching via BookMetadataCache
 * - All chapter access via streaming
 *
 * 
 */

#ifndef SUMI_EPUB_PARSER_H
#define SUMI_EPUB_PARSER_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"
#include "core/BookMetadataCache.h"
#include "core/ZipReader.h"

// Forward declare Expat types
extern "C" {
    #include "expat.h"
}

// =============================================================================
// Constants
// =============================================================================
#define EPUB_CHUNK_SIZE     1024
#define TEMP_HTML_PATH      "/.sumi/temp_chapter.html"

// =============================================================================
// Parser State
// =============================================================================
enum class EpubParserState {
    IDLE,
    PARSING_CONTAINER,
    PARSING_OPF_METADATA,
    PARSING_OPF_MANIFEST,
    PARSING_OPF_SPINE,
    PARSING_NCX,
    PARSING_NAV
};

// =============================================================================
// Source Type
// =============================================================================
enum class EpubSourceType {
    ZIP_FILE,
    FOLDER
};

// =============================================================================
// Chapter Info (compatibility with existing code)
// =============================================================================
struct Chapter {
    String title;
    String href;
    String anchor;
    uint32_t size;
    int spineIndex;
    
    Chapter() : size(0), spineIndex(0) {}
};

// =============================================================================
// Manifest Item (temp during OPF parsing)
// =============================================================================
struct ManifestItem {
    char id[64];
    char href[MAX_HREF_LEN];
    char mediaType[48];
    char properties[32];
    
    void clear() { memset(this, 0, sizeof(ManifestItem)); }
};

// =============================================================================
// EpubParser Class
// =============================================================================
class EpubParser {
public:
    EpubParser();
    ~EpubParser();
    
    // ==========================================================================
    // Open/Close
    // ==========================================================================
    bool open(const String& path);
    void close();
    
    // ==========================================================================
    // Metadata
    // ==========================================================================
    String getTitle() const { return String(_metadata.title); }
    String getAuthor() const { return String(_metadata.author); }
    String getLanguage() const { return String(_metadata.language); }
    String getPublisher() const { return ""; }
    String getCoverImagePath() const { return String(_metadata.coverHref); }
    bool hasCover() const { return strlen(_metadata.coverHref) > 0; }
    
    // ==========================================================================
    // Chapters (Spine Items)
    // ==========================================================================
    int getChapterCount() const { return _metadata.spineCount; }
    const Chapter& getChapter(int index) const;
    
    /**
     * Stream chapter HTML to temp file
     * THIS IS THE PRIMARY WAY TO ACCESS CHAPTER CONTENT
     */
    bool streamChapterToFile(int chapterIndex, const String& outputPath);
    
    /**
     * Extract cover image to file
     */
    bool extractCoverImage(const String& outputPath);
    
    /**
     * Extract any image from EPUB
     */
    bool extractImage(const String& imagePath, const String& outputPath);
    
    // ==========================================================================
    // DEPRECATED: These load entire files to RAM - use streaming instead
    // ==========================================================================
    
    /**
     * @deprecated Use streamChapterToFile() + StreamingHtmlProcessor instead
     * Still works for small chapters but will log warnings
     */
    String getChapterText(int chapterIndex);
    
    /**
     * @deprecated Use streamChapterToFile() instead
     * Still works for small chapters but will log warnings
     */
    String getChapterHTML(int chapterIndex);
    
    /**
     * @deprecated Legacy method - use streaming API instead
     * Will return error message instead of book content
     */
    String getAllText();
    
    // ==========================================================================
    // Table of Contents
    // ==========================================================================
    int getTocCount() const { return _metadata.tocCount; }
    const TocEntry& getTocEntry(int index) const;
    int getChapterForToc(int tocIndex) const;
    
    // ==========================================================================
    // Status
    // ==========================================================================
    bool isOpen() const { return _isOpen; }
    bool isFolder() const { return _sourceType == EpubSourceType::FOLDER; }
    const String& getError() const { return _error; }
    const String& getPath() const { return _path; }
    
    // ==========================================================================
    // Cache Access (added for streaming)
    // ==========================================================================
    const BookMetadataCache& getMetadata() const { return _metadata; }
    BookMetadataCache& getMetadata() { return _metadata; }
    const String& getCachePath() const { return _cachePath; }
    
private:
    bool _isOpen;
    String _path;
    String _cachePath;
    String _error;
    String _contentBasePath;
    String _opfPath;
    String _ncxPath;
    String _navPath;
    EpubSourceType _sourceType;
    
    // Two-tier metadata cache
    BookMetadataCache _metadata;
    
    // ZIP reader
    ZipReader _zip;
    
    // Expat streaming parser
    XML_Parser _xmlParser;
    EpubParserState _parserState;
    
    // Parsing buffers (reused, never grows)
    char _tempBuffer[EPUB_CHUNK_SIZE];
    char _currentElement[32];
    char _currentId[64];
    char _currentHref[MAX_HREF_LEN];
    char _currentMediaType[48];
    char _currentTitle[64];
    int _currentDepth;
    bool _inMetadata;
    bool _inManifest;
    bool _inSpine;
    
    // Manifest temp file (written to SD, not RAM)
    File _manifestFile;
    int _manifestCount;
    
    // Compatibility: cached chapter for getChapter()
    mutable Chapter _tempChapter;
    
    // Static empty objects
    static TocEntry _emptyTocEntry;
    
    // ==========================================================================
    // Streaming Parse Methods
    // ==========================================================================
    bool parseContainer();
    bool parseOPF();
    bool parseNCX();
    bool parseNAV();
    bool streamParseFile(const String& innerPath, EpubParserState initialState);
    bool findManifestItem(const char* id, ManifestItem& out);
    
    // ==========================================================================
    // Expat Callbacks
    // ==========================================================================
    static void XMLCALL startElementHandler(void* userData, const char* name, const char** atts);
    static void XMLCALL endElementHandler(void* userData, const char* name);
    static void XMLCALL characterDataHandler(void* userData, const char* s, int len);
    
    void handleStartElement(const char* name, const char** atts);
    void handleEndElement(const char* name);
    void handleCharacterData(const char* s, int len);
    void handleOPFElement(const char* name, const char** atts);
    void handleOPFEndElement(const char* name);
    void handleNCXElement(const char* name, const char** atts);
    void handleNCXEndElement(const char* name);
    void handleNAVElement(const char* name, const char** atts);
    void handleNAVEndElement(const char* name);
    
    static const char* getAttr(const char** atts, const char* name);
    
    // ==========================================================================
    // Path Utilities
    // ==========================================================================
    String resolvePath(const String& base, const String& relative);
    static String normalizePath(const String& path);
    static String urlDecode(const String& path);
    
    // ==========================================================================
    // Folder Mode Support
    // ==========================================================================
    bool openFolder(const String& path);
    String readFileFromFolder(const String& relativePath);
    bool streamFolderFileToFile(const String& innerPath, const String& outputPath);
};

// =============================================================================
// Global Instance
// =============================================================================
extern EpubParser* epubParser;

// =============================================================================
// Utility Functions
// =============================================================================
bool isValidEpubFolder(const String& path);
bool isEpubFile(const String& path);

#endif // SUMI_EPUB_PARSER_H
