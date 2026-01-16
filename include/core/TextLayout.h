/**
 * @file TextLayout.h
 * @brief Text Layout Engine for Sumi E-Reader
 * @version 2.1.27
 *
 * Features:
 * - Word-wrap with proper text measurement
 * - Text justification with even spacing
 * - Pre-computed word positions for instant rendering
 * - Paragraph detection and handling
 * - Support for multiple font styles
 *
 * Usage:
 *   TextLayout layout;
 *   layout.setPageSize(800, 480);
 *   layout.setMargins(20, 20, 40, 40);
 *   layout.setLineHeight(22);
 *
 *   std::vector<CachedPage> pages = layout.paginateText(text);
 *
 *   for (int i = 0; i < pages.size(); i++) {
 *       pageCache.savePage(chapter, i, pages[i]);
 *   }
 * 
 * Version: 2.1.27
 */

#ifndef SUMI_TEXT_LAYOUT_H
#define SUMI_TEXT_LAYOUT_H

#include <Arduino.h>
#include <vector>
#include <GxEPD2_BW.h>
#include "PageCache.h"
#include "config.h"

// Forward declaration
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// =============================================================================
// Text Alignment Modes
// =============================================================================
enum class TextAlign : uint8_t {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2,
    JUSTIFY = 3
};

// =============================================================================
// Font Style
// =============================================================================
enum class FontStyle : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3
};

// =============================================================================
// Word with measured width (used during layout)
// =============================================================================
struct MeasuredWord {
    String text;
    int width;              // Pixel width
    FontStyle style;
    bool endsWithSpace;     // Word had trailing space
    bool endsParagraph;     // Word ends a paragraph
    
    MeasuredWord() : width(0), style(FontStyle::REGULAR), 
                     endsWithSpace(false), endsParagraph(false) {}
    MeasuredWord(const String& t, int w, FontStyle s = FontStyle::REGULAR) 
        : text(t), width(w), style(s), endsWithSpace(false), endsParagraph(false) {}
};

// =============================================================================
// Line of words (used during layout)
// =============================================================================
struct LayoutLine {
    std::vector<MeasuredWord> words;
    int totalWordWidth;     // Sum of all word widths (no spaces)
    int spaceWidth;         // Width of a space character
    bool isLastInPara;      // Don't justify this line
    bool isHeader;          // This is a heading
    TextAlign align;        // Alignment for this line
    
    LayoutLine() : totalWordWidth(0), spaceWidth(6), isLastInPara(false), 
                   isHeader(false), align(TextAlign::JUSTIFY) {}
    
    void addWord(const MeasuredWord& w) {
        words.push_back(w);
        totalWordWidth += w.width;
    }
    
    int wordCount() const { return words.size(); }
    
    // Calculate minimum width (words without any spacing)
    int minWidth() const { return totalWordWidth; }
    
    // Calculate natural width (words with single space between)
    int naturalWidth() const {
        if (words.size() <= 1) return totalWordWidth;
        return totalWordWidth + spaceWidth * (words.size() - 1);
    }
};

// =============================================================================
// Text Layout Engine Class
// =============================================================================
class TextLayout {
public:
    TextLayout();
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /**
     * Set page dimensions
     */
    void setPageSize(int width, int height);
    
    /**
     * Set margins
     */
    void setMargins(int left, int right, int top, int bottom);
    
    /**
     * Set line height (pixels between baselines)
     */
    void setLineHeight(int height);
    
    /**
     * Set paragraph spacing (extra space after paragraph)
     */
    void setParaSpacing(int spacing);
    
    /**
     * Set default text alignment
     */
    void setDefaultAlign(TextAlign align);
    
    /**
     * Set whether to justify text
     */
    void setJustify(bool justify);
    
    /**
     * Set the font to use for measuring
     */
    void setFont(const GFXfont* font);
    
    // =========================================================================
    // Main Layout Method
    // =========================================================================
    
    /**
     * Paginate text into cached pages
     * This is the main entry point for layout
     * 
     * @param text Full text content to paginate
     * @return Vector of CachedPage ready to save to cache
     */
    std::vector<CachedPage> paginateText(const String& text);
    
    // =========================================================================
    // Incremental Layout (for streaming)
    // =========================================================================
    
    /**
     * Begin a new layout session
     */
    void beginLayout();
    
    /**
     * Add a paragraph to the layout
     * @param para Paragraph text
     * @param isHeader Whether this is a heading
     */
    void addParagraph(const String& para, bool isHeader = false);
    
    /**
     * Get completed pages and clear internal buffer
     */
    std::vector<CachedPage> getCompletedPages();
    
    /**
     * Finish layout and get the final (possibly partial) page
     */
    CachedPage finishLayout();
    
    // =========================================================================
    // Metrics
    // =========================================================================
    
    /**
     * Get content width (page width minus margins)
     */
    int getContentWidth() const { return _contentWidth; }
    
    /**
     * Get content height (page height minus margins)
     */
    int getContentHeight() const { return _contentHeight; }
    
    /**
     * Get number of lines that fit on a page
     */
    int getLinesPerPage() const { return _linesPerPage; }
    
    /**
     * Measure text width using current font
     */
    int measureText(const String& text);
    
    /**
     * Get space width in current font
     */
    int getSpaceWidth() const { return _spaceWidth; }

private:
    // Page dimensions
    int _pageWidth;
    int _pageHeight;
    int _marginLeft, _marginRight, _marginTop, _marginBottom;
    int _contentWidth;
    int _contentHeight;
    
    // Typography
    int _lineHeight;
    int _paraSpacing;
    int _spaceWidth;
    int _linesPerPage;
    TextAlign _defaultAlign;
    bool _justify;
    
    // Font
    const GFXfont* _font;
    
    // Layout state
    CachedPage _currentPage;
    int _currentY;                      // Current Y position on page
    int _currentOffset;                 // Current position in source text
    std::vector<CachedPage> _completedPages;
    bool _inLayout;
    
    // =========================================================================
    // Internal Methods
    // =========================================================================
    
    /**
     * Split text into paragraphs
     */
    std::vector<String> splitParagraphs(const String& text);
    
    /**
     * Split paragraph into words and measure them
     */
    std::vector<MeasuredWord> measureWords(const String& para);
    
    /**
     * Wrap measured words into lines
     */
    std::vector<LayoutLine> wrapToLines(const std::vector<MeasuredWord>& words);
    
    /**
     * Convert a LayoutLine to a CachedLine with positioned words
     */
    CachedLine positionLine(const LayoutLine& line, int y);
    
    /**
     * Add a line to the current page
     * Returns true if line fit, false if we need a new page
     */
    bool addLineToPage(const CachedLine& line);
    
    /**
     * Start a new page
     */
    void newPage();
    
    /**
     * Finalize current page and add to completed list
     */
    void completePage();
    
    /**
     * Check for room for another line
     */
    bool hasRoomForLine() const;
    
    /**
     * Calculate justified word positions
     * @param words Words in the line
     * @param lineWidth Available width
     * @param isLast True if last line of paragraph (skip justification)
     * @return Vector of X positions for each word
     */
    std::vector<int> calculateJustifiedPositions(
        const std::vector<MeasuredWord>& words, 
        int lineWidth, 
        bool isLast);
    
    /**
     * Update internal metrics after config change
     */
    void updateMetrics();
};

// Global instance
extern TextLayout textLayout;

#endif // SUMI_TEXT_LAYOUT_H
