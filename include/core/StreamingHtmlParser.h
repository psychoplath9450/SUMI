/**
 * @file StreamingHtmlParser.h
 * @brief Streaming HTML Parser for EPUB Chapters
 * @version 2.1.16
 *
 * Features:
 * - Chunk-based parsing (never loads full file)
 * - Callback-based paragraph delivery
 * - HTML entity decoding
 * - Basic formatting support (bold, italic, headers)
 * - Skip irrelevant content (head, script, style, table)
 *
 * Lightweight streaming HTML parser that handles the subset of HTML
 * found in EPUBs.
 *
 * Usage:
 *   StreamingHtmlParser parser;
 *   parser.parse(htmlFile, [](const String& text, bool isHeader, bool isBold, bool isItalic) {
 *       textLayout.addParagraph(text, isHeader);
 *   });
 */

#ifndef SUMI_STREAMING_HTML_PARSER_H
#define SUMI_STREAMING_HTML_PARSER_H

#include <Arduino.h>
#include <SD.h>
#include <functional>

// =============================================================================
// Parser Configuration
// =============================================================================

// Read buffer size (balance between memory and efficiency)
#define HTML_PARSE_BUFFER_SIZE 1024

// Maximum tag name length
#define HTML_MAX_TAG_NAME 32

// Maximum entity length (e.g., "&nbsp;")
#define HTML_MAX_ENTITY 12

// Maximum word buffer size
#define HTML_MAX_WORD 128

// =============================================================================
// Paragraph Info Structure
// =============================================================================
struct ParsedParagraph {
    String text;            // Paragraph text content
    bool isHeader;          // Is this a heading (h1-h6)?
    int headerLevel;        // 1-6 for h1-h6, 0 for not a header
    bool hasFormatting;     // Contains bold/italic spans
};

// =============================================================================
// Paragraph Callback Type
// =============================================================================
// Called for each completed paragraph
// Parameters: text, isHeader, isBold, isItalic
using ParagraphCallback = std::function<void(const String& text, bool isHeader)>;

// =============================================================================
// Streaming HTML Parser Class
// =============================================================================
class StreamingHtmlParser {
public:
    StreamingHtmlParser();
    ~StreamingHtmlParser();
    
    // =========================================================================
    // Main Parse Method
    // =========================================================================
    
    /**
     * Parse HTML file and emit paragraphs via callback
     * @param file Open file handle to read from
     * @param onParagraph Callback for each paragraph
     * @return true if parsing succeeded
     */
    bool parse(File& file, ParagraphCallback onParagraph);
    
    /**
     * Parse HTML string (for small content)
     */
    bool parseString(const String& html, ParagraphCallback onParagraph);
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /**
     * Set whether to preserve basic formatting markers
     * If false, all formatting is stripped
     */
    void setPreserveFormatting(bool preserve) { _preserveFormatting = preserve; }
    
    /**
     * Set minimum paragraph length to emit
     * Shorter paragraphs are merged with next
     */
    void setMinParagraphLength(int len) { _minParaLength = len; }
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    int getParagraphCount() const { return _paragraphCount; }
    int getCharacterCount() const { return _characterCount; }
    int getBytesProcessed() const { return _bytesProcessed; }

private:
    // Callback
    ParagraphCallback _onParagraph;
    
    // Configuration
    bool _preserveFormatting;
    int _minParaLength;
    
    // Parser state
    enum class State {
        TEXT,           // Reading text content
        TAG_START,      // Just saw '<'
        TAG_NAME,       // Reading tag name
        TAG_ATTRS,      // Reading tag attributes (skip)
        TAG_CLOSE,      // In closing tag </...>
        ENTITY,         // Reading entity like &amp;
        COMMENT,        // Inside <!-- ... -->
        CDATA           // Inside <![CDATA[ ... ]]>
    };
    
    State _state;
    
    // Tag tracking
    char _tagName[HTML_MAX_TAG_NAME];
    int _tagNameLen;
    bool _isClosingTag;
    
    // Content tracking
    bool _inBody;           // Past <body> tag
    bool _inParagraph;      // Inside <p>, <div>, etc.
    bool _inHeader;         // Inside <h1>-<h6>
    int _headerLevel;       // 1-6 for h1-h6
    bool _inBold;           // Inside <b>, <strong>
    bool _inItalic;         // Inside <i>, <em>
    int _skipDepth;         // Depth of skip tags (head, script, style, table)
    int _depth;             // Current nesting depth
    
    // Entity decoding
    char _entityBuffer[HTML_MAX_ENTITY];
    int _entityLen;
    
    // Current paragraph being built
    String _currentPara;
    bool _lastWasSpace;     // For whitespace normalization
    
    // Statistics
    int _paragraphCount;
    int _characterCount;
    int _bytesProcessed;
    
    // =========================================================================
    // Internal Methods
    // =========================================================================
    
    /**
     * Process a chunk of data
     */
    void processChunk(const char* data, size_t len);
    
    /**
     * Process a single character
     */
    void processChar(char c);
    
    /**
     * Handle start of a tag
     */
    void handleTagStart();
    
    /**
     * Handle end of a tag
     */
    void handleTagEnd();
    
    /**
     * Handle tag by name
     */
    void handleStartTag(const char* name);
    void handleEndTag(const char* name);
    
    /**
     * Add character to current paragraph
     */
    void addChar(char c);
    
    /**
     * Add text to current paragraph
     */
    void addText(const char* text);
    
    /**
     * Flush current paragraph
     */
    void flushParagraph();
    
    /**
     * Decode HTML entity in buffer
     */
    char decodeEntity();
    
    /**
     * Check if tag name matches
     */
    bool tagIs(const char* name) const;
    bool tagIsAny(const char** names, int count) const;
    
    /**
     * Reset parser state
     */
    void reset();
};

// =============================================================================
// HTML Entity Decoding (standalone function)
// =============================================================================

/**
 * Decode HTML entities in a string
 * @param text Text with entities like &amp;, &#123;, etc.
 * @return Decoded text
 */
String decodeHtmlEntities(const String& text);

/**
 * Decode a single HTML entity
 * @param entity Entity string including & and ; (e.g., "&amp;")
 * @return Decoded character or string, or original if unknown
 */
String decodeSingleEntity(const String& entity);

#endif // SUMI_STREAMING_HTML_PARSER_H
