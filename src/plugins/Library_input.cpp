/**
 * @file Library_input.cpp
 * @brief Library input handling
 * @version 1.5.0
 */

#include "plugins/Library.h"

#if FEATURE_READER

// =============================================================================
// Input Handling
// =============================================================================
bool LibraryApp::isReading() const { 
    return state == ViewState::READING && bookIsOpen; 
}

bool LibraryApp::handleKeyboardKey(uint8_t keyCode, bool pressed) {
    if (!pressed || state != ViewState::READING) return false;
    
    switch (keyCode) {
        case 0x50: case 0x4B: case 0x2C: // Left, PageUp, Space
            return handleReadingInput(BTN_LEFT);
        case 0x4F: case 0x4E: case 0x28: // Right, PageDown, Enter
            return handleReadingInput(BTN_RIGHT);
        case 0x52: // Up
            return handleReadingInput(BTN_UP);
        case 0x29: // Escape
            return handleReadingInput(BTN_BACK);
    }
    return false;
}

bool LibraryApp::handleInput(Button btn) {
    if (btn == BTN_NONE) return false;
    bool handled = handleButtonPress(btn);
    if (handled) {
        _pendingRedraw = true;
    }
    return handled;
}

bool LibraryApp::handleButtonPress(Button btn) {
    switch (state) {
        case ViewState::MAIN_MENU:
            return handleMainMenuInput(btn);
        case ViewState::BROWSER:
            return useFlipBrowser ? handleFlipBrowserInput(btn) : handleListBrowserInput(btn);
        case ViewState::BROWSER_LIST:
            return handleListBrowserInput(btn);
        case ViewState::READING:
            return handleReadingInput(btn);
        case ViewState::CHAPTER_SELECT:
            return handleChapterSelectInput(btn);
        case ViewState::SETTINGS_MENU:
            return handleSettingsInput(btn);
        case ViewState::BOOKMARK_SELECT:
            return handleBookmarkSelectInput(btn);
        case ViewState::READING_STATS:
            return handleReadingStatsInput(btn);
        default:
            return false;
    }
}

bool LibraryApp::handleBrowserInput(Button btn) {
    return useFlipBrowser ? handleFlipBrowserInput(btn) : handleListBrowserInput(btn);
}

bool LibraryApp::handleMainMenuInput(Button btn) {
    LastBookInfo lastBook;
    bool hasLastBook = getLastBookInfo(lastBook);
    int maxItems = hasLastBook ? 4 : 3;
    
    switch (btn) {
        case BTN_UP:
            if (mainMenuCursor > 0) mainMenuCursor--;  // Partial refresh
            return true;
            
        case BTN_DOWN:
            if (mainMenuCursor < maxItems - 1) mainMenuCursor++;  // Partial refresh
            return true;
            
        case BTN_CONFIRM:
            if (hasLastBook) {
                switch (mainMenuCursor) {
                    case 0: // Continue Reading
                        resumeLastBook();
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 1: // Browse Library
                        state = ViewState::BROWSER;
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 2: // Reading Statistics
                        state = ViewState::READING_STATS;
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 3: // Reader Settings
                        settingsCursor = 0;
                        state = ViewState::SETTINGS_MENU;
                        needsFullRedraw = true;  // Screen change
                        break;
                }
            } else {
                switch (mainMenuCursor) {
                    case 0: // Browse Library
                        state = ViewState::BROWSER;
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 1: // Reading Statistics
                        state = ViewState::READING_STATS;
                        needsFullRedraw = true;  // Screen change
                        break;
                    case 2: // Reader Settings
                        settingsCursor = 0;
                        state = ViewState::SETTINGS_MENU;
                        needsFullRedraw = true;  // Screen change
                        break;
                }
            }
            return true;
            
        case BTN_BACK:
            return false;  // Exit to home
            
        default:
            return false;
    }
}

bool LibraryApp::handleFlipBrowserInput(Button btn) {
    switch (btn) {
        case BTN_LEFT:
            if (cursor > 0) {
                // Skip regular directories in flip mode
                do { cursor--; } while (cursor > 0 && ({
                    BookEntry b; getBook(cursor, b); b.isRegularDir;
                }));
                _pendingRedraw = true;
            }
            return true;
            
        case BTN_RIGHT:
            if (cursor < bookCount - 1) {
                do { cursor++; } while (cursor < bookCount - 1 && ({
                    BookEntry b; getBook(cursor, b); b.isRegularDir;
                }));
                _pendingRedraw = true;
            }
            return true;
            
        case BTN_CONFIRM: {
            BookEntry book;
            if (getBook(cursor, book)) {
                if (book.isRegularDir) {
                    // Enter directory
                    snprintf(currentPath, sizeof(currentPath), "%s/%s", currentPath, book.filename);
                    scanDirectory();
                    needsFullRedraw = true;  // Directory change
                } else {
                    openBook(cursor);
                    needsFullRedraw = true;  // Screen change
                }
                _pendingRedraw = true;
            }
            return true;
        }
        
        case BTN_UP:
            // Switch to list view
            state = ViewState::BROWSER_LIST;
            needsFullRedraw = true;  // Screen change
            _pendingRedraw = true;
            return true;
            
        case BTN_DOWN:
            // Switch to list view
            state = ViewState::BROWSER_LIST;
            needsFullRedraw = true;  // Screen change
            _pendingRedraw = true;
            return true;
            
        case BTN_BACK:
            // Go up directory, or back to main menu if at root
            if (strcmp(currentPath, "/books") != 0) {
                char* lastSlash = strrchr(currentPath, '/');
                if (lastSlash && lastSlash != currentPath) {
                    *lastSlash = '\0';
                    scanDirectory();
                    needsFullRedraw = true;  // Directory change
                    _pendingRedraw = true;
                }
                return true;  // Consumed - navigated up
            }
            state = ViewState::MAIN_MENU;
            mainMenuCursor = 0;
            needsFullRedraw = true;  // Screen change
            _pendingRedraw = true;
            return true;
            
        default:
            return false;
    }
}

bool LibraryApp::handleListBrowserInput(Button btn) {
    switch (btn) {
        case BTN_UP:
            if (cursor > 0) {
                cursor--;
                if (cursor < scrollOffset) scrollOffset = cursor;
            }  // Partial refresh
            return true;
            
        case BTN_DOWN:
            if (cursor < bookCount - 1) {
                cursor++;
                if (cursor >= scrollOffset + itemsPerPage) scrollOffset = cursor - itemsPerPage + 1;
            }  // Partial refresh
            return true;
            
        case BTN_CONFIRM: {
            BookEntry book;
            if (getBook(cursor, book)) {
                if (book.isDirectory) {
                    snprintf(currentPath, sizeof(currentPath), "%s/%s", currentPath, book.filename);
                    scanDirectory();
                    needsFullRedraw = true;  // Directory change
                } else {
                    openBook(cursor);
                    needsFullRedraw = true;  // Screen change
                }
            }
            return true;
        }
        
        case BTN_LEFT:
            state = ViewState::BROWSER;
            needsFullRedraw = true;  // Screen change
            return true;
            
        case BTN_RIGHT:
            state = ViewState::BROWSER;
            needsFullRedraw = true;  // Screen change
            return true;
            
        case BTN_BACK:
            if (strcmp(currentPath, "/books") != 0) {
                char* lastSlash = strrchr(currentPath, '/');
                if (lastSlash && lastSlash != currentPath) {
                    *lastSlash = '\0';
                    scanDirectory();
                    needsFullRedraw = true;  // Directory change
                }
                return true;  // Consumed - navigated up
            }
            state = ViewState::MAIN_MENU;
            mainMenuCursor = 0;
            needsFullRedraw = true;  // Screen change
            return true;
            
        default:
            return false;
    }
}

bool LibraryApp::handleReadingInput(Button btn) {
    switch (btn) {
        case BTN_RIGHT:
            // Next page
            if (currentPage < totalPages - 1) {
                currentPage++;
                stats.recordPageTurn();
                pagesUntilFullRefresh--;
                pagesUntilHalfRefresh--;
                updateRequired = true;
                preloadNextPage();  // Preload upcoming page
            } else if (currentChapter < totalChapters - 1) {
                currentChapter++;
                currentPage = 0;
                pendingChapterLoad = true;
                pendingChapterToLoad = currentChapter;
                cacheValid = false;
            }
            return true;
            
        case BTN_LEFT:
            // Previous page
            if (currentPage > 0) {
                currentPage--;
                pagesUntilFullRefresh--;
                pagesUntilHalfRefresh--;
                updateRequired = true;
                preloadPrevPage();  // Preload previous page
            } else if (currentChapter > 0) {
                currentChapter--;
                currentPage = -1; // Will be set to last page of chapter
                pendingChapterLoad = true;
                pendingChapterToLoad = currentChapter;
                cacheValid = false;
            }
            return true;
            
        case BTN_UP:
            // Settings menu
            settingsCursor = 0;
            state = ViewState::SETTINGS_MENU;
            needsFullRedraw = true;  // Screen change
            return true;
            
        case BTN_DOWN:
            // Chapter select
            chapterCursor = currentChapter;
            chapterScrollOffset = max(0, currentChapter - 3);
            state = ViewState::CHAPTER_SELECT;
            needsFullRedraw = true;  // Screen change
            return true;
            
        case BTN_CONFIRM:
            // Toggle settings menu
            settingsCursor = 0;
            state = ViewState::SETTINGS_MENU;
            needsFullRedraw = true;  // Screen change
            return true;
            
        case BTN_BACK:
            // Close book and return to browser
            saveProgress();
            closeBook();
            state = ViewState::BROWSER;
            needsFullRedraw = true;  // Screen change
            return true;
            
        default:
            return false;
    }
}

bool LibraryApp::handleChapterSelectInput(Button btn) {
    int displayCount = min(chapterTitleCount, totalChapters);
    int maxVisible = (screenH - 100) / 40;
    
    switch (btn) {
        case BTN_UP:
            if (chapterCursor > 0) {
                chapterCursor--;
                if (chapterCursor < chapterScrollOffset) chapterScrollOffset = chapterCursor;
            }  // Partial refresh
            return true;
            
        case BTN_DOWN:
            if (chapterCursor < displayCount - 1) {
                chapterCursor++;
                if (chapterCursor >= chapterScrollOffset + maxVisible) {
                    chapterScrollOffset = chapterCursor - maxVisible + 1;
                }
            }  // Partial refresh
            return true;
            
        case BTN_CONFIRM:
            if (chapterCursor != currentChapter) {
                currentChapter = chapterCursor;
                currentPage = 0;
                pendingChapterLoad = true;
                pendingChapterToLoad = currentChapter;
                cacheValid = false;
            }
            state = ViewState::READING;
            needsFullRedraw = true;  // Screen change
            return true;
            
        case BTN_BACK:
            state = ViewState::READING;
            needsFullRedraw = true;  // Screen change
            return true;
            
        default:
            return false;
    }
}

bool LibraryApp::handleSettingsInput(Button btn) {
    int itemCount = (int)SettingsItem::COUNT;
    
    switch (btn) {
        case BTN_UP:
            if (settingsCursor > 0) settingsCursor--;  // Partial refresh
            return true;
            
        case BTN_DOWN:
            if (settingsCursor < itemCount - 1) settingsCursor++;  // Partial refresh
            return true;
            
        case BTN_CONFIRM: {
            LibReaderSettings& settings = readerSettings.get();
            
            switch ((SettingsItem)settingsCursor) {
                case SettingsItem::FONT_SIZE:
                    // Cycle through 4 font sizes: Small, Medium, Large, Extra Large
                    settings.fontSize = (FontSize)(((int)settings.fontSize + 1) % 4);
                    applyFontSettings();
                    pageCache->invalidateBook();
                    cacheValid = false;
                    pendingChapterLoad = true;
                    pendingChapterToLoad = currentChapter;
                    needsFullRedraw = true;  // Redraw menu to show new value
                    break;
                    
                case SettingsItem::MARGINS:
                    // Cycle screen margin: 0, 5, 10, 15, 20
                    if (settings.screenMargin == 0) settings.screenMargin = 5;
                    else if (settings.screenMargin == 5) settings.screenMargin = 10;
                    else if (settings.screenMargin == 10) settings.screenMargin = 15;
                    else if (settings.screenMargin == 15) settings.screenMargin = 20;
                    else settings.screenMargin = 0;
                    applyFontSettings();
                    pageCache->invalidateBook();
                    cacheValid = false;
                    pendingChapterLoad = true;
                    pendingChapterToLoad = currentChapter;
                    needsFullRedraw = true;  // Redraw menu to show new value
                    break;
                    
                case SettingsItem::LINE_SPACING:
                    settings.lineSpacing = (LineSpacing)(((int)settings.lineSpacing + 1) % 3);
                    applyFontSettings();
                    pageCache->invalidateBook();
                    cacheValid = false;
                    pendingChapterLoad = true;
                    pendingChapterToLoad = currentChapter;
                    needsFullRedraw = true;  // Redraw menu to show new value
                    break;
                    
                case SettingsItem::JUSTIFY:
                    // Toggle between justified and left-aligned
                    if (settings.textAlign == TextAlign::JUSTIFIED) {
                        settings.textAlign = TextAlign::LEFT;
                    } else {
                        settings.textAlign = TextAlign::JUSTIFIED;
                    }
                    applyFontSettings();
                    pageCache->invalidateBook();
                    cacheValid = false;
                    pendingChapterLoad = true;
                    pendingChapterToLoad = currentChapter;
                    needsFullRedraw = true;  // Redraw menu to show new value
                    break;
                    
                case SettingsItem::REFRESH_FREQ:
                    // Cycle through: 5, 10, 15, 20, 30
                    if (settings.refreshFrequency <= 5) settings.refreshFrequency = 10;
                    else if (settings.refreshFrequency <= 10) settings.refreshFrequency = 15;
                    else if (settings.refreshFrequency <= 15) settings.refreshFrequency = 20;
                    else if (settings.refreshFrequency <= 20) settings.refreshFrequency = 30;
                    else settings.refreshFrequency = 5;
                    pagesUntilFullRefresh = settings.refreshFrequency;
                    needsFullRedraw = true;  // Redraw menu to show new value
                    break;
                    
                case SettingsItem::CHAPTERS:
                    chapterCursor = currentChapter;
                    state = ViewState::CHAPTER_SELECT;
                    needsFullRedraw = true;  // Screen change
                    return true;
                    
                case SettingsItem::BOOKMARKS:
                    bookmarkCursor = 0;
                    bookmarkScrollOffset = 0;
                    state = ViewState::BOOKMARK_SELECT;
                    needsFullRedraw = true;  // Screen change
                    return true;
                    
                case SettingsItem::ADD_BOOKMARK:
                    bookmarks.add(currentChapter, currentPage, nullptr);
                    saveBookmarks();
                    break;
                    
                case SettingsItem::STATS:
                    state = ViewState::READING_STATS;
                    needsFullRedraw = true;  // Screen change
                    return true;
                    
                case SettingsItem::CLEAR_CACHE:
                    pageCache->invalidateBook();
                    cacheValid = false;
                    pendingChapterLoad = true;
                    pendingChapterToLoad = currentChapter;
                    break;
                    
                case SettingsItem::BACK:
                    readerSettings.save();
                    state = ViewState::READING;
                    needsFullRedraw = true;  // Screen change
                    return true;
                    
                default:
                    break;
            }
            readerSettings.markDirty();
            return true;
        }
        
        case BTN_BACK:
            readerSettings.saveIfDirty();
            state = ViewState::READING;
            needsFullRedraw = true;  // Screen change
            return true;
            
        default:
            return false;
    }
}

bool LibraryApp::handleBookmarkSelectInput(Button btn) {
    int maxVisible = (screenH - 100) / 40;
    
    switch (btn) {
        case BTN_UP:
            if (bookmarkCursor > 0) {
                bookmarkCursor--;
                if (bookmarkCursor < bookmarkScrollOffset) bookmarkScrollOffset = bookmarkCursor;
            }  // Partial refresh
            return true;
            
        case BTN_DOWN:
            if (bookmarkCursor < bookmarks.count - 1) {
                bookmarkCursor++;
                if (bookmarkCursor >= bookmarkScrollOffset + maxVisible) {
                    bookmarkScrollOffset = bookmarkCursor - maxVisible + 1;
                }
            }  // Partial refresh
            return true;
            
        case BTN_CONFIRM:
            if (bookmarkCursor < bookmarks.count) {
                Bookmark& bm = bookmarks.bookmarks[bookmarkCursor];
                if (bm.chapter != currentChapter) {
                    currentChapter = bm.chapter;
                    pendingChapterLoad = true;
                    pendingChapterToLoad = currentChapter;
                    cacheValid = false;
                }
                currentPage = bm.page;
                state = ViewState::READING;
                needsFullRedraw = true;  // Screen change
            }
            return true;
            
        case BTN_LEFT:
            // Delete bookmark
            if (bookmarkCursor < bookmarks.count) {
                bookmarks.remove(bookmarkCursor);
                saveBookmarks();
                if (bookmarkCursor >= bookmarks.count && bookmarks.count > 0) {
                    bookmarkCursor = bookmarks.count - 1;
                }
            }  // Partial refresh for deletion
            return true;
            
        case BTN_BACK:
            state = ViewState::SETTINGS_MENU;
            needsFullRedraw = true;  // Screen change
            return true;
            
        default:
            return false;
    }
}

bool LibraryApp::handleReadingStatsInput(Button btn) {
    if (btn == BTN_BACK || btn == BTN_CONFIRM) {
        state = ViewState::SETTINGS_MENU;
        needsFullRedraw = true;  // Screen change
        return true;
    }
    return false;
}

// =============================================================================
// Bookmarks
// =============================================================================
String LibraryApp::getBookmarkPath() {
    uint32_t hash = 0;
    for (const char* p = currentBookPath; *p; p++) hash = hash * 31 + *p;
    char path[96];
    snprintf(path, sizeof(path), "/.sumi/bm_%08x.bin", hash);
    return String(path);
}

void LibraryApp::loadBookmarks() {
    bookmarks.load(getBookmarkPath());
}

void LibraryApp::saveBookmarks() {
    bookmarks.save(getBookmarkPath());
}

// =============================================================================
// Progress
// =============================================================================
String LibraryApp::getDocumentHash() {
    uint32_t hash = 0;
    for (const char* p = currentBookPath; *p; p++) hash = hash * 31 + *p;
    char hashStr[16];
    snprintf(hashStr, sizeof(hashStr), "%08x", hash);
    return String(hashStr);
}

float LibraryApp::getReadingProgress() {
    if (totalChapters <= 1 && totalPages > 0) {
        return (float)currentPage / (float)totalPages;
    }
    float chapterProgress = (float)currentChapter / (float)totalChapters;
    float pageProgress = totalPages > 0 ? (float)currentPage / (float)totalPages / (float)totalChapters : 0;
    return chapterProgress + pageProgress;
}

void LibraryApp::saveProgress() {
    if (!bookIsOpen) return;
    pageCache->saveProgress(currentChapter, currentPage);
    saveLastBookInfo();
}

void LibraryApp::syncProgressToKOSync() {
    // KOSync implementation - TODO
}

void LibraryApp::syncProgressFromKOSync() {
    // KOSync implementation - TODO
}

void LibraryApp::syncToKOReader() {
    // KOReader sync - TODO
}

void LibraryApp::syncFromKOReader() {
    // KOReader sync - TODO
}

// =============================================================================
// Last Book Info
// =============================================================================
void LibraryApp::saveLastBookInfo() {
    LastBookInfo info;
    memset(&info, 0, sizeof(info));
    
    info.magic = 0x4C415354;  // "LAST"
    strncpy(info.title, currentBook, sizeof(info.title) - 1);
    
    // Get author and cover path from book entry
    BookEntry book;
    if (getBook(cursor, book)) {
        strncpy(info.author, book.author, sizeof(info.author) - 1);
        strncpy(info.coverPath, book.coverPath, sizeof(info.coverPath) - 1);
    }
    
    strncpy(info.bookPath, currentBookPath, sizeof(info.bookPath) - 1);
    info.chapter = currentChapter;
    info.page = currentPage;
    info.totalPages = totalPages;
    info.progress = getReadingProgress();
    
    File f = SD.open(LAST_BOOK_PATH, FILE_WRITE);
    if (f) {
        f.write((uint8_t*)&info, sizeof(info));
        f.close();
        Serial.printf("[LIBRARY] Saved last book: %s (cover: %s)\n", info.title, info.coverPath);
    }
}

bool LibraryApp::getLastBookInfo(LastBookInfo& info) {
    File f = SD.open(LAST_BOOK_PATH, FILE_READ);
    if (!f) return false;
    
    bool ok = f.read((uint8_t*)&info, sizeof(info)) == sizeof(info);
    f.close();
    return ok && info.magic == 0x4C415354 && strlen(info.bookPath) > 0;
}

bool LibraryApp::resumeLastBook() {
    LastBookInfo info;
    if (!getLastBookInfo(info)) return false;
    if (!SD.exists(info.bookPath)) return false;
    
    // Find book in current scan or use path directly
    strncpy(currentBookPath, info.bookPath, sizeof(currentBookPath) - 1);
    strncpy(currentBook, info.title, sizeof(currentBook) - 1);
    
    // Detect type and open
    BookType type = detectBookType(currentBookPath);
    isEpub = (type == BookType::EPUB_FILE || type == BookType::EPUB_FOLDER);
    
    showLoadingScreen("Resuming...");
    suspendForReading();
    
    if (isEpub) {
        // Compute hash from filename only (not full path)
        const char* filename = strrchr(currentBookPath, '/');
        filename = filename ? filename + 1 : currentBookPath;
        
        uint32_t hash = 0;
        for (const char* p = filename; *p; p++) {
            hash = ((hash << 5) - hash) + (uint8_t)*p;
        }
        
        // Use preprocessed metadata only
        if (!openPreprocessedMetadata(hash)) {
            showErrorScreen("Process this book\nin the portal first");
            state = ViewState::BROWSER;
            return false;
        }
    } else {
        openTxtMetadata(currentBookPath);
    }
    
    pageCache->init(currentBookPath);
    textLayout->setPageSize(getLayoutWidth(), screenH);
    applyFontSettings();
    
    currentChapter = info.chapter;
    currentPage = info.page;
    
    // Use synchronous loading (no render task)
    pendingChapterLoad = true;
    pendingChapterToLoad = currentChapter;
    cacheValid = false;
    renderTaskHandle = nullptr;
    renderMutex = nullptr;
    
    // Load chapter synchronously
    if (loadChapterSync(currentChapter)) {
        cacheValid = true;
        if (currentPage >= totalPages) currentPage = totalPages - 1;
        if (currentPage < 0) currentPage = 0;
    }
    
    stats.load();
    stats.startSession();
    bookIsOpen = true;
    firstRenderAfterOpen = true;
    state = ViewState::READING;
    
    loadBookmarks();
    preloadNextPage();
    
    return true;
}

#endif // FEATURE_READER
