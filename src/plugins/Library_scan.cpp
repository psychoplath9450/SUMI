/**
 * @file Library_scan.cpp
 * @brief Library scanning, cover extraction, and book opening
 * @version 1.4.2
 */

#include "plugins/Library.h"
#include <ArduinoJson.h>

#if FEATURE_READER

// =============================================================================
// Directory Scanning
// =============================================================================
void LibraryApp::scanDirectory() {
    Serial.printf("[LIBRARY] Scanning: %s\n", currentPath);
    MEM_LOG("scan_start");
    
    state = ViewState::INDEXING;
    indexingProgress = 0;
    
    // Ensure .sumi directory exists
    SD.mkdir("/.sumi");
    
    // Remove old index
    clearBooks();
    
    // Create new index file
    File indexFile = SD.open(LIBRARY_INDEX_PATH, FILE_WRITE);
    if (!indexFile) {
        Serial.println("[LIBRARY] Failed to create index");
        state = ViewState::BROWSER;
        return;
    }
    
    // Write header (will update count at end)
    LibraryIndexHeader header;
    strncpy(header.currentPath, currentPath, sizeof(header.currentPath) - 1);
    header.timestamp = millis();
    header.bookCount = 0;
    indexFile.write((uint8_t*)&header, sizeof(header));
    
    // Scan directory
    File dir = SD.open(currentPath);
    if (!dir) {
        Serial.printf("[LIBRARY] Failed to open: %s\n", currentPath);
        indexFile.close();
        state = ViewState::BROWSER;
        return;
    }
    
    // Process one book at a time - no temp array needed
    int count = 0;
    BookEntry book;  // Single entry, reused
    
    while (File entry = dir.openNextFile()) {
        if (count >= LIBRARY_MAX_BOOKS) { entry.close(); break; }
        
        const char* name = entry.name();
        if (name[0] == '.') { entry.close(); continue; }
        
        book.clear();
        
        strncpy(book.filename, name, sizeof(book.filename) - 1);
        book.size = entry.size();
        book.isDirectory = entry.isDirectory();
        book.isRegularDir = false;
        
        // Default title from filename
        strncpy(book.title, book.filename, sizeof(book.title) - 1);
        char* dot = strrchr(book.title, '.');
        if (dot && !book.isDirectory) *dot = '\0';
        
        // Build full path
        char fullPath[192];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, name);
        book.bookType = detectBookType(fullPath);
        
        bool isBook = (book.bookType == BookType::TXT || 
                      book.bookType == BookType::EPUB_FILE || 
                      book.bookType == BookType::EPUB_FOLDER);
        bool isRegularDir = book.isDirectory && book.bookType == BookType::UNKNOWN;
        
        if (isBook || isRegularDir) {
            // Check memory
            if (ESP.getFreeHeap() < 8000) {
                Serial.println("[LIBRARY] Low memory, stopping scan");
                entry.close();
                break;
            }
            
            // Load metadata for EPUBs
            if (book.bookType == BookType::EPUB_FILE || book.bookType == BookType::EPUB_FOLDER) {
                loadBookMetadata(book, fullPath);
            }
            
            book.isRegularDir = isRegularDir;
            
            // Write directly to index file
            book.serialize(indexFile);
            count++;
            indexingProgress = count;
            
            Serial.printf("[LIBRARY] Found: %s (%s)\n", book.title, 
                         book.bookType == BookType::EPUB_FILE ? "EPUB" : 
                         book.bookType == BookType::TXT ? "TXT" : "DIR");
        }
        
        entry.close();
        yield();
    }
    dir.close();
    
    // Update header with final count
    header.bookCount = count;
    indexFile.seek(0);
    indexFile.write((uint8_t*)&header, sizeof(header));
    indexFile.close();
    
    bookCount = count;
    cursor = 0;
    scrollOffset = 0;
    
    Serial.printf("[LIBRARY] Scan complete: %d items\n", count);
    MEM_LOG("scan_complete");
    
    // Extract covers in background
    if (count > 0) {
        extractAllCoversLightweight();
    }
    
    state = ViewState::BROWSER;
}

// =============================================================================
// Cover Extraction
// =============================================================================
String LibraryApp::findCoverInZip(ZipReader& zip) {
    const char* coverPaths[] = {
        "cover.jpg", "cover.jpeg", "cover.png",
        "OEBPS/cover.jpg", "OEBPS/cover.jpeg", "OEBPS/cover.png",
        "OEBPS/images/cover.jpg", "OEBPS/Images/cover.jpg",
        "images/cover.jpg", "Images/cover.jpg",
        nullptr
    };
    
    for (int i = 0; coverPaths[i]; i++) {
        if (zip.fileExists(coverPaths[i])) return String(coverPaths[i]);
    }
    
    // Search for any file with "cover" in name
    char filename[128];
    int count = zip.getFileCount();
    for (int i = 0; i < count && i < 100; i++) {
        if (zip.getFilename(i, filename, sizeof(filename))) {
            String f = String(filename);
            f.toLowerCase();
            if (f.indexOf("cover") >= 0 && 
                (f.endsWith(".jpg") || f.endsWith(".jpeg") || f.endsWith(".png"))) {
                return String(filename);
            }
        }
    }
    
    return "";
}

void LibraryApp::extractAllCoversLightweight() {
    Serial.println("[LIBRARY] Checking for covers...");
    MEM_LOG("covers_start");
    
    // Ensure cover directory exists
    SD.mkdir(COVER_CACHE_DIR);
    
    int found = 0, missing = 0;
    
    for (int i = 0; i < bookCount; i++) {
        BookEntry book;
        if (!getBook(i, book)) continue;
        if (book.bookType != BookType::EPUB_FILE) continue;
        
        char fullPath[192];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
        
        // Check for portal-uploaded cover (full size for library)
        String cachePath = getCoverCachePath(fullPath, false);  // false = full size
        Serial.printf("[COVER] Book %d: %s -> %s\n", i, book.title, cachePath.c_str());
        
        // Check if portal uploaded a cover
        if (isValidCoverFile(cachePath.c_str())) {
            Serial.printf("[COVER] Found portal cover: %s\n", cachePath.c_str());
            strncpy(book.coverPath, cachePath.c_str(), sizeof(book.coverPath) - 1);
            book.hasCover = true;
            updateBook(i, book);
            found++;
            continue;
        }
        
        // No cover found - user needs to re-upload via portal
        Serial.printf("[COVER] No cover for: %s (upload via portal to add)\n", book.title);
        book.hasCover = false;
        updateBook(i, book);
        missing++;
        
        yield();
    }
    
    Serial.printf("[LIBRARY] Covers: %d found, %d missing (upload via portal)\n", found, missing);
    MEM_LOG("covers_done");
}

bool LibraryApp::isValidCoverFile(const char* path) {
    if (!SD.exists(path)) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t sz = f.size();
    f.close();
    if (sz < 100) { SD.remove(path); return false; }
    return true;
}

void LibraryApp::loadBookMetadata(BookEntry& book, const char* fullPath) {
    // Check for pre-processed metadata first (from portal)
    const char* filename = book.filename;
    uint32_t hash = 0;
    for (const char* p = filename; *p; p++) {
        hash = ((hash << 5) - hash) + (uint8_t)*p;
    }
    hash = hash & 0xFFFFFFFF;
    
    char preprocessedDir[64];
    snprintf(preprocessedDir, sizeof(preprocessedDir), "/.sumi/books/%08x", hash);
    
    char metaPath[80];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.json", preprocessedDir);
    
    // Try pre-processed metadata first
    if (SD.exists(metaPath)) {
        Serial.printf("[SCAN] Using pre-processed metadata: %s\n", metaPath);
        
        File metaFile = SD.open(metaPath, FILE_READ);
        if (metaFile) {
            String json = metaFile.readString();
            metaFile.close();
            
            // Simple JSON parsing for title and author
            int titleStart = json.indexOf("\"title\":");
            if (titleStart > 0) {
                int valueStart = json.indexOf("\"", titleStart + 8);
                int valueEnd = json.indexOf("\"", valueStart + 1);
                if (valueStart > 0 && valueEnd > valueStart) {
                    String title = json.substring(valueStart + 1, valueEnd);
                    title.trim();
                    if (title.length() > 0) {
                        strncpy(book.title, title.c_str(), sizeof(book.title) - 1);
                    }
                }
            }
            
            int authorStart = json.indexOf("\"author\":");
            if (authorStart > 0) {
                int valueStart = json.indexOf("\"", authorStart + 9);
                int valueEnd = json.indexOf("\"", valueStart + 1);
                if (valueStart > 0 && valueEnd > valueStart) {
                    String author = json.substring(valueStart + 1, valueEnd);
                    author.trim();
                    if (author.length() > 0) {
                        strncpy(book.author, author.c_str(), sizeof(book.author) - 1);
                    }
                }
            }
            
            int chaptersStart = json.indexOf("\"totalChapters\":");
            if (chaptersStart > 0) {
                int valueStart = chaptersStart + 16;
                book.totalChapters = atoi(json.c_str() + valueStart);
            }
            
            book.hasCache = true;  // Mark that this book has pre-processed cache
        }
        
        // Check for pre-processed cover
        char coverPath[80];
        snprintf(coverPath, sizeof(coverPath), "%s/cover_full.jpg", preprocessedDir);
        if (SD.exists(coverPath)) {
            strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
            book.hasCover = true;
            Serial.printf("[SCAN] Found pre-processed cover: %s\n", coverPath);
        } else {
            book.hasCover = false;
        }
        
        // Load saved progress
        PageCache tempCache;
        tempCache.init(fullPath);
        int savedChapter, savedPage;
        if (tempCache.loadProgress(savedChapter, savedPage)) {
            book.lastChapter = savedChapter;
            book.lastPage = savedPage;
            if (book.totalChapters > 0) {
                book.progress = (float)savedChapter / (float)book.totalChapters;
            } else {
                book.progress = (float)savedChapter / 10.0f;
            }
            if (book.progress > 1.0f) book.progress = 1.0f;
        }
        tempCache.close();
        
        return;  // Done - used pre-processed data
    }
    
    // Fallback: Generate cover cache path (old-style)
    String cachePath = getCoverCachePath(fullPath);
    
    if (isValidCoverFile(cachePath.c_str())) {
        strncpy(book.coverPath, cachePath.c_str(), sizeof(book.coverPath) - 1);
        book.hasCover = true;
    } else {
        strncpy(book.coverPath, cachePath.c_str(), sizeof(book.coverPath) - 1);
        book.hasCover = false;
    }
    
    // Try to load saved progress
    PageCache tempCache;
    tempCache.init(fullPath);
    int savedChapter, savedPage;
    if (tempCache.loadProgress(savedChapter, savedPage)) {
        book.lastChapter = savedChapter;
        book.lastPage = savedPage;
        book.progress = (float)savedChapter / 10.0f;
        if (book.progress > 1.0f) book.progress = 1.0f;
    }
    tempCache.close();
    
    // Quick metadata extraction from EPUB (fallback - no pre-processed cache)
    // Only try this if requirePreprocessed is disabled
    if (book.bookType == BookType::EPUB_FILE && !settingsManager.reader.requirePreprocessed) {
        Serial.println("[SCAN] No pre-processed cache, parsing EPUB metadata...");
        ZipReader zip;
        if (zip.open(fullPath)) {
            String container = zip.readFileToString("META-INF/container.xml", 4096);
            if (container.length() > 0) {
                int fpStart = container.indexOf("full-path=\"");
                if (fpStart > 0) {
                    fpStart += 11;
                    int fpEnd = container.indexOf("\"", fpStart);
                    if (fpEnd > fpStart) {
                        String opfPath = container.substring(fpStart, fpEnd);
                        String opf = zip.readFileToString(opfPath, 16384);
                        
                        // Extract title
                        int titleStart = opf.indexOf("<dc:title");
                        if (titleStart > 0) {
                            int tagEnd = opf.indexOf(">", titleStart);
                            int closeTag = opf.indexOf("</dc:title>", tagEnd);
                            if (tagEnd > 0 && closeTag > tagEnd) {
                                String title = opf.substring(tagEnd + 1, closeTag);
                                title.trim();
                                if (title.length() > 0) {
                                    strncpy(book.title, title.c_str(), sizeof(book.title) - 1);
                                }
                            }
                        }
                        
                        // Extract author
                        int authorStart = opf.indexOf("<dc:creator");
                        if (authorStart > 0) {
                            int tagEnd = opf.indexOf(">", authorStart);
                            int closeTag = opf.indexOf("</dc:creator>", tagEnd);
                            if (tagEnd > 0 && closeTag > tagEnd) {
                                String author = opf.substring(tagEnd + 1, closeTag);
                                author.trim();
                                if (author.length() > 0) {
                                    strncpy(book.author, author.c_str(), sizeof(book.author) - 1);
                                }
                            }
                        }
                    }
                }
            }
            zip.close();;
        }
    }
}

String LibraryApp::getCoverCachePath(const char* bookPath, bool forWidget) {
    // Extract just the filename from path
    const char* filename = bookPath;
    const char* lastSlash = strrchr(bookPath, '/');
    if (lastSlash) filename = lastSlash + 1;
    
    // Simple hash matching portal's simpleHash function
    uint32_t hash = 0;
    for (const char* p = filename; *p; p++) {
        uint32_t charCode = (uint8_t)*p;
        hash = ((hash << 5) - hash) + charCode;
    }
    hash = hash & 0xFFFFFFFF;  // Ensure 32-bit
    
    // Portal saves covers to /.sumi/books/{hash}/cover_thumb.jpg or cover_full.jpg
    char path[96];
    const char* suffix = forWidget ? "cover_thumb.jpg" : "cover_full.jpg";
    snprintf(path, sizeof(path), "/.sumi/books/%08x/%s", hash, suffix);
    return String(path);
}

// Legacy overload for backward compatibility
String LibraryApp::getCoverCachePath(const char* bookPath) {
    return getCoverCachePath(bookPath, false);
}

void LibraryApp::cacheBookCover(const char* bookPath) {
    // Cover extraction happens in extractAllCoversLightweight
}

void LibraryApp::extractCoverOnDemand(BookEntry& book, const char* fullPath) {
    if (book.hasCover) return;
    
    String cachePath = getCoverCachePath(fullPath);
    
    ZipReader zip;
    if (!zip.open(fullPath)) return;
    
    String coverInZip = findCoverInZip(zip);
    if (coverInZip.length() == 0) { zip.close(); return; }
    
    File outFile = SD.open(cachePath, FILE_WRITE);
    if (!outFile) { zip.close(); return; }
    
    bool success = zip.streamFileTo(coverInZip, outFile, 1024);
    outFile.close();
    zip.close();
    
    if (success) {
        strncpy(book.coverPath, cachePath.c_str(), sizeof(book.coverPath) - 1);
        book.hasCover = true;
    }
}

// =============================================================================
// Book Opening
// =============================================================================
void LibraryApp::openBook(int index) {
    Serial.printf("[LIBRARY] openBook(%d)\n", index);
    MEM_LOG("openBook_start");
    
    if (index >= bookCount) {
        Serial.printf("[LIBRARY] Invalid index: %d >= %d\n", index, bookCount);
        return;
    }
    
    showLoadingScreen("Loading...");
    
    // Suspend WiFi for memory
    suspendForReading();
    ZipReader_preallocateBuffer();
    stats.load();
    
    BookEntry book;
    if (!getBook(index, book)) {
        showErrorScreen("Failed to load book");
        state = ViewState::BROWSER;
        return;
    }
    
    snprintf(currentBookPath, sizeof(currentBookPath), "%s/%s", currentPath, book.filename);
    strncpy(currentBook, book.title, sizeof(currentBook) - 1);
    
    // Note: Cover extraction skipped here - large covers (100KB+) don't fit in memory
    // Widget and library views show title as fallback
    
    Serial.printf("[LIBRARY] Book: %s\n", currentBook);
    Serial.printf("[LIBRARY] Path: %s\n", currentBookPath);
    
    isEpub = (book.bookType == BookType::EPUB_FOLDER || book.bookType == BookType::EPUB_FILE);
    
    if (isEpub) {
        // Try to load from pre-processed cache first (FAST!)
        if (!openPreprocessedMetadata(currentBookPath)) {
            // Check if we should try fallback EPUB parsing
            if (settingsManager.reader.requirePreprocessed) {
                // Pre-processing required but not found - show friendly error
                Serial.println("[LIBRARY] Pre-processing required but book not processed");
                showErrorScreen("Process this book\nin the portal first");
                state = ViewState::BROWSER;
                return;
            }
            
            // Fall back to EPUB parsing (SLOW, unreliable)
            Serial.println("[LIBRARY] No pre-processed cache, parsing EPUB...");
            if (!openEpubMetadata(currentBookPath)) {
                showErrorScreen("Failed to open EPUB");
                state = ViewState::BROWSER;
                return;
            }
        }
    } else {
        openTxtMetadata(currentBookPath);
    }
    
    // Initialize cache
    pageCache->init(currentBookPath);
    
    CacheKey checkKey;
    LibReaderSettings& settings = readerSettings.get();
    checkKey.fontSize = (uint8_t)settings.fontSize;
    checkKey.margins = settings.screenMargin;
    checkKey.lineSpacing = (uint8_t)settings.lineSpacing;
    checkKey.setJustify(settings.justifyText());
    checkKey.screenWidth = getLayoutWidth();
    checkKey.screenHeight = screenH;
    
    if (!pageCache->hasValidCache(checkKey)) {
        pageCache->invalidateBook();
    }
    
    textLayout->setPageSize(getLayoutWidth(), screenH);
    applyFontSettings();
    
    // Restore progress
    int savedChapter = 0, savedPage = 0;
    if (pageCache->loadProgress(savedChapter, savedPage)) {
        if (savedChapter < totalChapters) currentChapter = savedChapter;
        currentPage = savedPage;
    } else {
        currentChapter = 0;
        currentPage = 0;
    }
    
    // Defer chapter loading to render task
    pendingChapterLoad = true;
    pendingChapterToLoad = currentChapter;
    cacheValid = false;
    
    // Always use synchronous loading (render task uses too much stack for miniz)
    renderTaskHandle = nullptr;
    renderMutex = nullptr;
    
    Serial.printf("[LIBRARY] Using sync loading (heap=%d)\n", ESP.getFreeHeap());
    
    // Skip empty chapters (like cover pages)
    int chapToLoad = currentChapter;
    while (chapToLoad < totalChapters) {
        Serial.printf("[LIBRARY] Sync loading chapter %d\n", chapToLoad);
        if (loadChapterSync(chapToLoad)) {
            Serial.printf("[LIBRARY] Sync: chapter %d loaded, pages=%d\n", chapToLoad, totalPages);
            if (totalPages > 0) {
                currentChapter = chapToLoad;
                cacheValid = true;
                updateRequired = true;
                break;
            }
            Serial.printf("[LIBRARY] Sync: Chapter %d empty, trying next\n", chapToLoad);
        } else {
            Serial.printf("[LIBRARY] Sync: Chapter %d load FAILED\n", chapToLoad);
        }
        chapToLoad++;
    }
    if (!cacheValid) {
        Serial.println("[LIBRARY] Sync: No loadable chapters found!");
    }
    
    stats.startSession();
    bookIsOpen = true;
    firstRenderAfterOpen = true;
    state = ViewState::READING;
    pagesUntilFullRefresh = settings.pagesPerFullRefresh();
    pagesUntilHalfRefresh = settings.pagesPerFullRefresh() / 3;  // Half refresh every ~5 pages
    
    loadBookmarks();
    saveLastBookInfo();
    
    MEM_LOG("openBook_done");
}

void LibraryApp::openTxtMetadata(const char* path) {
    totalChapters = 1;
    currentChapter = 0;
    strncpy(chapterTitle, currentBook, sizeof(chapterTitle) - 1);
    chapterTitleCount = 0;
    strncpy(chapterTitles[chapterTitleCount++].title, currentBook, sizeof(ChapterTitle::title) - 1);
}

bool LibraryApp::openPreprocessedMetadata(const char* path) {
    // Calculate hash from filename (same as portal's simpleHash)
    const char* filename = strrchr(path, '/');
    if (filename) filename++; else filename = path;
    
    uint32_t hash = 0;
    for (const char* p = filename; *p; p++) {
        hash = ((hash << 5) - hash) + (uint8_t)*p;
    }
    
    Serial.printf("[LIBRARY] openPreprocessedMetadata: filename='%s' hash=%08x\n", filename, hash);
    
    // Check for pre-processed meta.json
    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "/.sumi/books/%08x/meta.json", hash);
    
    if (!SD.exists(metaPath)) {
        Serial.printf("[LIBRARY] No pre-processed cache at %s\n", metaPath);
        return false;
    }
    
    Serial.printf("[LIBRARY] Found pre-processed cache: %s\n", metaPath);
    
    File metaFile = SD.open(metaPath);
    if (!metaFile) return false;
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, metaFile);
    metaFile.close();
    
    if (err) {
        Serial.printf("[LIBRARY] Failed to parse meta.json: %s\n", err.c_str());
        return false;
    }
    
    totalChapters = doc["totalChapters"] | 1;
    Serial.printf("[LIBRARY] Loaded from pre-processed cache: %d chapters\n", totalChapters);
    
    // Build chapter titles
    chapterTitleCount = 0;
    JsonArray chapters = doc["chapters"].as<JsonArray>();
    if (chapters) {
        for (JsonObject ch : chapters) {
            if (chapterTitleCount >= MAX_CHAPTER_TITLES) break;
            // Use index+1 as chapter title (we don't store titles in meta.json)
            snprintf(chapterTitles[chapterTitleCount].title, sizeof(ChapterTitle::title), 
                     "Chapter %d", chapterTitleCount + 1);
            chapterTitleCount++;
        }
    } else {
        // Fallback: just use the totalChapters count
        for (int i = 0; i < totalChapters && chapterTitleCount < MAX_CHAPTER_TITLES; i++) {
            snprintf(chapterTitles[chapterTitleCount].title, sizeof(ChapterTitle::title), 
                     "Chapter %d", i + 1);
            chapterTitleCount++;
        }
    }
    
    if (currentChapter < chapterTitleCount) {
        strncpy(chapterTitle, chapterTitles[currentChapter].title, sizeof(chapterTitle) - 1);
    }
    
    return true;
}

bool LibraryApp::openEpubMetadata(const char* path) {
    if (!epub.open(path)) {
        Serial.printf("[LIBRARY] EPUB open failed: %s\n", epub.getError().c_str());
        return false;
    }
    
    totalChapters = epub.getChapterCount();
    if (totalChapters == 0) totalChapters = 1;
    
    // Build chapter title list from TOC or spine
    chapterTitleCount = 0;
    int tocCount = epub.getTocCount();
    
    if (tocCount > 0) {
        for (int i = 0; i < tocCount && chapterTitleCount < MAX_CHAPTER_TITLES; i++) {
            const TocEntry& toc = epub.getTocEntry(i);
            strncpy(chapterTitles[chapterTitleCount++].title, toc.title, sizeof(ChapterTitle::title) - 1);
        }
    } else {
        for (int i = 0; i < totalChapters && chapterTitleCount < MAX_CHAPTER_TITLES; i++) {
            snprintf(chapterTitles[chapterTitleCount++].title, sizeof(ChapterTitle::title), "Chapter %d", i + 1);
        }
    }
    
    if (currentChapter < chapterTitleCount) {
        strncpy(chapterTitle, chapterTitles[currentChapter].title, sizeof(chapterTitle) - 1);
    }
    
    return true;
}

void LibraryApp::closeBook() {
    if (!bookIsOpen) return;
    
    MEM_LOG("closeBook_start");
    
    saveProgress();
    stats.endSession();
    stats.save();
    
    if (renderTaskHandle) {
        vTaskDelete(renderTaskHandle);
        renderTaskHandle = nullptr;
    }
    if (renderMutex) {
        vSemaphoreDelete(renderMutex);
        renderMutex = nullptr;
    }
    
    epub.close();
    pageCache->close();
    
    bookIsOpen = false;
    cacheValid = false;
    currentPage = 0;
    totalPages = 0;
    
    MEM_LOG("closeBook_done");
}

#endif // FEATURE_READER
