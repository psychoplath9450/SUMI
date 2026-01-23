/**
 * @file TextLayout.h
 * @brief Advanced Text Layout Engine for Sumi E-Reader
 * @version 2.4.0
 *
 * Professional e-reader text rendering with:
 * - Per-word font styles (Bold, Italic, Bold-Italic)
 * - Optimal line breaking with Knuth-Plass style DP algorithm
 * - Text justification with proper word spacing
 * - Paragraph indentation with em-space
 * - Extra paragraph spacing option  
 * - Line compression (compact/normal/relaxed spacing)
 * - Soft hyphen support for EPUBs
 * - Image page handling
 *
 * Architecture based on professional e-reader implementations
 */

#ifndef SUMI_TEXT_LAYOUT_H
#define SUMI_TEXT_LAYOUT_H

#include <Arduino.h>
#include <vector>
#include <list>
#include <GxEPD2_BW.h>
#include "PageCache.h"
#include "config.h"

// Forward declarations
extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;

// LineSpacing is defined in ReaderSettings.h - forward declare here
// Values: COMPACT=0, NORMAL=1, RELAXED=2
enum class LineSpacing : uint8_t;

// =============================================================================
// Text Alignment Modes (matches EPUB standards)
// =============================================================================
enum class TextAlign : uint8_t {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2,
    JUSTIFY = 3
};

// =============================================================================
// Font Style (per-word styling for inline formatting)
// =============================================================================
enum class FontStyle : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3
};

// =============================================================================
// Styled Word - word with its individual style
// =============================================================================
struct StyledWord {
    String text;
    uint16_t width;              // Pixel width when rendered
    FontStyle style;             // Bold/Italic/etc for this word
    
    StyledWord() : width(0), style(FontStyle::REGULAR) {}
    StyledWord(const String& t, uint16_t w, FontStyle s = FontStyle::REGULAR) 
        : text(t), width(w), style(s) {}
};

// =============================================================================
// Measured Word (used during layout) - LEGACY COMPATIBILITY
// =============================================================================
struct MeasuredWord {
    String text;
    int width;
    FontStyle style;
    bool endsWithSpace;
    bool endsParagraph;
    
    MeasuredWord() : width(0), style(FontStyle::REGULAR), 
                     endsWithSpace(false), endsParagraph(false) {}
    MeasuredWord(const String& t, int w, FontStyle s = FontStyle::REGULAR) 
        : text(t), width(w), style(s), endsWithSpace(false), endsParagraph(false) {}
};

// =============================================================================
// Text Block - a paragraph being processed
// Contains words with styles, handles layout into lines
// =============================================================================
class TextBlock {
public:
    TextAlign alignment;
    bool extraParagraphSpacing;
    bool hyphenationEnabled;
    
    TextBlock(TextAlign align = TextAlign::JUSTIFY, 
              bool extraSpacing = true,
              bool hyphenation = false)
        : alignment(align), 
          extraParagraphSpacing(extraSpacing),
          hyphenationEnabled(hyphenation) {}
    
    // Add a word with its style
    void addWord(const String& word, FontStyle style);
    
    // Set alignment
    void setAlignment(TextAlign align) { alignment = align; }
    TextAlign getAlignment() const { return alignment; }
    
    // Check if empty
    bool isEmpty() const { return _words.empty(); }
    size_t size() const { return _words.size(); }
    
    // Clear all words
    void clear() { _words.clear(); _wordStyles.clear(); }
    
    // Get words and styles (for iteration)
    std::list<String>& getWords() { return _words; }
    std::list<FontStyle>& getWordStyles() { return _wordStyles; }
    const std::list<String>& getWords() const { return _words; }
    const std::list<FontStyle>& getWordStyles() const { return _wordStyles; }
    
    // Apply paragraph indent (em-space at start)
    void applyParagraphIndent();
    
private:
    std::list<String> _words;
    std::list<FontStyle> _wordStyles;
};

// =============================================================================
// Layout Line - a single line ready for rendering
// =============================================================================
struct LayoutLine {
    std::vector<StyledWord> words;
    std::vector<uint16_t> wordXPositions;  // Pre-calculated X position for each word
    TextAlign alignment;
    bool isLastInParagraph;
    bool isHeader;
    int spaceWidth;  // Space width for this line
    int totalWordWidth;  // Sum of word widths
    
    LayoutLine() : alignment(TextAlign::JUSTIFY), 
                   isLastInParagraph(false), 
                   isHeader(false),
                   spaceWidth(6),
                   totalWordWidth(0) {}
    
    bool isEmpty() const { return words.empty(); }
    int wordCount() const { return words.size(); }
    
    void addWord(const StyledWord& w) {
        words.push_back(w);
        totalWordWidth += w.width;
    }
    
    // Calculate natural width with single spaces
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
    
    void setPageSize(int width, int height);
    void setMargins(int left, int right, int top, int bottom);
    void setLineHeight(int height);
    void setLineSpacing(LineSpacing spacing);
    void setParaSpacing(int spacing);
    void setDefaultAlign(TextAlign align);
    void setJustify(bool justify);
    void setExtraParagraphSpacing(bool extra);
    void setHyphenationEnabled(bool enabled);
    void setParagraphIndent(bool indent);
    
    // Font management - single font (legacy)
    void setFont(const GFXfont* font);
    
    // Font management - font family with styles (4 variants)
    void setFontFamily(const GFXfont* regular, 
                       const GFXfont* bold,
                       const GFXfont* italic, 
                       const GFXfont* boldItalic);
    
    // =========================================================================
    // Main Layout Method
    // =========================================================================
    
    /**
     * Paginate text into cached pages
     * @param text Full text content to paginate
     * @return Vector of CachedPage ready to save to cache
     */
    std::vector<CachedPage> paginateText(const String& text);
    
    // =========================================================================
    // Incremental Layout (for streaming HTML parsing)
    // =========================================================================
    
    void beginLayout();
    void addParagraph(const String& para, bool isHeader = false);
    void addStyledParagraph(TextBlock& block, bool isHeader = false);
    void addImagePage(const String& imagePath);
    std::vector<CachedPage> getCompletedPages();
    CachedPage finishLayout();
    
    // =========================================================================
    // Metrics
    // =========================================================================
    
    int getContentWidth() const { return _contentWidth; }
    int getContentHeight() const { return _contentHeight; }
    int getLinesPerPage() const { return _linesPerPage; }
    int getSpaceWidth() const { return _spaceWidth; }
    float getLineCompression() const { return _lineCompression; }
    
    // Measure text width with specific style
    int measureText(const String& text, FontStyle style);
    
    // Measure text with default style (REGULAR)
    int measureTextRegular(const String& text);

private:
    // Page dimensions
    int _pageWidth;
    int _pageHeight;
    int _marginLeft, _marginRight, _marginTop, _marginBottom;
    int _contentWidth;
    int _contentHeight;
    
    // Typography
    int _lineHeight;
    int _baseLineHeight;
    float _lineCompression;      // Multiplier for line spacing
    int _paraSpacing;
    int _spaceWidth;
    int _linesPerPage;
    TextAlign _defaultAlign;
    bool _justify;
    bool _extraParagraphSpacing;
    bool _hyphenationEnabled;
    bool _paragraphIndent;
    
    // Font family (4 variants for styled text)
    const GFXfont* _fontRegular;
    const GFXfont* _fontBold;
    const GFXfont* _fontItalic;
    const GFXfont* _fontBoldItalic;
    
    // Layout state
    CachedPage _currentPage;
    int _currentY;
    int _currentOffset;
    std::vector<CachedPage> _completedPages;
    bool _inLayout;
    int _wordCount;
    
    // =========================================================================
    // Internal Methods
    // =========================================================================
    
    // Get the appropriate font for a style
    const GFXfont* getFontForStyle(FontStyle style) const;
    
    // Split text into paragraphs
    std::vector<String> splitParagraphs(const String& text);
    
    // Create TextBlock from paragraph text (basic - all regular style)
    TextBlock createTextBlock(const String& para, TextAlign align);
    
    // Split paragraph into words and measure them (legacy method)
    std::vector<MeasuredWord> measureWords(const String& para);
    
    // Calculate word widths with their styles
    std::vector<uint16_t> calculateWordWidths(TextBlock& block);
    
    // Optimal line breaking using dynamic programming
    // Minimizes "badness" (squared remaining space) for even appearance
    std::vector<size_t> computeOptimalLineBreaks(
        const std::vector<uint16_t>& wordWidths,
        int pageWidth, int spaceWidth);
    
    // Greedy line breaking (faster, used for very long paragraphs)
    std::vector<size_t> computeGreedyLineBreaks(
        const std::vector<uint16_t>& wordWidths,
        int pageWidth, int spaceWidth);
    
    // Wrap measured words into lines (legacy method)
    std::vector<LayoutLine> wrapToLines(const std::vector<MeasuredWord>& words);
    
    // Layout a TextBlock into lines and add to pages
    void layoutTextBlock(TextBlock& block, bool isHeader);
    
    // Convert LayoutLine to CachedLine with positioned words
    CachedLine positionLine(const LayoutLine& line, int y);
    
    // Calculate word X positions based on alignment
    std::vector<uint16_t> calculateWordPositions(
        const std::vector<StyledWord>& words,
        int lineWidth, int spaceWidth,
        TextAlign align, bool isLastLine);
    
    // Legacy: Calculate justified positions
    std::vector<int> calculateJustifiedPositions(
        const std::vector<MeasuredWord>& words, 
        int lineWidth, 
        bool isLast);
    
    // Page management
    bool addLineToPage(const CachedLine& line);
    void newPage();
    void completePage();
    bool hasRoomForLine() const;
    
    // Update metrics after config change
    void updateMetrics();
    
    // Soft hyphen handling
    static bool containsSoftHyphen(const String& word);
    static void stripSoftHyphens(String& word);
};

// Global instance
extern TextLayout textLayout;

#endif // SUMI_TEXT_LAYOUT_H
