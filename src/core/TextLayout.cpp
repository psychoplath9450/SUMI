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
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

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
    , _marginLeft(10)          // Tight margins (screenMargin 5 + base)
    , _marginRight(10)
    , _marginTop(10)
    , _marginBottom(30)        // Extra space for status bar
    , _contentWidth(460)       // 480 - 10 - 10
    , _contentHeight(760)      // 800 - 10 - 30
    , _lineHeight(40)          // Default for 18pt font
    , _lineHeightMultiplier(1.0f)  // Normal = 1.0
    , _paraSpacing(20)         // Half line height when extra spacing enabled
    , _useParaIndent(false)    // When true, use indent instead of spacing
    , _justify(true)
    , _font(&FreeSans18pt7b)   // Default to 18pt
    , _fontBold(&FreeSansBold18pt7b)
    , _fontItalic(&FreeSans18pt7b)  // Fallback if no italic
    , _fontBoldItalic(&FreeSansBold18pt7b)  // Fallback to bold
    , _currentStyle(FontStyle::NORMAL)
    , _currentY(0)
    , _pageCount(0)
    , _isHeaderLine(false)
    , _isBulletLine(false)
    , _nextLineIsParaStart(false)
    , _cachedSpaceWidth(0)     // Will be calculated on first use
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
    Serial.printf("[LAYOUT] setMargins: L=%d R=%d T=%d B=%d, page=%dx%d, content=%dx%d\n",
        left, right, top, bottom, _pageWidth, _pageHeight, _contentWidth, _contentHeight);
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
    _nextLineIsParaStart = false;
    _cachedSpaceWidth = 0;  // Reset so it recalculates for new chapter
    _currentLine.clear();
    _currentLineWidth = 0;
    _currentPage.clear();
    _wordBufferLen = 0;
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
    
    // Clear page info array
    for (int i = 0; i < MAX_PAGES_PER_CHAPTER; i++) {
        _pageInfos[i].setText();
    }
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
            // (text is preprocessed by portal - newlines are just whitespace)
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
    bool thisLineIsHeader = false;
    
    // Check for header line (starts with "# ")
    if (len >= 2 && text[0] == '#' && text[1] == ' ') {
        // If there's a previous header that wasn't closed, end it first
        if (_isHeaderLine && _currentLine.wordCount > 0) {
            finishLine(true);
        }
        _isHeaderLine = true;
        thisLineIsHeader = true;
        _currentStyle = FontStyle::BOLD;
        i = 2;  // Skip "# "
    }
    // Check for bullet line (starts with "• ")
    else if (len >= 4 && (uint8_t)text[0] == 0xE2 && (uint8_t)text[1] == 0x80 && (uint8_t)text[2] == 0xA2) {
        // UTF-8 bullet is E2 80 A2
        // If previous line was a header, end it first
        if (_isHeaderLine) {
            if (_currentLine.wordCount > 0) {
                finishLine(true);
            }
            _isHeaderLine = false;
        }
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
    else {
        // Regular text line (not header, not bullet)
        // If previous line was a header, end that paragraph first
        if (_isHeaderLine) {
            if (_currentLine.wordCount > 0) {
                finishLine(true);  // End the header line
            }
            _isHeaderLine = false;
            // Add paragraph spacing after header
            if (_paraSpacing > 0) {
                _currentY += _paraSpacing;
            }
        }
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
        
        // Check for [!IMG:filename,w=X,h=Y] marker (from portal preprocessing)
        // This creates a dedicated full-page image display
        if (c == '[' && i + 5 < len && strncmp(&text[i], "[!IMG:", 6) == 0) {
            flushWord();
            
            // Parse the marker: [!IMG:img_001.jpg,w=300,h=400]
            size_t end = i + 6;
            while (end < len && text[end] != ']') end++;
            
            if (end < len) {
                // Extract filename and dimensions
                char imgFilename[IMAGE_PATH_SIZE] = {0};
                int imgWidth = 300, imgHeight = 400;  // Defaults
                
                // Parse: img_001.jpg,w=300,h=400
                size_t pos = i + 6;
                size_t fnEnd = pos;
                while (fnEnd < end && text[fnEnd] != ',') fnEnd++;
                
                size_t fnLen = fnEnd - pos;
                if (fnLen > 0 && fnLen < IMAGE_PATH_SIZE) {
                    memcpy(imgFilename, &text[pos], fnLen);
                    imgFilename[fnLen] = '\0';
                }
                
                // Parse w= and h= if present
                const char* wPos = strstr(&text[pos], ",w=");
                if (wPos && wPos < &text[end]) {
                    imgWidth = atoi(wPos + 3);
                }
                const char* hPos = strstr(&text[pos], ",h=");
                if (hPos && hPos < &text[end]) {
                    imgHeight = atoi(hPos + 3);
                }
                
                // Create a dedicated image page
                if (imgFilename[0] != '\0') {
                    Serial.printf("[LAYOUT] Creating image page: %s (%dx%d)\n", 
                        imgFilename, imgWidth, imgHeight);
                    createImagePage(imgFilename, imgWidth, imgHeight);
                }
                
                i = end + 1;
                continue;
            }
        }
        
        // Check for legacy [Image] or [Image: ...] placeholder (backwards compatibility)
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
    if (thisLineIsHeader) {
        _currentStyle = savedStyle;
        // Force end of line after header - header should be on its own line
        if (_currentLine.wordCount > 0) {
            finishLine(true);
        }
        // Add paragraph spacing after header
        if (_paraSpacing > 0) {
            _currentY += _paraSpacing;
        }
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
    
    // Mark that the next line should be indented (paragraph start)
    _nextLineIsParaStart = true;
    Serial.println("[LAYOUT] endParagraph() - next line will be indented");
    
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
    
    // Paragraph indent (about 3 em widths)
    const int PARA_INDENT = 30;
    
    // Check if this is the first word of a new paragraph
    bool applyIndent = (_currentLine.wordCount == 0 && _nextLineIsParaStart && !_isHeaderLine && !_isBulletLine);
    
    // Debug: log when indent would be applied
    if (_currentLine.wordCount == 0 && _nextLineIsParaStart) {
        Serial.printf("[LAYOUT] Para start: indent=%s, header=%d, bullet=%d, word='%.10s'\n",
            applyIndent ? "YES" : "NO", _isHeaderLine, _isBulletLine, _wordBuffer);
    }
    
    // Check if word fits on current line
    int neededWidth = wordWidth;
    if (_currentLine.wordCount > 0) {
        neededWidth += spaceWidth;  // Space before word
    } else if (applyIndent) {
        neededWidth += PARA_INDENT;  // Account for indent on first word
    }
    
    if (_currentLineWidth + neededWidth > _contentWidth) {
        // Word doesn't fit, finish current line and start new
        if (_currentLine.wordCount > 0) {
            finishLine(false);
        }
        // Recalculate - new line doesn't get paragraph indent (only first line of para)
        applyIndent = false;
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
            if (applyIndent) {
                _currentLine.indent = PARA_INDENT;  // Mark this line as indented
                _currentLineWidth = PARA_INDENT + wordWidth;
                _nextLineIsParaStart = false;  // Clear flag after applying
            } else {
                _currentLine.indent = 0;
                _currentLineWidth = wordWidth;
                // Also clear flag if first word but not applying indent (e.g., header)
                if (_nextLineIsParaStart) {
                    _nextLineIsParaStart = false;
                }
            }
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
        // Left-align: position words with standard spacing (+ indent if applicable)
        int x = _marginLeft + _currentLine.indent;
        
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
        Serial.printf("[LAYOUT] First line: y=%d, %d words, marginL=%d, contentW=%d, indent=%d\n", 
                     _currentLine.y, _currentLine.wordCount, _marginLeft, _contentWidth, _currentLine.indent);
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
    
    // Available width is reduced by indent
    int availableWidth = _contentWidth - line.indent;
    int extraSpace = availableWidth - totalWordWidth;
    int gaps = line.wordCount - 1;
    
    if (gaps <= 0 || extraSpace <= 0) {
        // Fall back to left-align
        int x = _marginLeft + line.indent;
        int spaceWidth = getSpaceWidth();
        for (int i = 0; i < line.wordCount; i++) {
            line.words[i].x = x;
            x += line.words[i].width + spaceWidth;
        }
        return;
    }
    
    // Calculate space per gap
    float spacePerGap = (float)extraSpace / gaps;
    
    // Cap maximum gap size at 2.0x normal space width to avoid huge gaps
    // This happens when a line has few words (e.g. end of paragraph or short lines)
    int normalSpace = getSpaceWidth();
    float maxGap = normalSpace * 2.0f;
    
    if (spacePerGap > maxGap) {
        // Too much space - fall back to left-align
        int x = _marginLeft + line.indent;
        for (int i = 0; i < line.wordCount; i++) {
            line.words[i].x = x;
            x += line.words[i].width + normalSpace;
        }
        return;
    }
    
    // Distribute space evenly
    float x = _marginLeft + line.indent;
    
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
    
    // Mark this as a text page
    if (_pageCount < MAX_PAGES_PER_CHAPTER) {
        _pageInfos[_pageCount].setText();
    }
    
    // Save page to cache
    savePageToCache(_currentPage);
    
    _pageCount++;
    
    // Reset for new page with effective line height
    int effectiveLineHeight = (int)(_lineHeight * _lineHeightMultiplier);
    _currentPage.clear();
    _currentY = _marginTop + effectiveLineHeight;
}

void TextLayout::createImagePage(const char* imagePath, int width, int height) {
    // First, finish any partially-filled text page
    // Even if only a few lines, show them before the image
    if (_currentLine.wordCount > 0) {
        finishLine(false);
    }
    if (_currentPage.lineCount > 0) {
        finishPage();
    }
    
    // Now create an image-only page
    if (_pageCount < MAX_PAGES_PER_CHAPTER) {
        _pageInfos[_pageCount].setImage(imagePath, width, height);
        
        // Create an empty page file for this slot (needed for page count consistency)
        // Static to avoid stack overflow - LayoutPage is large
        static LayoutPage emptyPage;
        emptyPage.clear();
        emptyPage.pageNumber = _pageCount;
        emptyPage.lineCount = 0;
        savePageToCache(emptyPage);
        
        Serial.printf("[LAYOUT] Image page %d: %s (%dx%d)\n", _pageCount, imagePath, width, height);
        _pageCount++;
    }
    
    // Reset for next text page
    int effectiveLineHeight = (int)(_lineHeight * _lineHeightMultiplier);
    _currentPage.clear();
    _currentY = _marginTop + effectiveLineHeight;
}

PageType TextLayout::getPageType(int pageNum) const {
    if (pageNum < 0 || pageNum >= MAX_PAGES_PER_CHAPTER) {
        return PageType::TEXT;
    }
    return _pageInfos[pageNum].type;
}

bool TextLayout::getImageInfo(int pageNum, char* pathOut, size_t pathSize, int* widthOut, int* heightOut) const {
    if (pageNum < 0 || pageNum >= MAX_PAGES_PER_CHAPTER) {
        return false;
    }
    if (_pageInfos[pageNum].type != PageType::IMAGE) {
        return false;
    }
    
    if (pathOut && pathSize > 0) {
        strncpy(pathOut, _pageInfos[pageNum].imagePath, pathSize - 1);
        pathOut[pathSize - 1] = '\0';
    }
    if (widthOut) *widthOut = _pageInfos[pageNum].imageWidth;
    if (heightOut) *heightOut = _pageInfos[pageNum].imageHeight;
    
    return true;
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
    
    // Static to avoid stack overflow - LayoutPage is large (~15KB)
    static LayoutPage page;
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
    // Return cached value if already calculated
    if (_cachedSpaceWidth > 0) {
        return _cachedSpaceWidth;
    }
    
    // Calculate and cache the space width
    if (!_measureDisplay) {
        _cachedSpaceWidth = 8;  // Fallback
        return _cachedSpaceWidth;
    }
    
    // Use the natural space width from font metrics
    int16_t x1, y1;
    uint16_t w1, w2, h;
    
    _measureDisplay->setFont(getFontForStyle(FontStyle::NORMAL));
    _measureDisplay->getTextBounds("n", 0, 0, &x1, &y1, &w1, &h);
    _measureDisplay->getTextBounds("n n", 0, 0, &x1, &y1, &w2, &h);
    
    int spaceWidth = w2 - (2 * w1);
    
    Serial.printf("[LAYOUT] SpaceWidth: 'n'=%d, 'n n'=%d, raw=%d\n", w1, w2, spaceWidth);
    
    // Use the actual measured space width, with reasonable bounds
    if (spaceWidth < 5) spaceWidth = 5;
    if (spaceWidth > 12) spaceWidth = 12;
    
    _cachedSpaceWidth = spaceWidth;
    Serial.printf("[LAYOUT] Cached spaceWidth = %d (will be used for entire chapter)\n", _cachedSpaceWidth);
    
    return _cachedSpaceWidth;
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
