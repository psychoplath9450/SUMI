/**
 * @file Library_scan.cpp
 * @brief Library scanning, cover extraction, and book opening
 * @version 1.5.0
 * 
 * All books must be preprocessed via portal before reading.
 * No on-device EPUB parsing - portal handles all extraction.
 */

#include "plugins/Library.h"
#include <ArduinoJson.h>

#if FEATURE_READER

// =============================================================================
// Portal Library Index Loading
// =============================================================================

bool LibraryApp::loadPortalLibraryIndex() {
    const char* indexPath = "/.sumi/library.json";
    
    if (!SD.exists(indexPath)) {
        Serial.println("[LIBRARY] No portal index found");
        return false;
    }
    
    File f = SD.open(indexPath, FILE_READ);
    if (!f) {
        Serial.println("[LIBRARY] Failed to open portal index");
        return false;
    }
    
    // Use streaming JSON parser for memory efficiency
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    
    if (err) {
        Serial.printf("[LIBRARY] Portal index parse error: %s\n", err.c_str());
        return false;
    }
    
    int version = doc["version"] | 0;
    if (version != 1) {
        Serial.printf("[LIBRARY] Portal index version mismatch: %d\n", version);
        return false;
    }
    
    JsonArray books = doc["books"];
    if (books.isNull() || books.size() == 0) {
        Serial.println("[LIBRARY] Portal index has no books");
        return false;
    }
    
    // Clear existing books and create new index
    clearBooks();
    
    // Create binary index file from JSON data
    File indexFile = SD.open(LIBRARY_INDEX_PATH, FILE_WRITE);
    if (!indexFile) {
        Serial.println("[LIBRARY] Failed to create binary index");
        return false;
    }
    
    // Write header
    LibraryIndexHeader header;
    strncpy(header.currentPath, "/books", sizeof(header.currentPath) - 1);
    header.timestamp = millis();
    header.bookCount = 0;
    indexFile.write((uint8_t*)&header, sizeof(header));
    
    int count = 0;
    BookEntry book;
    
    for (JsonObject bookObj : books) {
        if (count >= LIBRARY_MAX_BOOKS) break;
        
        memset(&book, 0, sizeof(book));
        
        // Copy data from JSON
        const char* hash = bookObj["hash"] | "";
        const char* filename = bookObj["filename"] | "";
        const char* title = bookObj["title"] | "Unknown";
        const char* author = bookObj["author"] | "";
        const char* coverPath = bookObj["coverPath"] | "";
        
        strncpy(book.filename, filename, sizeof(book.filename) - 1);
        strncpy(book.title, title, sizeof(book.title) - 1);
        strncpy(book.author, author, sizeof(book.author) - 1);
        strncpy(book.coverPath, coverPath, sizeof(book.coverPath) - 1);
        
        // Parse hash for cacheHash field
        book.cacheHash = strtoul(hash, NULL, 16);
        
        book.totalChapters = bookObj["totalChapters"] | 0;
        book.totalWords = bookObj["totalWords"] | 0;
        book.estimatedPages = bookObj["estimatedPages"] | 0;
        book.pubYear = bookObj["pubYear"] | 0;
        book.hasCover = bookObj["hasCover"] | false;
        book.hasCache = true;  // By definition, portal-indexed books are preprocessed
        book.bookType = BookType::EPUB_FILE;
        
        // Write to binary index using proper serialization
        book.serialize(indexFile);
        count++;
        
        Serial.printf("[LIBRARY] Loaded: %s by %s\n", book.title, book.author);
    }
    
    // Update header with final count
    indexFile.seek(0);
    header.bookCount = count;
    indexFile.write((uint8_t*)&header, sizeof(header));
    indexFile.close();
    
    bookCount = count;
    strncpy(currentPath, "/books", sizeof(currentPath) - 1);
    
    Serial.printf("[LIBRARY] Portal index loaded: %d books\n", count);
    return count > 0;
}

// =============================================================================
// Book Sorting
// =============================================================================

// Simple case-insensitive string compare for sorting
static int strcasecmp_title(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

void LibraryApp::sortBooks(int sortMode) {
    // sortMode: 0 = by title (A-Z), 1 = by title (Z-A), 2 = by type
    if (bookCount < 2) return;
    
    Serial.printf("[LIBRARY] Sorting %d books (mode=%d)\n", bookCount, sortMode);
    
    // Allocate array for all book entries
    BookEntry* books = (BookEntry*)malloc(bookCount * sizeof(BookEntry));
    if (!books) {
        Serial.println("[LIBRARY] Sort failed: not enough memory");
        return;
    }
    
    // Read all books from index
    File f = SD.open(LIBRARY_INDEX_PATH, FILE_READ);
    if (!f) {
        free(books);
        return;
    }
    
    f.seek(sizeof(LibraryIndexHeader));
    for (int i = 0; i < bookCount; i++) {
        books[i].deserialize(f);
    }
    f.close();
    
    // Simple bubble sort (books array is small, typically <100)
    for (int i = 0; i < bookCount - 1; i++) {
        for (int j = 0; j < bookCount - i - 1; j++) {
            bool swap = false;
            
            // Directories always come first
            if (books[j].isRegularDir != books[j+1].isRegularDir) {
                swap = !books[j].isRegularDir && books[j+1].isRegularDir;
            } else {
                // Sort by title
                int cmp = strcasecmp_title(books[j].title, books[j+1].title);
                swap = (sortMode == 0) ? (cmp > 0) : (cmp < 0);
            }
            
            if (swap) {
                BookEntry temp = books[j];
                books[j] = books[j+1];
                books[j+1] = temp;
            }
        }
    }
    
    // Write sorted books back to index
    f = SD.open(LIBRARY_INDEX_PATH, FILE_WRITE);
    if (!f) {
        free(books);
        return;
    }
    
    // Write header
    LibraryIndexHeader header;
    header.magic = 0x4C494258; // "LIBX"
    header.version = 1;
    header.bookCount = bookCount;
    f.write((uint8_t*)&header, sizeof(header));
    
    // Write sorted books
    for (int i = 0; i < bookCount; i++) {
        books[i].serialize(f);
    }
    f.close();
    
    free(books);
    Serial.println("[LIBRARY] Sort complete");
}

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
    
    // Process one book at a time - only include preprocessed books
    int count = 0;
    BookEntry book;  // Single entry, reused
    
    while (File entry = dir.openNextFile()) {
        if (count >= LIBRARY_MAX_BOOKS) { entry.close(); break; }
        
        const char* name = entry.name();
        if (name[0] == '.') { entry.close(); continue; }
        
        bool isDir = entry.isDirectory();
        String fileName = String(name);
        fileName.toLowerCase();
        bool isEpub = fileName.endsWith(".epub");
        bool isTxt = fileName.endsWith(".txt");
        
        if (!isDir && !isEpub && !isTxt) { entry.close(); continue; }
        
        entry.close();
        
        // Compute hash matching portal's simpleHash(filename) algorithm
        // JS: hash = ((hash << 5) - hash) + charCode; hash = hash >>> 0;
        uint32_t hash = 0;
        const char* p = name;  // Use filename only, not full path
        while (*p) {
            hash = ((hash << 5) - hash) + (uint8_t)*p++;
        }
        
        char metaPath[80];
        snprintf(metaPath, sizeof(metaPath), "/.sumi/books/%08x/meta.json", hash);
        
        // Check if meta.json exists
        if (!SD.exists(metaPath)) {
            Serial.printf("[SCAN] Skipping (no meta.json): %s\n", name);
            continue;
        }
        
        // Also verify at least one chapter file exists (ch_000.txt)
        char chapterPath[80];
        snprintf(chapterPath, sizeof(chapterPath), "/.sumi/books/%08x/ch_000.txt", hash);
        if (!SD.exists(chapterPath)) {
            Serial.printf("[SCAN] Skipping (no chapters): %s\n", name);
            continue;
        }
        
        // Initialize book entry
        memset(&book, 0, sizeof(book));
        strncpy(book.filename, name, sizeof(book.filename) - 1);
        book.cacheHash = hash;  // Store hash computed from full filename
        
        if (isDir) {
            book.bookType = BookType::EPUB_FOLDER;
            strncpy(book.title, name, sizeof(book.title) - 1);
        } else if (isEpub) {
            book.bookType = BookType::EPUB_FILE;
            String title = String(name);
            title.replace(".epub", "");
            title.replace("_", " ");
            strncpy(book.title, title.c_str(), sizeof(book.title) - 1);
        } else {
            book.bookType = BookType::TXT;
            String title = String(name);
            title.replace(".txt", "");
            title.replace("_", " ");
            strncpy(book.title, title.c_str(), sizeof(book.title) - 1);
        }
        
        // Build full path for metadata loading
        char fullPath[192];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, name);
        
        // Load metadata from preprocessed cache
        loadBookMetadata(book, fullPath);
        
        // Write to index
        indexFile.write((uint8_t*)&book, sizeof(book));
        count++;
        
        // Update progress
        indexingProgress = (count * 100) / LIBRARY_MAX_BOOKS;
        if (indexingProgress > 95) indexingProgress = 95;
        
        yield();
    }
    
    dir.close();
    
    // Update header with final count
    indexFile.seek(0);
    header.bookCount = count;
    indexFile.write((uint8_t*)&header, sizeof(header));
    indexFile.close();
    
    bookCount = count;
    
    // Find covers from preprocessed cache (portal extracts them)
    checkForCovers();
    
    Serial.printf("[LIBRARY] Found %d books\n", bookCount);
    MEM_LOG("scan_done");
    
    // Auto-sort
    sortBooks(0);
    state = ViewState::BROWSER;
}

// =============================================================================
// Cover Handling - Portal extracts all covers during preprocessing
// =============================================================================
void LibraryApp::checkForCovers() {
    Serial.println("[LIBRARY] Checking for portal-extracted covers...");
    MEM_LOG("covers_start");
    
    int found = 0, missing = 0;
    
    for (int i = 0; i < bookCount; i++) {
        BookEntry book;
        if (!getBook(i, book)) continue;
        if (book.bookType != BookType::EPUB_FILE) continue;
        
        char fullPath[192];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
        
        // Check for portal-uploaded cover (full size for library)
        String cachePath = getCoverCachePath(fullPath, false);
        
        if (isValidCoverFile(cachePath.c_str())) {
            strncpy(book.coverPath, cachePath.c_str(), sizeof(book.coverPath) - 1);
            book.hasCover = true;
            updateBook(i, book);
            found++;
        } else {
            book.hasCover = false;
            updateBook(i, book);
            missing++;
        }
        
        yield();
    }
    
    Serial.printf("[LIBRARY] Covers: %d found, %d without covers\n", found, missing);
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
    // Check for pre-processed metadata (from portal)
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
    
    // Try pre-processed metadata
    if (SD.exists(metaPath)) {
        Serial.printf("[SCAN] Using pre-processed metadata: %s\n", metaPath);
        
        File metaFile = SD.open(metaPath, FILE_READ);
        if (metaFile) {
            String json = metaFile.readString();
            metaFile.close();
            
            // Helper lambda to extract string value
            auto extractString = [&json](const char* key, char* dest, size_t maxLen) {
                String searchKey = String("\"") + key + "\":";
                int keyPos = json.indexOf(searchKey);
                if (keyPos > 0) {
                    int valueStart = json.indexOf("\"", keyPos + searchKey.length());
                    int valueEnd = json.indexOf("\"", valueStart + 1);
                    if (valueStart > 0 && valueEnd > valueStart) {
                        String value = json.substring(valueStart + 1, valueEnd);
                        value.trim();
                        if (value.length() > 0 && value != "null") {
                            strncpy(dest, value.c_str(), maxLen - 1);
                            dest[maxLen - 1] = '\0';
                        }
                    }
                }
            };
            
            // Helper lambda to extract int value
            auto extractInt = [&json](const char* key) -> int {
                String searchKey = String("\"") + key + "\":";
                int keyPos = json.indexOf(searchKey);
                if (keyPos > 0) {
                    int valueStart = keyPos + searchKey.length();
                    // Skip whitespace
                    while (valueStart < json.length() && (json[valueStart] == ' ' || json[valueStart] == '\t')) {
                        valueStart++;
                    }
                    int valueEnd = valueStart;
                    while (valueEnd < json.length() && (isdigit(json[valueEnd]) || json[valueEnd] == '-')) {
                        valueEnd++;
                    }
                    if (valueEnd > valueStart) {
                        return json.substring(valueStart, valueEnd).toInt();
                    }
                }
                return 0;
            };
            
            // Extract core metadata
            extractString("title", book.title, sizeof(book.title));
            extractString("author", book.author, sizeof(book.author));
            
            // Extract extended metadata
            book.totalChapters = extractInt("totalChapters");
            book.totalWords = extractInt("totalWords");
            book.estimatedPages = extractInt("estimatedPages");
            book.pubYear = extractInt("pubYear");
            
            // Mark as cached
            book.hasCache = true;
            
            Serial.printf("[SCAN] Loaded: %s by %s (%d ch, %d words, ~%d pg)\n", 
                book.title, book.author, book.totalChapters, book.totalWords, book.estimatedPages);
            
            // Check for cover - prefer thumbnail for library view (faster loading)
            String thumbPath = String(preprocessedDir) + "/cover_thumb.jpg";
            String fullPath = String(preprocessedDir) + "/cover_full.jpg";
            if (SD.exists(thumbPath)) {
                strncpy(book.coverPath, thumbPath.c_str(), sizeof(book.coverPath) - 1);
                book.hasCover = true;
            } else if (SD.exists(fullPath)) {
                strncpy(book.coverPath, fullPath.c_str(), sizeof(book.coverPath) - 1);
                book.hasCover = true;
            }
        }
    } else {
        // No preprocessing - book needs to be uploaded via portal
        Serial.printf("[SCAN] Book not preprocessed: %s\n", book.filename);
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
    hash = hash & 0xFFFFFFFF;
    
    // Portal saves covers to /.sumi/books/{hash}/cover_thumb.jpg or cover_full.jpg
    char path[96];
    const char* suffix = forWidget ? "cover_thumb.jpg" : "cover_full.jpg";
    snprintf(path, sizeof(path), "/.sumi/books/%08x/%s", hash, suffix);
    return String(path);
}

// Legacy overload
String LibraryApp::getCoverCachePath(const char* bookPath) {
    return getCoverCachePath(bookPath, false);
}

// =============================================================================
// Book Opening - All books must be preprocessed
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
    stats.load();
    
    BookEntry book;
    if (!getBook(index, book)) {
        showErrorScreen("Failed to load book");
        state = ViewState::BROWSER;
        return;
    }
    
    snprintf(currentBookPath, sizeof(currentBookPath), "%s/%s", currentPath, book.filename);
    strncpy(currentBook, book.title, sizeof(currentBook) - 1);
    currentBookHash = book.cacheHash;  // Use stored hash (computed from full filename before truncation)
    
    // Build and store the preprocessed directory path - this is used for chapter loading
    snprintf(bookCacheDir, sizeof(bookCacheDir), "/.sumi/books/%08x", book.cacheHash);
    
    Serial.printf("[LIBRARY] Book: %s\n", currentBook);
    Serial.printf("[LIBRARY] Path: %s\n", currentBookPath);
    Serial.printf("[LIBRARY] Cache dir: %s (hash=%08x)\n", bookCacheDir, currentBookHash);
    
    isEpub = (book.bookType == BookType::EPUB_FOLDER || book.bookType == BookType::EPUB_FILE);
    
    if (isEpub) {
        // Load from pre-processed cache (required)
        // Use stored cacheHash to avoid issues with truncated filenames
        if (!openPreprocessedMetadata(book.cacheHash)) {
            Serial.println("[LIBRARY] Book not preprocessed - upload via portal first");
            showErrorScreen("Process this book\nin the portal first");
            state = ViewState::BROWSER;
            return;
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
    
    // Use synchronous loading for reliability
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
        }
        chapToLoad++;
    }
    
    if (!cacheValid) {
        showErrorScreen("No readable content");
        state = ViewState::BROWSER;
        return;
    }
    
    // Clamp page
    if (currentPage >= totalPages) currentPage = totalPages - 1;
    if (currentPage < 0) currentPage = 0;
    
    pendingChapterLoad = false;
    bookIsOpen = true;
    firstRenderAfterOpen = true;
    state = ViewState::READING;
    
    // Start preloading next page
    preloadNextPage();
    
    Serial.printf("[LIBRARY] Book open: ch=%d pg=%d/%d (heap=%d)\n",
                  currentChapter, currentPage, totalPages, ESP.getFreeHeap());
    MEM_LOG("openBook_done");
}

void LibraryApp::openTxtMetadata(const char* path) {
    totalChapters = 1;
    currentChapter = 0;
    chapterTitleCount = 1;
    
    const char* filename = path;
    const char* lastSlash = strrchr(path, '/');
    if (lastSlash) filename = lastSlash + 1;
    
    String title = String(filename);
    title.replace(".txt", "");
    title.replace("_", " ");
    strncpy(chapterTitles[0].title, title.c_str(), sizeof(ChapterTitle::title) - 1);
    strncpy(chapterTitle, chapterTitles[0].title, sizeof(chapterTitle) - 1);
}

bool LibraryApp::openPreprocessedMetadata(uint32_t hash) {
    // Use pre-computed hash from BookEntry to find cache directory
    char cacheDir[64];
    snprintf(cacheDir, sizeof(cacheDir), "/.sumi/books/%08x", hash);
    
    // Store for later use (image loading)
    strncpy(bookCacheDir, cacheDir, sizeof(bookCacheDir) - 1);
    bookCacheDir[sizeof(bookCacheDir) - 1] = '\0';
    
    char metaPath[80];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.json", cacheDir);
    
    if (!SD.exists(metaPath)) {
        Serial.printf("[LIBRARY] No pre-processed cache: %s\n", metaPath);
        return false;
    }
    
    Serial.printf("[LIBRARY] Loading pre-processed: %s\n", cacheDir);
    
    // Load meta.json
    File metaFile = SD.open(metaPath, FILE_READ);
    if (!metaFile) {
        Serial.println("[LIBRARY] Failed to open meta.json");
        return false;
    }
    
    String json = metaFile.readString();
    metaFile.close();
    
    Serial.printf("[LIBRARY] meta.json length: %d\n", json.length());
    
    // Parse chapter count - portal outputs "totalChapters"
    int chaptersStart = json.indexOf("\"totalChapters\":");
    Serial.printf("[LIBRARY] totalChapters pos: %d\n", chaptersStart);
    if (chaptersStart < 0) {
        // Fallback for legacy format
        chaptersStart = json.indexOf("\"chapters\":");
        Serial.printf("[LIBRARY] chapters pos: %d\n", chaptersStart);
    }
    if (chaptersStart < 0) {
        Serial.println("[LIBRARY] No chapters key in meta.json");
        Serial.printf("[LIBRARY] JSON preview: %.200s\n", json.c_str());
        return false;
    }
    
    int chaptersValue = json.indexOf(":", chaptersStart);
    int chaptersEnd = json.indexOf(",", chaptersValue);
    if (chaptersEnd < 0) chaptersEnd = json.indexOf("}", chaptersValue);
    
    String chaptersStr = json.substring(chaptersValue + 1, chaptersEnd);
    chaptersStr.trim();
    Serial.printf("[LIBRARY] Parsed chapters string: '%s'\n", chaptersStr.c_str());
    totalChapters = chaptersStr.toInt();
    
    if (totalChapters <= 0 || totalChapters > 500) {
        Serial.printf("[LIBRARY] Invalid chapter count: %d\n", totalChapters);
        return false;
    }
    
    Serial.printf("[LIBRARY] Pre-processed book: %d chapters\n", totalChapters);
    
    // Load chapter titles from toc.json
    char tocPath[80];
    snprintf(tocPath, sizeof(tocPath), "%s/toc.json", cacheDir);
    
    chapterTitleCount = 0;
    if (SD.exists(tocPath)) {
        File tocFile = SD.open(tocPath, FILE_READ);
        if (tocFile) {
            String tocJson = tocFile.readString();
            tocFile.close();
            
            // Parse TOC array - format: [{"title":"...","chapter":0},...]
            int pos = 0;
            while (chapterTitleCount < MAX_CHAPTER_TITLES && chapterTitleCount < totalChapters) {
                int titlePos = tocJson.indexOf("\"title\":", pos);
                if (titlePos < 0) break;
                
                int valueStart = tocJson.indexOf("\"", titlePos + 8);
                int valueEnd = tocJson.indexOf("\"", valueStart + 1);
                if (valueStart < 0 || valueEnd < 0) break;
                
                String title = tocJson.substring(valueStart + 1, valueEnd);
                // Unescape basic JSON escapes
                title.replace("\\\"", "\"");
                title.replace("\\n", " ");
                
                strncpy(chapterTitles[chapterTitleCount++].title, title.c_str(), sizeof(ChapterTitle::title) - 1);
                pos = valueEnd + 1;
            }
        }
    }
    
    // Fill remaining with generic titles
    for (int i = chapterTitleCount; i < totalChapters && i < MAX_CHAPTER_TITLES; i++) {
        snprintf(chapterTitles[i].title, sizeof(ChapterTitle::title), "Chapter %d", i + 1);
        chapterTitleCount++;
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
    
    pageCache->close();
    
    // Clear preloaded page
    _preloadedChapter = -1;
    _preloadedPageNum = -1;
    
    bookIsOpen = false;
    cacheValid = false;
    currentPage = 0;
    totalPages = 0;
    
    MEM_LOG("closeBook_done");
}

// =============================================================================
// Page Preloading - Read next/prev page in background for instant turns
// =============================================================================
void LibraryApp::preloadNextPage() {
    if (!pageCache || !cacheValid) return;
    
    int nextPage = currentPage + 1;
    int nextChapter = currentChapter;
    
    // Wrap to next chapter if needed
    if (nextPage >= totalPages && nextChapter + 1 < totalChapters) {
        nextPage = 0;
        nextChapter++;
    }
    
    // Don't preload if we're at the end
    if (nextChapter >= totalChapters || (nextChapter == currentChapter && nextPage >= totalPages)) {
        _preloadedChapter = -1;
        _preloadedPageNum = -1;
        return;
    }
    
    // Check if already preloaded
    if (_preloadedChapter == nextChapter && _preloadedPageNum == nextPage) {
        return;
    }
    
    // Load the page
    if (pageCache->loadPage(nextChapter, nextPage, _preloadedPageData)) {
        _preloadedChapter = nextChapter;
        _preloadedPageNum = nextPage;
        Serial.printf("[PRELOAD] Loaded ch%d pg%d\n", nextChapter, nextPage);
    }
}

void LibraryApp::preloadPrevPage() {
    if (!pageCache || !cacheValid) return;
    
    int prevPage = currentPage - 1;
    int prevChapter = currentChapter;
    
    // Wrap to previous chapter if needed
    if (prevPage < 0 && prevChapter > 0) {
        prevChapter--;
        // Get page count for previous chapter
        int prevChapterPages = pageCache->getPageCount(prevChapter);
        prevPage = prevChapterPages > 0 ? prevChapterPages - 1 : 0;
    }
    
    // Don't preload if we're at the beginning
    if (prevChapter < 0 || (prevChapter == currentChapter && prevPage < 0)) {
        _preloadedChapter = -1;
        _preloadedPageNum = -1;
        return;
    }
    
    // Check if already preloaded
    if (_preloadedChapter == prevChapter && _preloadedPageNum == prevPage) {
        return;
    }
    
    // Load the page
    if (pageCache->loadPage(prevChapter, prevPage, _preloadedPageData)) {
        _preloadedChapter = prevChapter;
        _preloadedPageNum = prevPage;
        Serial.printf("[PRELOAD] Loaded ch%d pg%d\n", prevChapter, prevPage);
    }
}

bool LibraryApp::usePreloadedPage(int chapter, int page, CachedPage& outPage) {
    if (_preloadedChapter == chapter && _preloadedPageNum == page) {
        // Copy preloaded data
        memcpy(&outPage, &_preloadedPageData, sizeof(CachedPage));
        Serial.printf("[PRELOAD] Using cached ch%d pg%d\n", chapter, page);
        return true;
    }
    return false;
}

// =============================================================================
// Quick Update from Portal (without full directory scan)
// =============================================================================
void LibraryApp::updateBooksFromPortal() {
    Serial.println("[LIBRARY] Quick update from portal cache...");
    
    // Just update covers and metadata from portal without rescanning
    for (int i = 0; i < bookCount; i++) {
        BookEntry book;
        if (!getBook(i, book)) continue;
        if (book.bookType != BookType::EPUB_FILE) continue;
        
        char fullPath[192];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, book.filename);
        
        // Check for portal cover
        String cachePath = getCoverCachePath(fullPath, false);
        if (isValidCoverFile(cachePath.c_str())) {
            if (!book.hasCover || strcmp(book.coverPath, cachePath.c_str()) != 0) {
                strncpy(book.coverPath, cachePath.c_str(), sizeof(book.coverPath) - 1);
                book.hasCover = true;
                updateBook(i, book);
            }
        }
        
        // Check for updated metadata from portal
        const char* filename = book.filename;
        uint32_t hash = 0;
        for (const char* p = filename; *p; p++) {
            hash = ((hash << 5) - hash) + (uint8_t)*p;
        }
        
        char metaPath[80];
        snprintf(metaPath, sizeof(metaPath), "/.sumi/books/%08x/meta.json", hash);
        
        if (SD.exists(metaPath) && book.totalChapters == 0) {
            // Portal has processed this book - reload metadata
            loadBookMetadata(book, fullPath);
            updateBook(i, book);
        }
        
        yield();
    }
    
    Serial.println("[LIBRARY] Quick update complete");
}

#endif // FEATURE_READER
