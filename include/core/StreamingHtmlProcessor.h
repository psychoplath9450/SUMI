/**
 * @file StreamingHtmlProcessor.h
 * @brief Streaming HTML to Text Processor
 * @version 1.4.2
 *
 * Processes HTML chapter files in 1KB chunks to extract text.
 * NEVER loads entire file into RAM.
 *
 * Features:
 * - Streaming state machine for tag parsing
 * - Graceful degradation for unsupported content
 * - Direct output to TextLayout or file
 * - Handles entities, scripts, styles
 */

#ifndef SUMI_STREAMING_HTML_PROCESSOR_H
#define SUMI_STREAMING_HTML_PROCESSOR_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

// =============================================================================
// Processing Callbacks
// =============================================================================

/**
 * Callback for processed text output
 * Called with chunks of clean text (no HTML tags)
 */
typedef void (*TextOutputCallback)(const char* text, size_t len, void* userData);

/**
 * Callback for paragraph breaks
 */
typedef void (*ParagraphCallback)(void* userData);

/**
 * Callback for style changes (bold, italic)
 */
enum class TextStyle { NORMAL, BOLD, ITALIC, BOLD_ITALIC };
typedef void (*StyleChangeCallback)(TextStyle style, void* userData);

// =============================================================================
// HTML Processing State
// =============================================================================
enum class HtmlState {
    TEXT,           // Normal text content
    TAG_START,      // Just saw '<'
    TAG_NAME,       // Reading tag name
    TAG_ATTRS,      // In tag attributes
    TAG_CLOSE,      // In closing tag </...>
    COMMENT,        // In <!-- ... -->
    SCRIPT,         // In <script>...</script>
    STYLE,          // In <style>...</style>
    ENTITY,         // In &...; entity
    CDATA           // In <![CDATA[...]]>
};

// =============================================================================
// Streaming HTML Processor
// =============================================================================
class StreamingHtmlProcessor {
public:
    StreamingHtmlProcessor();
    
    /**
     * Set callbacks for output
     */
    void setTextCallback(TextOutputCallback cb, void* userData) {
        _textCallback = cb;
        _textUserData = userData;
    }
    
    void setParagraphCallback(ParagraphCallback cb, void* userData) {
        _paraCallback = cb;
        _paraUserData = userData;
    }
    
    void setStyleCallback(StyleChangeCallback cb, void* userData) {
        _styleCallback = cb;
        _styleUserData = userData;
    }
    
    /**
     * Process an HTML file in streaming fashion
     * @param htmlPath Path to HTML file on SD card
     * @return true if successful
     */
    bool processFile(const String& htmlPath);
    
    /**
     * Process a chunk of HTML data
     * Call multiple times for streaming, then call finish()
     */
    void processChunk(const uint8_t* data, size_t len);
    
    /**
     * Finish processing (flush any pending output)
     */
    void finish();
    
    /**
     * Reset state for new document
     */
    void reset();
    
    /**
     * Get error message
     */
    const String& getError() const { return _error; }
    
    /**
     * Statistics
     */
    size_t getTextBytes() const { return _textBytesOutput; }
    size_t getParagraphCount() const { return _paragraphCount; }
    
private:
    // Callbacks
    TextOutputCallback _textCallback;
    void* _textUserData;
    ParagraphCallback _paraCallback;
    void* _paraUserData;
    StyleChangeCallback _styleCallback;
    void* _styleUserData;
    
    // State
    HtmlState _state;
    String _error;
    
    // Tag parsing
    char _tagName[32];
    int _tagNameLen;
    bool _isClosingTag;
    
    // Entity parsing
    char _entity[16];
    int _entityLen;
    
    // Style tracking
    int _boldDepth;
    int _italicDepth;
    TextStyle _currentStyle;
    
    // Text output buffer (small, for batching)
    char _textBuffer[256];
    int _textBufferLen;
    
    // Statistics
    size_t _textBytesOutput;
    size_t _paragraphCount;
    
    // Whitespace normalization
    bool _lastWasWhitespace;
    bool _inBody;
    bool _skipContent;  // For script/style content
    
    // ==========================================================================
    // Internal Methods
    // ==========================================================================
    
    void processChar(char c);
    void handleTagStart(char c);
    void handleTagName(char c);
    void handleTagAttrs(char c);
    void handleEntity(char c);
    void handleText(char c);
    
    void finishTag();
    void handleOpenTag();
    void handleCloseTag();
    
    void outputChar(char c);
    void outputText(const char* text, size_t len);
    void flushTextBuffer();
    void outputParagraphBreak();
    
    void updateStyle();
    char decodeEntity(const char* entity);
    
    bool isBlockTag(const char* tag);
    bool isParagraphTag(const char* tag);
    bool isWhitespace(char c);
    
    // Skip tags - graceful degradation
    bool shouldSkipTag(const char* tag);
    void outputSkipPlaceholder(const char* tag);
};

// =============================================================================
// Implementation
// =============================================================================

inline StreamingHtmlProcessor::StreamingHtmlProcessor()
    : _textCallback(nullptr)
    , _textUserData(nullptr)
    , _paraCallback(nullptr)
    , _paraUserData(nullptr)
    , _styleCallback(nullptr)
    , _styleUserData(nullptr)
    , _state(HtmlState::TEXT)
    , _tagNameLen(0)
    , _isClosingTag(false)
    , _entityLen(0)
    , _boldDepth(0)
    , _italicDepth(0)
    , _currentStyle(TextStyle::NORMAL)
    , _textBufferLen(0)
    , _textBytesOutput(0)
    , _paragraphCount(0)
    , _lastWasWhitespace(true)
    , _inBody(false)
    , _skipContent(false) {
    memset(_tagName, 0, sizeof(_tagName));
    memset(_entity, 0, sizeof(_entity));
    memset(_textBuffer, 0, sizeof(_textBuffer));
}

inline void StreamingHtmlProcessor::reset() {
    _state = HtmlState::TEXT;
    _tagNameLen = 0;
    _isClosingTag = false;
    _entityLen = 0;
    _boldDepth = 0;
    _italicDepth = 0;
    _currentStyle = TextStyle::NORMAL;
    _textBufferLen = 0;
    _textBytesOutput = 0;
    _paragraphCount = 0;
    _lastWasWhitespace = true;
    _inBody = false;
    _skipContent = false;
    _error = "";
    memset(_tagName, 0, sizeof(_tagName));
    memset(_entity, 0, sizeof(_entity));
    memset(_textBuffer, 0, sizeof(_textBuffer));
}

inline bool StreamingHtmlProcessor::processFile(const String& htmlPath) {
    reset();
    
    File f = SD.open(htmlPath, FILE_READ);
    if (!f) {
        _error = "Failed to open: " + htmlPath;
        return false;
    }
    
    MEM_LOG("html_process_start");
    
    uint8_t buffer[1024];
    size_t totalRead = 0;
    
    while (f.available()) {
        size_t bytesRead = f.read(buffer, sizeof(buffer));
        if (bytesRead == 0) break;
        
        processChunk(buffer, bytesRead);
        totalRead += bytesRead;
        
        // Yield every 8KB
        if (totalRead % 8192 == 0) {
            yield();
        }
    }
    
    f.close();
    finish();
    
    MEM_LOG("html_process_end");
    
    Serial.printf("[HTML] Processed %d bytes -> %d text bytes, %d paragraphs\n",
                  (int)totalRead, (int)_textBytesOutput, (int)_paragraphCount);
    
    return true;
}

inline void StreamingHtmlProcessor::processChunk(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        processChar((char)data[i]);
    }
}

inline void StreamingHtmlProcessor::finish() {
    // Flush any pending entity
    if (_state == HtmlState::ENTITY && _entityLen > 0) {
        // Output entity as-is if not decoded
        outputChar('&');
        for (int i = 0; i < _entityLen; i++) {
            outputChar(_entity[i]);
        }
    }
    
    flushTextBuffer();
}

inline void StreamingHtmlProcessor::processChar(char c) {
    switch (_state) {
        case HtmlState::TEXT:
            handleText(c);
            break;
            
        case HtmlState::TAG_START:
            handleTagStart(c);
            break;
            
        case HtmlState::TAG_NAME:
            handleTagName(c);
            break;
            
        case HtmlState::TAG_ATTRS:
            handleTagAttrs(c);
            break;
            
        case HtmlState::TAG_CLOSE:
            handleTagAttrs(c);  // Same handling
            break;
            
        case HtmlState::COMMENT:
            // Look for -->
            // Simplified: just wait for >
            if (c == '>') {
                _state = HtmlState::TEXT;
            }
            break;
            
        case HtmlState::SCRIPT:
        case HtmlState::STYLE:
            // Look for </script> or </style>
            if (c == '<') {
                _state = HtmlState::TAG_START;
            }
            break;
            
        case HtmlState::ENTITY:
            handleEntity(c);
            break;
            
        case HtmlState::CDATA:
            // Look for ]]>
            if (c == '>') {
                _state = HtmlState::TEXT;
            } else if (c != ']') {
                outputChar(c);
            }
            break;
    }
}

inline void StreamingHtmlProcessor::handleText(char c) {
    if (c == '<') {
        _state = HtmlState::TAG_START;
        _tagNameLen = 0;
        _isClosingTag = false;
        memset(_tagName, 0, sizeof(_tagName));
    }
    else if (c == '&') {
        _state = HtmlState::ENTITY;
        _entityLen = 0;
        memset(_entity, 0, sizeof(_entity));
    }
    else if (_inBody && !_skipContent) {
        outputChar(c);
    }
}

inline void StreamingHtmlProcessor::handleTagStart(char c) {
    if (c == '/') {
        _isClosingTag = true;
        _state = HtmlState::TAG_NAME;
    }
    else if (c == '!') {
        // Could be comment or CDATA
        _state = HtmlState::COMMENT;
    }
    else if (c == '?') {
        // XML declaration, treat as comment
        _state = HtmlState::COMMENT;
    }
    else if (isalpha(c)) {
        _tagName[0] = tolower(c);
        _tagNameLen = 1;
        _state = HtmlState::TAG_NAME;
    }
    else {
        // Not a valid tag, output the '<' and this char
        if (_inBody && !_skipContent) {
            outputChar('<');
            outputChar(c);
        }
        _state = HtmlState::TEXT;
    }
}

inline void StreamingHtmlProcessor::handleTagName(char c) {
    if (isalnum(c) || c == '-' || c == '_' || c == ':') {
        if (_tagNameLen < (int)sizeof(_tagName) - 1) {
            _tagName[_tagNameLen++] = tolower(c);
        }
    }
    else if (c == '>') {
        finishTag();
        _state = HtmlState::TEXT;
    }
    else if (c == '/' && !_isClosingTag) {
        // Self-closing tag like <br/>
        _state = HtmlState::TAG_ATTRS;
    }
    else if (isWhitespace(c)) {
        _state = HtmlState::TAG_ATTRS;
    }
    else {
        _state = HtmlState::TAG_ATTRS;
    }
}

inline void StreamingHtmlProcessor::handleTagAttrs(char c) {
    if (c == '>') {
        finishTag();
        _state = HtmlState::TEXT;
    }
    // Otherwise skip attribute content
}

inline void StreamingHtmlProcessor::handleEntity(char c) {
    if (c == ';') {
        // Decode entity
        char decoded = decodeEntity(_entity);
        if (decoded != 0) {
            outputChar(decoded);
        }
        // If not decoded, just skip it (graceful degradation)
        _state = HtmlState::TEXT;
    }
    else if (_entityLen < (int)sizeof(_entity) - 1 && 
             (isalnum(c) || c == '#')) {
        _entity[_entityLen++] = c;
    }
    else {
        // Invalid entity, output as-is
        outputChar('&');
        for (int i = 0; i < _entityLen; i++) {
            outputChar(_entity[i]);
        }
        if (c != '&') {
            outputChar(c);
            _state = HtmlState::TEXT;
        } else {
            _entityLen = 0;
        }
    }
}

inline void StreamingHtmlProcessor::finishTag() {
    _tagName[_tagNameLen] = '\0';
    
    if (_isClosingTag) {
        handleCloseTag();
    } else {
        handleOpenTag();
    }
}

inline void StreamingHtmlProcessor::handleOpenTag() {
    // Track body
    if (strcmp(_tagName, "body") == 0) {
        _inBody = true;
        return;
    }
    
    if (!_inBody) return;
    
    // Check for tags to skip entirely
    if (strcmp(_tagName, "script") == 0) {
        _state = HtmlState::SCRIPT;
        _skipContent = true;
        return;
    }
    if (strcmp(_tagName, "style") == 0) {
        _state = HtmlState::STYLE;
        _skipContent = true;
        return;
    }
    
    // Graceful degradation - skip unsupported content
    if (shouldSkipTag(_tagName)) {
        outputSkipPlaceholder(_tagName);
        return;
    }
    
    // Block elements cause line breaks
    if (isParagraphTag(_tagName)) {
        outputParagraphBreak();
    }
    else if (isBlockTag(_tagName)) {
        // Just ensure whitespace
        if (!_lastWasWhitespace) {
            outputChar(' ');
        }
    }
    
    // Style tags
    if (strcmp(_tagName, "b") == 0 || strcmp(_tagName, "strong") == 0) {
        _boldDepth++;
        updateStyle();
    }
    else if (strcmp(_tagName, "i") == 0 || strcmp(_tagName, "em") == 0) {
        _italicDepth++;
        updateStyle();
    }
    
    // Line break
    if (strcmp(_tagName, "br") == 0) {
        flushTextBuffer();
        if (!_lastWasWhitespace) {
            outputChar(' ');
        }
    }
}

inline void StreamingHtmlProcessor::handleCloseTag() {
    // Check for script/style end
    if (strcmp(_tagName, "script") == 0 || strcmp(_tagName, "style") == 0) {
        _skipContent = false;
        _state = HtmlState::TEXT;
        return;
    }
    
    if (!_inBody) return;
    
    if (strcmp(_tagName, "body") == 0) {
        flushTextBuffer();
        _inBody = false;
        return;
    }
    
    // Block elements
    if (isParagraphTag(_tagName)) {
        outputParagraphBreak();
    }
    else if (isBlockTag(_tagName)) {
        if (!_lastWasWhitespace) {
            outputChar(' ');
        }
    }
    
    // Style tags
    if (strcmp(_tagName, "b") == 0 || strcmp(_tagName, "strong") == 0) {
        if (_boldDepth > 0) _boldDepth--;
        updateStyle();
    }
    else if (strcmp(_tagName, "i") == 0 || strcmp(_tagName, "em") == 0) {
        if (_italicDepth > 0) _italicDepth--;
        updateStyle();
    }
}

inline void StreamingHtmlProcessor::outputChar(char c) {
    if (_skipContent) return;
    
    // Filter invisible UTF-8 characters
    // We use a simple state machine to detect and skip:
    // - Soft hyphen (U+00AD) = 0xC2 0xAD
    // - Zero-width space (U+200B) = 0xE2 0x80 0x8B
    // - Other zero-width chars (U+200C-200F) = 0xE2 0x80 0x8C-0x8F
    // - BOM (U+FEFF) = 0xEF 0xBB 0xBF
    static uint8_t utf8State = 0;
    static uint8_t utf8Byte1 = 0;
    static uint8_t utf8Byte2 = 0;
    uint8_t uc = (uint8_t)c;
    
    // State machine for UTF-8 invisible char detection
    if (utf8State == 0) {
        if (uc == 0xC2) { utf8State = 1; utf8Byte1 = uc; return; }  // Possible soft hyphen start
        if (uc == 0xE2) { utf8State = 2; utf8Byte1 = uc; return; }  // Possible zero-width start
        if (uc == 0xEF) { utf8State = 5; utf8Byte1 = uc; return; }  // Possible BOM start
        // Other non-ASCII: let through (could be accented chars)
    }
    else if (utf8State == 1) {  // After 0xC2
        utf8State = 0;
        if (uc == 0xAD) return;  // Soft hyphen - skip both bytes
        // Not soft hyphen - was legitimate UTF-8, skip it (simplify for e-ink)
        return;
    }
    else if (utf8State == 2) {  // After 0xE2
        utf8State = 3; utf8Byte2 = uc; return;
    }
    else if (utf8State == 3) {  // After 0xE2 0x??
        utf8State = 0;
        // Check for zero-width chars: 0xE2 0x80 0x8B-0x8F
        if (utf8Byte2 == 0x80 && uc >= 0x8B && uc <= 0x8F) return;  // Zero-width - skip
        // Check for BOM-like: 0xE2 0x80 0xA8/0xA9 (line/para separator)
        if (utf8Byte2 == 0x80 && (uc == 0xA8 || uc == 0xA9)) return;
        // Other 3-byte sequence - skip for simplicity
        return;
    }
    else if (utf8State == 5) {  // After 0xEF
        utf8State = 6; utf8Byte2 = uc; return;
    }
    else if (utf8State == 6) {  // After 0xEF 0x??
        utf8State = 0;
        // BOM: 0xEF 0xBB 0xBF
        if (utf8Byte2 == 0xBB && uc == 0xBF) return;  // BOM - skip
        return;  // Other - skip
    }
    
    bool isWS = isWhitespace(c);
    
    // Normalize whitespace
    if (isWS) {
        if (_lastWasWhitespace) return;  // Skip consecutive whitespace
        c = ' ';
        _lastWasWhitespace = true;
    } else {
        _lastWasWhitespace = false;
    }
    
    // Buffer the character
    _textBuffer[_textBufferLen++] = c;
    
    // Flush if buffer full
    if (_textBufferLen >= (int)sizeof(_textBuffer) - 1) {
        flushTextBuffer();
    }
}

inline void StreamingHtmlProcessor::outputText(const char* text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        outputChar(text[i]);
    }
}

inline void StreamingHtmlProcessor::flushTextBuffer() {
    if (_textBufferLen > 0 && _textCallback) {
        _textBuffer[_textBufferLen] = '\0';
        _textCallback(_textBuffer, _textBufferLen, _textUserData);
        _textBytesOutput += _textBufferLen;
    }
    _textBufferLen = 0;
}

inline void StreamingHtmlProcessor::outputParagraphBreak() {
    flushTextBuffer();
    
    if (_paraCallback) {
        _paraCallback(_paraUserData);
    }
    
    _paragraphCount++;
    _lastWasWhitespace = true;
}

inline void StreamingHtmlProcessor::updateStyle() {
    TextStyle newStyle;
    
    if (_boldDepth > 0 && _italicDepth > 0) {
        newStyle = TextStyle::BOLD_ITALIC;
    } else if (_boldDepth > 0) {
        newStyle = TextStyle::BOLD;
    } else if (_italicDepth > 0) {
        newStyle = TextStyle::ITALIC;
    } else {
        newStyle = TextStyle::NORMAL;
    }
    
    if (newStyle != _currentStyle) {
        flushTextBuffer();
        _currentStyle = newStyle;
        if (_styleCallback) {
            _styleCallback(_currentStyle, _styleUserData);
        }
    }
}

inline char StreamingHtmlProcessor::decodeEntity(const char* entity) {
    if (!entity || entity[0] == '\0') return 0;
    
    // Numeric entities
    if (entity[0] == '#') {
        int value = 0;
        if (entity[1] == 'x' || entity[1] == 'X') {
            // Hex
            value = strtol(entity + 2, nullptr, 16);
        } else {
            // Decimal
            value = atoi(entity + 1);
        }
        
        // Only handle ASCII range for simplicity
        if (value >= 32 && value < 127) {
            return (char)value;
        }
        // Skip invisible/formatting characters (return 0 = don't output)
        if (value == 173) return 0;     // soft hyphen (&shy;)
        if (value == 0xAD) return 0;    // soft hyphen (hex)
        if (value >= 0x200B && value <= 0x200F) return 0;  // zero-width spaces, direction marks
        if (value == 0xFEFF) return 0;  // BOM / zero-width no-break space
        if (value == 0x00AD) return 0;  // soft hyphen again (for completeness)
        
        // Common UTF-8 mappings
        if (value == 160) return ' ';   // nbsp
        if (value == 8217) return '\''; // rsquo
        if (value == 8216) return '\''; // lsquo
        if (value == 8220) return '"';  // ldquo
        if (value == 8221) return '"';  // rdquo
        if (value == 8211) return '-';  // ndash
        if (value == 8212) return '-';  // mdash
        if (value == 8230) return '.';  // hellip (simplified)
        
        return ' ';  // Default to space for unknown
    }
    
    // Named entities
    if (strcmp(entity, "amp") == 0) return '&';
    if (strcmp(entity, "lt") == 0) return '<';
    if (strcmp(entity, "gt") == 0) return '>';
    if (strcmp(entity, "quot") == 0) return '"';
    if (strcmp(entity, "apos") == 0) return '\'';
    if (strcmp(entity, "nbsp") == 0) return ' ';
    if (strcmp(entity, "shy") == 0) return 0;  // Soft hyphen - skip entirely
    if (strcmp(entity, "ndash") == 0) return '-';
    if (strcmp(entity, "mdash") == 0) return '-';
    if (strcmp(entity, "lsquo") == 0) return '\'';
    if (strcmp(entity, "rsquo") == 0) return '\'';
    if (strcmp(entity, "ldquo") == 0) return '"';
    if (strcmp(entity, "rdquo") == 0) return '"';
    if (strcmp(entity, "hellip") == 0) return '.';
    if (strcmp(entity, "copy") == 0) return 'c';
    if (strcmp(entity, "reg") == 0) return 'r';
    if (strcmp(entity, "trade") == 0) return 't';
    if (strcmp(entity, "deg") == 0) return 'd';
    if (strcmp(entity, "plusmn") == 0) return '+';
    if (strcmp(entity, "frac12") == 0) return ' ';
    if (strcmp(entity, "frac14") == 0) return ' ';
    if (strcmp(entity, "frac34") == 0) return ' ';
    
    // Unknown - skip
    return ' ';
}

inline bool StreamingHtmlProcessor::isBlockTag(const char* tag) {
    return strcmp(tag, "div") == 0 ||
           strcmp(tag, "section") == 0 ||
           strcmp(tag, "article") == 0 ||
           strcmp(tag, "header") == 0 ||
           strcmp(tag, "footer") == 0 ||
           strcmp(tag, "nav") == 0 ||
           strcmp(tag, "aside") == 0 ||
           strcmp(tag, "main") == 0 ||
           strcmp(tag, "li") == 0 ||
           strcmp(tag, "dd") == 0 ||
           strcmp(tag, "dt") == 0 ||
           strcmp(tag, "blockquote") == 0 ||
           strcmp(tag, "pre") == 0 ||
           strcmp(tag, "figure") == 0 ||
           strcmp(tag, "figcaption") == 0;
}

inline bool StreamingHtmlProcessor::isParagraphTag(const char* tag) {
    return strcmp(tag, "p") == 0 ||
           strcmp(tag, "h1") == 0 ||
           strcmp(tag, "h2") == 0 ||
           strcmp(tag, "h3") == 0 ||
           strcmp(tag, "h4") == 0 ||
           strcmp(tag, "h5") == 0 ||
           strcmp(tag, "h6") == 0;
}

inline bool StreamingHtmlProcessor::isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline bool StreamingHtmlProcessor::shouldSkipTag(const char* tag) {
    // Tags we can't render properly - graceful degradation
    return strcmp(tag, "table") == 0 ||
           strcmp(tag, "svg") == 0 ||
           strcmp(tag, "canvas") == 0 ||
           strcmp(tag, "video") == 0 ||
           strcmp(tag, "audio") == 0 ||
           strcmp(tag, "iframe") == 0 ||
           strcmp(tag, "object") == 0 ||
           strcmp(tag, "embed") == 0 ||
           strcmp(tag, "form") == 0 ||
           strcmp(tag, "input") == 0 ||
           strcmp(tag, "button") == 0 ||
           strcmp(tag, "select") == 0 ||
           strcmp(tag, "textarea") == 0;
}

inline void StreamingHtmlProcessor::outputSkipPlaceholder(const char* tag) {
    // Graceful degradation - show placeholder for unsupported content
    if (strcmp(tag, "table") == 0) {
        outputText("\n[Table omitted]\n", 17);
    }
    else if (strcmp(tag, "svg") == 0 || strcmp(tag, "canvas") == 0) {
        outputText("\n[Image]\n", 9);
    }
    else if (strcmp(tag, "video") == 0) {
        outputText("\n[Video]\n", 9);
    }
    else if (strcmp(tag, "audio") == 0) {
        outputText("\n[Audio]\n", 9);
    }
    else if (strcmp(tag, "form") == 0) {
        outputText("\n[Form omitted]\n", 16);
    }
    // Other tags silently skipped
}

// =============================================================================
// Global instance
// =============================================================================
extern StreamingHtmlProcessor htmlProcessor;

#endif // SUMI_STREAMING_HTML_PROCESSOR_H
