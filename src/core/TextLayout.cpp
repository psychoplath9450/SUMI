/**
 * @file TextLayout.cpp
 * @brief Text Layout Engine Implementation
 * @version 1.4.2
 *
 * Streaming text layout that:
 * - Accepts text in chunks via addText()
 * - Builds lines and pages incrementally
 * - Saves pages to SD card cache
 * - Loads only current page for rendering
 */

#include "core/TextLayout.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// Global instance
TextLayout* textLayout = nullptr;

// Temporary display reference for text measurement
static GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>* _measureDisplay = nullptr;

void TextLayout_setDisplay(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>* disp) {
    _measureDisplay = disp;
}

// =============================================================================
// Constructor
// =============================================================================

TextLayout::TextLayout()
    : _pageWidth(480)          // Portrait mode default
    , _pageHeight(800)
    , _marginLeft(8)           // Tighter margins (base 3 + screenMargin 5)
    , _marginRight(8)
    , _marginTop(14)           // Base 9 + screenMargin 5
    , _marginBottom(24)        // Base 3 + screenMargin 5 + status bar area
    , _contentWidth(464)       // 480 - 8 - 8
    , _contentHeight(762)      // 800 - 14 - 24
    , _lineHeight(28)          // Will be calculated from font
    , _lineHeightMultiplier(1.0f)  // Normal = 1.0
    , _paraSpacing(14)         // Half line height when extra spacing enabled
    , _useParaIndent(false)    // When true, use indent instead of spacing
    , _justify(true)
    , _font(&FreeSans12pt7b)
    , _fontBold(&FreeSansBold12pt7b)
    , _fontItalic(&FreeSans12pt7b)  // Fallback if no italic
    , _fontBoldItalic(&FreeSansBold12pt7b)  // Fallback to bold
    , _currentStyle(FontStyle::NORMAL)
    , _currentY(0)
    , _pageCount(0)
    , _isHeaderLine(false)
    , _isBulletLine(false)
    , _currentLineWidth(0)
    , _wordBufferLen(0) {
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
}

// =============================================================================
// Configuration
// =============================================================================

void TextLayout::setPageSize(int width, int height) {
    _pageWidth = width;
    _pageHeight = height;
    _contentWidth = width - _marginLeft - _marginRight;
    _contentHeight = height - _marginTop - _marginBottom;
}

void TextLayout::setMargins(int left, int right, int top, int bottom) {
    _marginLeft = left;
    _marginRight = right;
    _marginTop = top;
    _marginBottom = bottom;
    _contentWidth = _pageWidth - left - right;
    _contentHeight = _pageHeight - top - bottom;
    Serial.printf("[LAYOUT] setMargins: L=%d R=%d T=%d B=%d, pageW=%d, contentW=%d\n",
        left, right, top, bottom, _pageWidth, _contentWidth);
}

void TextLayout::setLineHeight(int height) {
    _lineHeight = height;
}

void TextLayout::setLineHeightMultiplier(float mult) {
    _lineHeightMultiplier = mult;
}

void TextLayout::setParaSpacing(int spacing) {
    _paraSpacing = spacing;
}

void TextLayout::setUseParaIndent(bool useIndent) {
    _useParaIndent = useIndent;
}

void TextLayout::setJustify(bool enable) {
    _justify = enable;
}

void TextLayout::setFont(const GFXfont* font) {
    _font = font;
}

void TextLayout::setBoldFont(const GFXfont* font) {
    _fontBold = font;
}

void TextLayout::setItalicFont(const GFXfont* font) {
    _fontItalic = font;
}

void TextLayout::setBoldItalicFont(const GFXfont* font) {
    _fontBoldItalic = font;
}

void TextLayout::setCachePath(const String& path) {
    _cachePath = path;
}

// =============================================================================
// Reset
// =============================================================================

void TextLayout::reset() {
    _currentStyle = FontStyle::NORMAL;
    // Apply line height multiplier for actual line spacing
    int effectiveLineHeight = (int)(_lineHeight * _lineHeightMultiplier);
    _currentY = _marginTop + effectiveLineHeight;
    _pageCount = 0;
    _isHeaderLine = false;
    _isBulletLine = false;
    _currentLine.clear();
    _currentLineWidth = 0;
    _currentPage.clear();
    _wordBufferLen = 0;
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
}

// =============================================================================
// Streaming Input
// =============================================================================

void TextLayout::addText(const char* text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        
        // Check for whitespace (word boundary)
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            flushWord();
            
            // Newlines in source don't create line breaks
            // (HTML already converted to spaces in StreamingHtmlProcessor)
        }
        else {
            // Add character to word buffer
            if (_wordBufferLen < WORD_BUFFER_SIZE - 1) {
                _wordBuffer[_wordBufferLen++] = c;
                _wordBuffer[_wordBufferLen] = '\0';
            }
        }
    }
}

// =============================================================================
// Rich Text Parsing
// =============================================================================

void TextLayout::addRichText(const char* text, size_t len) {
    // Process text looking for markers
    // **bold**, *italic*, # headers, • bullets, [Image], [Table]
    
    size_t i = 0;
    FontStyle savedStyle = _currentStyle;
    
    // Check for header line (starts with "# ")
    if (len >= 2 && text[0] == '#' && text[1] == ' ') {
        _isHeaderLine = true;
        _currentStyle = FontStyle::BOLD;
        i = 2;  // Skip "# "
    }
    // Check for bullet line (starts with "• ")
    else if (len >= 4 && (uint8_t)text[0] == 0xE2 && (uint8_t)text[1] == 0x80 && (uint8_t)text[2] == 0xA2) {
        // UTF-8 bullet is E2 80 A2
        _isBulletLine = true;
        // Add bullet character as a word
        _wordBuffer[0] = text[0];
        _wordBuffer[1] = text[1];
        _wordBuffer[2] = text[2];
        _wordBufferLen = 3;
        _wordBuffer[3] = '\0';
        flushWord();
        i = 3;
        // Skip space after bullet if present
        if (i < len && text[i] == ' ') i++;
    }
    
    while (i < len) {
        char c = text[i];
        
        // Check for ** (bold marker)
        if (c == '*' && i + 1 < len && text[i + 1] == '*') {
            flushWord();
            // Toggle bold
            if (_currentStyle == FontStyle::BOLD || _currentStyle == FontStyle::BOLD_ITALIC) {
                _currentStyle = (_currentStyle == FontStyle::BOLD_ITALIC) ? FontStyle::ITALIC : FontStyle::NORMAL;
            } else {
                _currentStyle = (_currentStyle == FontStyle::ITALIC) ? FontStyle::BOLD_ITALIC : FontStyle::BOLD;
            }
            i += 2;
            continue;
        }
        
        // Check for * (italic marker) - but not **
        if (c == '*' && (i + 1 >= len || text[i + 1] != '*')) {
            flushWord();
            // Toggle italic
            if (_currentStyle == FontStyle::ITALIC || _currentStyle == FontStyle::BOLD_ITALIC) {
                _currentStyle = (_currentStyle == FontStyle::BOLD_ITALIC) ? FontStyle::BOLD : FontStyle::NORMAL;
            } else {
                _currentStyle = (_currentStyle == FontStyle::BOLD) ? FontStyle::BOLD_ITALIC : FontStyle::ITALIC;
            }
            i++;
            continue;
        }
        
        // Check for [Image] or [Image: ...] placeholder
        if (c == '[' && i + 5 < len && strncmp(&text[i], "[Image", 6) == 0) {
            flushWord();
            // Find closing ]
            size_t end = i + 6;
            while (end < len && text[end] != ']') end++;
            if (end < len) {
                // Add placeholder as italic text
                FontStyle prevStyle = _currentStyle;
                _currentStyle = FontStyle::ITALIC;
                // Copy just "[Image]"
                const char* placeholder = "[Image]";
                for (int j = 0; placeholder[j]; j++) {
                    if (_wordBufferLen < WORD_BUFFER_SIZE - 1) {
                        _wordBuffer[_wordBufferLen++] = placeholder[j];
                    }
                }
                _wordBuffer[_wordBufferLen] = '\0';
                flushWord();
                _currentStyle = prevStyle;
                i = end + 1;
                continue;
            }
        }
        
        // Check for [Table] placeholder
        if (c == '[' && i + 6 < len && strncmp(&text[i], "[Table]", 7) == 0) {
            flushWord();
            FontStyle prevStyle = _currentStyle;
            _currentStyle = FontStyle::ITALIC;
            const char* placeholder = "[Table]";
            for (int j = 0; placeholder[j]; j++) {
                if (_wordBufferLen < WORD_BUFFER_SIZE - 1) {
                    _wordBuffer[_wordBufferLen++] = placeholder[j];
                }
            }
            _wordBuffer[_wordBufferLen] = '\0';
            flushWord();
            _currentStyle = prevStyle;
            i += 7;
            continue;
        }
        
        // Check for whitespace (word boundary)
        if (c == ' ' || c == '\t') {
            flushWord();
            i++;
            continue;
        }
        
        // Check for soft hyphen (U+00AD = C2 AD in UTF-8) - preserve it
        if ((uint8_t)c == 0xC2 && i + 1 < len && (uint8_t)text[i + 1] == 0xAD) {
            if (_wordBufferLen < WORD_BUFFER_SIZE - 2) {
                _wordBuffer[_wordBufferLen++] = c;
                _wordBuffer[_wordBufferLen++] = text[i + 1];
                _wordBuffer[_wordBufferLen] = '\0';
            }
            i += 2;
            continue;
        }
        
        // Regular character - add to word buffer
        if (_wordBufferLen < WORD_BUFFER_SIZE - 1) {
            _wordBuffer[_wordBufferLen++] = c;
            _wordBuffer[_wordBufferLen] = '\0';
        }
        i++;
    }
    
    // Flush any remaining word
    flushWord();
    
    // Restore style for header lines
    if (_isHeaderLine) {
        _currentStyle = savedStyle;
    }
}

void TextLayout::endParagraph() {
    flushWord();
    
    // Finish current line (not justified at paragraph end)
    if (_currentLine.wordCount > 0) {
        finishLine(true);
    }
    
    // Reset header/bullet flags
    _isHeaderLine = false;
    _isBulletLine = false;
    
    // Apply line height multiplier
    int effectiveLineHeight = (int)(_lineHeight * _lineHeightMultiplier);
    
    // Add paragraph spacing (only if not using indent mode)
    if (!_useParaIndent && _paraSpacing > 0) {
        _currentY += _paraSpacing;
    }
    
    // Check for page overflow
    if (_currentY + effectiveLineHeight > _pageHeight - _marginBottom) {
        finishPage();
    }
}

void TextLayout::setCurrentStyle(FontStyle style) {
    // Flush current word before style change
    flushWord();
    _currentStyle = style;
}

void TextLayout::finalize() {
    // Flush any remaining word
    flushWord();
    
    // Finish current line
    if (_currentLine.wordCount > 0) {
        finishLine(true);
    }
    
    // Save current page if it has content
    if (_currentPage.lineCount > 0) {
        finishPage();
    }
    
    Serial.printf("[LAYOUT] Finalized: %d pages\n", _pageCount);
}

// =============================================================================
// Internal: Word Handling
// =============================================================================

void TextLayout::flushWord() {
    if (_wordBufferLen == 0) return;
    
    // Measure word width
    int wordWidth = getTextWidth(_wordBuffer, _currentStyle);
    int spaceWidth = getSpaceWidth();
    
    // Check if word fits on current line
    int neededWidth = wordWidth;
    if (_currentLine.wordCount > 0) {
        neededWidth += spaceWidth;  // Space before word
    }
    
    // DEBUG: Log when line breaks happen
    if (_currentLineWidth + neededWidth > _contentWidth && _currentLine.wordCount > 0) {
        Serial.printf("[LAYOUT] Line break: lineW=%d + needed=%d > contentW=%d, word='%s'\n",
            _currentLineWidth, neededWidth, _contentWidth, _wordBuffer);
    }
    
    if (_currentLineWidth + neededWidth > _contentWidth) {
        // Word doesn't fit, finish current line and start new
        if (_currentLine.wordCount > 0) {
            finishLine(false);
        }
    }
    
    // Add word to current line
    if (_currentLine.wordCount < MAX_WORDS_PER_LINE) {
        LayoutWord& word = _currentLine.words[_currentLine.wordCount];
        strncpy(word.text, _wordBuffer, WORD_BUFFER_SIZE - 1);
        word.width = wordWidth;
        word.style = _currentStyle;
        word.x = 0;  // Will be set during justification
        
        _currentLine.wordCount++;
        
        if (_currentLine.wordCount == 1) {
            _currentLineWidth = wordWidth;
        } else {
            _currentLineWidth += spaceWidth + wordWidth;
        }
        _currentLine.width = _currentLineWidth;
    }
    
    // Clear word buffer
    _wordBufferLen = 0;
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
}

// =============================================================================
// Internal: Line Handling
// =============================================================================

void TextLayout::finishLine(bool isParagraphEnd) {
    if (_currentLine.wordCount == 0) return;
    
    // Set Y position
    _currentLine.y = _currentY;
    
    // Calculate total line width for centering
    int totalLineWidth = 0;
    int spaceWidth = getSpaceWidth();
    for (int i = 0; i < _currentLine.wordCount; i++) {
        totalLineWidth += _currentLine.words[i].width;
        if (i < _currentLine.wordCount - 1) {
            totalLineWidth += spaceWidth;
        }
    }
    
    // Header lines: center the text
    if (_isHeaderLine) {
        int startX = _marginLeft + (_contentWidth - totalLineWidth) / 2;
        int x = startX;
        for (int i = 0; i < _currentLine.wordCount; i++) {
            _currentLine.words[i].x = x;
            x += _currentLine.words[i].width;
            if (i < _currentLine.wordCount - 1) {
                x += spaceWidth;
            }
        }
    }
    // Apply justification if enabled and not paragraph end
    else if (_justify && !isParagraphEnd && _currentLine.wordCount > 1) {
        justifyLine(_currentLine);
    } else {
        // Left-align: position words with standard spacing
        int x = _marginLeft;
        
        for (int i = 0; i < _currentLine.wordCount; i++) {
            _currentLine.words[i].x = x;
            x += _currentLine.words[i].width;
            if (i < _currentLine.wordCount - 1) {
                x += spaceWidth;
            }
        }
    }
    
    // Log first line of each page for debugging
    static int linesLogged = 0;
    if (_currentPage.lineCount == 0 && linesLogged < 5) {
        Serial.printf("[LAYOUT] First line: y=%d, %d words, marginL=%d, contentW=%d\n", 
                     _currentLine.y, _currentLine.wordCount, _marginLeft, _contentWidth);
        if (_currentLine.wordCount > 0) {
            Serial.printf("[LAYOUT]   Word 0: x=%d w=%d '%s'\n", 
                         _currentLine.words[0].x, _currentLine.words[0].width, _currentLine.words[0].text);
        }
        linesLogged++;
    }
    
    // Add line to current page
    if (_currentPage.lineCount < MAX_LINES_PER_PAGE) {
        _currentPage.lines[_currentPage.lineCount] = _currentLine;
        _currentPage.lineCount++;
    }
    
    // Apply line height multiplier for actual spacing
    int effectiveLineHeight = (int)(_lineHeight * _lineHeightMultiplier);
    
    // Advance Y position
    _currentY += effectiveLineHeight;
    
    // Check for page overflow
    if (_currentY + effectiveLineHeight > _pageHeight - _marginBottom) {
        finishPage();
    }
    
    // Reset current line
    _currentLine.clear();
    _currentLineWidth = 0;
}

void TextLayout::justifyLine(LayoutLine& line) {
    if (line.wordCount <= 1) return;
    
    // Calculate extra space to distribute
    int totalWordWidth = 0;
    for (int i = 0; i < line.wordCount; i++) {
        totalWordWidth += line.words[i].width;
    }
    
    int extraSpace = _contentWidth - totalWordWidth;
    int gaps = line.wordCount - 1;
    
    if (gaps <= 0 || extraSpace <= 0) {
        // Fall back to left-align
        int x = _marginLeft;
        int spaceWidth = getSpaceWidth();
        for (int i = 0; i < line.wordCount; i++) {
            line.words[i].x = x;
            x += line.words[i].width + spaceWidth;
        }
        return;
    }
    
    // Distribute space evenly
    float spacePerGap = (float)extraSpace / gaps;
    float x = _marginLeft;
    
    for (int i = 0; i < line.wordCount; i++) {
        line.words[i].x = (int16_t)(x + 0.5f);
        x += line.words[i].width;
        if (i < line.wordCount - 1) {
            x += spacePerGap;
        }
    }
    
    line.justified = true;
}

// =============================================================================
// Internal: Page Handling
// =============================================================================

void TextLayout::finishPage() {
    if (_currentPage.lineCount == 0) return;
    
    _currentPage.pageNumber = _pageCount;
    
    // Save page to cache
    savePageToCache(_currentPage);
    
    _pageCount++;
    
    // Reset for new page with effective line height
    int effectiveLineHeight = (int)(_lineHeight * _lineHeightMultiplier);
    _currentPage.clear();
    _currentY = _marginTop + effectiveLineHeight;
}

void TextLayout::savePageToCache(const LayoutPage& page) {
    if (_cachePath.length() == 0) {
        _cachePath = "/.sumi/temp_pages";
    }
    
    // Ensure directory exists
    if (!SD.exists(_cachePath)) {
        SD.mkdir(_cachePath);
    }
    
    // Create page file
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/page_%04d.bin", _cachePath.c_str(), page.pageNumber);
    
    File f = SD.open(filename, FILE_WRITE);
    if (f) {
        page.saveTo(f);
        f.close();
        Serial.printf("[LAYOUT] Saved page %d to %s (%d lines)\n", page.pageNumber, filename, page.lineCount);
    } else {
        Serial.printf("[LAYOUT] Failed to save page %d to %s\n", page.pageNumber, filename);
    }
}

bool TextLayout::loadPageFromCache(int pageNum, LayoutPage& out) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/page_%04d.bin", _cachePath.c_str(), pageNum);
    
    File f = SD.open(filename, FILE_READ);
    if (!f) {
        Serial.printf("[LAYOUT] Page %d not found in cache\n", pageNum);
        return false;
    }
    
    bool ok = out.loadFrom(f);
    f.close();
    
    return ok;
}

// =============================================================================
// Rendering
// =============================================================================

void TextLayout::renderPage(int pageNum, GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& display) {
    if (pageNum < 0 || pageNum >= _pageCount) {
        Serial.printf("[LAYOUT] Invalid page: %d (max %d)\n", pageNum, _pageCount);
        return;
    }
    
    LayoutPage page;
    if (!loadPageFromCache(pageNum, page)) {
        Serial.printf("[LAYOUT] Failed to load page %d from cache path: %s\n", pageNum, _cachePath.c_str());
        return;
    }
    
    // Only log once (not for each buffer pass)
    static int lastLoggedPage = -1;
    static bool debugLogDone = false;
    if (pageNum != lastLoggedPage) {
        Serial.printf("[LAYOUT] Rendering page %d: %d lines\n", pageNum, page.lineCount);
        if (page.lineCount > 0) {
            const LayoutLine& line = page.lines[0];
            Serial.printf("[LAYOUT] Line 0: y=%d, %d words\n", line.y, line.wordCount);
            for (int w = 0; w < min((int)line.wordCount, 5); w++) {
                Serial.printf("[LAYOUT]   Word %d: x=%d w=%d '%s'\n", 
                    w, line.words[w].x, line.words[w].width, line.words[w].text);
            }
        }
        lastLoggedPage = pageNum;
        debugLogDone = false;
    }
    
    display.setTextColor(GxEPD_BLACK);
    
    for (int i = 0; i < page.lineCount; i++) {
        const LayoutLine& line = page.lines[i];
        
        for (int j = 0; j < line.wordCount; j++) {
            const LayoutWord& word = line.words[j];
            
            // Set font for style
            display.setFont(getFontForStyle(word.style));
            
            // Draw word at its calculated position
            display.setCursor(word.x, line.y);
            display.print(word.text);
        }
    }
}

// =============================================================================
// Helpers
// =============================================================================

int TextLayout::getTextWidth(const char* text, FontStyle style) {
    if (!_measureDisplay) {
        // Rough estimate: 8 pixels per character
        static bool warnedOnce = false;
        if (!warnedOnce) {
            Serial.println("[LAYOUT] WARNING: No display for text measurement, using rough estimate!");
            warnedOnce = true;
        }
        return strlen(text) * 8;
    }
    
    _measureDisplay->setFont(getFontForStyle(style));
    
    int16_t x1, y1;
    uint16_t w, h;
    _measureDisplay->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    
    return w;
}

int TextLayout::getSpaceWidth() {
    // getTextBounds(" ") returns ~0 for spaces, so we calculate based on character width
    // Measure "n" vs "n n" to get actual space width, or use fraction of font height
    if (!_measureDisplay) {
        return 5;  // Fallback
    }
    
    // Use roughly 1/3 of the typical character width as space
    // Measure a reference character and derive space from it
    int16_t x1, y1;
    uint16_t w1, w2, h;
    
    _measureDisplay->setFont(getFontForStyle(FontStyle::NORMAL));
    _measureDisplay->getTextBounds("n", 0, 0, &x1, &y1, &w1, &h);
    _measureDisplay->getTextBounds("n n", 0, 0, &x1, &y1, &w2, &h);
    
    int spaceWidth = w2 - (2 * w1);
    
    // DEBUG: Log the space width calculation
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Serial.printf("[LAYOUT] SpaceWidth: 'n'=%d, 'n n'=%d, raw=%d\n", w1, w2, spaceWidth);
        loggedOnce = true;
    }
    
    // IMPORTANT: getTextBounds returns bounding box, not advance width.
    // Actual rendered spacing is tighter. Reduce by 50% to pack words better.
    // This allows more words per line before justification spreads them out.
    spaceWidth = spaceWidth / 2;
    
    // Sanity check - space should be at least 3 pixels, at most 8
    if (spaceWidth < 3) spaceWidth = w1 / 3;  // Third of character width
    if (spaceWidth > 8) spaceWidth = 8;
    
    return spaceWidth;
}

const GFXfont* TextLayout::getFontForStyle(FontStyle style) {
    switch (style) {
        case FontStyle::BOLD:
            return _fontBold ? _fontBold : _font;
        case FontStyle::ITALIC:
            return _fontItalic ? _fontItalic : _font;
        case FontStyle::BOLD_ITALIC:
            return _fontBoldItalic ? _fontBoldItalic : (_fontBold ? _fontBold : _font);
        default:
            return _font;
    }
}
