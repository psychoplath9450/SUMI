/**
 * @file Library_render.cpp
 * @brief Library rendering task and chapter loading
 * @version 1.5.0
 * 
 * All EPUB chapters loaded from preprocessed text files.
 * No on-device EPUB/XML parsing.
 * Supports inline image pages from preprocessed EPUBs.
 */

#include "plugins/Library.h"
#include <TJpg_Decoder.h>
#include <Fonts/FreeSans9pt7b.h>

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
    // Use stored bookCacheDir (set in openBook from book.cacheHash)
    // This avoids hash mismatch issues with truncated filenames
    char path[96];
    snprintf(path, sizeof(path), "%s/ch_%03d.txt", bookCacheDir, chapter);
    
    Serial.printf("[LOAD] Looking for: %s\n", path);
    
    if (SD.exists(path)) {
        return String(path);
    }
    Serial.printf("[LOAD] Chapter file not found!\n");
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
        // Load pre-processed rich text with markers
        // Markers: **bold**, *italic*, # header, • bullet, [Image], [Table]
        Serial.printf("[LOAD] Using pre-processed: %s\n", preprocessedPath.c_str());
        
        File file = SD.open(preprocessedPath);
        if (file) {
            const int bufSize = 4096;
            char* buffer = (char*)malloc(bufSize + 1);
            if (buffer) {
                int consecutiveNewlines = 0;  // Track across buffer reads
                
                while (file.available()) {
                    int bytesRead = file.readBytes(buffer, bufSize);
                    buffer[bytesRead] = '\0';
                    
                    // Process character by character to detect paragraph breaks
                    // Double newlines (\n\n) indicate paragraph breaks
                    char* ptr = buffer;
                    char* lineStart = buffer;
                    
                    while (*ptr) {
                        if (*ptr == '\n' || *ptr == '\r') {
                            // End of line - process it
                            *ptr = '\0';  // Terminate the line
                            
                            // Trim trailing whitespace
                            char* lineEnd = ptr - 1;
                            while (lineEnd >= lineStart && (*lineEnd == ' ' || *lineEnd == '\t' || *lineEnd == '\r')) {
                                *lineEnd-- = '\0';
                            }
                            
                            // Skip leading whitespace for non-marker lines
                            char* start = lineStart;
                            if (*start != '#' && (uint8_t)*start != 0xE2) {  // Not header or bullet
                                while (*start == ' ' || *start == '\t') start++;
                            }
                            
                            size_t len = strlen(start);
                            
                            if (len > 0) {
                                // If we had 2+ newlines before this, it's a new paragraph
                                if (consecutiveNewlines >= 2) {
                                    Serial.printf("[LOAD] Para break (%d newlines) -> endParagraph()\n", consecutiveNewlines);
                                    textLayout->endParagraph();
                                }
                                
                                textLayout->addRichText(start, len);
                                textLayout->addText(" ", 1);
                                consecutiveNewlines = 1;  // Reset to 1 (this newline)
                            } else {
                                // Empty line - increment counter
                                consecutiveNewlines++;
                            }
                            
                            // Skip \r\n sequences
                            if (*ptr == '\r' && *(ptr + 1) == '\n') {
                                ptr++;
                            }
                            
                            lineStart = ptr + 1;
                        }
                        ptr++;
                    }
                    
                    // Handle any remaining text (no trailing newline)
                    if (lineStart < ptr && *lineStart) {
                        char* start = lineStart;
                        if (*start != '#' && (uint8_t)*start != 0xE2) {
                            while (*start == ' ' || *start == '\t') start++;
                        }
                        size_t len = strlen(start);
                        if (len > 0) {
                            if (consecutiveNewlines >= 2) {
                                textLayout->endParagraph();
                            }
                            textLayout->addRichText(start, len);
                            textLayout->addText(" ", 1);
                        }
                    }
                    
                    yield();
                }
                
                // End final paragraph
                textLayout->endParagraph();
                
                free(buffer);
            }
            file.close();
        } else {
            success = false;
        }
    } else if (!isEpub) {
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
    } else {
        // Chapter file not found - this shouldn't happen if book is in library
        Serial.printf("[LOAD] ERROR: Chapter file not found! bookCacheDir='%s'\n", bookCacheDir);
        textLayout->addText("Chapter file not found", 22);
        textLayout->endParagraph();
        textLayout->addText("Try reprocessing book", 21);
        textLayout->endParagraph();
        success = false;
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

// =============================================================================
// Page Rendering
// =============================================================================

// Static variables for TJpgDec callback (image page rendering)
static int _imgTargetX = 0;
static int _imgTargetY = 0;
static int _imgMaxW = 0;
static int _imgMaxH = 0;
static GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>* _imgDisplay = nullptr;

// TJpgDec callback for inline image rendering
static bool _inlineImageCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_imgDisplay) return false;
    
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = _imgTargetX + x + i;
            int py = _imgTargetY + y + j;
            
            if (px >= 0 && px < _imgMaxW && py >= 0 && py < _imgMaxH) {
                uint16_t color = bitmap[j * w + i];
                // RGB565 to grayscale with dithering
                uint8_t r = (color >> 11) & 0x1F;
                uint8_t g = (color >> 5) & 0x3F;
                uint8_t b = color & 0x1F;
                uint8_t gray = (r * 8 + g * 4 + b * 8) / 3;  // 0-255
                
                // Bayer 4x4 dithering
                static const uint8_t bayer[4][4] = {
                    {  15, 135,  45, 165 },
                    { 195,  75, 225, 105 },
                    {  60, 180,  30, 150 },
                    { 240, 120, 210,  90 }
                };
                uint8_t threshold = bayer[py % 4][px % 4];
                _imgDisplay->drawPixel(px, py, gray > threshold ? GxEPD_WHITE : GxEPD_BLACK);
            }
        }
    }
    return true;
}

void LibraryApp::renderCurrentPage() {
    if (!cacheValid || totalPages == 0) return;
    
    // Prevent double-render
    static bool isRendering = false;
    if (isRendering) return;
    isRendering = true;
    
    MEM_LOG("render_start");
    
    // Check if this is an image page
    PageType pageType = textLayout->getPageType(currentPage);
    
    if (pageType == PageType::IMAGE) {
        // Render image page
        char imgFilename[IMAGE_PATH_SIZE];
        int imgW, imgH;
        
        if (textLayout->getImageInfo(currentPage, imgFilename, sizeof(imgFilename), &imgW, &imgH)) {
            // Build full path: bookCacheDir/images/img_XXX.jpg
            char imgPath[128];
            snprintf(imgPath, sizeof(imgPath), "%s/images/%s", bookCacheDir, imgFilename);
            
            Serial.printf("[RENDER] Image page %d: %s\n", currentPage, imgPath);
            
            display.setFullWindow();
            display.firstPage();
            do {
                display.fillScreen(GxEPD_WHITE);
                
                if (SD.exists(imgPath)) {
                    // Calculate centered position
                    int centerX = (screenW - imgW) / 2;
                    int centerY = (screenH - imgH) / 2;
                    if (centerX < 0) centerX = 0;
                    if (centerY < 0) centerY = 0;
                    
                    // Setup TJpgDec callback
                    _imgTargetX = centerX;
                    _imgTargetY = centerY;
                    _imgMaxW = screenW;
                    _imgMaxH = screenH;
                    _imgDisplay = &display;
                    
                    TJpgDec.setJpgScale(1);
                    TJpgDec.setCallback(_inlineImageCallback);
                    TJpgDec.drawSdJpg(0, 0, imgPath);
                } else {
                    // Image not found - show placeholder
                    display.setFont(&FreeSans9pt7b);
                    display.setTextColor(GxEPD_BLACK);
                    
                    const char* msg = "[Image not found]";
                    int16_t x1, y1;
                    uint16_t tw, th;
                    display.getTextBounds(msg, 0, 0, &x1, &y1, &tw, &th);
                    display.setCursor((screenW - tw) / 2, screenH / 2);
                    display.print(msg);
                }
                
                // Draw minimal status bar
                drawStatusBarInPage();
            } while (display.nextPage());
        }
    } else {
        // Render text page (normal)
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            textLayout->renderPage(currentPage, display);
            drawStatusBarInPage();
        } while (display.nextPage());
    }
    
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

bool LibraryApp::needsFullRefresh() {
    return pagesUntilFullRefresh <= 0;
}

bool LibraryApp::needsRedraw() {
    return _pendingRedraw;
}

void LibraryApp::requestFullRefresh() {
    pagesUntilFullRefresh = 0;
}

#endif // FEATURE_READER
