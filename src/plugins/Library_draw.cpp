/**
 * @file Library_draw.cpp
 * @brief Library drawing methods - Enhanced UI
 * @version 2.0.0
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

// JPG decode callback - draws to display with software scaling
bool LibraryApp::_jpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    float scale = _coverScale;
    
    // Bayer 4x4 dithering matrix
    static const uint8_t bayer[4][4] = {
        {  15, 135,  45, 165 },
        { 195,  75, 225, 105 },
        {  60, 180,  30, 150 },
        { 240, 120, 210,  90 }
    };
    
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint16_t color = bitmap[j * w + i];
            uint8_t r = ((color >> 11) & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t b = (color & 0x1F) << 3;
            uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
            
            int px1 = _coverOffsetX + (int)((x + i) * scale);
            int py1 = _coverOffsetY + (int)((y + j) * scale);
            int px2 = _coverOffsetX + (int)((x + i + 1) * scale);
            int py2 = _coverOffsetY + (int)((y + j + 1) * scale);
            
            if (px2 <= px1) px2 = px1 + 1;
            if (py2 <= py1) py2 = py1 + 1;
            
            for (int fy = py1; fy < py2 && fy < _coverMaxY; fy++) {
                if (fy < _coverOffsetY) continue;
                for (int fx = px1; fx < px2 && fx < _coverMaxX; fx++) {
                    if (fx < _coverOffsetX) continue;
                    uint8_t threshold = bayer[fy & 3][fx & 3];
                    display.drawPixel(fx, fy, (gray > threshold) ? GxEPD_WHITE : GxEPD_BLACK);
                }
            }
        }
    }
    
    _jpgCallbackCount++;
    return true;
}

// =============================================================================
// UI Helper Functions
// =============================================================================
void LibraryApp::drawHeader(const char* title, const char* subtitle) {
    int headerH = subtitle ? 48 : 40;
    display.fillRect(0, 0, screenW, headerH, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    centerText(title, screenW / 2, subtitle ? 26 : 28);
    if (subtitle) {
        display.setFont(&FreeSans9pt7b);
        centerText(subtitle, screenW / 2, 42);
    }
    display.setTextColor(GxEPD_BLACK);
}

void LibraryApp::drawFooter(const char* text) {
    display.drawFastHLine(0, screenH - 36, screenW, GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);
    centerText(text, screenW / 2, screenH - 12);
}

void LibraryApp::drawToggle(int x, int y, bool enabled) {
    int sw = 44, sh = 24;
    if (enabled) {
        display.fillRoundRect(x, y, sw, sh, sh / 2, GxEPD_BLACK);
        display.fillCircle(x + sw - sh / 2, y + sh / 2, 8, GxEPD_WHITE);
    } else {
        display.drawRoundRect(x, y, sw, sh, sh / 2, GxEPD_BLACK);
        display.fillCircle(x + sh / 2, y + sh / 2, 8, GxEPD_BLACK);
    }
}

void LibraryApp::centerText(const char* text, int x, int y) {
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(x - tw / 2, y);
    display.print(text);
}

void LibraryApp::drawMiniCover(int x, int y, int w, int h, const char* coverPath, const char* title) {
    if (coverPath && strlen(coverPath) > 0 && SD.exists(coverPath)) {
        drawCoverImage(coverPath, x, y, w, h);
    } else {
        display.fillRoundRect(x, y, w, h, 4, GxEPD_WHITE);
        display.drawRoundRect(x, y, w, h, 4, GxEPD_BLACK);
        if (title && strlen(title) > 0) {
            display.setFont(&FreeSansBold9pt7b);
            char letter[2] = { (char)toupper(title[0]), '\0' };
            centerText(letter, x + w/2, y + h/2 + 5);
        }
    }
}

// =============================================================================
// Main Draw Dispatcher
// =============================================================================
void LibraryApp::draw() {
    if (!textLayout || !pageCache) {
        showErrorScreen("Memory Error");
        return;
    }
    
    // Reset the flags
    needsFullRedraw = false;
    _pendingRedraw = false;
    
    switch (state) {
        case ViewState::MAIN_MENU:
            drawMainMenu();
            break;
        case ViewState::BROWSER:
            useFlipBrowser ? drawFlipBrowser() : drawListBrowser();
            break;
        case ViewState::BROWSER_LIST:
            drawListBrowser();
            break;
        case ViewState::READING:
            if (!renderTaskHandle && pendingChapterLoad) {
                showLoadingScreen("Loading chapter...");
                int chapToLoad = pendingChapterToLoad;
                pendingChapterLoad = false;
                
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

void LibraryApp::drawPartial() {
    // For now, just call draw() - individual draw functions handle their own refresh
    draw();
}

void LibraryApp::drawFullScreen() {
    needsFullRedraw = true;
    draw();
}

bool LibraryApp::update() {
    // Library doesn't need continuous updates
    return false;
}

void LibraryApp::drawBrowser() {
    useFlipBrowser ? drawFlipBrowser() : drawListBrowser();
}

// =============================================================================
// Main Menu (NEW)
// =============================================================================
void LibraryApp::drawMainMenu() {
    // Use partial refresh for cursor navigation, full refresh for state changes
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        char subtitle[32];
        snprintf(subtitle, 32, "%d books", bookCount);
        drawHeader("Library", subtitle);
        
        int y = 60;
        
        // Currently Reading Card
        LastBookInfo lastBook;
        bool hasLastBook = getLastBookInfo(lastBook);
        
        if (hasLastBook) {
            display.drawRoundRect(16, y, screenW - 32, 110, 12, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(28, y + 22);
            display.print("Currently Reading");
            
            // Mini cover
            int coverX = 28, coverY = y + 32;
            int coverW = 50, coverH = 70;
            drawMiniCover(coverX, coverY, coverW, coverH, lastBook.coverPath, lastBook.title);
            display.drawRect(coverX - 1, coverY - 1, coverW + 2, coverH + 2, GxEPD_BLACK);
            
            // Book info
            int infoX = coverX + coverW + 16;
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(infoX, y + 48);
            
            char truncTitle[24];
            strncpy(truncTitle, lastBook.title, 23);
            truncTitle[23] = '\0';
            display.print(truncTitle);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(infoX, y + 66);
            display.print(lastBook.author);
            
            // Progress bar
            int barX = infoX, barY = y + 78;
            int barW = screenW - infoX - 40, barH = 6;
            display.drawRoundRect(barX, barY, barW, barH, 3, GxEPD_BLACK);
            int fillW = (barW * (int)(lastBook.progress * 100)) / 100;
            if (fillW > 0) {
                display.fillRoundRect(barX, barY, fillW, barH, 3, GxEPD_BLACK);
            }
            
            char progStr[32];
            snprintf(progStr, 32, "Ch %d - %d%%", lastBook.chapter + 1, (int)(lastBook.progress * 100));
            display.setCursor(infoX, y + 100);
            display.print(progStr);
            
            y += 125;
        } else {
            y += 10;
        }
        
        // Menu items
        const char* labels[4];
        const char* descs[4];
        int numItems;
        
        if (hasLastBook) {
            labels[0] = "Continue Reading"; descs[0] = lastBook.title;
            labels[1] = "Browse Library"; descs[1] = "Select a different book";
            labels[2] = "Reading Statistics"; descs[2] = "View your reading history";
            labels[3] = "Reader Settings"; descs[3] = "Font, margins, display";
            numItems = 4;
        } else {
            labels[0] = "Browse Library"; descs[0] = "Select a book to read";
            labels[1] = "Reading Statistics"; descs[1] = "View your reading history";
            labels[2] = "Reader Settings"; descs[2] = "Font, margins, display";
            numItems = 3;
        }
        
        for (int i = 0; i < numItems; i++) {
            int itemY = y + i * 62;
            bool sel = (mainMenuCursor == i);
            
            if (sel) {
                display.fillRoundRect(16, itemY, screenW - 32, 56, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(16, itemY, screenW - 32, 56, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(32, itemY + 24);
            display.print(labels[i]);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(32, itemY + 44);
            
            // Truncate desc if needed
            char truncDesc[32];
            strncpy(truncDesc, descs[i], 31);
            truncDesc[31] = '\0';
            display.print(truncDesc);
            
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(screenW - 50, itemY + 34);
            display.print(">");
        }
        
        display.setTextColor(GxEPD_BLACK);
        
    } while (display.nextPage());
}

// =============================================================================
// Flip Browser (Cover View) - ENHANCED
// =============================================================================
void LibraryApp::drawFlipBrowser() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        if (bookCount == 0) {
            drawHeader("Library", "0 books");
            display.setTextColor(GxEPD_BLACK);
            display.setFont(&FreeSansBold12pt7b);
            centerText("No books found", screenW/2, screenH/2 - 20);
            display.setFont(&FreeSans9pt7b);
            centerText("Add .epub files to /books", screenW/2, screenH/2 + 20);
            continue;
        }
        
        char subtitle[32];
        snprintf(subtitle, 32, "%d books - Cover View", bookCount);
        drawHeader("Library", subtitle);
        
        BookEntry book;
        if (!getBook(cursor, book)) continue;
        
        // Cover dimensions
        int coverW, coverH, coverX, coverY, infoY;
        
        if (landscape) {
            coverW = 280; coverH = 420;
            coverX = 40; coverY = 60;
            infoY = 60;
        } else {
            coverW = screenW - 80;
            coverH = screenH - 280;
            coverX = (screenW - coverW) / 2;
            coverY = 60;
            infoY = coverY + coverH + 16;
        }
        
        if (book.hasCover && strlen(book.coverPath) > 0 && SD.exists(book.coverPath)) {
            drawCoverImage(book.coverPath, coverX, coverY, coverW, coverH);
        } else {
            drawCoverPlaceholder(coverX, coverY, coverW, coverH, book.title);
        }
        
        // Cover border
        display.drawRect(coverX - 2, coverY - 2, coverW + 4, coverH + 4, GxEPD_BLACK);
        display.drawRect(coverX - 3, coverY - 3, coverW + 6, coverH + 6, GxEPD_BLACK);
        
        // Book info card
        int cardX = 16, cardW = screenW - 32, cardH = 80;
        display.drawRoundRect(cardX, infoY, cardW, cardH, 8, GxEPD_BLACK);
        
        display.setTextColor(GxEPD_BLACK);
        
        // Title
        display.setFont(&FreeSansBold12pt7b);
        int16_t x1, y1; uint16_t tw, th;
        String title = book.title;
        display.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
        int maxTitleW = cardW - 32;
        while ((int)tw > maxTitleW && title.length() > 3) {
            title = title.substring(0, title.length() - 4) + "...";
            display.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
        }
        display.setCursor(cardX + 16, infoY + 24);
        display.print(title);
        
        // Author
        if (strlen(book.author) > 0) {
            display.setFont(&FreeSans9pt7b);
            display.setCursor(cardX + 16, infoY + 44);
            display.print(book.author);
        }
        
        // Stats line
        display.setFont(&FreeSans9pt7b);
        char statsStr[64];
        if (book.totalChapters > 0 && book.estimatedPages > 0) {
            if (book.progress > 0.01f) {
                snprintf(statsStr, 64, "%d chapters - ~%d pages - %d%%",
                         book.totalChapters, book.estimatedPages, (int)(book.progress * 100));
            } else {
                snprintf(statsStr, 64, "%d chapters - ~%d pages", book.totalChapters, book.estimatedPages);
            }
        } else if (book.progress > 0.01f) {
            snprintf(statsStr, 64, "%d%% complete", (int)(book.progress * 100));
        } else {
            strcpy(statsStr, "Not started");
        }
        display.setCursor(cardX + 16, infoY + 66);
        display.print(statsStr);
        
        // Navigation dots
        int dotY = infoY + cardH + 16;
        int numDots = min(bookCount, 7);
        int startDot = 0;
        int selectedDot = cursor;
        
        if (bookCount > 7) {
            if (cursor < 3) {
                startDot = 0; selectedDot = cursor;
            } else if (cursor > bookCount - 4) {
                startDot = bookCount - 7; selectedDot = cursor - startDot;
            } else {
                startDot = cursor - 3; selectedDot = 3;
            }
        }
        
        int dotsWidth = (numDots - 1) * 16 + 24;
        int dotX = (screenW - dotsWidth) / 2;
        
        for (int i = 0; i < numDots; i++) {
            bool isCurrent = (i == selectedDot);
            if (isCurrent) {
                display.fillRoundRect(dotX, dotY, 24, 8, 4, GxEPD_BLACK);
                dotX += 32;
            } else {
                display.fillCircle(dotX + 4, dotY + 4, 4, GxEPD_BLACK);
                dotX += 16;
            }
        }
        
        drawFooter("Left/Right: Browse - OK: Open - Down: List");
        
    } while (display.nextPage());
}

// =============================================================================
// List Browser - ENHANCED with mini covers
// =============================================================================
void LibraryApp::drawListBrowser() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        char subtitle[32];
        snprintf(subtitle, 32, "%d books - List View", bookCount);
        drawHeader("Library", subtitle);
        
        if (bookCount == 0) {
            display.setFont(&FreeSansBold12pt7b);
            centerText("No books found", screenW/2, screenH/2 - 20);
            display.setFont(&FreeSans9pt7b);
            centerText("Add .epub files to /books", screenW/2, screenH/2 + 20);
            continue;
        }
        
        int y = 56;
        int itemH = 80;
        int maxVisible = (screenH - 56 - 40) / itemH;
        
        for (int i = scrollOffset; i < min(scrollOffset + maxVisible, bookCount); i++) {
            BookEntry book;
            if (!getBook(i, book)) continue;
            
            bool selected = (i == cursor);
            int itemY = y + (i - scrollOffset) * itemH;
            
            if (selected) {
                display.fillRoundRect(12, itemY, screenW - 24, itemH - 4, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(12, itemY, screenW - 24, itemH - 4, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Mini cover placeholder
            int coverX = 24, coverY = itemY + 8;
            int coverW = 40, coverH = itemH - 20;
            display.drawRoundRect(coverX, coverY, coverW, coverH, 4, 
                                  selected ? GxEPD_WHITE : GxEPD_BLACK);
            
            // Title & Author
            int textX = coverX + coverW + 12;
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(textX, itemY + 24);
            
            char truncTitle[28];
            strncpy(truncTitle, book.title, 27);
            truncTitle[27] = '\0';
            display.print(truncTitle);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(textX, itemY + 42);
            if (strlen(book.author) > 0) {
                display.print(book.author);
            }
            
            // Progress bar
            int barX = textX, barY = itemY + 52;
            int barW = screenW - textX - 70, barH = 4;
            
            if (selected) {
                display.fillRoundRect(barX, barY, barW, barH, 2, GxEPD_WHITE);
            } else {
                display.drawRoundRect(barX, barY, barW, barH, 2, GxEPD_BLACK);
            }
            
            if (book.progress > 0.01f) {
                int fillW = (barW * (int)(book.progress * 100)) / 100;
                if (fillW > 0) {
                    if (selected) {
                        display.fillRoundRect(barX + fillW, barY, barW - fillW, barH, 2, GxEPD_BLACK);
                    } else {
                        display.fillRoundRect(barX, barY, fillW, barH, 2, GxEPD_BLACK);
                    }
                }
            }
            
            // Progress percentage
            display.setFont(&FreeSansBold9pt7b);
            char pctStr[8];
            int pct = (int)(book.progress * 100);
            snprintf(pctStr, 8, "%d%%", pct);
            
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(pctStr, 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(screenW - 36 - tw, itemY + 40);
            display.print(pctStr);
        }
        
        display.setTextColor(GxEPD_BLACK);
        drawFooter("Up/Down: Select - OK: Open - Up: Covers");
        
    } while (display.nextPage());
}

// =============================================================================
// Reading Page
// =============================================================================
void LibraryApp::drawReadingPage() {
    display.setTextColor(GxEPD_BLACK);
    
    CachedPage page;
    if (!pageCache->loadPage(currentChapter, currentPage, page)) {
        display.setFont(&FreeSans12pt7b);
        display.setCursor(screenW/2 - 80, screenH/2);
        display.print("Page unavailable");
        return;
    }
    
    const GFXfont* font = getReaderFont();
    display.setFont(font);
    
    for (int i = 0; i < page.lineCount; i++) {
        CachedLine& line = page.lines[i];
        if (line.isImage()) continue;
        
        for (int w = 0; w < line.wordCount; w++) {
            CachedWord& word = line.words[w];
            display.setCursor(word.xPos, line.yPos);
            display.print(word.text);
        }
    }
    
    drawStatusBarInPage();
}

void LibraryApp::drawStatusBarInPage() {
    LibReaderSettings& settings = readerSettings.get();
    
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    int y = screenH - 20;
    int margin = settings.screenMargin;
    
    char pageStr[32];
    snprintf(pageStr, sizeof(pageStr), "Ch %d - %d/%d", currentChapter + 1, currentPage + 1, totalPages);
    display.setCursor(margin, y);
    display.print(pageStr);
    
    float progress = getReadingProgress();
    char progStr[16];
    snprintf(progStr, sizeof(progStr), "%d%%", (int)(progress * 100));
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(progStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(screenW - margin - w, y);
    display.print(progStr);
}

// =============================================================================
// Chapter Select - ENHANCED
// =============================================================================
void LibraryApp::drawChapterSelect() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        drawHeader("Chapters", currentBook);
        
        int totalCh = (chapterTitleCount > 0) ? chapterTitleCount : totalChapters;
        int y = 56;
        int itemH = 52;
        int maxVisible = (screenH - 56 - 40) / itemH;
        
        for (int i = chapterScrollOffset; i < min(chapterScrollOffset + maxVisible, totalCh); i++) {
            int itemY = y + (i - chapterScrollOffset) * itemH;
            bool sel = (i == chapterCursor);
            bool isCurrent = (i == currentChapter);
            
            if (sel) {
                display.fillRoundRect(14, itemY, screenW - 28, itemH - 4, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(14, itemY, screenW - 28, itemH - 4, 6, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Chapter number
            display.setFont(&FreeSansBold12pt7b);
            char numStr[8];
            snprintf(numStr, 8, "%d", i + 1);
            display.setCursor(30, itemY + 28);
            display.print(numStr);
            
            // Chapter title
            display.setFont(&FreeSans9pt7b);
            if (chapterTitleCount > 0 && i < chapterTitleCount) {
                char truncTitle[32];
                strncpy(truncTitle, chapterTitles[i].title, 31);
                truncTitle[31] = '\0';
                display.setCursor(70, itemY + 28);
                display.print(truncTitle);
            } else {
                char chTitle[32];
                snprintf(chTitle, 32, "Chapter %d%s", i + 1, isCurrent ? " (current)" : "");
                display.setCursor(70, itemY + 28);
                display.print(chTitle);
            }
            
            // Current badge
            if (isCurrent) {
                int badgeX = screenW - 90;
                if (sel) {
                    display.fillRoundRect(badgeX, itemY + 12, 60, 24, 4, GxEPD_WHITE);
                    display.setTextColor(GxEPD_BLACK);
                } else {
                    display.fillRoundRect(badgeX, itemY + 12, 60, 24, 4, GxEPD_BLACK);
                    display.setTextColor(GxEPD_WHITE);
                }
                display.setFont(&FreeSansBold9pt7b);
                display.setCursor(badgeX + 8, itemY + 28);
                display.print("NOW");
                display.setTextColor(sel ? GxEPD_WHITE : GxEPD_BLACK);
            }
        }
        
        display.setTextColor(GxEPD_BLACK);
        drawFooter("Up/Down: Select - OK: Jump - Back: Return");
        
    } while (display.nextPage());
}

// =============================================================================
// Settings Menu - ENHANCED with toggles
// =============================================================================
void LibraryApp::drawSettingsMenu() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        drawHeader("Reader Settings", nullptr);
        
        LibReaderSettings& settings = readerSettings.get();
        
        int y = 56;
        
        // DISPLAY Section
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, y + 14);
        display.print("DISPLAY");
        y += 26;
        
        char marginStr[8], refreshStr[16];
        snprintf(marginStr, 8, "%dpx", settings.screenMargin);
        snprintf(refreshStr, 16, "%d pages", settings.refreshFrequency);
        
        // Settings structure
        struct SettingItem {
            const char* label;
            const char* value;
            bool isToggle;
            bool toggleVal;
        };
        
        SettingItem displayItems[] = {
            { "Font Size", LibReaderSettings::getFontSizeName(settings.fontSize), false, false },
            { "Margins", marginStr, false, false },
            { "Line Spacing", LibReaderSettings::getLineSpacingName(settings.lineSpacing), false, false },
            { "Justify Text", nullptr, true, settings.justifyText() },
            { "Full Refresh", refreshStr, false, false }
        };
        
        int itemH = 46;
        
        // Draw display settings
        for (int i = 0; i < 5; i++) {
            int itemY = y + i * itemH;
            bool sel = (settingsCursor == i);
            
            if (sel) {
                display.drawRoundRect(14, itemY - 2, screenW - 28, itemH, 6, GxEPD_BLACK);
                display.drawRoundRect(15, itemY - 1, screenW - 30, itemH - 2, 5, GxEPD_BLACK);
            }
            
            display.drawRoundRect(16, itemY, screenW - 32, itemH - 4, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(28, itemY + 28);
            display.print(displayItems[i].label);
            
            if (displayItems[i].isToggle) {
                drawToggle(screenW - 70, itemY + 10, displayItems[i].toggleVal);
            } else if (displayItems[i].value) {
                display.setFont(&FreeSansBold9pt7b);
                char valStr[24];
                snprintf(valStr, 24, "%s <>", displayItems[i].value);
                int16_t tx, ty; uint16_t tw, th;
                display.getTextBounds(valStr, 0, 0, &tx, &ty, &tw, &th);
                display.setCursor(screenW - 40 - tw, itemY + 28);
                display.print(valStr);
            }
        }
        
        y += 5 * itemH + 10;
        
        // NAVIGATION Section
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, y + 14);
        display.print("NAVIGATION");
        y += 26;
        
        const char* navItems[] = { "Chapters...", "Bookmarks...", "Add Bookmark", "Reading Stats", "Clear Cache", "Back" };
        
        for (int i = 0; i < 6; i++) {
            int itemY = y + i * itemH;
            int itemIdx = 5 + i;
            bool sel = (settingsCursor == itemIdx);
            
            if (sel) {
                display.drawRoundRect(14, itemY - 2, screenW - 28, itemH, 6, GxEPD_BLACK);
                display.drawRoundRect(15, itemY - 1, screenW - 30, itemH - 2, 5, GxEPD_BLACK);
            }
            
            display.drawRoundRect(16, itemY, screenW - 32, itemH - 4, 6, GxEPD_BLACK);
            display.setTextColor(GxEPD_BLACK);
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(28, itemY + 28);
            display.print(navItems[i]);
            
            if (i < 4) {
                display.setFont(&FreeSansBold9pt7b);
                display.setCursor(screenW - 50, itemY + 28);
                display.print(">");
            }
        }
        
        drawFooter("Up/Down: Select - Left/Right: Change - Back: Save");
        
    } while (display.nextPage());
}

// =============================================================================
// Bookmark Select - ENHANCED
// =============================================================================
void LibraryApp::drawBookmarkSelect() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        char subtitle[16];
        snprintf(subtitle, 16, "%d saved", bookmarks.count);
        drawHeader("Bookmarks", subtitle);
        
        int y = 56;
        
        // Add bookmark button
        display.drawRoundRect(16, y, screenW - 32, 50, 8, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        centerText("+ Add Bookmark at Current Page", screenW / 2, y + 32);
        
        y += 65;
        
        if (bookmarks.count == 0) {
            display.setFont(&FreeSans9pt7b);
            centerText("No bookmarks yet", screenW / 2, y + 40);
            centerText("Press OK to add one", screenW / 2, y + 60);
        } else {
            int itemH = 60;
            int maxVisible = (screenH - y - 40) / itemH;
            
            for (int i = bookmarkScrollOffset; i < min(bookmarkScrollOffset + maxVisible, bookmarks.count); i++) {
                int itemY = y + (i - bookmarkScrollOffset) * itemH;
                bool sel = (bookmarkCursor == i);
                Bookmark& bm = bookmarks.bookmarks[i];
                
                if (sel) {
                    display.fillRoundRect(14, itemY, screenW - 28, itemH - 4, 8, GxEPD_BLACK);
                    display.setTextColor(GxEPD_WHITE);
                } else {
                    display.drawRoundRect(14, itemY, screenW - 28, itemH - 4, 8, GxEPD_BLACK);
                    display.setTextColor(GxEPD_BLACK);
                }
                
                display.setFont(&FreeSansBold9pt7b);
                display.setCursor(28, itemY + 24);
                display.print(bm.label);
                
                display.setFont(&FreeSans9pt7b);
                char locStr[32];
                snprintf(locStr, 32, "Chapter %d, Page %d", bm.chapter + 1, bm.page + 1);
                display.setCursor(28, itemY + 44);
                display.print(locStr);
                
                if (sel) {
                    display.setCursor(screenW - 100, itemY + 34);
                    display.print("< Delete");
                }
            }
        }
        
        display.setTextColor(GxEPD_BLACK);
        drawFooter("Up/Down: Select - OK: Jump - Left: Delete");
        
    } while (display.nextPage());
}

// =============================================================================
// Reading Stats - ENHANCED with cards
// =============================================================================
void LibraryApp::drawReadingStatsScreen() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        drawHeader("Reading Statistics", nullptr);
        
        int y = 56;
        
        // This Session Card
        display.drawRoundRect(16, y, screenW - 32, 100, 12, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(28, y + 22);
        display.print("This Session");
        
        int boxW = (screenW - 60) / 3;
        int boxY = y + 35;
        
        char timeVal[16], pagesVal[16], booksVal[16];
        int sessMin = stats.getSessionMinutes();
        if (sessMin >= 60) {
            snprintf(timeVal, 16, "%dh %dm", sessMin / 60, sessMin % 60);
        } else {
            snprintf(timeVal, 16, "%dm", sessMin);
        }
        snprintf(pagesVal, 16, "%d", stats.sessionPagesRead);
        strcpy(booksVal, currentBook[0] ? "1" : "0");
        
        const char* sessLabels[] = { "Time", "Pages", "Book" };
        const char* sessVals[] = { timeVal, pagesVal, booksVal };
        
        for (int i = 0; i < 3; i++) {
            int bx = 24 + i * (boxW + 6);
            display.drawRoundRect(bx, boxY, boxW, 55, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(sessVals[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 28);
            display.print(sessVals[i]);
            
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(sessLabels[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 46);
            display.print(sessLabels[i]);
        }
        
        y += 115;
        
        // All Time Card
        display.drawRoundRect(16, y, screenW - 32, 100, 12, GxEPD_BLACK);
        
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(28, y + 22);
        display.print("All Time");
        
        boxY = y + 35;
        
        char hoursVal[16], totalPagesVal[16], finishedVal[16];
        snprintf(hoursVal, 16, "%d", stats.totalMinutesRead / 60);
        snprintf(totalPagesVal, 16, "%lu", (unsigned long)stats.totalPagesRead);
        snprintf(finishedVal, 16, "%lu", (unsigned long)stats.booksFinished);
        
        const char* allLabels[] = { "Hours", "Pages", "Books" };
        const char* allVals[] = { hoursVal, totalPagesVal, finishedVal };
        
        for (int i = 0; i < 3; i++) {
            int bx = 24 + i * (boxW + 6);
            display.drawRoundRect(bx, boxY, boxW, 55, 6, GxEPD_BLACK);
            
            display.setFont(&FreeSansBold12pt7b);
            int16_t tx, ty; uint16_t tw, th;
            display.getTextBounds(allVals[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 28);
            display.print(allVals[i]);
            
            display.setFont(&FreeSans9pt7b);
            display.getTextBounds(allLabels[i], 0, 0, &tx, &ty, &tw, &th);
            display.setCursor(bx + (boxW - tw) / 2, boxY + 46);
            display.print(allLabels[i]);
        }
        
        y += 115;
        
        // Current book card
        if (currentBook[0]) {
            display.fillRoundRect(16, y, screenW - 32, 90, 12, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
            
            display.setFont(&FreeSansBold9pt7b);
            display.setCursor(28, y + 22);
            display.print(currentBook);
            
            display.setFont(&FreeSans9pt7b);
            
            float prog = getReadingProgress();
            char line1[32], line2[32];
            snprintf(line1, 32, "Progress: %d%%  |  Chapter %d/%d", 
                     (int)(prog * 100), currentChapter + 1, totalChapters);
            snprintf(line2, 32, "Page %d of %d", currentPage + 1, totalPages);
            
            display.setCursor(28, y + 50);
            display.print(line1);
            display.setCursor(28, y + 72);
            display.print(line2);
            
            display.setTextColor(GxEPD_BLACK);
        }
        
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
        display.setFont(&FreeSansBold12pt7b);
        centerText("Scanning Library...", screenW/2, screenH/2 - 20);
        
        display.setFont(&FreeSans9pt7b);
        char countStr[32];
        snprintf(countStr, sizeof(countStr), "Found %d books", indexingProgress);
        centerText(countStr, screenW/2, screenH/2 + 20);
        
    } while (display.nextPage());
}

// =============================================================================
// Info Screen
// =============================================================================
void LibraryApp::drawInfo() {
    if (needsFullRedraw) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, screenW, screenH);
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        drawHeader("Book Info", nullptr);
        
        BookEntry book;
        if (!getBook(cursor, book)) {
            display.setFont(&FreeSans12pt7b);
            centerText("No book selected", screenW/2, screenH/2);
            continue;
        }
        
        int y = 70;
        
        if (book.hasCover && strlen(book.coverPath) > 0) {
            int coverW = 120, coverH = 180;
            drawCoverImage(book.coverPath, 20, y, coverW, coverH);
            display.drawRect(19, y - 1, coverW + 2, coverH + 2, GxEPD_BLACK);
        }
        
        int infoX = book.hasCover ? 160 : 20;
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(infoX, y + 20);
        display.print(book.title);
        
        display.setFont(&FreeSans9pt7b);
        if (strlen(book.author) > 0) {
            display.setCursor(infoX, y + 45);
            display.print("by ");
            display.print(book.author);
        }
        
        y += 70;
        display.setCursor(infoX, y);
        display.print("Chapters: ");
        display.print(book.totalChapters);
        
        y += 25;
        display.setCursor(infoX, y);
        display.print("Est. Pages: ");
        display.print(book.estimatedPages);
        
        if (book.progress > 0.01f) {
            y += 25;
            display.setCursor(infoX, y);
            display.print("Progress: ");
            display.print((int)(book.progress * 100));
            display.print("%");
        }
        
    } while (display.nextPage());
}

// =============================================================================
// Cover Image Drawing
// =============================================================================
void LibraryApp::drawCoverImage(const char* path, int x, int y, int maxW, int maxH) {
    _coverOffsetX = x;
    _coverOffsetY = y;
    _coverMaxX = x + maxW;
    _coverMaxY = y + maxH;
    _jpgCallbackCount = 0;
    
    uint16_t imgW = 0, imgH = 0;
    TJpgDec.getFsJpgSize(&imgW, &imgH, path, SD);
    
    if (imgW == 0 || imgH == 0) {
        drawCoverPlaceholder(x, y, maxW, maxH, "?");
        return;
    }
    
    float scaleW = (float)maxW / imgW;
    float scaleH = (float)maxH / imgH;
    _coverScale = min(scaleW, scaleH);
    
    int scaledW = (int)(imgW * _coverScale);
    int scaledH = (int)(imgH * _coverScale);
    _coverOffsetX = x + (maxW - scaledW) / 2;
    _coverOffsetY = y + (maxH - scaledH) / 2;
    
    TJpgDec.setCallback(_jpgDrawCallback);
    TJpgDec.setJpgScale(1);
    TJpgDec.drawFsJpg(0, 0, path, SD);
}

void LibraryApp::drawCoverPlaceholder(int x, int y, int maxW, int maxH, const char* label) {
    display.fillRoundRect(x, y, maxW, maxH, 8, GxEPD_WHITE);
    display.drawRoundRect(x, y, maxW, maxH, 8, GxEPD_BLACK);
    display.drawRoundRect(x + 4, y + 4, maxW - 8, maxH - 8, 6, GxEPD_BLACK);
    
    if (label && strlen(label) > 0) {
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        char letter[2] = { (char)toupper(label[0]), '\0' };
        
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds(letter, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(x + (maxW - tw) / 2, y + maxH / 2 + th / 2);
        display.print(letter);
    }
}

// =============================================================================
// Utility Screens
// =============================================================================
void LibraryApp::showLoadingScreen(const char* message) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeSans12pt7b);
        display.setTextColor(GxEPD_BLACK);
        centerText(message, screenW/2, screenH/2);
    } while (display.nextPage());
}

void LibraryApp::showErrorScreen(const char* message) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        centerText("Error", screenW/2, screenH/2 - 30);
        display.setFont(&FreeSans9pt7b);
        centerText(message, screenW/2, screenH/2 + 10);
    } while (display.nextPage());
}

// =============================================================================
// Sleep Cover Display
// =============================================================================
void LibraryApp::drawSleepCover(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& disp, int w, int h) {
    LastBookInfo info;
    if (!getLastBookInfo(info)) return;
    
    disp.fillScreen(GxEPD_WHITE);
    
    if (strlen(info.coverPath) > 0 && SD.exists(info.coverPath)) {
        _coverOffsetX = (w - 280) / 2;
        _coverOffsetY = 40;
        _coverMaxX = _coverOffsetX + 280;
        _coverMaxY = _coverOffsetY + 400;
        _coverScale = 1.0f;
        
        uint16_t imgW = 0, imgH = 0;
        TJpgDec.getFsJpgSize(&imgW, &imgH, info.coverPath, SD);
        
        if (imgW > 0 && imgH > 0) {
            float scaleW = 280.0f / imgW;
            float scaleH = 400.0f / imgH;
            _coverScale = min(scaleW, scaleH);
            
            int scaledW = (int)(imgW * _coverScale);
            int scaledH = (int)(imgH * _coverScale);
            _coverOffsetX = (w - scaledW) / 2;
            _coverOffsetY = 40 + (400 - scaledH) / 2;
            
            TJpgDec.setCallback(_jpgDrawCallback);
            TJpgDec.setJpgScale(1);
            TJpgDec.drawFsJpg(0, 0, info.coverPath, SD);
        }
    }
    
    disp.setFont(&FreeSans9pt7b);
    disp.setTextColor(GxEPD_BLACK);
    
    char progStr[32];
    snprintf(progStr, 32, "%d%%", (int)(info.progress * 100));
    
    int16_t x1, y1; uint16_t tw, th;
    disp.getTextBounds(progStr, 0, 0, &x1, &y1, &tw, &th);
    disp.setCursor((w - tw) / 2, h - 20);
    disp.print(progStr);
}

#endif // FEATURE_READER
