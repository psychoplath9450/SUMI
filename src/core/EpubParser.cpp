/**
 * @file EpubParser.cpp
 * @brief Streaming EPUB Parser Implementation
 * @version 1.4.2
 *
 * Memory-efficient EPUB parsing:
 * - Expat streaming XML parser (1KB chunks)
 * - Manifest written to temp file, not RAM
 * - Two-tier caching via BookMetadataCache
 * - No full-file loading
 */

#include "core/EpubParser.h"

// Global instance
EpubParser* epubParser = nullptr;

// Static members
TocEntry EpubParser::_emptyTocEntry;

// =============================================================================
// Constructor / Destructor
// =============================================================================

EpubParser::EpubParser()
    : _isOpen(false)
    , _xmlParser(nullptr)
    , _parserState(EpubParserState::IDLE)
    , _manifestCount(0)
    , _currentDepth(0)
    , _inMetadata(false)
    , _inManifest(false)
    , _inSpine(false) {
    memset(_tempBuffer, 0, sizeof(_tempBuffer));
    memset(_currentElement, 0, sizeof(_currentElement));
    memset(_currentId, 0, sizeof(_currentId));
    memset(_currentHref, 0, sizeof(_currentHref));
    memset(_currentMediaType, 0, sizeof(_currentMediaType));
    memset(_currentTitle, 0, sizeof(_currentTitle));
}

EpubParser::~EpubParser() {
    close();
}

// =============================================================================
// Open / Close
// =============================================================================

bool EpubParser::open(const String& path) {
    MEM_LOG("epub_open_start");
    
    close();
    
    _path = path;
    _error = "";
    
    // Detect source type
    if (isValidEpubFolder(path)) {
        _sourceType = EpubSourceType::FOLDER;
        return openFolder(path);
    }
    
    if (!isEpubFile(path)) {
        _error = "Not an EPUB file: " + path;
        return false;
    }
    
    _sourceType = EpubSourceType::ZIP_FILE;
    
    // Open ZIP file
    if (!_zip.open(path)) {
        _error = "Failed to open EPUB: " + _zip.getError();
        Serial.printf("[EPUB] %s\n", _error.c_str());
        return false;
    }
    
    // Get cache path
    _cachePath = BookCacheManager::getCachePath(path);
    BookCacheManager::ensureCacheDir(_cachePath);
    
    // Try to load from cache first
    String bookBinPath = BookCacheManager::getBookBinPath(_cachePath);
    if (SD.exists(bookBinPath)) {
        if (_metadata.load(bookBinPath)) {
            Serial.printf("[EPUB] Loaded from cache: \"%s\"\n", _metadata.title);
            _contentBasePath = _metadata.contentBasePath;
            _isOpen = true;
            MEM_LOG("epub_open_cached");
            return true;
        }
        Serial.println("[EPUB] Cache invalid, reparsing...");
    }
    
    // Parse container.xml to find OPF
    if (!parseContainer()) {
        _zip.close();
        return false;
    }
    
    MEM_LOG("epub_after_container");
    
    // Parse OPF file
    if (!parseOPF()) {
        _zip.close();
        return false;
    }
    
    MEM_LOG("epub_after_opf");
    
    // Parse NCX or NAV for TOC
    if (_ncxPath.length() > 0) {
        parseNCX();
    } else if (_navPath.length() > 0) {
        parseNAV();
    }
    
    // Link TOC entries to spine
    _metadata.linkTocToSpine();
    
    // Save metadata cache
    strncpy(_metadata.contentBasePath, _contentBasePath.c_str(), MAX_HREF_LEN - 1);
    _metadata.save(bookBinPath);
    
    _isOpen = true;
    
    Serial.printf("[EPUB] Parsed: \"%s\" by %s, %d chapters, %d TOC entries\n",
                  _metadata.title, _metadata.author, 
                  _metadata.spineCount, _metadata.tocCount);
    
    MEM_LOG("epub_open_complete");
    
    return true;
}

bool EpubParser::openFolder(const String& path) {
    _path = path;
    _cachePath = BookCacheManager::getCachePath(path);
    BookCacheManager::ensureCacheDir(_cachePath);
    
    // For folder mode, we read files directly from SD
    // Parse container.xml
    String containerPath = path + "/META-INF/container.xml";
    if (!SD.exists(containerPath)) {
        _error = "container.xml not found";
        return false;
    }
    
    // Read container.xml to find OPF path
    File f = SD.open(containerPath, FILE_READ);
    if (!f) {
        _error = "Failed to open container.xml";
        return false;
    }
    
    // Simple parse for rootfile
    String content;
    content.reserve(2048);
    while (f.available() && content.length() < 2048) {
        content += (char)f.read();
    }
    f.close();
    
    // Find full-path attribute
    int rootfilePos = content.indexOf("rootfile");
    if (rootfilePos > 0) {
        int fullPathPos = content.indexOf("full-path", rootfilePos);
        if (fullPathPos > 0) {
            int quoteStart = content.indexOf('"', fullPathPos);
            if (quoteStart > 0) {
                int quoteEnd = content.indexOf('"', quoteStart + 1);
                if (quoteEnd > quoteStart) {
                    _opfPath = content.substring(quoteStart + 1, quoteEnd);
                }
            }
        }
    }
    
    if (_opfPath.length() == 0) {
        _error = "No OPF path in container.xml";
        return false;
    }
    
    // Set content base path
    int lastSlash = _opfPath.lastIndexOf('/');
    if (lastSlash > 0) {
        _contentBasePath = _opfPath.substring(0, lastSlash + 1);
    }
    
    // Try cache
    String bookBinPath = BookCacheManager::getBookBinPath(_cachePath);
    if (SD.exists(bookBinPath) && _metadata.load(bookBinPath)) {
        _isOpen = true;
        return true;
    }
    
    // Parse OPF from folder
    String opfFullPath = path + "/" + _opfPath;
    f = SD.open(opfFullPath, FILE_READ);
    if (!f) {
        _error = "Failed to open OPF: " + opfFullPath;
        return false;
    }
    
    // Simple OPF parsing for folder mode
    // For full streaming, use Expat - but for folders, simple parsing is OK
    content = "";
    content.reserve(32768);
    while (f.available() && content.length() < 32768) {
        content += (char)f.read();
    }
    f.close();
    
    // Extract title
    int titleStart = content.indexOf("<dc:title");
    if (titleStart > 0) {
        int tagEnd = content.indexOf('>', titleStart);
        int closeTag = content.indexOf("</dc:title>", tagEnd);
        if (tagEnd > 0 && closeTag > tagEnd) {
            String title = content.substring(tagEnd + 1, closeTag);
            title.trim();
            strncpy(_metadata.title, title.c_str(), MAX_TITLE_LEN - 1);
        }
    }
    
    // Extract author
    int authorStart = content.indexOf("<dc:creator");
    if (authorStart > 0) {
        int tagEnd = content.indexOf('>', authorStart);
        int closeTag = content.indexOf("</dc:creator>", tagEnd);
        if (tagEnd > 0 && closeTag > tagEnd) {
            String author = content.substring(tagEnd + 1, closeTag);
            author.trim();
            strncpy(_metadata.author, author.c_str(), MAX_AUTHOR_LEN - 1);
        }
    }
    
    // Parse spine items (simplified)
    int spineStart = content.indexOf("<spine");
    int spineEnd = content.indexOf("</spine>");
    if (spineStart > 0 && spineEnd > spineStart) {
        String spineSection = content.substring(spineStart, spineEnd);
        
        int pos = 0;
        while ((pos = spineSection.indexOf("idref=", pos)) > 0) {
            int quoteStart = spineSection.indexOf('"', pos);
            int quoteEnd = spineSection.indexOf('"', quoteStart + 1);
            if (quoteStart > 0 && quoteEnd > quoteStart) {
                String idref = spineSection.substring(quoteStart + 1, quoteEnd);
                
                // Find this item in manifest
                String searchStr = "id=\"" + idref + "\"";
                int itemPos = content.indexOf(searchStr);
                if (itemPos > 0) {
                    int hrefPos = content.indexOf("href=", itemPos);
                    if (hrefPos > 0 && hrefPos < itemPos + 200) {
                        int hq1 = content.indexOf('"', hrefPos);
                        int hq2 = content.indexOf('"', hq1 + 1);
                        if (hq1 > 0 && hq2 > hq1) {
                            String href = content.substring(hq1 + 1, hq2);
                            String fullHref = _contentBasePath + urlDecode(href);
                            
                            // Get file size
                            String filePath = path + "/" + fullHref;
                            uint32_t fileSize = 0;
                            if (SD.exists(filePath)) {
                                File chf = SD.open(filePath, FILE_READ);
                                if (chf) {
                                    fileSize = chf.size();
                                    chf.close();
                                }
                            }
                            
                            _metadata.addSpineEntry(fullHref.c_str(), fileSize);
                        }
                    }
                }
            }
            pos = quoteEnd + 1;
        }
    }
    
    // Save cache
    strncpy(_metadata.contentBasePath, _contentBasePath.c_str(), MAX_HREF_LEN - 1);
    _metadata.save(bookBinPath);
    
    _isOpen = true;
    return true;
}

void EpubParser::close() {
    if (_xmlParser) {
        XML_ParserFree(_xmlParser);
        _xmlParser = nullptr;
    }
    
    if (_manifestFile) {
        _manifestFile.close();
    }
    
    _zip.close();
    _isOpen = false;
    _path = "";
    _cachePath = "";
    _contentBasePath = "";
    _opfPath = "";
    _ncxPath = "";
    _navPath = "";
    _metadata.clear();
    _parserState = EpubParserState::IDLE;
}

// =============================================================================
// Container Parsing
// =============================================================================

bool EpubParser::parseContainer() {
    Serial.println("[EPUB] Parsing container.xml...");
    
    _opfPath = "";
    
    if (!streamParseFile("META-INF/container.xml", EpubParserState::PARSING_CONTAINER)) {
        _error = "Failed to parse container.xml";
        return false;
    }
    
    if (_opfPath.length() == 0) {
        _error = "No OPF path found in container.xml";
        return false;
    }
    
    Serial.printf("[EPUB] OPF path: %s\n", _opfPath.c_str());
    return true;
}

// =============================================================================
// OPF Parsing
// =============================================================================

bool EpubParser::parseOPF() {
    Serial.printf("[EPUB] Parsing OPF: %s\n", _opfPath.c_str());
    
    // Set content base path
    int lastSlash = _opfPath.lastIndexOf('/');
    if (lastSlash > 0) {
        _contentBasePath = _opfPath.substring(0, lastSlash + 1);
    } else {
        _contentBasePath = "";
    }
    
    // Create temp file for manifest items
    String manifestPath = "/.sumi/temp_manifest.bin";
    SD.remove(manifestPath);
    _manifestFile = SD.open(manifestPath, FILE_WRITE);
    if (!_manifestFile) {
        _error = "Failed to create manifest temp file";
        return false;
    }
    _manifestCount = 0;
    
    // Parse OPF
    _inMetadata = false;
    _inManifest = false;
    _inSpine = false;
    
    bool result = streamParseFile(_opfPath, EpubParserState::PARSING_OPF_METADATA);
    
    _manifestFile.close();
    
    if (!result) {
        SD.remove(manifestPath);
        _error = "Failed to parse OPF file";
        return false;
    }
    
    // Clean up temp file
    SD.remove(manifestPath);
    
    if (_metadata.spineCount == 0) {
        _error = "No chapters found in spine";
        Serial.println("[EPUB] Error: spine is empty!");
    }
    
    return _metadata.spineCount > 0;
}

// =============================================================================
// NCX Parsing (EPUB 2 TOC)
// =============================================================================

bool EpubParser::parseNCX() {
    Serial.printf("[EPUB] Parsing NCX: %s\n", _ncxPath.c_str());
    
    _currentDepth = 0;
    memset(_currentTitle, 0, sizeof(_currentTitle));
    memset(_currentHref, 0, sizeof(_currentHref));
    
    return streamParseFile(_ncxPath, EpubParserState::PARSING_NCX);
}

// =============================================================================
// NAV Parsing (EPUB 3 TOC)
// =============================================================================

bool EpubParser::parseNAV() {
    Serial.printf("[EPUB] Parsing NAV: %s\n", _navPath.c_str());
    
    _currentDepth = 0;
    memset(_currentTitle, 0, sizeof(_currentTitle));
    memset(_currentHref, 0, sizeof(_currentHref));
    
    return streamParseFile(_navPath, EpubParserState::PARSING_NAV);
}

// =============================================================================
// Streaming Parse Core
// =============================================================================

bool EpubParser::streamParseFile(const String& innerPath, EpubParserState initialState) {
    // Create Expat parser
    if (_xmlParser) {
        XML_ParserFree(_xmlParser);
    }
    _xmlParser = XML_ParserCreate(nullptr);
    if (!_xmlParser) {
        _error = "Failed to create XML parser";
        return false;
    }
    
    XML_SetUserData(_xmlParser, this);
    XML_SetElementHandler(_xmlParser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(_xmlParser, characterDataHandler);
    
    _parserState = initialState;
    memset(_currentElement, 0, sizeof(_currentElement));
    
    // Get file size
    size_t fileSize = 0;
    if (!_zip.getFileSize(innerPath, fileSize)) {
        Serial.printf("[EPUB] File not found: %s\n", innerPath.c_str());
        XML_ParserFree(_xmlParser);
        _xmlParser = nullptr;
        return false;
    }
    
    // Stream through parser in chunks
    bool success = true;
    size_t offset = 0;
    size_t totalRead = 0;
    
    while (offset < fileSize) {
        size_t toRead = min((size_t)EPUB_CHUNK_SIZE, fileSize - offset);
        size_t bytesRead = _zip.readFileChunk(innerPath, offset, (uint8_t*)_tempBuffer, toRead);
        
        if (bytesRead == 0) break;
        
        bool isFinal = (offset + bytesRead >= fileSize);
        
        if (XML_Parse(_xmlParser, _tempBuffer, bytesRead, isFinal) == XML_STATUS_ERROR) {
            Serial.printf("[EPUB] XML error at line %lu: %s\n",
                          XML_GetCurrentLineNumber(_xmlParser),
                          XML_ErrorString(XML_GetErrorCode(_xmlParser)));
            success = false;
            break;
        }
        
        offset += bytesRead;
        totalRead += bytesRead;
        
        // Yield every 8KB
        if (totalRead % 8192 == 0) {
            yield();
        }
    }
    
    XML_ParserFree(_xmlParser);
    _xmlParser = nullptr;
    _parserState = EpubParserState::IDLE;
    
    return success;
}

// =============================================================================
// Expat Callbacks
// =============================================================================

void XMLCALL EpubParser::startElementHandler(void* userData, const char* name, const char** atts) {
    EpubParser* self = static_cast<EpubParser*>(userData);
    self->handleStartElement(name, atts);
}

void XMLCALL EpubParser::endElementHandler(void* userData, const char* name) {
    EpubParser* self = static_cast<EpubParser*>(userData);
    self->handleEndElement(name);
}

void XMLCALL EpubParser::characterDataHandler(void* userData, const char* s, int len) {
    EpubParser* self = static_cast<EpubParser*>(userData);
    self->handleCharacterData(s, len);
}

const char* EpubParser::getAttr(const char** atts, const char* name) {
    for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], name) == 0) {
            return atts[i + 1];
        }
    }
    return nullptr;
}

// =============================================================================
// Element Handlers
// =============================================================================

void EpubParser::handleStartElement(const char* name, const char** atts) {
    const char* localName = strrchr(name, ':');
    localName = localName ? localName + 1 : name;
    
    strncpy(_currentElement, localName, sizeof(_currentElement) - 1);
    
    switch (_parserState) {
        case EpubParserState::PARSING_CONTAINER:
            if (strcmp(localName, "rootfile") == 0) {
                const char* fullPath = getAttr(atts, "full-path");
                if (fullPath) {
                    _opfPath = fullPath;
                }
            }
            break;
            
        case EpubParserState::PARSING_OPF_METADATA:
        case EpubParserState::PARSING_OPF_MANIFEST:
        case EpubParserState::PARSING_OPF_SPINE:
            handleOPFElement(localName, atts);
            break;
            
        case EpubParserState::PARSING_NCX:
            handleNCXElement(localName, atts);
            break;
            
        case EpubParserState::PARSING_NAV:
            handleNAVElement(localName, atts);
            break;
            
        default:
            break;
    }
}

void EpubParser::handleEndElement(const char* name) {
    const char* localName = strrchr(name, ':');
    localName = localName ? localName + 1 : name;
    
    switch (_parserState) {
        case EpubParserState::PARSING_OPF_METADATA:
        case EpubParserState::PARSING_OPF_MANIFEST:
        case EpubParserState::PARSING_OPF_SPINE:
            handleOPFEndElement(localName);
            break;
            
        case EpubParserState::PARSING_NCX:
            handleNCXEndElement(localName);
            break;
            
        case EpubParserState::PARSING_NAV:
            handleNAVEndElement(localName);
            break;
            
        default:
            break;
    }
    
    memset(_currentElement, 0, sizeof(_currentElement));
}

void EpubParser::handleCharacterData(const char* s, int len) {
    if (_parserState == EpubParserState::PARSING_OPF_METADATA) {
        if (strcmp(_currentElement, "title") == 0 && strlen(_metadata.title) == 0) {
            size_t currentLen = strlen(_metadata.title);
            size_t copyLen = min((size_t)len, MAX_TITLE_LEN - currentLen - 1);
            strncat(_metadata.title, s, copyLen);
        }
        else if ((strcmp(_currentElement, "creator") == 0 || 
                  strcmp(_currentElement, "author") == 0) && 
                 strlen(_metadata.author) == 0) {
            size_t currentLen = strlen(_metadata.author);
            size_t copyLen = min((size_t)len, MAX_AUTHOR_LEN - currentLen - 1);
            strncat(_metadata.author, s, copyLen);
        }
        else if (strcmp(_currentElement, "language") == 0 && strlen(_metadata.language) == 0) {
            size_t copyLen = min((size_t)len, sizeof(_metadata.language) - 1);
            strncat(_metadata.language, s, copyLen);
        }
    }
    else if (_parserState == EpubParserState::PARSING_NCX) {
        if (strcmp(_currentElement, "text") == 0) {
            size_t currentLen = strlen(_currentTitle);
            size_t copyLen = min((size_t)len, sizeof(_currentTitle) - currentLen - 1);
            strncat(_currentTitle, s, copyLen);
        }
    }
    else if (_parserState == EpubParserState::PARSING_NAV) {
        if (strcmp(_currentElement, "a") == 0 && strlen(_currentHref) > 0) {
            size_t currentLen = strlen(_currentTitle);
            size_t copyLen = min((size_t)len, sizeof(_currentTitle) - currentLen - 1);
            strncat(_currentTitle, s, copyLen);
        }
    }
}

// =============================================================================
// OPF Handlers
// =============================================================================

void EpubParser::handleOPFElement(const char* name, const char** atts) {
    if (strcmp(name, "metadata") == 0) {
        _inMetadata = true;
        _parserState = EpubParserState::PARSING_OPF_METADATA;
    }
    else if (strcmp(name, "manifest") == 0) {
        _inManifest = true;
        _parserState = EpubParserState::PARSING_OPF_MANIFEST;
    }
    else if (strcmp(name, "spine") == 0) {
        // Close manifest file before spine parsing so findManifestItem can read it
        if (_manifestFile) {
            _manifestFile.close();
        }
        _inSpine = true;
        _parserState = EpubParserState::PARSING_OPF_SPINE;
    }
    else if (strcmp(name, "item") == 0 && _inManifest) {
        ManifestItem item;
        item.clear();
        
        const char* id = getAttr(atts, "id");
        const char* href = getAttr(atts, "href");
        const char* mediaType = getAttr(atts, "media-type");
        const char* properties = getAttr(atts, "properties");
        
        if (id) strncpy(item.id, id, sizeof(item.id) - 1);
        if (href) strncpy(item.href, urlDecode(href).c_str(), sizeof(item.href) - 1);
        if (mediaType) strncpy(item.mediaType, mediaType, sizeof(item.mediaType) - 1);
        if (properties) strncpy(item.properties, properties, sizeof(item.properties) - 1);
        
        // Check for NCX
        if (mediaType && strcmp(mediaType, "application/x-dtbncx+xml") == 0) {
            _ncxPath = resolvePath(_contentBasePath, item.href);
        }
        
        // Check for NAV
        if (properties && strstr(properties, "nav") != nullptr) {
            _navPath = resolvePath(_contentBasePath, item.href);
        }
        
        // Check for cover image
        if (properties && strstr(properties, "cover-image") != nullptr) {
            strncpy(_metadata.coverHref, 
                    resolvePath(_contentBasePath, item.href).c_str(),
                    MAX_HREF_LEN - 1);
        }
        
        // Write to temp file
        if (_manifestFile) {
            _manifestFile.write((uint8_t*)&item, sizeof(ManifestItem));
            _manifestCount++;
            if (_manifestCount <= 5 || _manifestCount % 20 == 0) {
                Serial.printf("[EPUB] Manifest item %d: id=%s href=%s\n", 
                             _manifestCount, item.id, item.href);
            }
        }
    }
    else if (strcmp(name, "itemref") == 0 && _inSpine) {
        const char* idref = getAttr(atts, "idref");
        if (idref) {
            Serial.printf("[EPUB] Spine itemref: %s\n", idref);
            ManifestItem item;
            if (findManifestItem(idref, item)) {
                String fullPath = resolvePath(_contentBasePath, item.href);
                
                size_t fileSize = 0;
                _zip.getFileSize(fullPath, fileSize);
                
                _metadata.addSpineEntry(fullPath.c_str(), fileSize);
                Serial.printf("[EPUB] Added spine: %s (%d bytes)\n", fullPath.c_str(), fileSize);
            } else {
                Serial.printf("[EPUB] Manifest item not found: %s\n", idref);
            }
        }
    }
    else if (strcmp(name, "meta") == 0 && _inMetadata) {
        const char* nameAttr = getAttr(atts, "name");
        const char* content = getAttr(atts, "content");
        
        if (nameAttr && content && strcmp(nameAttr, "cover") == 0) {
            strncpy(_currentId, content, sizeof(_currentId) - 1);
        }
    }
}

void EpubParser::handleOPFEndElement(const char* name) {
    if (strcmp(name, "metadata") == 0) {
        _inMetadata = false;
        
        if (strlen(_currentId) > 0 && strlen(_metadata.coverHref) == 0) {
            ManifestItem item;
            if (findManifestItem(_currentId, item)) {
                strncpy(_metadata.coverHref,
                        resolvePath(_contentBasePath, item.href).c_str(),
                        MAX_HREF_LEN - 1);
            }
            memset(_currentId, 0, sizeof(_currentId));
        }
    }
    else if (strcmp(name, "manifest") == 0) {
        _inManifest = false;
    }
    else if (strcmp(name, "spine") == 0) {
        _inSpine = false;
    }
}

// =============================================================================
// NCX Handlers
// =============================================================================

void EpubParser::handleNCXElement(const char* name, const char** atts) {
    if (strcmp(name, "navPoint") == 0) {
        _currentDepth++;
        memset(_currentTitle, 0, sizeof(_currentTitle));
        memset(_currentHref, 0, sizeof(_currentHref));
    }
    else if (strcmp(name, "content") == 0) {
        const char* src = getAttr(atts, "src");
        if (src) {
            const char* hash = strchr(src, '#');
            if (hash) {
                size_t hrefLen = hash - src;
                strncpy(_currentHref, src, min(hrefLen, sizeof(_currentHref) - 1));
            } else {
                strncpy(_currentHref, src, sizeof(_currentHref) - 1);
            }
        }
    }
}

void EpubParser::handleNCXEndElement(const char* name) {
    if (strcmp(name, "navPoint") == 0) {
        if (strlen(_currentTitle) > 0 && strlen(_currentHref) > 0) {
            String fullPath = resolvePath(_contentBasePath, urlDecode(_currentHref));
            
            const char* anchor = nullptr;
            int hashPos = fullPath.indexOf('#');
            String anchorStr;
            if (hashPos > 0) {
                anchorStr = fullPath.substring(hashPos + 1);
                anchor = anchorStr.c_str();
                fullPath = fullPath.substring(0, hashPos);
            }
            
            _metadata.addTocEntry(_currentTitle, fullPath.c_str(), anchor, _currentDepth - 1);
        }
        
        _currentDepth--;
        memset(_currentTitle, 0, sizeof(_currentTitle));
        memset(_currentHref, 0, sizeof(_currentHref));
    }
}

// =============================================================================
// NAV Handlers (EPUB 3)
// =============================================================================

void EpubParser::handleNAVElement(const char* name, const char** atts) {
    if (strcmp(name, "ol") == 0) {
        _currentDepth++;
    }
    else if (strcmp(name, "a") == 0) {
        memset(_currentTitle, 0, sizeof(_currentTitle));
        const char* href = getAttr(atts, "href");
        if (href) {
            strncpy(_currentHref, href, sizeof(_currentHref) - 1);
        }
    }
}

void EpubParser::handleNAVEndElement(const char* name) {
    if (strcmp(name, "ol") == 0) {
        _currentDepth--;
    }
    else if (strcmp(name, "a") == 0) {
        if (strlen(_currentTitle) > 0 && strlen(_currentHref) > 0) {
            String fullPath = resolvePath(_contentBasePath, urlDecode(_currentHref));
            
            const char* anchor = nullptr;
            int hashPos = fullPath.indexOf('#');
            String anchorStr;
            if (hashPos > 0) {
                anchorStr = fullPath.substring(hashPos + 1);
                anchor = anchorStr.c_str();
                fullPath = fullPath.substring(0, hashPos);
            }
            
            _metadata.addTocEntry(_currentTitle, fullPath.c_str(), anchor, _currentDepth - 1);
        }
        memset(_currentHref, 0, sizeof(_currentHref));
    }
}

// =============================================================================
// Manifest Lookup
// =============================================================================

bool EpubParser::findManifestItem(const char* id, ManifestItem& out) {
    String manifestPath = "/.sumi/temp_manifest.bin";
    File f = SD.open(manifestPath, FILE_READ);
    if (!f) return false;
    
    ManifestItem item;
    while (f.read((uint8_t*)&item, sizeof(ManifestItem)) == sizeof(ManifestItem)) {
        if (strcmp(item.id, id) == 0) {
            out = item;
            f.close();
            return true;
        }
    }
    
    f.close();
    return false;
}

// =============================================================================
// Chapter Access
// =============================================================================

const Chapter& EpubParser::getChapter(int index) const {
    static Chapter empty;
    
    if (!_isOpen || index < 0 || index >= _metadata.spineCount) {
        return empty;
    }
    
    const SpineEntry& spine = _metadata.spine[index];
    
    _tempChapter.href = spine.href;
    _tempChapter.size = spine.size;
    _tempChapter.spineIndex = index;
    
    // Get title from TOC if available
    int tocIdx = _metadata.getTocIndexForSpine(index);
    if (tocIdx >= 0) {
        _tempChapter.title = _metadata.toc[tocIdx].title;
        _tempChapter.anchor = _metadata.toc[tocIdx].anchor;
    } else {
        _tempChapter.title = "Chapter " + String(index + 1);
        _tempChapter.anchor = "";
    }
    
    return _tempChapter;
}

bool EpubParser::streamChapterToFile(int chapterIndex, const String& outputPath) {
    if (!_isOpen || chapterIndex < 0 || chapterIndex >= _metadata.spineCount) {
        _error = "Invalid chapter index";
        return false;
    }
    
    const SpineEntry& entry = _metadata.spine[chapterIndex];
    
    MEM_LOG("stream_chapter_start");
    
    if (_sourceType == EpubSourceType::FOLDER) {
        return streamFolderFileToFile(entry.href, outputPath);
    }
    
    // Check if ZIP is still open
    if (!_zip.isOpen()) {
        if (!_zip.open(_path)) {
            _error = "Failed to reopen ZIP: " + _zip.getError();
            MEM_LOG("stream_chapter_end");
            return false;
        }
    }
    
    SD.mkdir("/.sumi");
    
    File outFile = SD.open(outputPath, FILE_WRITE);
    if (!outFile) {
        _error = "Failed to create output file: " + outputPath;
        MEM_LOG("stream_chapter_end");
        return false;
    }
    
    bool success = _zip.streamFileTo(entry.href, outFile, EPUB_CHUNK_SIZE);
    size_t fileSize = outFile.size();
    outFile.close();
    
    if (!success) {
        _error = "Failed to stream from ZIP: " + _zip.getError();
        SD.remove(outputPath);
    } else {
        Serial.printf("[EPUB] Streamed ch%d: %d bytes\n", chapterIndex, fileSize);
    }
    
    MEM_LOG("stream_chapter_end");
    
    return success;
}

bool EpubParser::streamFolderFileToFile(const String& innerPath, const String& outputPath) {
    String srcPath = _path + "/" + innerPath;
    
    File src = SD.open(srcPath, FILE_READ);
    if (!src) {
        _error = "Failed to open: " + srcPath;
        return false;
    }
    
    File dst = SD.open(outputPath, FILE_WRITE);
    if (!dst) {
        src.close();
        _error = "Failed to create: " + outputPath;
        return false;
    }
    
    uint8_t buffer[1024];
    while (src.available()) {
        size_t read = src.read(buffer, sizeof(buffer));
        if (read > 0) {
            dst.write(buffer, read);
        }
        yield();
    }
    
    src.close();
    dst.close();
    return true;
}

bool EpubParser::extractCoverImage(const String& outputPath) {
    if (!_isOpen || strlen(_metadata.coverHref) == 0) {
        _error = "No cover image available";
        return false;
    }
    
    return extractImage(_metadata.coverHref, outputPath);
}

bool EpubParser::extractImage(const String& imagePath, const String& outputPath) {
    if (!_isOpen) return false;
    
    String resolvedPath = imagePath;
    if (!imagePath.startsWith("/") && !imagePath.startsWith(_contentBasePath)) {
        resolvedPath = _contentBasePath + imagePath;
    }
    
    if (_sourceType == EpubSourceType::FOLDER) {
        return streamFolderFileToFile(resolvedPath, outputPath);
    }
    
    File outFile = SD.open(outputPath, FILE_WRITE);
    if (!outFile) {
        _error = "Failed to create image file";
        return false;
    }
    
    bool success = _zip.streamFileTo(resolvedPath, outFile, 1024);
    outFile.close();
    
    if (!success) {
        _error = "Failed to extract image: " + _zip.getError();
        SD.remove(outputPath);
    }
    
    return success;
}

// =============================================================================
// TOC Access
// =============================================================================

const TocEntry& EpubParser::getTocEntry(int index) const {
    if (index >= 0 && index < _metadata.tocCount) {
        return _metadata.toc[index];
    }
    return _emptyTocEntry;
}

int EpubParser::getChapterForToc(int tocIndex) const {
    if (tocIndex >= 0 && tocIndex < _metadata.tocCount) {
        return _metadata.toc[tocIndex].spineIndex;
    }
    return 0;
}

// =============================================================================
// DEPRECATED Methods (compatibility only)
// =============================================================================

String EpubParser::getChapterText(int chapterIndex) {
    Serial.println("[EPUB] WARNING: getChapterText() is deprecated - use streamChapterToFile()");
    MEM_LOG("deprecated_getChapterText");
    
    // Stream to temp, read back, delete - inefficient but compatible
    if (!streamChapterToFile(chapterIndex, TEMP_HTML_PATH)) {
        return "";
    }
    
    File f = SD.open(TEMP_HTML_PATH, FILE_READ);
    if (!f) return "";
    
    // Only read up to 32KB to avoid memory issues
    size_t maxRead = min((size_t)32768, (size_t)f.size());
    String content;
    content.reserve(maxRead);
    
    while (f.available() && content.length() < maxRead) {
        content += (char)f.read();
    }
    f.close();
    SD.remove(TEMP_HTML_PATH);
    
    // Strip HTML tags (simple version)
    String text;
    text.reserve(content.length());
    bool inTag = false;
    for (size_t i = 0; i < content.length(); i++) {
        char c = content[i];
        if (c == '<') inTag = true;
        else if (c == '>') inTag = false;
        else if (!inTag) text += c;
    }
    
    return text;
}

String EpubParser::getChapterHTML(int chapterIndex) {
    Serial.println("[EPUB] WARNING: getChapterHTML() is deprecated - use streamChapterToFile()");
    MEM_LOG("deprecated_getChapterHTML");
    
    if (!streamChapterToFile(chapterIndex, TEMP_HTML_PATH)) {
        return "";
    }
    
    File f = SD.open(TEMP_HTML_PATH, FILE_READ);
    if (!f) return "";
    
    size_t maxRead = min((size_t)32768, (size_t)f.size());
    String content;
    content.reserve(maxRead);
    
    while (f.available() && content.length() < maxRead) {
        content += (char)f.read();
    }
    f.close();
    SD.remove(TEMP_HTML_PATH);
    
    return content;
}

String EpubParser::getAllText() {
    // This function was CATASTROPHIC - tried to load entire book to RAM
    Serial.println("[EPUB] getAllText() is deprecated - use streamChapterToFile() instead");
    Serial.println("[EPUB] Use streamChapterToFile() and process chapters individually");
    return "[Chapter content - use streaming methods]";
}

// =============================================================================
// Path Utilities
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
    
    result.replace("\\", "/");
    result.replace("/./", "/");
    if (result.startsWith("./")) {
        result = result.substring(2);
    }
    
    int pos;
    while ((pos = result.indexOf("/../")) >= 0) {
        int prevSlash = result.lastIndexOf('/', pos - 1);
        if (prevSlash >= 0) {
            result = result.substring(0, prevSlash) + result.substring(pos + 3);
        } else {
            result = result.substring(pos + 4);
        }
    }
    
    while (result.indexOf("//") >= 0) {
        result.replace("//", "/");
    }
    
    if (result.startsWith("/")) {
        result = result.substring(1);
    }
    
    return result;
}

String EpubParser::urlDecode(const String& path) {
    String result;
    result.reserve(path.length());
    
    for (size_t i = 0; i < path.length(); i++) {
        char c = path[i];
        if (c == '%' && i + 2 < path.length()) {
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

// =============================================================================
// Utility Functions
// =============================================================================

bool isValidEpubFolder(const String& path) {
    String containerPath = path + "/META-INF/container.xml";
    return SD.exists(containerPath);
}

bool isEpubFile(const String& path) {
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".epub");
}
