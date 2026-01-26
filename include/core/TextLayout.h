/**
 * @file TextLayout.h
 * @brief Text Layout Engine for E-Ink Display
 * @version 1.4.2
 *
 * Professional text layout with:
 * - Streaming text input (addText() called incrementally)
 * - Word wrapping with justification
 * - Paragraph handling
 * - Style tracking (bold, italic)
 * - Page-based output for rendering
 *
 * Memory-conscious design:
 *
 * - Pages stored to SD card, not RAM
 * - Only current page loaded for rendering
 */

#ifndef SUMI_TEXT_LAYOUT_H
#define SUMI_TEXT_LAYOUT_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include "config.h"
#include "core/ReaderSettings.h"

// =============================================================================
// Layout Constants
// =============================================================================
// Layout limits - minimal for ESP32-C3 (pages cached to SD anyway)
#define MAX_WORDS_PER_LINE      12      // Words per line
#define MAX_LINES_PER_PAGE      20      // Lines per page (800px / 34px line height)
#define MAX_PAGES_PER_CHAPTER   500
#define WORD_BUFFER_SIZE        24      // Max word length

// =============================================================================
// Word Structure
// =============================================================================
struct LayoutWord {
    char text[WORD_BUFFER_SIZE];  // Word text (uses WORD_BUFFER_SIZE)
    int16_t x;              // X position on line
    int16_t width;          // Pixel width
    FontStyle style;        // Font style
    
    LayoutWord() : x(0), width(0), style(FontStyle::NORMAL) {
        memset(text, 0, sizeof(text));
    }
};

// =============================================================================
// Line Structure
// =============================================================================
struct LayoutLine {
    LayoutWord words[MAX_WORDS_PER_LINE];
    uint8_t wordCount;
    int16_t y;              // Y position (baseline)
    int16_t width;          // Total width of words
    bool justified;         // Whether spacing was applied
    
    LayoutLine() : wordCount(0), y(0), width(0), justified(false) {}
    
    void clear() {
        wordCount = 0;
        y = 0;
        width = 0;
        justified = false;
    }
};

// =============================================================================
// Page Structure
// =============================================================================
struct LayoutPage {
    LayoutLine lines[MAX_LINES_PER_PAGE];
    uint8_t lineCount;
    uint16_t pageNumber;
    
    LayoutPage() : lineCount(0), pageNumber(0) {}
    
    void clear() {
        lineCount = 0;
        for (int i = 0; i < MAX_LINES_PER_PAGE; i++) {
            lines[i].clear();
        }
    }
    
    // Serialize to file
    bool saveTo(File& f) const {
        f.write((const uint8_t*)&pageNumber, sizeof(pageNumber));
        f.write((const uint8_t*)&lineCount, sizeof(lineCount));
        
        for (int i = 0; i < lineCount; i++) {
            const LayoutLine& line = lines[i];
            f.write((const uint8_t*)&line.wordCount, sizeof(line.wordCount));
            f.write((const uint8_t*)&line.y, sizeof(line.y));
            f.write((const uint8_t*)&line.width, sizeof(line.width));
            f.write((const uint8_t*)&line.justified, sizeof(line.justified));
            
            for (int j = 0; j < line.wordCount; j++) {
                const LayoutWord& word = line.words[j];
                f.write((const uint8_t*)word.text, sizeof(word.text));
                f.write((const uint8_t*)&word.x, sizeof(word.x));
                f.write((const uint8_t*)&word.width, sizeof(word.width));
                f.write((const uint8_t*)&word.style, sizeof(word.style));
            }
        }
        return true;
    }
    
    // Deserialize from file
    bool loadFrom(File& f) {
        clear();
        
        if (f.read((uint8_t*)&pageNumber, sizeof(pageNumber)) != sizeof(pageNumber)) return false;
        if (f.read((uint8_t*)&lineCount, sizeof(lineCount)) != sizeof(lineCount)) return false;
        
        if (lineCount > MAX_LINES_PER_PAGE) return false;
        
        for (int i = 0; i < lineCount; i++) {
            LayoutLine& line = lines[i];
            if (f.read((uint8_t*)&line.wordCount, sizeof(line.wordCount)) != sizeof(line.wordCount)) return false;
            if (f.read((uint8_t*)&line.y, sizeof(line.y)) != sizeof(line.y)) return false;
            if (f.read((uint8_t*)&line.width, sizeof(line.width)) != sizeof(line.width)) return false;
            if (f.read((uint8_t*)&line.justified, sizeof(line.justified)) != sizeof(line.justified)) return false;
            
            if (line.wordCount > MAX_WORDS_PER_LINE) return false;
            
            for (int j = 0; j < line.wordCount; j++) {
                LayoutWord& word = line.words[j];
                if (f.read((uint8_t*)word.text, sizeof(word.text)) != sizeof(word.text)) return false;
                if (f.read((uint8_t*)&word.x, sizeof(word.x)) != sizeof(word.x)) return false;
                if (f.read((uint8_t*)&word.width, sizeof(word.width)) != sizeof(word.width)) return false;
                if (f.read((uint8_t*)&word.style, sizeof(word.style)) != sizeof(word.style)) return false;
            }
        }
        return true;
    }
};

// =============================================================================
// Text Layout Engine
// =============================================================================
class TextLayout {
public:
    TextLayout();
    
    // ==========================================================================
    // Configuration
    // ==========================================================================
    void setPageSize(int width, int height);
    void setMargins(int left, int right, int top, int bottom);
    void setLineHeight(int height);
    void setLineHeightMultiplier(float mult);  // 0.95 (tight), 1.0 (normal), 1.1 (wide)
    void setParaSpacing(int spacing);
    void setUseParaIndent(bool useIndent);     // Use em-dash indent instead of spacing
    void setJustify(bool enable);
    void setFont(const GFXfont* font);
    void setBoldFont(const GFXfont* font);
    void setItalicFont(const GFXfont* font);
    void setBoldItalicFont(const GFXfont* font);
    
    // ==========================================================================
    // Streaming Input
    // ==========================================================================
    
    /**
     * Reset layout for new content
     */
    void reset();
    
    /**
     * Add plain text to the layout (call multiple times for streaming)
     * Text is broken into words and laid out into lines/pages
     */
    void addText(const char* text, size_t len);
    
    /**
     * Add rich text with markers (from portal preprocessor)
     * Parses: **bold**, *italic*, # headers, • bullets, [Image]
     * Soft hyphens (U+00AD) are preserved for line breaking
     */
    void addRichText(const char* text, size_t len);
    
    /**
     * Signal end of paragraph (adds vertical space)
     */
    void endParagraph();
    
    /**
     * Set current style for subsequent text
     */
    void setCurrentStyle(FontStyle style);
    
    /**
     * Finalize layout (complete last line/page)
     */
    void finalize();
    
    // ==========================================================================
    // Output
    // ==========================================================================
    
    /**
     * Get total number of pages
     */
    int getPageCount() const { return _pageCount; }
    
    /**
     * Render a specific page to display
     * Loads page from cache if necessary
     */
    void renderPage(int pageNum, GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT>& display);
    
    /**
     * Set cache path for page storage
     */
    void setCachePath(const String& path);
    
private:
    // Configuration
    int _pageWidth, _pageHeight;
    int _marginLeft, _marginRight, _marginTop, _marginBottom;
    int _contentWidth, _contentHeight;
    int _lineHeight;
    float _lineHeightMultiplier;  // Line compression (0.95, 1.0, 1.1)
    int _paraSpacing;
    bool _useParaIndent;          // When true, use em-dash indent instead of spacing
    bool _justify;
    const GFXfont* _font;
    const GFXfont* _fontBold;
    const GFXfont* _fontItalic;
    const GFXfont* _fontBoldItalic;
    
    // Current state
    FontStyle _currentStyle;
    int _currentY;          // Current Y position on page
    int _pageCount;
    bool _isHeaderLine;     // True when processing # header line (centered, bold)
    bool _isBulletLine;     // True when processing • bullet line
    
    // Current line being built
    LayoutLine _currentLine;
    int _currentLineWidth;
    
    // Current page being built
    LayoutPage _currentPage;
    
    // Word buffer for accumulating characters
    char _wordBuffer[WORD_BUFFER_SIZE];
    int _wordBufferLen;
    
    // Cache path for storing pages
    String _cachePath;
    
    // ==========================================================================
    // Internal Methods
    // ==========================================================================
    
    /**
     * Flush current word to line
     */
    void flushWord();
    
    /**
     * Complete current line (apply justification, move to next)
     */
    void finishLine(bool isParagraphEnd = false);
    
    /**
     * Complete current page (save to cache, start new)
     */
    void finishPage();
    
    /**
     * Get pixel width of text with current font
     */
    int getTextWidth(const char* text, FontStyle style);
    
    /**
     * Get space width for current font
     */
    int getSpaceWidth();
    
    /**
     * Apply justification spacing to a line
     */
    void justifyLine(LayoutLine& line);
    
    /**
     * Get font for style
     */
    const GFXfont* getFontForStyle(FontStyle style);
    
    /**
     * Save page to cache file
     */
    void savePageToCache(const LayoutPage& page);
    
    /**
     * Load page from cache file
     */
    bool loadPageFromCache(int pageNum, LayoutPage& out);
};

// =============================================================================
// Global Instance
// =============================================================================
extern TextLayout* textLayout;

#endif // SUMI_TEXT_LAYOUT_H
