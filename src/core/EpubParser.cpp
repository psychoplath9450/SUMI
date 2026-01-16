/**
 * @file EpubParser.cpp
 * @brief EPUB Parser Implementation with ZIP Support
 * @version 2.1.16
 *
 * Supports:
 * - Direct .epub file reading (ZIP archives)
 * - Extracted EPUB folders
 * - Streaming decompression for large chapters
 */

#include "core/EpubParser.h"
#include "core/StreamingHtmlParser.h"
#include "core/ZipReader.h"
#include <SD.h>


// Static members
Chapter EpubParser::_emptyChapter;
TocEntry EpubParser::_emptyTocEntry;

// Global ZipReader instance for current EPUB
static ZipReader epubZip;

// =============================================================================
// Constructor / Destructor
// =============================================================================

EpubParser::EpubParser() 
    : _isOpen(false), _sourceType(EpubSourceType::FOLDER) {
}

EpubParser::~EpubParser() {
    close();
}

// =============================================================================
// Open / Close
// =============================================================================

bool EpubParser::open(const String& path) {
    close();
    
    _path = path;
    _error = "";
    
    // Determine source type
    if (path.endsWith(".epub") || path.endsWith(".EPUB")) {
        _sourceType = EpubSourceType::ZIP_FILE;
        Serial.printf("[EPUB] Opening ZIP: %s\n", path.c_str());
        
        // Open as ZIP file
        if (!epubZip.open(path)) {
            _error = "Failed to open EPUB ZIP: " + epubZip.getError();
            Serial.printf("[EPUB] %s\n", _error.c_str());
            return false;
        }
    } else {
        _sourceType = EpubSourceType::FOLDER;
        Serial.printf("[EPUB] Opening folder: %s\n", path.c_str());
        
        // Check if folder exists
        if (!SD.exists(path)) {
            _error = "Path does not exist";
            return false;
        }
    }
    
    // Parse container.xml to find OPF
    if (!parseContainer()) {
        if (_sourceType == EpubSourceType::ZIP_FILE) {
            epubZip.close();
        }
        return false;
    }
    
    _isOpen = true;
    Serial.printf("[EPUB] Opened: \"%s\" by %s, %d chapters\n", 
                  _title.c_str(), _author.c_str(), (int)_chapters.size());
    
    return true;
}

void EpubParser::close() {
    if (_sourceType == EpubSourceType::ZIP_FILE) {
        epubZip.close();
    }
    _isOpen = false;
    _path = "";
    _error = "";
    _title = "";
    _author = "";
    _language = "";
    _publisher = "";
    _chapters.clear();
    _toc.clear();
    _contentBasePath = "";
    _ncxPath = "";
    _coverImagePath = "";
}

// =============================================================================
// Chapter Access
// =============================================================================

const Chapter& EpubParser::getChapter(int index) const {
    if (index >= 0 && index < (int)_chapters.size()) {
        return _chapters[index];
    }
    return _emptyChapter;
}

String EpubParser::getChapterText(int chapterIndex) {
    String html = getChapterHTML(chapterIndex);
    if (html.length() == 0) {
        return "";
    }
    return extractTextFromHTML(html);
}

String EpubParser::getChapterHTML(int chapterIndex) {
    if (chapterIndex < 0 || chapterIndex >= (int)_chapters.size()) {
        return "";
    }
    
    String href = _chapters[chapterIndex].href;
    String fullPath = resolvePath(_contentBasePath, href);
    
    return readFile(fullPath);
}

bool EpubParser::streamChapterToFile(int chapterIndex, const String& outputPath) {
    if (chapterIndex < 0 || chapterIndex >= (int)_chapters.size()) {
        _error = "Invalid chapter index";
        return false;
    }
    
    String href = _chapters[chapterIndex].href;
    String fullPath = resolvePath(_contentBasePath, href);
    
    Serial.printf("[EPUB] Streaming chapter %d to %s\n", chapterIndex, outputPath.c_str());
    Serial.printf("[EPUB] Source path: %s\n", fullPath.c_str());
    
    // Open output file
    File outFile = SD.open(outputPath, FILE_WRITE);
    if (!outFile) {
        _error = "Failed to open output file: " + outputPath;
        Serial.printf("[EPUB] %s\n", _error.c_str());
        return false;
    }
    
    bool success = false;
    
    if (_sourceType == EpubSourceType::ZIP_FILE) {
        // Stream from ZIP to file using ZipReader's streamFileTo
        success = epubZip.streamFileTo(fullPath, outFile, 1024);
        if (!success) {
            _error = "Failed to stream from ZIP: " + epubZip.getError();
        }
    } else {
        // Copy from folder
        String srcPath = _path + "/" + fullPath;
        File srcFile = SD.open(srcPath, FILE_READ);
        if (srcFile) {
            uint8_t buffer[1024];
            while (srcFile.available()) {
                size_t read = srcFile.read(buffer, sizeof(buffer));
                outFile.write(buffer, read);
            }
            srcFile.close();
            success = true;
        } else {
            _error = "Failed to open source file: " + srcPath;
        }
    }
    
    outFile.close();
    
    if (success) {
        Serial.printf("[EPUB] Streamed chapter %d successfully\n", chapterIndex);
    } else {
        Serial.printf("[EPUB] Stream failed: %s\n", _error.c_str());
        SD.remove(outputPath);  // Clean up failed file
    }
    
    return success;
}

String EpubParser::getAllText() {
    String result;
    for (int i = 0; i < (int)_chapters.size(); i++) {
        result += getChapterText(i);
        result += "\n\n";
    }
    return result;
}

// =============================================================================
// TOC Access
// =============================================================================

const TocEntry& EpubParser::getTocEntry(int index) const {
    if (index >= 0 && index < (int)_toc.size()) {
        return _toc[index];
    }
    return _emptyTocEntry;
}

int EpubParser::getChapterForToc(int tocIndex) const {
    if (tocIndex >= 0 && tocIndex < (int)_toc.size()) {
        return _toc[tocIndex].chapterIndex;
    }
    return 0;
}

// =============================================================================
// Container Parsing
// =============================================================================

bool EpubParser::parseContainer() {
    // Read META-INF/container.xml
    String containerPath = "META-INF/container.xml";
    String container = readFile(containerPath);
    
    if (container.length() == 0) {
        _error = "Cannot read container.xml";
        return false;
    }
    
    // Find rootfile element
    int rfStart = container.indexOf("<rootfile");
    if (rfStart < 0) {
        _error = "No rootfile in container.xml";
        return false;
    }
    
    // Extract full-path attribute
    String opfPath = extractAttribute(container.substring(rfStart), "full-path");
    if (opfPath.length() == 0) {
        _error = "No full-path in rootfile";
        return false;
    }
    
    Serial.printf("[EPUB] OPF path: %s\n", opfPath.c_str());
    
    // Parse OPF
    return parseOPF(opfPath);
}

// =============================================================================
// OPF Parsing
// =============================================================================

bool EpubParser::parseOPF(const String& opfPath) {
    String opf = readFile(opfPath);
    if (opf.length() == 0) {
        _error = "Cannot read OPF file";
        return false;
    }
    
    // Set content base path (directory containing OPF)
    int lastSlash = opfPath.lastIndexOf('/');
    if (lastSlash > 0) {
        _contentBasePath = opfPath.substring(0, lastSlash + 1);
    } else {
        _contentBasePath = "";
    }
    
    // Extract metadata
    int metaStart = opf.indexOf("<metadata");
    int metaEnd = opf.indexOf("</metadata>");
    String coverId = "";  // Will store cover item ID from metadata
    
    if (metaStart >= 0 && metaEnd > metaStart) {
        String meta = opf.substring(metaStart, metaEnd);
        
        // Title
        int titleStart = meta.indexOf("<dc:title");
        if (titleStart >= 0) {
            int tagEnd = meta.indexOf('>', titleStart);
            int closeTag = meta.indexOf("</dc:title>", tagEnd);
            if (tagEnd >= 0 && closeTag > tagEnd) {
                _title = meta.substring(tagEnd + 1, closeTag);
                _title.trim();
            }
        }
        
        // Author
        int authorStart = meta.indexOf("<dc:creator");
        if (authorStart >= 0) {
            int tagEnd = meta.indexOf('>', authorStart);
            int closeTag = meta.indexOf("</dc:creator>", tagEnd);
            if (tagEnd >= 0 && closeTag > tagEnd) {
                _author = meta.substring(tagEnd + 1, closeTag);
                _author.trim();
            }
        }
        
        // Language
        int langStart = meta.indexOf("<dc:language");
        if (langStart >= 0) {
            int tagEnd = meta.indexOf('>', langStart);
            int closeTag = meta.indexOf("</dc:language>", tagEnd);
            if (tagEnd >= 0 && closeTag > tagEnd) {
                _language = meta.substring(tagEnd + 1, closeTag);
                _language.trim();
            }
        }
        
        // =====================================================
        // COVER IMAGE: Look for <meta name="cover" content="..." />
        // =====================================================
        int metaPos = 0;
        while ((metaPos = meta.indexOf("<meta", metaPos)) >= 0) {
            int metaTagEnd = meta.indexOf(">", metaPos);
            if (metaTagEnd < 0) break;
            
            String metaTag = meta.substring(metaPos, metaTagEnd + 1);
            String name = extractAttribute(metaTag, "name");
            
            if (name == "cover") {
                coverId = extractAttribute(metaTag, "content");
                Serial.printf("[EPUB] Found cover meta, id: %s\n", coverId.c_str());
            }
            
            metaPos = metaTagEnd;
        }
    }
    
    // Build manifest map (id -> href)
    std::vector<std::pair<String, String>> manifest;
    std::vector<std::pair<String, String>> manifestMediaType;
    
    int manifestStart = opf.indexOf("<manifest");
    int manifestEnd = opf.indexOf("</manifest>");
    if (manifestStart >= 0 && manifestEnd > manifestStart) {
        String manifestSection = opf.substring(manifestStart, manifestEnd);
        
        int pos = 0;
        while ((pos = manifestSection.indexOf("<item", pos)) >= 0) {
            int itemEnd = manifestSection.indexOf(">", pos);
            if (itemEnd < 0) break;
            
            String item = manifestSection.substring(pos, itemEnd + 1);
            String id = extractAttribute(item, "id");
            String href = extractAttribute(item, "href");
            String mediaType = extractAttribute(item, "media-type");
            
            if (id.length() > 0 && href.length() > 0) {
                manifest.push_back(std::make_pair(id, urlDecode(href)));
                manifestMediaType.push_back(std::make_pair(id, mediaType));
                
                // Check for NCX
                if (mediaType == "application/x-dtbncx+xml") {
                    _ncxPath = resolvePath(_contentBasePath, href);
                }
                
                // Check for cover image by id or properties
                if (coverId.length() > 0 && id == coverId) {
                    _coverImagePath = resolvePath(_contentBasePath, href);
                    Serial.printf("[EPUB] Cover image from meta: %s\n", _coverImagePath.c_str());
                }
                // Also check for cover-image property (EPUB3)
                String properties = extractAttribute(item, "properties");
                if (properties.indexOf("cover-image") >= 0) {
                    _coverImagePath = resolvePath(_contentBasePath, href);
                    Serial.printf("[EPUB] Cover image from properties: %s\n", _coverImagePath.c_str());
                }
            }
            
            pos = itemEnd;
        }
    }
    
    // If no cover found by metadata, try common cover filenames
    if (_coverImagePath.length() == 0) {
        String commonCoverNames[] = {"cover.jpg", "cover.jpeg", "cover.png", "Cover.jpg", "Cover.jpeg", "images/cover.jpg"};
        for (const String& name : commonCoverNames) {
            String testPath = resolvePath(_contentBasePath, name);
            if (fileExists(testPath)) {
                _coverImagePath = testPath;
                Serial.printf("[EPUB] Cover image by common name: %s\n", _coverImagePath.c_str());
                break;
            }
        }
    }
    
    // Parse spine to get reading order
    int spineStart = opf.indexOf("<spine");
    int spineEnd = opf.indexOf("</spine>");
    if (spineStart >= 0 && spineEnd > spineStart) {
        String spine = opf.substring(spineStart, spineEnd);
        
        int spineIndex = 0;
        int pos = 0;
        while ((pos = spine.indexOf("<itemref", pos)) >= 0) {
            int itemEnd = spine.indexOf(">", pos);
            if (itemEnd < 0) break;
            
            String itemref = spine.substring(pos, itemEnd + 1);
            String idref = extractAttribute(itemref, "idref");
            
            // Look up in manifest
            for (const auto& item : manifest) {
                if (item.first == idref) {
                    Chapter chapter;
                    chapter.href = item.second;
                    chapter.spineIndex = spineIndex;
                    chapter.title = "Chapter " + String(spineIndex + 1);
                    
                    // Check for anchor
                    int hashPos = chapter.href.indexOf('#');
                    if (hashPos > 0) {
                        chapter.anchor = chapter.href.substring(hashPos + 1);
                        chapter.href = chapter.href.substring(0, hashPos);
                    }
                    
                    _chapters.push_back(chapter);
                    spineIndex++;
                    break;
                }
            }
            
            pos = itemEnd;
        }
    }
    
    // Parse NCX for proper chapter titles
    if (_ncxPath.length() > 0) {
        parseNCX(_ncxPath);
    }
    
    return _chapters.size() > 0;
}

// =============================================================================
// NCX Parsing (Table of Contents)
// =============================================================================

bool EpubParser::parseNCX(const String& ncxPath) {
    String ncx = readFile(ncxPath);
    if (ncx.length() == 0) {
        return false;
    }
    
    // Find navMap
    int navMapStart = ncx.indexOf("<navMap");
    int navMapEnd = ncx.lastIndexOf("</navMap>");
    if (navMapStart < 0 || navMapEnd < 0) {
        return false;
    }
    
    String navMap = ncx.substring(navMapStart, navMapEnd);
    
    // Parse navPoints
    int pos = 0;
    while ((pos = navMap.indexOf("<navPoint", pos)) >= 0) {
        // Find the end of this navPoint
        int contentStart = navMap.indexOf("<content", pos);
        int textStart = navMap.indexOf("<text>", pos);
        int textEnd = navMap.indexOf("</text>", textStart);
        
        if (contentStart < 0) break;
        
        // Extract title
        String title = "";
        if (textStart >= 0 && textEnd > textStart) {
            title = navMap.substring(textStart + 6, textEnd);
            title.trim();
            title = decodeEntities(title);
        }
        
        // Extract src from content
        int contentEnd = navMap.indexOf("/>", contentStart);
        if (contentEnd < 0) contentEnd = navMap.indexOf(">", contentStart);
        
        String contentTag = navMap.substring(contentStart, contentEnd + 1);
        String src = extractAttribute(contentTag, "src");
        src = urlDecode(src);
        
        // Create TOC entry
        TocEntry entry;
        entry.title = title;
        entry.level = 0;
        
        // Split href and anchor
        int hashPos = src.indexOf('#');
        if (hashPos > 0) {
            entry.href = src.substring(0, hashPos);
            entry.anchor = src.substring(hashPos + 1);
        } else {
            entry.href = src;
        }
        
        // Find matching chapter
        entry.chapterIndex = -1;
        for (int i = 0; i < (int)_chapters.size(); i++) {
            if (_chapters[i].href == entry.href || 
                _chapters[i].href.endsWith("/" + entry.href)) {
                entry.chapterIndex = i;
                
                // Update chapter title from TOC
                if (title.length() > 0) {
                    _chapters[i].title = title;
                }
                break;
            }
        }
        
        if (entry.chapterIndex >= 0) {
            _toc.push_back(entry);
        }
        
        pos = contentEnd;
    }
    
    Serial.printf("[EPUB] Parsed NCX: %d TOC entries\n", (int)_toc.size());
    return true;
}

// =============================================================================
// File Reading (handles both ZIP and folder)
// =============================================================================

String EpubParser::readFile(const String& relativePath) {
    if (_sourceType == EpubSourceType::ZIP_FILE) {
        return readFileFromZip(relativePath);
    } else {
        return readFileFromFolder(relativePath);
    }
}

String EpubParser::readFileFromZip(const String& innerPath) {
    if (!epubZip.isOpen()) {
        _error = "ZIP not open";
        return "";
    }
    
    String content = epubZip.readFile(innerPath.c_str());
    if (content.length() == 0) {
        // Try without leading slash
        if (innerPath.startsWith("/")) {
            content = epubZip.readFile(innerPath.substring(1).c_str());
        }
    }
    
    if (content.length() == 0) {
        Serial.printf("[EPUB] Failed to read from ZIP: %s\n", innerPath.c_str());
    }
    
    return content;
}

String EpubParser::readFileFromFolder(const String& relativePath) {
    String fullPath = _path + "/" + relativePath;
    
    File file = SD.open(fullPath, FILE_READ);
    if (!file) {
        Serial.printf("[EPUB] Cannot open: %s\n", fullPath.c_str());
        return "";
    }
    
    size_t size = file.size();
    if (size > 512 * 1024) {  // 512KB limit
        Serial.printf("[EPUB] File too large: %s (%d bytes)\n", fullPath.c_str(), (int)size);
        file.close();
        return "";
    }
    
    String content;
    content.reserve(size);
    
    while (file.available()) {
        content += (char)file.read();
    }
    
    file.close();
    return content;
}

bool EpubParser::fileExists(const String& relativePath) {
    if (_sourceType == EpubSourceType::ZIP_FILE) {
        return epubZip.hasFile(relativePath);
    } else {
        String fullPath = _path + "/" + relativePath;
        return SD.exists(fullPath);
    }
}

// =============================================================================
// HTML Processing
// =============================================================================

String EpubParser::extractTextFromHTML(const String& html) {
    String body = extractBody(html);
    String stripped = stripTags(body);
    String decoded = decodeEntities(stripped);
    return normalizeWhitespace(decoded);
}

String EpubParser::extractBody(const String& html) {
    int bodyStart = html.indexOf("<body");
    if (bodyStart < 0) {
        return html;
    }
    
    int bodyTagEnd = html.indexOf('>', bodyStart);
    int bodyEnd = html.lastIndexOf("</body>");
    
    if (bodyTagEnd >= 0 && bodyEnd > bodyTagEnd) {
        return html.substring(bodyTagEnd + 1, bodyEnd);
    }
    
    return html.substring(bodyTagEnd + 1);
}

String EpubParser::stripTags(const String& html) {
    String result;
    result.reserve(html.length());
    
    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    
    for (int i = 0; i < (int)html.length(); i++) {
        char c = html[i];
        
        if (c == '<') {
            inTag = true;
            
            String upcoming = html.substring(i, min((int)html.length(), i + 10));
            upcoming.toLowerCase();
            if (upcoming.startsWith("<script")) inScript = true;
            if (upcoming.startsWith("<style")) inStyle = true;
            if (upcoming.startsWith("</script")) inScript = false;
            if (upcoming.startsWith("</style")) inStyle = false;
            
            if (upcoming.startsWith("<p") || upcoming.startsWith("<div") ||
                upcoming.startsWith("<br") || upcoming.startsWith("<h") ||
                upcoming.startsWith("</p") || upcoming.startsWith("</div") ||
                upcoming.startsWith("</h")) {
                result += ' ';
            }
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag && !inScript && !inStyle) {
            result += c;
        }
    }
    
    return result;
}

String EpubParser::decodeEntities(const String& text) {
    return decodeHtmlEntities(text);
}

String EpubParser::normalizeWhitespace(const String& text) {
    String result;
    result.reserve(text.length());
    
    bool lastWasSpace = true;
    
    for (int i = 0; i < (int)text.length(); i++) {
        char c = text[i];
        bool isSpace = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        
        if (isSpace) {
            if (!lastWasSpace) {
                result += ' ';
            }
            lastWasSpace = true;
        } else {
            result += c;
            lastWasSpace = false;
        }
    }
    
    result.trim();
    return result;
}

// =============================================================================
// Path Helpers
// =============================================================================

String EpubParser::resolvePath(const String& base, const String& relative) {
    if (relative.startsWith("/")) {
        return normalizePath(relative.substring(1));
    }
    
    if (base.length() == 0) {
        return normalizePath(relative);
    }
    
    return normalizePath(base + relative);
}

String EpubParser::normalizePath(const String& path) {
    String result = path;
    
    // CRITICAL: Convert all backslashes to forward slashes FIRST
    // Windows-created EPUBs use backslashes internally
    result.replace("\\", "/");
    
    // Remove ./ sequences
    result.replace("/./", "/");
    if (result.startsWith("./")) {
        result = result.substring(2);
    }
    
    // Resolve ../ sequences
    int pos;
    while ((pos = result.indexOf("/../")) >= 0) {
        int prevSlash = result.lastIndexOf('/', pos - 1);
        if (prevSlash >= 0) {
            result = result.substring(0, prevSlash) + result.substring(pos + 3);
        } else {
            result = result.substring(pos + 4);
        }
    }
    
    // Remove double slashes
    while (result.indexOf("//") >= 0) {
        result.replace("//", "/");
    }
    
    // Remove leading slash if present
    if (result.startsWith("/")) {
        result = result.substring(1);
    }
    
    return result;
}

String EpubParser::urlDecode(const String& path) {
    String result;
    result.reserve(path.length());
    
    for (int i = 0; i < (int)path.length(); i++) {
        char c = path[i];
        if (c == '%' && i + 2 < (int)path.length()) {
            char hex[3] = {path[i+1], path[i+2], 0};
            int value = strtol(hex, nullptr, 16);
            result += (char)value;
            i += 2;
        } else if (c == '+') {
            result += ' ';
        } else {
            result += c;
        }
    }
    
    return result;
}

String EpubParser::extractAttribute(const String& tag, const char* attrName) {
    String search = String(attrName) + "=\"";
    int attrStart = tag.indexOf(search);
    if (attrStart < 0) {
        search = String(attrName) + "='";
        attrStart = tag.indexOf(search);
    }
    
    if (attrStart < 0) {
        return "";
    }
    
    int valueStart = attrStart + search.length();
    char quote = tag[valueStart - 1];
    int valueEnd = tag.indexOf(quote, valueStart);
    
    if (valueEnd < 0) {
        return "";
    }
    
    return tag.substring(valueStart, valueEnd);
}

// =============================================================================
// Cover Image Extraction
// =============================================================================

bool EpubParser::extractCoverImage(const String& outputPath) {
    if (_coverImagePath.length() == 0) {
        _error = "No cover image found in EPUB";
        Serial.println("[EPUB] No cover image to extract");
        return false;
    }
    
    Serial.printf("[EPUB] Extracting cover: %s -> %s\n", _coverImagePath.c_str(), outputPath.c_str());
    
    // Ensure output directory exists
    int lastSlash = outputPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = outputPath.substring(0, lastSlash);
        if (!SD.exists(dir)) {
            SD.mkdir(dir);
        }
    }
    
    // Open output file
    File outFile = SD.open(outputPath, FILE_WRITE);
    if (!outFile) {
        _error = "Failed to open output file: " + outputPath;
        Serial.printf("[EPUB] %s\n", _error.c_str());
        return false;
    }
    
    bool success = false;
    
    if (_sourceType == EpubSourceType::ZIP_FILE) {
        // Stream from ZIP to file
        success = epubZip.streamFileTo(_coverImagePath, outFile, 1024);
        if (!success) {
            _error = "Failed to extract cover from ZIP: " + epubZip.getError();
        }
    } else {
        // Copy from folder
        String srcPath = _path + "/" + _coverImagePath;
        File srcFile = SD.open(srcPath, FILE_READ);
        if (srcFile) {
            uint8_t buffer[1024];
            while (srcFile.available()) {
                size_t read = srcFile.read(buffer, sizeof(buffer));
                outFile.write(buffer, read);
            }
            srcFile.close();
            success = true;
        } else {
            _error = "Failed to open cover file: " + srcPath;
        }
    }
    
    outFile.close();
    
    if (success) {
        Serial.printf("[EPUB] Cover extracted successfully\n");
    } else {
        Serial.printf("[EPUB] Cover extraction failed: %s\n", _error.c_str());
        SD.remove(outputPath);  // Clean up failed file
    }
    
    return success;
}

// =============================================================================
// Utility Functions
// =============================================================================

bool isValidEpubFolder(const String& path) {
    String containerPath = path + "/META-INF/container.xml";
    return SD.exists(containerPath);
}

bool isEpubFile(const String& path) {
    return path.endsWith(".epub") || path.endsWith(".EPUB");
}
