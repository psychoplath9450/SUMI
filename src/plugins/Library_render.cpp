/**
 * @file Library_render.cpp
 * @brief Library rendering task and chapter loading
 * @version 1.4.2
 */

#include "plugins/Library.h"

#if FEATURE_READER

// =============================================================================
// Render Task
// =============================================================================
void LibraryApp::renderTaskTrampoline(void* param) {
    LibraryApp* app = (LibraryApp*)param;
    app->renderTaskLoop();
}

void LibraryApp::renderTaskLoop() {
    Serial.println("[RENDER] Task started");
    
    while (true) {
        // Check for pending chapter load
        if (pendingChapterLoad) {
            pendingChapterLoad = false;
            int chapter = pendingChapterToLoad;
            
            Serial.printf("[RENDER] Loading chapter %d\n", chapter);
            MEM_LOG("render_load_chapter");
            
            if (xSemaphoreTake(renderMutex, portMAX_DELAY) == pdTRUE) {
                if (loadChapterSync(chapter)) {
                    // If chapter has no pages (e.g., cover page with only an image),
                    // automatically skip to next chapter
                    if (totalPages == 0 && chapter < totalChapters - 1) {
                        Serial.printf("[RENDER] Chapter %d has no text, skipping to next\n", chapter);
                        currentChapter = chapter + 1;
                        pendingChapterLoad = true;
                        pendingChapterToLoad = currentChapter;
                        currentPage = 0;
                        xSemaphoreGive(renderMutex);
                        continue;  // Go back to check pendingChapterLoad
                    }
                    
                    // If currentPage was -1, go to last page
                    if (currentPage < 0) currentPage = totalPages - 1;
                    if (currentPage >= totalPages) currentPage = totalPages - 1;
                    if (currentPage < 0) currentPage = 0;
                    
                    if (chapter < chapterTitleCount) {
                        strncpy(chapterTitle, chapterTitles[chapter].title, sizeof(chapterTitle) - 1);
                    }
                    
                    cacheValid = true;
                    updateRequired = true;
                }
                xSemaphoreGive(renderMutex);
            }
        }
        
        // Check for page update
        if (updateRequired && cacheValid) {
            updateRequired = false;
            
            if (xSemaphoreTake(renderMutex, portMAX_DELAY) == pdTRUE) {
                renderCurrentPage();
                xSemaphoreGive(renderMutex);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// =============================================================================
// Chapter Loading
// =============================================================================

// Check if a pre-processed cache exists for this book
String LibraryApp::getPreprocessedChapterPath(int chapter) {
    // Hash the filename (same algorithm as portal)
    // Extract filename from currentBookPath, not currentBook (which is title)
    const char* filename = strrchr(currentBookPath, '/');
    if (filename) filename++; else filename = currentBookPath;
    
    uint32_t hash = 0;
    for (const char* p = filename; *p; p++) {
        hash = ((hash << 5) - hash) + (uint8_t)*p;
    }
    hash = hash & 0xFFFFFFFF;
    
    char path[96];
    snprintf(path, sizeof(path), "/.sumi/books/%08x/ch_%03d.txt", hash, chapter);
    
    if (SD.exists(path)) {
        return String(path);
    }
    return String();
}

bool LibraryApp::loadChapterSync(int chapter) {
    Serial.printf("[LOAD] Chapter %d (heap=%d)\n", chapter, ESP.getFreeHeap());
    MEM_LOG("loadChapter_start");
    
    if (ESP.getFreeHeap() < 10000) {
        Serial.println("[LOAD] Low heap!");
        return false;
    }
    
    // Update chapter title
    if (isEpub && chapter < chapterTitleCount) {
        strncpy(chapterTitle, chapterTitles[chapter].title, sizeof(chapterTitle) - 1);
    } else if (!isEpub) {
        strncpy(chapterTitle, currentBook, sizeof(chapterTitle) - 1);
    } else {
        snprintf(chapterTitle, sizeof(chapterTitle), "Chapter %d", chapter + 1);
    }
    
    // Build cache key
    LibReaderSettings& settings = readerSettings.get();
    CacheKey key;
    key.fontSize = (uint8_t)settings.fontSize;
    key.margins = settings.screenMargin;
    key.lineSpacing = (uint8_t)settings.lineSpacing;
    key.setJustify(settings.justifyText());
    key.screenWidth = getLayoutWidth();
    key.screenHeight = screenH;
    
    // Check page cache (pagination cache)
    if (pageCache->hasValidCache(key)) {
        int cachedCount = pageCache->getPageCount(chapter);
        if (cachedCount > 0) {
            totalPages = cachedCount;
            cacheValid = true;
            preloadedPage = -1;
            Serial.printf("[LOAD] Cache hit: %d pages\n", totalPages);
            return true;
        }
    }
    
    // Cache miss - need to parse
    Serial.println("[LOAD] Cache miss, parsing...");
    
    // Check for pre-processed plain text first (portal-generated)
    // This is fast, so only show loading screen if we don't have pre-processed
    String preprocessedPath = getPreprocessedChapterPath(chapter);
    bool hasPreprocessed = (preprocessedPath.length() > 0 && SD.exists(preprocessedPath));
    
    if (!hasPreprocessed) {
        // Only show loading screen for slow EPUB parsing path
        showLoadingScreen("Loading chapter...");
    }
    
    textLayout->setPageSize(getLayoutWidth(), screenH);
    applyFontSettings();
    textLayout->reset();
    
    // Set cache path for this chapter
    char cachePath[64];
    snprintf(cachePath, sizeof(cachePath), "/.sumi/ch%d", chapter);
    textLayout->setCachePath(String(cachePath));
    
    bool success = true;
    
    if (hasPreprocessed) {
        // FAST PATH: Load pre-processed rich text with markers
        // Markers: **bold**, *italic*, # header, • bullet, [Image], [Table]
        Serial.printf("[LOAD] Using pre-processed: %s\n", preprocessedPath.c_str());
        
        File file = SD.open(preprocessedPath);
        if (file) {
            const int bufSize = 4096;
            char* buffer = (char*)malloc(bufSize + 1);
            if (buffer) {
                while (file.available()) {
                    int bytesRead = file.readBytes(buffer, bufSize);
                    buffer[bytesRead] = '\0';
                    
                    // Process line by line
                    char* savePtr;
                    char* line = strtok_r(buffer, "\n", &savePtr);
                    while (line) {
                        // Trim trailing whitespace only (preserve leading for markers)
                        size_t len = strlen(line);
                        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\r')) {
                            line[--len] = '\0';
                        }
                        
                        // Skip leading whitespace for non-marker lines
                        char* start = line;
                        if (*start != '#' && (uint8_t)*start != 0xE2) {  // Not header or bullet
                            while (*start == ' ' || *start == '\t') start++;
                            len = strlen(start);
                        }
                        
                        if (len > 0) {
                            textLayout->addRichText(start, len);
                            textLayout->endParagraph();
                        } else {
                            // Empty line = paragraph break
                            textLayout->endParagraph();
                        }
                        
                        line = strtok_r(nullptr, "\n", &savePtr);
                    }
                    
                    yield();
                }
                free(buffer);
            }
            file.close();
        } else {
            success = false;
        }
    } else if (isEpub) {
        // SLOW PATH: Parse from EPUB (no pre-processed cache)
        Serial.println("[LOAD] No pre-processed cache, parsing EPUB...");
        SD.mkdir("/.sumi");
        String tempPath = getTempFilePath(chapter);
        
        // Stream from EPUB to temp file
        bool streamOk = epub.streamChapterToFile(chapter, tempPath);
        if (!streamOk) {
            Serial.println("[LOAD] Stream failed");
            textLayout->addText("Chapter unavailable", 19);
            textLayout->endParagraph();
            success = false;
        } else {
            // Parse with Expat - outputs rich text markers
            bool parseOk = expatParser.parseFile(tempPath, [this](const String& text, bool isHeader) {
                if (text.length() > 0) {
                    // Use addRichText to parse markers (**bold**, *italic*, # header)
                    textLayout->addRichText(text.c_str(), text.length());
                    textLayout->endParagraph();
                }
            });
            
            if (!parseOk) {
                Serial.println("[LOAD] Parse failed");
            }
            
            SD.remove(tempPath.c_str());
        }
    } else {
        // TXT file - may contain rich text markers
        File file = SD.open(currentBookPath);
        if (file) {
            const int bufSize = 8192;
            char* buffer = (char*)malloc(bufSize + 1);
            if (buffer) {
                while (file.available()) {
                    int bytesRead = file.readBytes(buffer, bufSize);
                    buffer[bytesRead] = '\0';
                    
                    // Process line by line
                    char* line = strtok(buffer, "\n");
                    while (line) {
                        // Trim
                        while (*line == ' ' || *line == '\t') line++;
                        char* end = line + strlen(line) - 1;
                        while (end > line && (*end == ' ' || *end == '\t' || *end == '\r')) *end-- = '\0';
                        
                        if (strlen(line) > 0) {
                            // Use addRichText to support markdown-style markers
                            textLayout->addRichText(line, strlen(line));
                            textLayout->endParagraph();
                        }
                        
                        line = strtok(nullptr, "\n");
                    }
                    
                    yield();
                }
                
                free(buffer);
            } else {
                success = false;
            }
            file.close();
        } else {
            success = false;
        }
    }
    
    // Finalize layout
    textLayout->finalize();
    
    totalPages = textLayout->getPageCount();
    pageCache->setPageCount(chapter, totalPages);
    pageCache->saveMeta(key, totalChapters);
    cacheValid = totalPages > 0;
    preloadedPage = -1;
    
    Serial.printf("[LOAD] Parsed: %d pages\n", totalPages);
    MEM_LOG("loadChapter_done");
    
    // Return true if we successfully parsed (even if 0 pages - caller will skip)
    return success;
}

void LibraryApp::loadChapter(int chapter) {
    pendingChapterLoad = true;
    pendingChapterToLoad = chapter;
    cacheValid = false;
}

String LibraryApp::getTempFilePath(int chapter) {
    char path[64];
    snprintf(path, sizeof(path), "/.sumi/ch%d.tmp", chapter);
    return String(path);
}

// =============================================================================
// Page Rendering
// =============================================================================
void LibraryApp::renderCurrentPage() {
    if (!cacheValid || totalPages == 0) return;
    
    // Prevent double-render
    static bool isRendering = false;
    if (isRendering) return;
    isRendering = true;
    
    MEM_LOG("render_start");
    
    // Always use full refresh for now until partial works properly
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        textLayout->renderPage(currentPage, display);
        drawStatusBarInPage();
    } while (display.nextPage());
    
    MEM_LOG("render_done");
    isRendering = false;
}

void LibraryApp::preloadAdjacentPages() {
    // TextLayout handles its own caching, so just track what we've "preloaded"
    if (currentPage + 1 < totalPages && preloadedPage != currentPage + 1) {
        preloadedPage = currentPage + 1;
    }
}

void LibraryApp::clearAllCache() {
    pageCache->invalidateBook();
    cacheValid = false;
}

// =============================================================================
// Loading/Error Screens
// =============================================================================
void LibraryApp::showLoadingScreen(const char* message) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans12pt7b);
        
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((screenW - w) / 2, screenH / 2);
        display.print(message);
    } while (display.nextPage());
}

void LibraryApp::showErrorScreen(const char* message) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans12pt7b);
        
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds("Error", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((screenW - w) / 2, screenH / 2 - 20);
        display.print("Error");
        
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((screenW - w) / 2, screenH / 2 + 20);
        display.print(message);
    } while (display.nextPage());
    
    delay(2000);
}

bool LibraryApp::needsFullRefresh() {
    return pagesUntilFullRefresh <= 0;
}

void LibraryApp::requestFullRefresh() {
    pagesUntilFullRefresh = 0;
}

#endif // FEATURE_READER
