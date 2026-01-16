/**
 * @file ExpatHtmlParser.h
 * @brief Expat-based Streaming HTML/XML Parser
 * 
 * Uses expat library for robust, memory-efficient XML parsing.
 * 
 * Features:
 * - Parses HTML/XHTML from file in chunks (not memory)
 * - Uses expat XML parser (battle-tested)
 * - Emits paragraphs via callback
 * - Handles HTML entities
 * - Low memory footprint
 * 
 * Version: 2.1.16
 */

#ifndef SUMI_EXPAT_HTML_PARSER_H
#define SUMI_EXPAT_HTML_PARSER_H

#include <Arduino.h>
#include <SD.h>
#include <functional>

// Forward declare expat types without including full header
// This avoids XMLCALL macro issues across platforms
struct XML_ParserStruct;
typedef struct XML_ParserStruct *XML_Parser;

// XMLCALL is a calling convention - empty on ESP32
#ifndef XMLCALL
#define XMLCALL
#endif

// Callback type: receives paragraph text and whether text is a header
using ExpatParagraphCallback = std::function<void(const String& text, bool isHeader)>;

// Maximum word size before splitting
#define EXPAT_MAX_WORD_SIZE 64

// Parse buffer size
#define EXPAT_PARSE_BUFFER_SIZE 1024

/**
 * Expat-based HTML Parser
 * 
 * Parses HTML/XHTML files using the expat XML library.
 * Designed for EPUB chapter parsing with minimal memory usage.
 */
class ExpatHtmlParser {
public:
    ExpatHtmlParser();
    ~ExpatHtmlParser();
    
    /**
     * Parse HTML file and emit paragraphs via callback
     * @param filepath Path to HTML file on SD card
     * @param callback Called for each paragraph
     * @return true if parsing succeeded
     */
    bool parseFile(const String& filepath, ExpatParagraphCallback callback);
    
    /**
     * Get number of paragraphs emitted
     */
    int getParagraphCount() const { return _paragraphCount; }
    
    /**
     * Get number of characters processed
     */
    int getCharacterCount() const { return _characterCount; }
    
    /**
     * Get last error message
     */
    const String& getError() const { return _error; }

private:
    // Callback
    ExpatParagraphCallback _callback;
    
    // State tracking
    int _depth;
    int _skipUntilDepth;
    int _boldUntilDepth;
    int _italicUntilDepth;
    bool _inBody;
    bool _inHeader;
    
    // Current text block
    String _currentText;
    bool _lastWasSpace;
    
    // Word buffer (for partial words across chunks)
    char _wordBuffer[EXPAT_MAX_WORD_SIZE + 1];
    int _wordBufferIndex;
    
    // Statistics
    int _paragraphCount;
    int _characterCount;
    
    // Error
    String _error;
    
    // Expat callbacks (static, called by expat)
    static void XMLCALL startElement(void* userData, const char* name, const char** atts);
    static void XMLCALL endElement(void* userData, const char* name);
    static void XMLCALL characterData(void* userData, const char* s, int len);
    
    // Internal helpers
    void handleStartTag(const char* name, const char** atts);
    void handleEndTag(const char* name);
    void handleCharacter(char c);
    void flushWordBuffer();
    void flushParagraph(bool isHeader = false);
    void startNewParagraph();
    
    // Tag matching
    bool isBlockTag(const char* name);
    bool isHeaderTag(const char* name);
    bool isBoldTag(const char* name);
    bool isItalicTag(const char* name);
    bool isSkipTag(const char* name);
    
    // Entity decoding
    String decodeEntities(const String& text);
};

/**
 * Replace common HTML entities in text
 */
String replaceHtmlEntities(const char* text);

#endif // SUMI_EXPAT_HTML_PARSER_H
