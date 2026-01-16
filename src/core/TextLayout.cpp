/**
 * @file TextLayout.cpp
 * @brief Text Layout Engine Implementation
 * @version 2.1.27
 *
 * Pre-computed text layout with word positions:
 * 1. Measuring all words once using getTextBounds()
 * 2. Computing line breaks
 * 3. Calculating justified positions
 * 4. Storing positions for instant rendering
 * 
 * FIXED v2.1.27: Text measurement now uses display.getTextBounds() properly
 * with wrapping disabled for accurate word width calculation.
 */

#include "core/TextLayout.h"
#include <Esp.h>  // For ESP.getFreeHeap()

// Global instance
TextLayout textLayout;

// =============================================================================
// Constructor
// =============================================================================

TextLayout::TextLayout() 
    : _pageWidth(800), _pageHeight(480),
      _marginLeft(20), _marginRight(20), _marginTop(40), _marginBottom(40),
      _contentWidth(760), _contentHeight(400),
      _lineHeight(22), _paraSpacing(8), _spaceWidth(6), _linesPerPage(18),
      _defaultAlign(TextAlign::LEFT), _justify(false),
      _font(nullptr), _currentY(0), _currentOffset(0), _inLayout(false) {
    updateMetrics();
}

// =============================================================================
// Configuration
// =============================================================================

void TextLayout::setPageSize(int width, int height) {
    Serial.printf("[LAYOUT] setPageSize(%d, %d)\n", width, height);
    _pageWidth = width;
    _pageHeight = height;
    updateMetrics();
}

void TextLayout::setMargins(int left, int right, int top, int bottom) {
    Serial.printf("[LAYOUT] setMargins(L=%d, R=%d, T=%d, B=%d)\n", left, right, top, bottom);
    _marginLeft = left;
    _marginRight = right;
    _marginTop = top;
    _marginBottom = bottom;
    updateMetrics();
}

void TextLayout::setLineHeight(int height) {
    _lineHeight = height;
    updateMetrics();
}

void TextLayout::setParaSpacing(int spacing) {
    _paraSpacing = spacing;
}

void TextLayout::setDefaultAlign(TextAlign align) {
    _defaultAlign = align;
}

void TextLayout::setJustify(bool justify) {
    _justify = justify;
    _defaultAlign = justify ? TextAlign::JUSTIFY : TextAlign::LEFT;
}

void TextLayout::setFont(const GFXfont* font) {
    _font = font;
    
    if (_font) {
        // Set font on display for measurement
        display.setFont(_font);
        
        // Measure actual space width using getTextBounds
        int16_t x1, y1;
        uint16_t w, h;
        display.setTextWrap(false);
        display.getTextBounds(" ", 0, 0, &x1, &y1, &w, &h);
        
        // Space often returns 0 width, so use a character-based estimate
        if (w < 2) {
            // Measure "i i" vs "ii" to get space width
            uint16_t w1, w2;
            display.getTextBounds("i i", 0, 0, &x1, &y1, &w1, &h);
            display.getTextBounds("ii", 0, 0, &x1, &y1, &w2, &h);
            _spaceWidth = w1 - w2;
            
            // Fallback if still bad
            if (_spaceWidth < 2 || _spaceWidth > 20) {
                _spaceWidth = _font->yAdvance / 3;
            }
        } else {
            _spaceWidth = w;
        }
        
        // Sanity check
        if (_spaceWidth < 3) _spaceWidth = 3;
        if (_spaceWidth > 15) _spaceWidth = 8;
        
        Serial.printf("[LAYOUT] setFont: yAdvance=%d, spaceWidth=%d\n", 
                      _font->yAdvance, _spaceWidth);
    } else {
        _spaceWidth = 6;
        Serial.println("[LAYOUT] setFont: font is NULL, using default spaceWidth=6");
    }
}

void TextLayout::updateMetrics() {
    // Add buffer for glyph overhang - glyphs can extend past xAdvance
    // This prevents text clipping on the right edge
    const int GLYPH_OVERHANG_BUFFER = 8;  // Extra pixels to prevent clipping
    
    _contentWidth = _pageWidth - _marginLeft - _marginRight - GLYPH_OVERHANG_BUFFER;
    _contentHeight = _pageHeight - _marginTop - _marginBottom;
    
    // Sanity checks - ensure reasonable values
    if (_contentWidth < 100) {
        Serial.printf("[LAYOUT] WARNING: contentWidth too small (%d), using minimum\n", _contentWidth);
        _contentWidth = _pageWidth - 40;  // Force minimal margins
        _marginLeft = 20;
        _marginRight = 20;
    }
    if (_contentHeight < 100) {
        Serial.printf("[LAYOUT] WARNING: contentHeight too small (%d), using minimum\n", _contentHeight);
        _contentHeight = _pageHeight - 60;
        _marginTop = 20;
        _marginBottom = 40;
    }
    
    _linesPerPage = _contentHeight / _lineHeight;
    if (_linesPerPage < 5) _linesPerPage = 5;  // Minimum lines
    
    Serial.printf("[LAYOUT] Page: %dx%d, Margins: L=%d R=%d T=%d B=%d, Content: %dx%d, Lines/page: %d\n",
                  _pageWidth, _pageHeight, _marginLeft, _marginRight, _marginTop, _marginBottom,
                  _contentWidth, _contentHeight, _linesPerPage);
}

// =============================================================================
// Main Layout Method
// =============================================================================

std::vector<CachedPage> TextLayout::paginateText(const String& text) {
    beginLayout();
    
    // Split into paragraphs
    std::vector<String> paragraphs = splitParagraphs(text);
    Serial.printf("[LAYOUT] Processing %d paragraphs\n", (int)paragraphs.size());
    
    // Process each paragraph
    for (const String& para : paragraphs) {
        if (para.length() > 0) {
            addParagraph(para, false);
        }
    }
    
    // Get final page
    CachedPage lastPage = finishLayout();
    
    // Get all completed pages
    std::vector<CachedPage> result = getCompletedPages();
    
    // Add the last page if it has content
    if (lastPage.lineCount > 0) {
        result.push_back(lastPage);
    }
    
    Serial.printf("[LAYOUT] Created %d pages\n", (int)result.size());
    return result;
}

// =============================================================================
// Incremental Layout
// =============================================================================

void TextLayout::beginLayout() {
    Serial.println("[LAYOUT] ========================================");
    Serial.println("[LAYOUT] beginLayout called - STARTING FRESH");
    Serial.printf("[LAYOUT]   pageSize: %dx%d\n", _pageWidth, _pageHeight);
    Serial.printf("[LAYOUT]   margins: L=%d R=%d T=%d B=%d\n", _marginLeft, _marginRight, _marginTop, _marginBottom);
    Serial.printf("[LAYOUT]   contentSize: %dx%d\n", _contentWidth, _contentHeight);
    Serial.printf("[LAYOUT]   lineHeight=%d, linesPerPage=%d\n", _lineHeight, _linesPerPage);
    Serial.printf("[LAYOUT]   spaceWidth=%d\n", _spaceWidth);
    if (_font) {
        Serial.printf("[LAYOUT]   font yAdvance=%d\n", _font->yAdvance);
    } else {
        Serial.println("[LAYOUT]   WARNING: _font is NULL!");
    }
    Serial.println("[LAYOUT] ========================================");
    
    _currentPage.clear();
    // Start Y at marginTop + lineHeight so first line's baseline allows
    // room for text above it (GxEPD2 cursor Y is baseline, not top)
    _currentY = _marginTop + _lineHeight;
    _currentOffset = 0;
    _completedPages.clear();
    _inLayout = true;
    
    Serial.printf("[LAYOUT]   Starting _currentY=%d (baseline for first line)\n", _currentY);
}

void TextLayout::addParagraph(const String& para, bool isHeader) {
    if (!_inLayout) beginLayout();
    
    // Safety check - limit paragraph size to prevent memory exhaustion
    String safePara = para;
    if (safePara.length() > 2000) {
        Serial.printf("[LAYOUT] Warning: Truncating long paragraph from %d to 2000 chars\n", safePara.length());
        safePara = safePara.substring(0, 2000);
    }
    
    // Check available memory before processing
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 30000) {
        Serial.printf("[LAYOUT] Low memory (%d bytes), skipping paragraph\n", freeHeap);
        return;
    }
    
    // Measure words
    std::vector<MeasuredWord> words = measureWords(safePara);
    if (words.empty()) return;
    
    // Mark last word as ending paragraph
    if (!words.empty()) {
        words.back().endsParagraph = true;
    }
    
    // Wrap into lines
    std::vector<LayoutLine> lines = wrapToLines(words);
    
    // Mark last line of paragraph
    if (!lines.empty()) {
        lines.back().isLastInPara = true;
        lines.back().isHeader = isHeader;
    }
    
    // Add each line to pages
    for (const LayoutLine& line : lines) {
        CachedLine cachedLine = positionLine(line, _currentY);
        
        if (!addLineToPage(cachedLine)) {
            // Didn't fit - start new page and try again
            completePage();
            newPage();
            // Re-position line with new Y after page break
            cachedLine = positionLine(line, _currentY);
            addLineToPage(cachedLine);
        }
        
        _currentY += _lineHeight;
    }
    
    // Add paragraph spacing
    _currentY += _paraSpacing;
}

std::vector<CachedPage> TextLayout::getCompletedPages() {
    std::vector<CachedPage> result = std::move(_completedPages);
    _completedPages.clear();
    return result;
}

CachedPage TextLayout::finishLayout() {
    _inLayout = false;
    CachedPage lastPage = _currentPage;
    _currentPage.clear();
    return lastPage;
}

// =============================================================================
// Text Measurement - Uses getTextBounds() properly
// =============================================================================
// 
// Key insight from GxEPD2 examples and other e-ink projects:
// - getTextBounds() must be called with wrapping DISABLED
// - Measure at position (0,0) for consistent results
// - The returned width (w) is what we need

int TextLayout::measureText(const String& text) {
    if (text.length() == 0) {
        return 0;
    }
    
    if (!_font) {
        // Fallback for no font - rough estimate
        return text.length() * 8;
    }
    
    // Use display.getTextBounds() for accurate measurement
    // This is the standard approach used by GxEPD2 examples
    
    // Disable wrapping - critical for accurate measurement!
    // (We always render with wrapping off for our layout system)
    display.setTextWrap(false);
    
    // Ensure font is set
    display.setFont(_font);
    
    // Measure at origin
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    
    // Return the width
    return (int)w;
}

// =============================================================================
// Internal: Paragraph Splitting
// =============================================================================

std::vector<String> TextLayout::splitParagraphs(const String& text) {
    std::vector<String> result;
    
    int start = 0;
    int len = text.length();
    
    for (int i = 0; i < len; i++) {
        char c = text[i];
        
        // Detect paragraph breaks (double newline or single newline)
        if (c == '\n') {
            // Check for double newline
            bool doubleLine = (i + 1 < len && text[i + 1] == '\n');
            
            // Extract paragraph
            String para = text.substring(start, i);
            para.trim();
            if (para.length() > 0) {
                result.push_back(para);
            }
            
            // Skip past newlines
            if (doubleLine) i++;
            start = i + 1;
        }
    }
    
    // Handle final paragraph
    if (start < len) {
        String para = text.substring(start);
        para.trim();
        if (para.length() > 0) {
            result.push_back(para);
        }
    }
    
    return result;
}

// =============================================================================
// Internal: Word Measurement
// =============================================================================

std::vector<MeasuredWord> TextLayout::measureWords(const String& para) {
    std::vector<MeasuredWord> result;
    
    int start = 0;
    int len = para.length();
    
    // Reset logging flag at start of each layout session
    static int paraCount = 0;
    bool shouldLog = (paraCount < 2);  // Log first 2 paragraphs
    
    if (shouldLog) {
        Serial.printf("[MEASURE] ===== Para %d: %d chars, contentWidth=%d =====\n", 
                      paraCount, len, _contentWidth);
        paraCount++;
    }
    
    for (int i = 0; i <= len; i++) {
        char c = (i < len) ? para[i] : ' ';
        
        // Split on whitespace
        if (c == ' ' || c == '\t' || i == len) {
            if (i > start) {
                String wordText = para.substring(start, i);
                int width = measureText(wordText);
                
                // Log measurements for debugging
                if (shouldLog && result.size() < 10) {
                    Serial.printf("[MEASURE]   '%s' -> %d px\n", wordText.c_str(), width);
                }
                
                MeasuredWord mw(wordText, width, FontStyle::REGULAR);
                mw.endsWithSpace = (c == ' ');
                result.push_back(mw);
            }
            start = i + 1;
        }
    }
    
    if (shouldLog) {
        Serial.printf("[MEASURE] Total: %d words\n", (int)result.size());
    }
    
    return result;
}

// =============================================================================
// Internal: Line Wrapping
// =============================================================================

std::vector<LayoutLine> TextLayout::wrapToLines(const std::vector<MeasuredWord>& words) {
    std::vector<LayoutLine> result;
    
    if (words.empty()) return result;
    
    LayoutLine currentLine;
    currentLine.spaceWidth = _spaceWidth;
    currentLine.align = _defaultAlign;
    
    for (const MeasuredWord& word : words) {
        // Check if word fits on current line
        int lineWidthWithWord = currentLine.naturalWidth();
        if (currentLine.wordCount() > 0) {
            lineWidthWithWord += _spaceWidth + word.width;
        } else {
            lineWidthWithWord = word.width;
        }
        
        if (lineWidthWithWord <= _contentWidth || currentLine.wordCount() == 0) {
            // Word fits (or line is empty - must add at least one word)
            currentLine.addWord(word);
        } else {
            // Word doesn't fit - finish current line and start new one
            result.push_back(currentLine);
            
            currentLine = LayoutLine();
            currentLine.spaceWidth = _spaceWidth;
            currentLine.align = _defaultAlign;
            currentLine.addWord(word);
        }
    }
    
    // Handle final line
    if (currentLine.wordCount() > 0) {
        result.push_back(currentLine);
    }
    
    return result;
}

// =============================================================================
// Internal: Word Positioning (Justification)
// =============================================================================

CachedLine TextLayout::positionLine(const LayoutLine& line, int y) {
    CachedLine result;
    result.yPos = y;
    result.setLastInPara(line.isLastInPara);
    
    if (line.words.empty()) return result;
    
    // Debug: log layout parameters once
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Serial.printf("[LAYOUT] positionLine: marginLeft=%d, contentWidth=%d, pageWidth=%d\n",
                      _marginLeft, _contentWidth, _pageWidth);
        loggedOnce = true;
    }
    
    // Calculate positions based on alignment
    std::vector<int> positions = calculateJustifiedPositions(
        line.words, _contentWidth, line.isLastInPara);
    
    // Create cached words with positions
    for (size_t i = 0; i < line.words.size() && i < positions.size(); i++) {
        CachedWord cw;
        cw.xPos = _marginLeft + positions[i];
        cw.style = static_cast<uint8_t>(line.words[i].style);
        
        // Copy text
        int textLen = line.words[i].text.length();
        if (textLen > CACHE_MAX_WORD_LEN - 1) textLen = CACHE_MAX_WORD_LEN - 1;
        strncpy(cw.text, line.words[i].text.c_str(), textLen);
        cw.text[textLen] = '\0';
        cw.length = textLen;
        
        result.addWord(cw);
    }
    
    return result;
}

std::vector<int> TextLayout::calculateJustifiedPositions(
    const std::vector<MeasuredWord>& words, 
    int lineWidth, 
    bool isLast) {
    
    std::vector<int> positions;
    if (words.empty()) return positions;
    
    // Calculate total word width
    int totalWordWidth = 0;
    for (const MeasuredWord& w : words) {
        totalWordWidth += w.width;
    }
    
    // Single word or last line of paragraph - left align
    if (words.size() == 1 || isLast || !_justify || _defaultAlign != TextAlign::JUSTIFY) {
        int x = 0;
        
        // Handle different alignments
        switch (_defaultAlign) {
            case TextAlign::CENTER:
                x = (lineWidth - totalWordWidth - _spaceWidth * (words.size() - 1)) / 2;
                break;
            case TextAlign::RIGHT:
                x = lineWidth - totalWordWidth - _spaceWidth * (words.size() - 1);
                break;
            default:  // LEFT or JUSTIFY (but last line)
                x = 0;
                break;
        }
        
        // Position words with natural spacing
        for (const MeasuredWord& w : words) {
            positions.push_back(x);
            x += w.width + _spaceWidth;
        }
        return positions;
    }
    
    // Full justification
    int availableSpace = lineWidth - totalWordWidth;
    int gaps = words.size() - 1;
    
    if (gaps <= 0) {
        positions.push_back(0);
        return positions;
    }
    
    // Calculate space per gap (integer division)
    int spacePerGap = availableSpace / gaps;
    int remainder = availableSpace % gaps;
    
    // Limit maximum gap to prevent ugly over-justification
    // This catches cases where measurement might still be slightly off
    int maxGap = _spaceWidth * 2;  // Max 2x normal space width (was 5x - too much!)
    if (spacePerGap > maxGap) {
        // Too wide - fall back to left alignment to avoid ugly gaps
        int x = 0;
        for (const MeasuredWord& w : words) {
            positions.push_back(x);
            x += w.width + _spaceWidth;
        }
        return positions;
    }
    
    // Position each word with justified spacing
    int x = 0;
    for (size_t i = 0; i < words.size(); i++) {
        positions.push_back(x);
        x += words[i].width;
        
        // Add inter-word spacing
        if (i < words.size() - 1) {
            x += spacePerGap;
            // Distribute remainder pixels to first N gaps
            if ((int)i < remainder) {
                x += 1;
            }
        }
    }
    
    return positions;
}

// =============================================================================
// Internal: Page Management
// =============================================================================

bool TextLayout::addLineToPage(const CachedLine& line) {
    if (!hasRoomForLine()) {
        return false;
    }
    
    _currentPage.addLine(line);
    return true;
}

void TextLayout::newPage() {
    _currentPage.clear();
    // Start Y at marginTop + lineHeight so first line's baseline allows
    // room for text above it (GxEPD2 cursor Y is baseline, not top)
    _currentY = _marginTop + _lineHeight;
}

void TextLayout::completePage() {
    if (_currentPage.lineCount > 0) {
        // Use emplace to avoid copying the large CachedPage structure
        _completedPages.emplace_back();
        _completedPages.back() = _currentPage;  // Copy data
    }
}

bool TextLayout::hasRoomForLine() const {
    return _currentY + _lineHeight <= _pageHeight - _marginBottom;
}
