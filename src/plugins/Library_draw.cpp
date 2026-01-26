/**
 * @file Library_draw.cpp
 * @brief Library drawing methods
 * @version 1.4.2
 */

#include "plugins/Library.h"

#if FEATURE_READER

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// =============================================================================
// Static JPG Callback Variables
// =============================================================================
int LibraryApp::_coverOffsetX = 0;
int LibraryApp::_coverOffsetY = 0;
int LibraryApp::_coverMaxX = 0;
int LibraryApp::_coverMaxY = 0;
int LibraryApp::_jpgCallbackCount = 0;
float LibraryApp::_coverScale = 1.0f;

// Dither matrix for better grayscale rendering
static const uint8_t _libDither[4][4] = {
    {   0, 128,  32, 160 },
    { 192,  64, 224,  96 },
    {  48, 176,  16, 144 },
    { 240, 112, 208,  80 }
};

// JPG decode callback - draws to display with software scaling
bool LibraryApp::_jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    float scale = _coverScale;
    
    // Bayer 4x4 dithering matrix (values 0-255)
    static const uint8_t bayer[4][4] = {
        {  15, 135,  45, 165 },
        { 195,  75, 225, 105 },
        {  60, 180,  30, 150 },
        { 240, 120, 210,  90 }
    };
    
    // Draw the image block with scaling
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            // Convert RGB565 to grayscale first
            uint16_t color = bitmap[j * w + i];
            uint8_t r = ((color >> 11) & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t b = (color & 0x1F) << 3;
            uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
            
            // Calculate scaled output rectangle for this source pixel
            int px1 = _coverOffsetX + (int)((x + i) * scale);
            int py1 = _coverOffsetY + (int)((y + j) * scale);
            int px2 = _coverOffsetX + (int)((x + i + 1) * scale);
            int py2 = _coverOffsetY + (int)((y + j + 1) * scale);
            
            // Ensure at least 1 pixel is drawn
            if (px2 <= px1) px2 = px1 + 1;
            if (py2 <= py1) py2 = py1 + 1;
            
            // Fill the entire scaled pixel area
            for (int fy = py1; fy < py2 && fy < _coverMaxY; fy++) {
                if (fy < _coverOffsetY) continue;
                for (int fx = px1; fx < px2 && fx < _coverMaxX; fx++) {
                    if (fx < _coverOffsetX) continue;
                    
                    // Use position-based dithering for smooth gradients
                    uint8_t threshold = bayer[fy & 3][fx & 3];
                    bool isWhite = (gray > threshold);
                    display.drawPixel(fx, fy, isWhite ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
        }
    }
    
    _jpgCallbackCount++;
    return true;  // Continue decoding
}

// =============================================================================
// Main Draw Dispatcher
// =============================================================================
void LibraryApp::draw() {
    // Guard against null subsystems
    if (!textLayout || !pageCache) {
        showErrorScreen("Memory Error");
        return;
    }
    
    switch (state) {
        case ViewState::BROWSER:
            useFlipBrowser ? drawFlipBrowser() : drawListBrowser();
            break;
        case ViewState::BROWSER_LIST:
            drawListBrowser();
            break;
        case ViewState::READING:
            // If no render task, handle chapter loading synchronously
            if (!renderTaskHandle && pendingChapterLoad) {
                showLoadingScreen("Loading chapter...");
                int chapToLoad = pendingChapterToLoad;
                pendingChapterLoad = false;
                
                // Try to load chapter, skip empty ones
                while (chapToLoad < totalChapters) {
                    if (loadChapterSync(chapToLoad)) {
                        if (totalPages > 0) {
                            currentChapter = chapToLoad;
                            if (currentPage < 0) currentPage = totalPages - 1;
                            if (currentPage >= totalPages) currentPage = 0;
                            cacheValid = true;
                            break;
                        }
                    }
                    chapToLoad++;
                }
            }
            
            if (!cacheValid) {
                // With render task: wait for it
                if (renderTaskHandle) {
                    showLoadingScreen("Loading chapter...");
                    int waitCount = 0;
                    while (!cacheValid && waitCount < 200) {
                        delay(50);
                        waitCount++;
                    }
                }
                
                if (!cacheValid) {
                    showErrorScreen("Loading failed");
                    state = ViewState::BROWSER;
                    return;
                }
            }
            renderCurrentPage();
            break;
        case ViewState::CHAPTER_SELECT:
            drawChapterSelect();
            break;
        case ViewState::SETTINGS_MENU:
            drawSettingsMenu();
            break;
        case ViewState::BOOKMARK_SELECT:
            drawBookmarkSelect();
            break;
        case ViewState::READING_STATS:
            drawReadingStatsScreen();
            break;
        case ViewState::INDEXING:
            drawIndexingScreen();
            break;
        case ViewState::INFO:
            drawInfo();
            break;
    }
}

void LibraryApp::drawBrowser() {
    useFlipBrowser ? drawFlipBrowser() : drawListBrowser();
}

// =============================================================================
// Flip Browser (Cover View)
// =============================================================================
void LibraryApp::drawFlipBrowser() {
    // Debug only on first page
    static bool debuggedThisDraw = false;
    debuggedThisDraw = false;
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        if (bookCount == 0) {
            display.setTextColor(GxEPD_BLACK);
            display.setFont(&FreeSans12pt7b);
            display.setCursor(screenW/2 - 80, screenH/2);
            display.print("No books found");
            display.setFont(&FreeSans9pt7b);
            display.setCursor(screenW/2 - 100, screenH/2 + 30);
            display.print("Add .epub files to /books");
            continue;
        }
        
        // Get current book
        BookEntry book;
        if (!getBook(cursor, book)) continue;
        
        // Debug cover info - only once per draw
        if (!debuggedThisDraw) {
            Serial.printf("[FLIP] Book: %s, hasCover=%d\n", book.title, book.hasCover);
            debuggedThisDraw = true;
        }
        
        // Draw cover - LARGE, taking most of screen
        // Portrait: 480x800 -> cover 320x480 centered
        // Landscape: 800x480 -> cover 280x420 on left
        int coverW, coverH, coverX, coverY;
        int infoY;
        
        if (landscape) {
            coverW = 280;
            coverH = 420;
            coverX = 40;
            coverY = 30;
            infoY = 50;  // Info goes on right side
        } else {
            // Portrait - big cover taking most of screen
            coverW = screenW - 80;  // 400px on 480 wide screen
            coverH = screenH - 220;  // Leave room for info below
            coverX = (screenW - coverW) / 2;
            coverY = 20;
            infoY = coverY + coverH + 20;
        }
        
        if (book.hasCover && strlen(book.coverPath) > 0 && SD.exists(book.coverPath)) {
            drawCoverImage(book.coverPath, coverX, coverY, coverW, coverH);
        } else {
            drawCoverPlaceholder(coverX, coverY, coverW, coverH, book.title);
        }
        
        // Draw border around cover
        display.drawRect(coverX - 2, coverY - 2, coverW + 4, coverH + 4, GxEPD_BLACK);
        
        // Draw info below cover (portrait) or to the right (landscape)
        display.setTextColor(GxEPD_BLACK);
        int16_t x1, y1;
        uint16_t w, h;
        
        int infoX = landscape ? (coverX + coverW + 40) : 0;
        int infoW = landscape ? (screenW - infoX - 40) : screenW;
        
        // Title
        display.setFont(&FreeSansBold12pt7b);
        String title = book.title;
        display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
        int maxTitleW = infoW - 40;
        while ((int)w > maxTitleW && title.length() > 3) {
            title = title.substring(0, title.length() - 4) + "...";
            display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
        }
        if (landscape) {
            display.setCursor(infoX, infoY);
        } else {
            display.setCursor((screenW - w) / 2, infoY);
        }
        display.print(title);
        infoY += 25;
        
        // Author
        if (strlen(book.author) > 0) {
            display.setFont(&FreeSans9pt7b);
            String author = book.author;
            display.getTextBounds(author, 0, 0, &x1, &y1, &w, &h);
            if ((int)w > maxTitleW) {
                author = author.substring(0, maxTitleW / 8) + "...";
                display.getTextBounds(author, 0, 0, &x1, &y1, &w, &h);
            }
            if (landscape) {
                display.setCursor(infoX, infoY);
            } else {
                display.setCursor((screenW - w) / 2, infoY);
            }
            display.print(author);
            infoY += 20;
        }
        
        // File size
        display.setFont(&FreeSans9pt7b);
        char sizeStr[32];
        if (book.size >= 1048576) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f MB", book.size / 1048576.0f);
        } else {
            snprintf(sizeStr, sizeof(sizeStr), "%d KB", book.size / 1024);
        }
        display.getTextBounds(sizeStr, 0, 0, &x1, &y1, &w, &h);
        if (landscape) {
            display.setCursor(infoX, infoY);
        } else {
            display.setCursor((screenW - w) / 2, infoY);
        }
        display.print(sizeStr);
        
        // Progress bar if started
        if (book.progress > 0.01f) {
            int barW = landscape ? (infoW - 40) : 300;
            int barH = 8;
            int barX = landscape ? infoX : ((screenW - barW) / 2);
            int barY = landscape ? (infoY + 30) : (screenH - 55);
            
            display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
            int fillW = (int)(barW * book.progress);
            display.fillRect(barX, barY, fillW, barH, GxEPD_BLACK);
            
            char pct[16];
            snprintf(pct, sizeof(pct), "%d%%", (int)(book.progress * 100));
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(pct, 0, 0, &x1, &y1, &w, &h);
            display.setCursor(barX + barW + 10, barY + 8);
            display.print(pct);
        }
        
        // Navigation arrows - on sides of cover
        if (cursor > 0) {
            int arrowY = coverY + coverH/2;
            display.fillTriangle(10, arrowY, 30, arrowY - 25, 30, arrowY + 25, GxEPD_BLACK);
        }
        if (cursor < bookCount - 1) {
            int arrowY = coverY + coverH/2;
            int arrowX = landscape ? (coverX + coverW + 15) : (screenW - 10);
            display.fillTriangle(arrowX, arrowY, arrowX - 20, arrowY - 25, arrowX - 20, arrowY + 25, GxEPD_BLACK);
        }
        
        // Book counter at bottom
        char counter[32];
        snprintf(counter, sizeof(counter), "%d / %d", cursor + 1, bookCount);
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds(counter, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((screenW - w) / 2, screenH - 10);
        display.print(counter);
        
    } while (display.nextPage());
}

// =============================================================================
// Cover Drawing
// =============================================================================
void LibraryApp::drawCoverImage(const char* path, int x, int y, int maxW, int maxH) {
    if (!SD.exists(path)) {
        drawCoverPlaceholder(x, y, maxW, maxH, "No Cover");
        return;
    }
    
    // Get JPEG dimensions first
    uint16_t jpgW = 0, jpgH = 0;
    JRESULT info = TJpgDec.getFsJpgSize(&jpgW, &jpgH, path, SD);
    if (info != JDR_OK || jpgW == 0 || jpgH == 0) {
        Serial.printf("[COVER] Failed to get size: %d\n", info);
        drawCoverPlaceholder(x, y, maxW, maxH, "Error");
        return;
    }
    
    // Calculate scale to fit within maxW x maxH while maintaining aspect ratio
    float scaleW = (float)maxW / jpgW;
    float scaleH = (float)maxH / jpgH;
    float scale = (scaleW < scaleH) ? scaleW : scaleH;  // Use smaller to fit
    
    // Calculate final dimensions
    int finalW = (int)(jpgW * scale);
    int finalH = (int)(jpgH * scale);
    
    // Center the cover in the target area
    int centerX = x + (maxW - finalW) / 2;
    int centerY = y + (maxH - finalH) / 2;
    
    Serial.printf("[COVER] %dx%d -> %dx%d (scale=%.2f) at (%d,%d)\n", 
                  jpgW, jpgH, finalW, finalH, scale, centerX, centerY);
    
    // Set up clipping and scaling for callback
    _coverOffsetX = centerX;
    _coverOffsetY = centerY;
    _coverMaxX = centerX + finalW;
    _coverMaxY = centerY + finalH;
    _coverScale = scale;
    _jpgCallbackCount = 0;
    
    // Decode at native size, callback handles scaling
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(_jpgDrawCallback);
    
    // Draw at 0,0 - callback offsets to correct position
    JRESULT result = TJpgDec.drawSdJpg(0, 0, path);
    
    if (result != JDR_OK) {
        Serial.printf("[COVER] Decode failed: %d\n", result);
        drawCoverPlaceholder(x, y, maxW, maxH, "Error");
    }
}

void LibraryApp::drawCoverPlaceholder(int x, int y, int maxW, int maxH, const char* label) {
    // Draw frame
    display.drawRect(x, y, maxW, maxH, GxEPD_BLACK);
    
    // Fill with pattern
    for (int py = y + 2; py < y + maxH - 2; py += 4) {
        for (int px = x + 2; px < x + maxW - 2; px += 4) {
            if ((px + py) % 8 == 0) {
                display.drawPixel(px, py, GxEPD_BLACK);
            }
        }
    }
    
    // Draw label
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    int16_t x1, y1;
    uint16_t w, h;
    
    // Word wrap the label
    String text = label;
    int lineY = y + maxH / 2 - 20;
    int maxLineW = maxW - 20;
    
    while (text.length() > 0 && lineY < y + maxH - 20) {
        String line = text;
        display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
        
        while (w > maxLineW && line.length() > 3) {
            int space = line.lastIndexOf(' ');
            if (space > 0) {
                line = line.substring(0, space);
            } else {
                line = line.substring(0, line.length() - 1);
            }
            display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
        }
        
        display.setCursor(x + (maxW - w) / 2, lineY);
        display.print(line);
        
        if (line.length() < text.length()) {
            text = text.substring(line.length());
            text.trim();
        } else {
            text = "";
        }
        
        lineY += 20;
    }
}

// =============================================================================
// List Browser
// =============================================================================
void LibraryApp::drawListBrowser() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Header
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Library");
        
        // Path
        display.setFont(&FreeSans9pt7b);
        display.setCursor(screenW - 150, 35);
        display.print(currentPath);
        
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        if (bookCount == 0) {
            display.setFont(&FreeSans12pt7b);
            display.setCursor(screenW/2 - 80, screenH/2);
            display.print("No books found");
            continue;
        }
        
        // Draw list items
        int y = 70;
        int itemH = 50;
        
        for (int i = scrollOffset; i < min(scrollOffset + itemsPerPage, bookCount); i++) {
            BookEntry book;
            if (!getBook(i, book)) continue;
            
            bool selected = (i == cursor);
            
            if (selected) {
                display.fillRect(0, y - 5, screenW, itemH, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Icon
            display.setFont(&FreeSans9pt7b);
            if (book.isDirectory) {
                display.setCursor(20, y + 20);
                display.print("[DIR]");
            } else if (book.bookType == BookType::EPUB_FILE || book.bookType == BookType::EPUB_FOLDER) {
                display.setCursor(20, y + 20);
                display.print("[EPUB]");
            } else {
                display.setCursor(20, y + 20);
                display.print("[TXT]");
            }
            
            // Title
            display.setFont(&FreeSans12pt7b);
            display.setCursor(90, y + 22);
            display.print(book.title);
            
            // Progress
            if (book.progress > 0.01f) {
                char pct[16];
                snprintf(pct, sizeof(pct), "%d%%", (int)(book.progress * 100));
                display.setFont(&FreeSans9pt7b);
                display.setCursor(screenW - 60, y + 20);
                display.print(pct);
            }
            
            y += itemH;
        }
        
        // Scroll indicator
        if (bookCount > itemsPerPage) {
            int barH = (screenH - 70) * itemsPerPage / bookCount;
            int barY = 70 + (screenH - 70 - barH) * scrollOffset / (bookCount - itemsPerPage);
            display.fillRect(screenW - 5, barY, 4, barH, GxEPD_BLACK);
        }
        
    } while (display.nextPage());
}

// =============================================================================
// Reading Page
// =============================================================================
void LibraryApp::drawReadingPage() {
    display.setTextColor(GxEPD_BLACK);
    
    // Load current page
    CachedPage page;
    if (!pageCache->loadPage(currentChapter, currentPage, page)) {
        display.setFont(&FreeSans12pt7b);
        display.setCursor(screenW/2 - 80, screenH/2);
        display.print("Page unavailable");
        return;
    }
    
    // Draw page content
    const GFXfont* font = getReaderFont();
    display.setFont(font);
    
    for (int i = 0; i < page.lineCount; i++) {
        CachedLine& line = page.lines[i];
        
        // Skip image lines for now
        if (line.isImage()) continue;
        
        // Draw each word in the line
        for (int w = 0; w < line.wordCount; w++) {
            CachedWord& word = line.words[w];
            display.setCursor(word.xPos, line.yPos);
            display.print(word.text);
        }
    }
    
    // Draw status bar
    drawStatusBarInPage();
}

void LibraryApp::drawStatusBarInPage() {
    int barY = screenH - 30;
    
    display.drawLine(0, barY, screenW, barY, GxEPD_BLACK);
    
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Chapter title (left)
    display.setCursor(10, barY + 20);
    String chapTitle = chapterTitle;
    if (chapTitle.length() > 30) chapTitle = chapTitle.substring(0, 27) + "...";
    display.print(chapTitle);
    
    // Page number (center)
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "%d / %d", currentPage + 1, totalPages);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(pageStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((screenW - w) / 2, barY + 20);
    display.print(pageStr);
    
    // Progress (right)
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", (int)(getReadingProgress() * 100));
    display.getTextBounds(pct, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(screenW - w - 10, barY + 20);
    display.print(pct);
}

// =============================================================================
// Chapter Select
// =============================================================================
void LibraryApp::drawChapterSelect() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Header
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Chapters");
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        // Draw chapter list
        int y = 70;
        int itemH = 40;
        int maxVisible = (screenH - 70) / itemH;
        int displayCount = min(chapterTitleCount, totalChapters);
        
        for (int i = chapterScrollOffset; i < min(chapterScrollOffset + maxVisible, displayCount); i++) {
            bool selected = (i == chapterCursor);
            bool isCurrent = (i == currentChapter);
            
            if (selected) {
                display.fillRect(0, y - 5, screenW, itemH, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(isCurrent ? &FreeSansBold12pt7b : &FreeSans12pt7b);
            display.setCursor(20, y + 20);
            
            // Chapter number
            char num[8];
            snprintf(num, sizeof(num), "%d.", i + 1);
            display.print(num);
            
            // Title
            display.setCursor(60, y + 20);
            String title = chapterTitles[i].title;
            if (title.length() > 50) title = title.substring(0, 47) + "...";
            display.print(title);
            
            if (isCurrent) {
                display.setCursor(screenW - 50, y + 20);
                display.print("*");
            }
            
            y += itemH;
        }
        
        // Scroll indicator
        if (displayCount > maxVisible) {
            int barH = (screenH - 70) * maxVisible / displayCount;
            int barY = 70 + (screenH - 70 - barH) * chapterScrollOffset / (displayCount - maxVisible);
            display.fillRect(screenW - 5, barY, 4, barH, GxEPD_BLACK);
        }
        
    } while (display.nextPage());
}

// =============================================================================
// Settings Menu
// =============================================================================
void LibraryApp::drawSettingsMenu() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Header
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Reader Settings");
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        LibReaderSettings& settings = readerSettings.get();
        
        // Build refresh frequency string
        char refreshStr[16];
        snprintf(refreshStr, sizeof(refreshStr), "%d pages", settings.refreshFrequency);
        
        const char* items[] = {
            "Orientation",
            "Font Size",
            "Margins",
            "Line Spacing",
            "Justify Text",
            "Full Refresh",
            "Chapters...",
            "Bookmarks...",
            "Add Bookmark",
            "Reading Stats",
            "Clear Cache",
            "Back"
        };
        
        const char* values[] = {
            landscape ? "Landscape" : "Portrait",
            LibReaderSettings::getFontSizeName(settings.fontSize),
            "", // Margin will be set below
            LibReaderSettings::getLineSpacingName(settings.lineSpacing),
            settings.justifyText() ? "On" : "Off",
            refreshStr,
            "", "", "", "", "", ""
        };
        
        // Set margin value string
        static char marginStr[8];
        snprintf(marginStr, sizeof(marginStr), "%dpx", settings.screenMargin);
        values[2] = marginStr;
        
        int y = 70;
        int itemH = 35;
        
        for (int i = 0; i < (int)SettingsItem::COUNT; i++) {
            bool selected = (i == settingsCursor);
            
            if (selected) {
                display.fillRect(0, y - 5, screenW, itemH, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSans12pt7b);
            display.setCursor(20, y + 20);
            display.print(items[i]);
            
            if (strlen(values[i]) > 0) {
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(values[i], 0, 0, &x1, &y1, &w, &h);
                display.setCursor(screenW - w - 20, y + 20);
                display.print(values[i]);
            }
            
            y += itemH;
        }
        
    } while (display.nextPage());
}

// =============================================================================
// Bookmark Select
// =============================================================================
void LibraryApp::drawBookmarkSelect() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Header
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Bookmarks");
        display.setFont(&FreeSans9pt7b);
        display.setCursor(screenW - 150, 35);
        display.print("LEFT=Delete");
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        if (bookmarks.count == 0) {
            display.setFont(&FreeSans12pt7b);
            display.setCursor(screenW/2 - 80, screenH/2);
            display.print("No bookmarks");
            continue;
        }
        
        int y = 70;
        int itemH = 40;
        int maxVisible = (screenH - 70) / itemH;
        
        for (int i = bookmarkScrollOffset; i < min(bookmarkScrollOffset + maxVisible, bookmarks.count); i++) {
            bool selected = (i == bookmarkCursor);
            Bookmark& bm = bookmarks.bookmarks[i];
            
            if (selected) {
                display.fillRect(0, y - 5, screenW, itemH, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSans12pt7b);
            display.setCursor(20, y + 20);
            display.print(bm.label);
            
            // Location
            char loc[32];
            snprintf(loc, sizeof(loc), "Ch%d Pg%d", bm.chapter + 1, bm.page + 1);
            display.setFont(&FreeSans9pt7b);
            display.setCursor(screenW - 100, y + 20);
            display.print(loc);
            
            y += itemH;
        }
        
    } while (display.nextPage());
}

// =============================================================================
// Reading Stats
// =============================================================================
void LibraryApp::drawReadingStatsScreen() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Reading Statistics");
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        display.setFont(&FreeSans12pt7b);
        int y = 90;
        int lineH = 40;
        
        // Total pages
        display.setCursor(40, y);
        display.print("Total pages read:");
        display.setCursor(screenW - 100, y);
        display.print(stats.totalPagesRead);
        y += lineH;
        
        // Total time
        display.setCursor(40, y);
        display.print("Total reading time:");
        int hours = stats.totalMinutesRead / 60;
        int mins = stats.totalMinutesRead % 60;
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "%dh %dm", hours, mins);
        display.setCursor(screenW - 100, y);
        display.print(timeStr);
        y += lineH;
        
        // Books finished
        display.setCursor(40, y);
        display.print("Books finished:");
        display.setCursor(screenW - 100, y);
        display.print(stats.booksFinished);
        y += lineH;
        
        // Session
        y += 20;
        display.drawLine(40, y, screenW - 40, y, GxEPD_BLACK);
        y += 30;
        
        display.setCursor(40, y);
        display.print("This session:");
        y += lineH;
        
        display.setFont(&FreeSans9pt7b);
        display.setCursor(60, y);
        char sessStr[64];
        snprintf(sessStr, sizeof(sessStr), "%d pages, %d minutes", 
                 stats.sessionPagesRead, stats.getSessionMinutes());
        display.print(sessStr);
        
    } while (display.nextPage());
}

// =============================================================================
// Indexing Screen
// =============================================================================
void LibraryApp::drawIndexingScreen() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans12pt7b);
        
        display.setCursor(screenW/2 - 80, screenH/2 - 20);
        display.print("Scanning...");
        
        display.setFont(&FreeSans9pt7b);
        char countStr[32];
        snprintf(countStr, sizeof(countStr), "Found %d books", indexingProgress);
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(countStr, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((screenW - w) / 2, screenH/2 + 20);
        display.print(countStr);
        
    } while (display.nextPage());
}

// =============================================================================
// Info Screen
// =============================================================================
void LibraryApp::drawInfo() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.print("Book Info");
        display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
        
        display.setFont(&FreeSans12pt7b);
        display.setCursor(20, 90);
        display.print("Title: ");
        display.print(currentBook);
        
        if (isEpub && epub.isOpen()) {
            display.setCursor(20, 130);
            display.print("Author: ");
            display.print(epub.getAuthor());
            
            display.setCursor(20, 170);
            display.print("Chapters: ");
            display.print(totalChapters);
        }
        
        display.setCursor(20, 210);
        display.print("Current: Ch");
        display.print(currentChapter + 1);
        display.print(" Pg");
        display.print(currentPage + 1);
        display.print("/");
        display.print(totalPages);
        
    } while (display.nextPage());
}

// =============================================================================
// Sleep Cover
// =============================================================================
void LibraryApp::drawSleepCover(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& disp, int w, int h) {
    LastBookInfo info;
    if (!getLastBookInfo(info)) {
        // No last book - draw default sleep screen
        disp.setFullWindow();
        disp.firstPage();
        do {
            disp.fillScreen(GxEPD_WHITE);
            disp.setTextColor(GxEPD_BLACK);
            disp.setFont(&FreeSans12pt7b);
            disp.setCursor(w/2 - 40, h/2);
            disp.print("SUMI");
        } while (disp.nextPage());
        return;
    }
    
    disp.setFullWindow();
    disp.firstPage();
    do {
        disp.fillScreen(GxEPD_WHITE);
        
        // Draw cover if available
        if (strlen(info.coverPath) > 0 && SD.exists(info.coverPath)) {
            _coverOffsetX = (w - 200) / 2;
            _coverOffsetY = 50;
            _coverMaxX = _coverOffsetX + 200;
            _coverMaxY = _coverOffsetY + 300;
            _jpgCallbackCount = 0;
            
            TJpgDec.setJpgScale(2); // Scale down for sleep screen
            TJpgDec.setCallback(_jpgDrawCallback);
            
            File f = SD.open(info.coverPath, FILE_READ);
            if (f) {
                size_t sz = f.size();
                if (sz > 0 && sz < 200000) {
                    uint8_t* buf = (uint8_t*)malloc(sz);
                    if (buf) {
                        f.read(buf, sz);
                        TJpgDec.drawJpg(_coverOffsetX, _coverOffsetY, buf, sz);
                        free(buf);
                    }
                }
                f.close();
            }
        }
        
        // Title
        disp.setTextColor(GxEPD_BLACK);
        disp.setFont(&FreeSansBold12pt7b);
        
        int16_t x1, y1;
        uint16_t tw, th;
        disp.getTextBounds(info.title, 0, 0, &x1, &y1, &tw, &th);
        disp.setCursor((w - min((int)tw, w - 40)) / 2, h - 80);
        
        String title = info.title;
        if (tw > w - 40) {
            title = title.substring(0, 25) + "...";
        }
        disp.print(title);
        
        // Progress
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", (int)(info.progress * 100));
        disp.setFont(&FreeSans9pt7b);
        disp.getTextBounds(pct, 0, 0, &x1, &y1, &tw, &th);
        disp.setCursor((w - tw) / 2, h - 50);
        disp.print(pct);
        
    } while (disp.nextPage());
}

#endif // FEATURE_READER
