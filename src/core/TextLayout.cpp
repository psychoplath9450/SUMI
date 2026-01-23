/**
 * @file TextLayout.cpp
 * @brief Advanced Text Layout Engine Implementation
 * @version 2.4.0
 *
 * Professional e-reader text layout with:
 * - Per-word font styles (Bold, Italic, Bold-Italic)
 * - Optimal line breaking using dynamic programming (Knuth-Plass style)
 * - Full text justification with even word spacing
 * - Paragraph indentation with em-space character
 * - Line compression for compact/normal/relaxed spacing
 * - Soft hyphen support for EPUBs
 */

#include "core/TextLayout.h"
#include "core/SettingsManager.h"
#include "core/ReaderSettings.h"
#include <Esp.h>
#include <limits>

// Global instance
TextLayout textLayout;

// Constants
static const int MAX_COST = std::numeric_limits<int>::max();

// Em-space character for paragraph indentation (U+2003)
static const char EM_SPACE[] = "\xe2\x80\x83";

// Soft hyphen UTF-8 bytes (U+00AD)
static const char SOFT_HYPHEN_UTF8[] = "\xC2\xAD";
static const size_t SOFT_HYPHEN_BYTES = 2;

// =============================================================================
// TextBlock Implementation
// =============================================================================

void TextBlock::addWord(const String& word, FontStyle style) {
    if (word.length() == 0) return;
    _words.push_back(word);
    _wordStyles.push_back(style);
}

void TextBlock::applyParagraphIndent() {
    // Only indent justified or left-aligned text
    // Skip if using extra paragraph spacing
    if (extraParagraphSpacing || _words.empty()) {
        return;
    }
    
    if (alignment == TextAlign::JUSTIFY || alignment == TextAlign::LEFT) {
        // Prepend em-space to first word
        if (!_words.empty()) {
            String& firstWord = _words.front();
            firstWord = String(EM_SPACE) + firstWord;
        }
    }
}

// =============================================================================
// Constructor
// =============================================================================

TextLayout::TextLayout() 
    : _pageWidth(800), _pageHeight(480),
      _marginLeft(20), _marginRight(20), _marginTop(40), _marginBottom(40),
      _contentWidth(760), _contentHeight(400),
      _lineHeight(22), _baseLineHeight(22), _lineCompression(1.0f),
      _paraSpacing(8), _spaceWidth(6), _linesPerPage(18),
      _defaultAlign(TextAlign::JUSTIFY), _justify(true),
      _extraParagraphSpacing(true), _hyphenationEnabled(false),
      _paragraphIndent(true),
      _fontRegular(nullptr), _fontBold(nullptr), 
      _fontItalic(nullptr), _fontBoldItalic(nullptr),
      _currentY(0), _currentOffset(0), _inLayout(false), _wordCount(0) {
    updateMetrics();
}

// =============================================================================
// Configuration Methods
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
    _baseLineHeight = height;
    _lineHeight = (int)(_baseLineHeight * _lineCompression);
    updateMetrics();
}

void TextLayout::setLineSpacing(LineSpacing spacing) {
    // LineSpacing enum from ReaderSettings.h: COMPACT=0, NORMAL=1, RELAXED=2
    switch (spacing) {
        case LineSpacing::COMPACT:
            _lineCompression = 0.90f;
            break;
        case LineSpacing::NORMAL:
        default:
            _lineCompression = 1.0f;
            break;
        case LineSpacing::RELAXED:
            _lineCompression = 1.1f;
            break;
    }
    _lineHeight = (int)(_baseLineHeight * _lineCompression);
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

void TextLayout::setExtraParagraphSpacing(bool extra) {
    _extraParagraphSpacing = extra;
    // If using extra spacing, no paragraph indent
    // If no extra spacing, use paragraph indent
    _paragraphIndent = !extra;
}

void TextLayout::setHyphenationEnabled(bool enabled) {
    _hyphenationEnabled = enabled;
}

void TextLayout::setParagraphIndent(bool indent) {
    _paragraphIndent = indent;
}

void TextLayout::setFont(const GFXfont* font) {
    // Single font - use for all styles
    setFontFamily(font, font, font, font);
}

void TextLayout::setFontFamily(const GFXfont* regular, const GFXfont* bold,
                               const GFXfont* italic, const GFXfont* boldItalic) {
    _fontRegular = regular;
    _fontBold = bold ? bold : regular;
    _fontItalic = italic ? italic : regular;
    _fontBoldItalic = boldItalic ? boldItalic : (bold ? bold : regular);
    
    if (_fontRegular) {
        display.setFont(_fontRegular);
        
        // Measure space width
        int16_t x1, y1;
        uint16_t w, h;
        display.setTextWrap(false);
        display.getTextBounds(" ", 0, 0, &x1, &y1, &w, &h);
        
        if (w < 2) {
            // Measure "i i" vs "ii" to get space width
            uint16_t w1, w2;
            display.getTextBounds("i i", 0, 0, &x1, &y1, &w1, &h);
            display.getTextBounds("ii", 0, 0, &x1, &y1, &w2, &h);
            _spaceWidth = w1 - w2;
            
            if (_spaceWidth < 2 || _spaceWidth > 20) {
                _spaceWidth = _fontRegular->yAdvance / 3;
            }
        } else {
            _spaceWidth = w;
        }
        
        if (_spaceWidth < 3) _spaceWidth = 3;
        if (_spaceWidth > 15) _spaceWidth = 8;
        
        Serial.printf("[LAYOUT] Font set: yAdvance=%d, spaceWidth=%d\n", 
                      _fontRegular->yAdvance, _spaceWidth);
    }
}

const GFXfont* TextLayout::getFontForStyle(FontStyle style) const {
    switch (style) {
        case FontStyle::BOLD:
            return _fontBold ? _fontBold : _fontRegular;
        case FontStyle::ITALIC:
            return _fontItalic ? _fontItalic : _fontRegular;
        case FontStyle::BOLD_ITALIC:
            return _fontBoldItalic ? _fontBoldItalic : _fontRegular;
        case FontStyle::REGULAR:
        default:
            return _fontRegular;
    }
}

void TextLayout::updateMetrics() {
    const int GLYPH_OVERHANG_BUFFER = 8;
    
    _contentWidth = _pageWidth - _marginLeft - _marginRight - GLYPH_OVERHANG_BUFFER;
    _contentHeight = _pageHeight - _marginTop - _marginBottom;
    
    if (_contentWidth < 100) {
        _contentWidth = _pageWidth - 40;
        _marginLeft = 20;
        _marginRight = 20;
    }
    if (_contentHeight < 100) {
        _contentHeight = _pageHeight - 60;
        _marginTop = 20;
        _marginBottom = 40;
    }
    
    _linesPerPage = _contentHeight / _lineHeight;
    if (_linesPerPage < 5) _linesPerPage = 5;
    
    Serial.printf("[LAYOUT] Content: %dx%d, Lines/page: %d, LineHeight: %d (compression: %.2f)\n",
                  _contentWidth, _contentHeight, _linesPerPage, _lineHeight, _lineCompression);
}

// =============================================================================
// Text Measurement
// =============================================================================

int TextLayout::measureText(const String& text, FontStyle style) {
    if (text.length() == 0) return 0;
    
    const GFXfont* font = getFontForStyle(style);
    if (!font) {
        font = _fontRegular;
        if (!font) return text.length() * 6;  // Fallback estimate
    }
    
    display.setFont(font);
    display.setTextWrap(false);
    
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    
    return w;
}

int TextLayout::measureTextRegular(const String& text) {
    return measureText(text, FontStyle::REGULAR);
}

// =============================================================================
// Soft Hyphen Handling
// =============================================================================

bool TextLayout::containsSoftHyphen(const String& word) {
    return word.indexOf(SOFT_HYPHEN_UTF8) >= 0;
}

void TextLayout::stripSoftHyphens(String& word) {
    int pos;
    while ((pos = word.indexOf(SOFT_HYPHEN_UTF8)) >= 0) {
        word.remove(pos, SOFT_HYPHEN_BYTES);
    }
}

// =============================================================================
// Main Layout Method
// =============================================================================

std::vector<CachedPage> TextLayout::paginateText(const String& text) {
    beginLayout();
    
    std::vector<String> paragraphs = splitParagraphs(text);
    Serial.printf("[LAYOUT] Processing %d paragraphs\n", (int)paragraphs.size());
    
    for (const String& para : paragraphs) {
        if (para.length() > 0) {
            addParagraph(para, false);
        }
    }
    
    CachedPage lastPage = finishLayout();
    std::vector<CachedPage> result = getCompletedPages();
    
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
    _currentPage.clear();
    _currentY = _marginTop + _lineHeight;
    _currentOffset = 0;
    _completedPages.clear();
    _wordCount = 0;
    _inLayout = true;
}

void TextLayout::addParagraph(const String& para, bool isHeader) {
    if (!_inLayout) beginLayout();
    
    // Handle scene break marker
    if (para == "~~~SCENE_BREAK~~~") {
        int sceneBreakSpacing = settingsManager.reader.sceneBreakSpacing;
        if (sceneBreakSpacing > 0) {
            _currentY += sceneBreakSpacing;
        }
        return;
    }
    
    // Safety checks
    String safePara = para;
    if (safePara.length() > 2000) {
        safePara = safePara.substring(0, 2000);
    }
    
    if (ESP.getFreeHeap() < 8000) {
        Serial.printf("[LAYOUT] Low memory (%d bytes), skipping\n", ESP.getFreeHeap());
        return;
    }
    
    // Create text block
    TextAlign align = isHeader ? TextAlign::CENTER : _defaultAlign;
    TextBlock block = createTextBlock(safePara, align);
    
    if (block.isEmpty()) return;
    
    // Apply paragraph indent if not using extra spacing
    if (_paragraphIndent && !_extraParagraphSpacing && !isHeader) {
        block.applyParagraphIndent();
    }
    
    // Layout the block
    layoutTextBlock(block, isHeader);
    
    // Memory management: flush if too many words
    _wordCount += block.size();
    if (_wordCount > 750) {
        Serial.printf("[LAYOUT] Word count %d > 750, flushing\n", _wordCount);
        // Check heap before completing page (CachedPage is ~14KB)
        if (ESP.getFreeHeap() < 20000) {
            Serial.printf("[LAYOUT] WARNING: Low heap (%d), skipping flush\n", ESP.getFreeHeap());
            _wordCount = 0;
        } else if (_currentPage.lineCount > 0) {
            completePage();
            newPage();
            _wordCount = 0;
        }
    }
}

void TextLayout::addStyledParagraph(TextBlock& block, bool isHeader) {
    if (!_inLayout) beginLayout();
    
    if (block.isEmpty()) return;
    
    // Apply paragraph indent if not using extra spacing
    if (_paragraphIndent && !_extraParagraphSpacing && !isHeader) {
        block.applyParagraphIndent();
    }
    
    layoutTextBlock(block, isHeader);
}

void TextLayout::addImagePage(const String& imagePath) {
    // Complete current page if has content
    if (_currentPage.lineCount > 0) {
        completePage();
    }
    
    // Create a page with just image reference
    // For now, add placeholder text - full image support to be added
    CachedPage imagePage;
    imagePage.clear();
    
    // Add centered placeholder
    CachedLine line;
    line.yPos = _marginTop + _contentHeight / 2;
    
    CachedWord cw;
    cw.xPos = _marginLeft + _contentWidth / 4;
    cw.style = static_cast<uint8_t>(FontStyle::ITALIC);
    strncpy(cw.text, "[Image]", CACHE_MAX_WORD_LEN - 1);
    cw.length = 7;
    line.addWord(cw);
    
    imagePage.addLine(line);
    _completedPages.push_back(imagePage);
    
    // Start fresh page after image
    newPage();
}

std::vector<CachedPage> TextLayout::getCompletedPages() {
    std::vector<CachedPage> result = std::move(_completedPages);
    _completedPages.clear();
    return result;
}

CachedPage TextLayout::finishLayout() {
    _inLayout = false;
    CachedPage result = _currentPage;
    _currentPage.clear();
    return result;
}

// =============================================================================
// Paragraph Splitting
// =============================================================================

std::vector<String> TextLayout::splitParagraphs(const String& text) {
    std::vector<String> result;
    
    int start = 0;
    int len = text.length();
    
    for (int i = 0; i < len; i++) {
        char c = text[i];
        
        // Check for paragraph break (double newline or CR+LF patterns)
        if (c == '\n') {
            // Extract paragraph
            if (i > start) {
                String para = text.substring(start, i);
                para.trim();
                if (para.length() > 0) {
                    result.push_back(para);
                }
            }
            start = i + 1;
        }
    }
    
    // Last paragraph
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
// TextBlock Creation
// =============================================================================

TextBlock TextLayout::createTextBlock(const String& para, TextAlign align) {
    TextBlock block(align, _extraParagraphSpacing, _hyphenationEnabled);
    
    int start = 0;
    int len = para.length();
    
    for (int i = 0; i <= len; i++) {
        char c = (i < len) ? para[i] : ' ';
        
        if (c == ' ' || c == '\t' || i == len) {
            if (i > start) {
                String wordText = para.substring(start, i);
                
                // Strip soft hyphens for display
                if (containsSoftHyphen(wordText)) {
                    stripSoftHyphens(wordText);
                }
                
                if (wordText.length() > 0) {
                    block.addWord(wordText, FontStyle::REGULAR);
                }
            }
            start = i + 1;
        }
    }
    
    return block;
}

// =============================================================================
// Word Width Calculation
// =============================================================================

std::vector<uint16_t> TextLayout::calculateWordWidths(TextBlock& block) {
    std::vector<uint16_t> widths;
    
    auto& words = block.getWords();
    auto& styles = block.getWordStyles();
    
    auto wordIt = words.begin();
    auto styleIt = styles.begin();
    
    while (wordIt != words.end() && styleIt != styles.end()) {
        uint16_t width = measureText(*wordIt, *styleIt);
        widths.push_back(width);
        ++wordIt;
        ++styleIt;
    }
    
    return widths;
}

// =============================================================================
// Optimal Line Breaking (Knuth-Plass Style DP Algorithm)
// =============================================================================

std::vector<size_t> TextLayout::computeOptimalLineBreaks(
    const std::vector<uint16_t>& wordWidths,
    int pageWidth, int spaceWidth) {
    
    if (wordWidths.empty()) return {};
    
    size_t n = wordWidths.size();
    
    // For very long paragraphs, use greedy to save memory
    if (n > 200) {
        return computeGreedyLineBreaks(wordWidths, pageWidth, spaceWidth);
    }
    
    // DP table: dp[i] = minimum cost to layout words[i..n-1]
    std::vector<int> dp(n);
    // ans[i] = index of last word on optimal line starting at i
    std::vector<size_t> ans(n);
    
    // Base case: last word has zero cost
    dp[n - 1] = 0;
    ans[n - 1] = n - 1;
    
    // Fill DP table from right to left
    for (int i = n - 2; i >= 0; --i) {
        int currLen = -spaceWidth;  // Will add spaceWidth for first word
        dp[i] = MAX_COST;
        
        for (size_t j = i; j < n; ++j) {
            currLen += wordWidths[j] + spaceWidth;
            
            if (currLen > pageWidth) break;
            
            int cost;
            if (j == n - 1) {
                // Last line has no penalty
                cost = 0;
            } else {
                int remainingSpace = pageWidth - currLen;
                // Squared cost to penalize uneven lines
                long long costLL = (long long)remainingSpace * remainingSpace + dp[j + 1];
                cost = (costLL > MAX_COST) ? MAX_COST : (int)costLL;
            }
            
            if (cost < dp[i]) {
                dp[i] = cost;
                ans[i] = j;
            }
        }
        
        // Handle oversized word
        if (dp[i] == MAX_COST) {
            ans[i] = i;
            dp[i] = (i + 1 < (int)n) ? dp[i + 1] : 0;
        }
    }
    
    // Extract line break indices
    std::vector<size_t> breaks;
    size_t idx = 0;
    
    while (idx < n) {
        size_t nextBreak = ans[idx] + 1;
        
        // Safety: prevent infinite loop
        if (nextBreak <= idx) nextBreak = idx + 1;
        
        breaks.push_back(nextBreak);
        idx = nextBreak;
    }
    
    return breaks;
}

std::vector<size_t> TextLayout::computeGreedyLineBreaks(
    const std::vector<uint16_t>& wordWidths,
    int pageWidth, int spaceWidth) {
    
    std::vector<size_t> breaks;
    size_t idx = 0;
    
    while (idx < wordWidths.size()) {
        int lineWidth = 0;
        size_t lineStart = idx;
        
        while (idx < wordWidths.size()) {
            int spacing = (idx == lineStart) ? 0 : spaceWidth;
            int wordWidth = wordWidths[idx] + spacing;
            
            if (lineWidth + wordWidth <= pageWidth || idx == lineStart) {
                lineWidth += wordWidth;
                ++idx;
            } else {
                break;
            }
        }
        
        breaks.push_back(idx);
    }
    
    return breaks;
}

// =============================================================================
// Layout TextBlock into Lines
// =============================================================================

void TextLayout::layoutTextBlock(TextBlock& block, bool isHeader) {
    if (block.isEmpty()) return;
    
    // Calculate word widths
    std::vector<uint16_t> wordWidths = calculateWordWidths(block);
    
    // Compute optimal line breaks
    std::vector<size_t> lineBreaks = computeOptimalLineBreaks(
        wordWidths, _contentWidth, _spaceWidth);
    
    // Extract lines and add to page
    auto& words = block.getWords();
    auto& styles = block.getWordStyles();
    
    size_t lineStart = 0;
    
    for (size_t lineIdx = 0; lineIdx < lineBreaks.size(); ++lineIdx) {
        size_t lineEnd = lineBreaks[lineIdx];
        bool isLastLine = (lineIdx == lineBreaks.size() - 1);
        
        // Create layout line
        LayoutLine layoutLine;
        layoutLine.alignment = block.alignment;
        layoutLine.isLastInParagraph = isLastLine;
        layoutLine.isHeader = isHeader;
        layoutLine.spaceWidth = _spaceWidth;
        
        // Get words and styles for this line
        auto wordIt = words.begin();
        auto styleIt = styles.begin();
        std::advance(wordIt, lineStart);
        std::advance(styleIt, lineStart);
        
        for (size_t i = lineStart; i < lineEnd && wordIt != words.end(); ++i) {
            StyledWord sw(*wordIt, wordWidths[i], *styleIt);
            layoutLine.addWord(sw);
            ++wordIt;
            ++styleIt;
        }
        
        // Calculate word positions
        layoutLine.wordXPositions = calculateWordPositions(
            layoutLine.words,
            _contentWidth, _spaceWidth,
            layoutLine.alignment, isLastLine);
        
        // Convert to cached line
        CachedLine cachedLine = positionLine(layoutLine, _currentY);
        
        // Add to page (handles page breaks)
        if (!addLineToPage(cachedLine)) {
            completePage();
            newPage();
            cachedLine.yPos = _currentY;
            addLineToPage(cachedLine);
        }
        
        _currentY += _lineHeight;
        lineStart = lineEnd;
    }
    
    // Add extra paragraph spacing if enabled
    if (_extraParagraphSpacing) {
        _currentY += _lineHeight / 2;
    }
}

// =============================================================================
// Word Position Calculation
// =============================================================================

std::vector<uint16_t> TextLayout::calculateWordPositions(
    const std::vector<StyledWord>& words,
    int lineWidth, int spaceWidth,
    TextAlign align, bool isLastLine) {
    
    std::vector<uint16_t> positions;
    if (words.empty()) return positions;
    
    // Calculate total word width
    int totalWordWidth = 0;
    for (const auto& w : words) {
        totalWordWidth += w.width;
    }
    
    int spareSpace = lineWidth - totalWordWidth;
    int numGaps = words.size() - 1;
    
    // Single word or last line: no justification
    if (words.size() == 1 || isLastLine || align != TextAlign::JUSTIFY) {
        uint16_t x = 0;
        
        switch (align) {
            case TextAlign::CENTER:
                x = (spareSpace - numGaps * spaceWidth) / 2;
                break;
            case TextAlign::RIGHT:
                x = spareSpace - numGaps * spaceWidth;
                break;
            default:  // LEFT or JUSTIFY on last line
                x = 0;
                break;
        }
        
        for (const auto& w : words) {
            positions.push_back(x);
            x += w.width + spaceWidth;
        }
        return positions;
    }
    
    // Full justification
    if (numGaps <= 0) {
        positions.push_back(0);
        return positions;
    }
    
    int spacePerGap = spareSpace / numGaps;
    int remainder = spareSpace % numGaps;
    
    // Limit maximum gap to prevent ugly over-justification
    int maxGap = spaceWidth * 2;
    if (spacePerGap > maxGap) {
        // Too wide - fall back to left alignment
        uint16_t x = 0;
        for (const auto& w : words) {
            positions.push_back(x);
            x += w.width + spaceWidth;
        }
        return positions;
    }
    
    // Position each word with justified spacing
    uint16_t x = 0;
    for (size_t i = 0; i < words.size(); ++i) {
        positions.push_back(x);
        x += words[i].width;
        
        if (i < words.size() - 1) {
            x += spacePerGap;
            // Distribute remainder pixels
            if ((int)i < remainder) {
                x += 1;
            }
        }
    }
    
    return positions;
}

// =============================================================================
// Convert LayoutLine to CachedLine
// =============================================================================

CachedLine TextLayout::positionLine(const LayoutLine& line, int y) {
    CachedLine result;
    result.yPos = y;
    result.setLastInPara(line.isLastInParagraph);
    
    if (line.words.empty()) return result;
    
    for (size_t i = 0; i < line.words.size() && i < line.wordXPositions.size(); ++i) {
        CachedWord cw;
        cw.xPos = _marginLeft + line.wordXPositions[i];
        cw.style = static_cast<uint8_t>(line.words[i].style);
        
        int textLen = line.words[i].text.length();
        if (textLen > CACHE_MAX_WORD_LEN - 1) textLen = CACHE_MAX_WORD_LEN - 1;
        strncpy(cw.text, line.words[i].text.c_str(), textLen);
        cw.text[textLen] = '\0';
        cw.length = textLen;
        
        result.addWord(cw);
    }
    
    return result;
}

// =============================================================================
// Page Management
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
    _currentY = _marginTop + _lineHeight;
}

void TextLayout::completePage() {
    if (_currentPage.lineCount > 0) {
        // Check heap before allocating - CachedPage is about 14KB
        if (ESP.getFreeHeap() < 18000) {
            Serial.printf("[LAYOUT] CRITICAL: Cannot complete page, only %d bytes free!\n", ESP.getFreeHeap());
            return;
        }
        _completedPages.emplace_back();
        _completedPages.back() = _currentPage;
    }
}

bool TextLayout::hasRoomForLine() const {
    return _currentY + _lineHeight <= _pageHeight - _marginBottom;
}

// =============================================================================
// Legacy Methods (for backward compatibility)
// =============================================================================

std::vector<MeasuredWord> TextLayout::measureWords(const String& para) {
    std::vector<MeasuredWord> result;
    
    int start = 0;
    int len = para.length();
    
    for (int i = 0; i <= len; i++) {
        char c = (i < len) ? para[i] : ' ';
        
        if (c == ' ' || c == '\t' || i == len) {
            if (i > start) {
                String wordText = para.substring(start, i);
                int width = measureTextRegular(wordText);
                
                MeasuredWord mw(wordText, width, FontStyle::REGULAR);
                mw.endsWithSpace = (c == ' ');
                result.push_back(mw);
            }
            start = i + 1;
        }
    }
    
    return result;
}

std::vector<LayoutLine> TextLayout::wrapToLines(const std::vector<MeasuredWord>& words) {
    std::vector<LayoutLine> result;
    
    if (words.empty()) return result;
    
    LayoutLine currentLine;
    currentLine.spaceWidth = _spaceWidth;
    currentLine.alignment = _defaultAlign;
    
    for (const MeasuredWord& word : words) {
        int lineWidthWithWord = currentLine.naturalWidth();
        if (currentLine.wordCount() > 0) {
            lineWidthWithWord += _spaceWidth + word.width;
        } else {
            lineWidthWithWord = word.width;
        }
        
        if (lineWidthWithWord <= _contentWidth || currentLine.wordCount() == 0) {
            StyledWord sw(word.text, word.width, word.style);
            currentLine.addWord(sw);
        } else {
            result.push_back(currentLine);
            
            currentLine = LayoutLine();
            currentLine.spaceWidth = _spaceWidth;
            currentLine.alignment = _defaultAlign;
            StyledWord sw(word.text, word.width, word.style);
            currentLine.addWord(sw);
        }
    }
    
    if (currentLine.wordCount() > 0) {
        result.push_back(currentLine);
    }
    
    return result;
}

std::vector<int> TextLayout::calculateJustifiedPositions(
    const std::vector<MeasuredWord>& words, 
    int lineWidth, 
    bool isLast) {
    
    std::vector<int> positions;
    if (words.empty()) return positions;
    
    int totalWordWidth = 0;
    for (const MeasuredWord& w : words) {
        totalWordWidth += w.width;
    }
    
    if (words.size() == 1 || isLast || !_justify || _defaultAlign != TextAlign::JUSTIFY) {
        int x = 0;
        
        switch (_defaultAlign) {
            case TextAlign::CENTER:
                x = (lineWidth - totalWordWidth - _spaceWidth * (words.size() - 1)) / 2;
                break;
            case TextAlign::RIGHT:
                x = lineWidth - totalWordWidth - _spaceWidth * (words.size() - 1);
                break;
            default:
                x = 0;
                break;
        }
        
        for (const MeasuredWord& w : words) {
            positions.push_back(x);
            x += w.width + _spaceWidth;
        }
        return positions;
    }
    
    int availableSpace = lineWidth - totalWordWidth;
    int gaps = words.size() - 1;
    
    if (gaps <= 0) {
        positions.push_back(0);
        return positions;
    }
    
    int spacePerGap = availableSpace / gaps;
    int remainder = availableSpace % gaps;
    
    int maxGap = _spaceWidth * 2;
    if (spacePerGap > maxGap) {
        int x = 0;
        for (const MeasuredWord& w : words) {
            positions.push_back(x);
            x += w.width + _spaceWidth;
        }
        return positions;
    }
    
    int x = 0;
    for (size_t i = 0; i < words.size(); i++) {
        positions.push_back(x);
        x += words[i].width;
        
        if (i < words.size() - 1) {
            x += spacePerGap;
            if ((int)i < remainder) {
                x += 1;
            }
        }
    }
    
    return positions;
}
