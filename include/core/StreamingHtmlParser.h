/**
 * @file StreamingHtmlParser.h
 * @brief Streaming HTML Parser for EPUB Chapters
 * @version 2.4.0
 *
 * Features:
 * - Chunk-based parsing (never loads full file)
 * - Per-word font style tracking (bold, italic, bold-italic)
 * - Depth-based formatting state (handles nested tags correctly)
 * - HTML entity decoding with full Unicode support
 * - Skip irrelevant content (head, script, style, table)
 * - List item bullet points
 * - Header detection and styling
 * - Scene break markers (<hr>)
 *
 * Architecture based on professional e-reader implementations.
 */

#ifndef SUMI_STREAMING_HTML_PARSER_H
#define SUMI_STREAMING_HTML_PARSER_H

#include <Arduino.h>
#include <SD.h>
#include <functional>
#include <limits.h>
#include "TextLayout.h"

// =============================================================================
// Parser Configuration
// =============================================================================

#define HTML_PARSE_BUFFER_SIZE 1024
#define HTML_MAX_TAG_NAME 32
#define HTML_MAX_ENTITY 12
#define HTML_MAX_WORD 200

// =============================================================================
// Styled Paragraph Callback
// =============================================================================

// Standard callback: paragraph text + header flag
using ParagraphCallback = std::function<void(const String& text, bool isHeader)>;

// Enhanced callback: receives a TextBlock with per-word styling
using StyledParagraphCallback = std::function<void(TextBlock& block, bool isHeader)>;

// =============================================================================
// Streaming HTML Parser Class
// =============================================================================
class StreamingHtmlParser {
public:
    StreamingHtmlParser();
    ~StreamingHtmlParser();
    
    // =========================================================================
    // Main Parse Methods
    // =========================================================================
    
    /**
     * Parse HTML file and emit paragraphs via callback (legacy)
     * @param file Open file handle
     * @param onParagraph Callback for each paragraph
     * @return true if parsing succeeded
     */
    bool parse(File& file, ParagraphCallback onParagraph);
    
    /**
     * Parse HTML file with styled output
     * @param file Open file handle
     * @param onStyledParagraph Callback for styled text blocks
     * @return true if parsing succeeded
     */
    bool parseStyled(File& file, StyledParagraphCallback onStyledParagraph);
    
    /**
     * Parse HTML string (for small content)
     */
    bool parseString(const String& html, ParagraphCallback onParagraph);
    bool parseStringStyled(const String& html, StyledParagraphCallback onStyledParagraph);
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    void setPreserveFormatting(bool preserve) { _preserveFormatting = preserve; }
    void setMinParagraphLength(int len) { _minParaLength = len; }
    void setExtraParagraphSpacing(bool extra) { _extraParagraphSpacing = extra; }
    void setDefaultAlignment(TextAlign align) { _defaultAlignment = align; }
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    int getParagraphCount() const { return _paragraphCount; }
    int getCharacterCount() const { return _characterCount; }
    int getBytesProcessed() const { return _bytesProcessed; }

private:
    // Callbacks
    ParagraphCallback _onParagraph;
    StyledParagraphCallback _onStyledParagraph;
    bool _useStyledCallback;
    
    // Configuration
    bool _preserveFormatting;
    int _minParaLength;
    bool _extraParagraphSpacing;
    TextAlign _defaultAlignment;
    
    // Parser state machine
    enum class State {
        TEXT,
        TAG_START,
        TAG_NAME,
        TAG_ATTRS,
        TAG_CLOSE,
        ENTITY,
        COMMENT,
        CDATA
    };
    
    State _state;
    
    // Tag tracking
    char _tagName[HTML_MAX_TAG_NAME];
    int _tagNameLen;
    bool _isClosingTag;
    
    // Content tracking with depth-based formatting
    bool _inBody;
    bool _inParagraph;
    bool _inHeader;
    int _headerLevel;
    
    // Depth-based style tracking (like the reference implementation)
    // When we enter a bold tag, we set boldUntilDepth = current depth
    // Any text parsed while depth > boldUntilDepth is bold
    int _boldUntilDepth;
    int _italicUntilDepth;
    int _skipUntilDepth;
    int _depth;
    
    // Legacy flags (for non-styled mode)
    bool _inBold;
    bool _inItalic;
    int _skipDepth;
    
    // Entity decoding
    char _entityBuffer[HTML_MAX_ENTITY];
    int _entityLen;
    
    // Word buffer for styled parsing
    char _wordBuffer[HTML_MAX_WORD];
    int _wordBufferLen;
    
    // Current paragraph (legacy mode)
    String _currentPara;
    bool _lastWasSpace;
    
    // Current styled text block
    TextBlock* _currentBlock;
    
    // Statistics
    int _paragraphCount;
    int _characterCount;
    int _bytesProcessed;
    
    // =========================================================================
    // Internal Methods
    // =========================================================================
    
    void processChunk(const char* data, size_t len);
    void processChar(char c);
    void handleTagStart();
    void handleTagEnd();
    void handleStartTag(const char* name);
    void handleEndTag(const char* name);
    
    // Get current font style based on depth tracking
    FontStyle getCurrentStyle() const;
    
    // Word handling for styled mode
    void flushWord();
    void addCharToWord(char c);
    
    // Legacy text handling
    void addChar(char c);
    void addText(const char* text);
    
    // Paragraph handling
    void startNewBlock(TextAlign alignment);
    void flushParagraph();
    void flushStyledParagraph();
    
    // Entity decoding
    char decodeEntity();
    
    // Tag matching
    bool tagIs(const char* name) const;
    bool tagIsAny(const char** names, int count) const;
    
    void reset();
};

// =============================================================================
// HTML Entity Decoding Functions
// =============================================================================

String decodeHtmlEntities(const String& text);
String decodeSingleEntity(const String& entity);

#endif // SUMI_STREAMING_HTML_PARSER_H
