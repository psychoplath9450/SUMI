/**
 * @file StreamingHtmlParser.cpp
 * @brief Streaming HTML Parser Implementation
 * @version 2.1.16
 *
 * Low-memory HTML parsing for EPUB chapters. This is a streaming parser that:
 * - Reads data in chunks to minimize memory usage
 * - Emits paragraphs via callback as they're completed
 * - Handles common EPUB HTML tags
 * - Decodes HTML entities
 */

#include "core/StreamingHtmlParser.h"

// =============================================================================
// Tag Lists
// =============================================================================

// Block-level tags that start new paragraphs
static const char* BLOCK_TAGS[] = {"p", "div", "li", "br", "blockquote", "article", "section"};
static const int NUM_BLOCK_TAGS = 7;

// Header tags (centered, bold)
static const char* HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
static const int NUM_HEADER_TAGS = 6;

// Bold tags
static const char* BOLD_TAGS[] = {"b", "strong"};
static const int NUM_BOLD_TAGS = 2;

// Italic tags
static const char* ITALIC_TAGS[] = {"i", "em", "cite"};
static const int NUM_ITALIC_TAGS = 3;

// Tags whose content should be skipped entirely
static const char* SKIP_TAGS[] = {"head", "script", "style", "table", "nav", "aside", "footer"};
static const int NUM_SKIP_TAGS = 7;

// =============================================================================
// Constructor / Destructor
// =============================================================================

StreamingHtmlParser::StreamingHtmlParser() 
    : _onParagraph(nullptr),
      _preserveFormatting(true),
      _minParaLength(0),
      _state(State::TEXT),
      _tagNameLen(0),
      _isClosingTag(false),
      _inBody(false),
      _inParagraph(false),
      _inHeader(false),
      _headerLevel(0),
      _inBold(false),
      _inItalic(false),
      _skipDepth(0),
      _depth(0),
      _entityLen(0),
      _lastWasSpace(true),
      _paragraphCount(0),
      _characterCount(0),
      _bytesProcessed(0) {
    memset(_tagName, 0, sizeof(_tagName));
    memset(_entityBuffer, 0, sizeof(_entityBuffer));
}

StreamingHtmlParser::~StreamingHtmlParser() {
}

// =============================================================================
// Main Parse Methods
// =============================================================================

bool StreamingHtmlParser::parse(File& file, ParagraphCallback onParagraph) {
    if (!file || !onParagraph) {
        return false;
    }
    
    reset();
    _onParagraph = onParagraph;
    
    // Read and process in chunks
    char buffer[HTML_PARSE_BUFFER_SIZE];
    
    while (file.available()) {
        size_t bytesRead = file.readBytes(buffer, sizeof(buffer));
        if (bytesRead == 0) break;
        
        processChunk(buffer, bytesRead);
        _bytesProcessed += bytesRead;
    }
    
    // Flush any remaining content
    flushParagraph();
    
    Serial.printf("[HTML] Parsed: %d paragraphs, %d chars, %d bytes\n",
                  _paragraphCount, _characterCount, _bytesProcessed);
    
    return true;
}

bool StreamingHtmlParser::parseString(const String& html, ParagraphCallback onParagraph) {
    if (html.length() == 0 || !onParagraph) {
        return false;
    }
    
    reset();
    _onParagraph = onParagraph;
    
    processChunk(html.c_str(), html.length());
    _bytesProcessed = html.length();
    
    // Flush any remaining content
    flushParagraph();
    
    return true;
}

void StreamingHtmlParser::reset() {
    _state = State::TEXT;
    _tagNameLen = 0;
    _isClosingTag = false;
    _inBody = false;
    _inParagraph = false;
    _inHeader = false;
    _headerLevel = 0;
    _inBold = false;
    _inItalic = false;
    _skipDepth = 0;
    _depth = 0;
    _entityLen = 0;
    _currentPara = "";
    _lastWasSpace = true;
    _paragraphCount = 0;
    _characterCount = 0;
    _bytesProcessed = 0;
}

// =============================================================================
// Chunk Processing
// =============================================================================

void StreamingHtmlParser::processChunk(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        processChar(data[i]);
    }
}

void StreamingHtmlParser::processChar(char c) {
    switch (_state) {
        case State::TEXT:
            if (c == '<') {
                _state = State::TAG_START;
                _tagNameLen = 0;
                _isClosingTag = false;
            } else if (c == '&') {
                _state = State::ENTITY;
                _entityLen = 0;
                _entityBuffer[_entityLen++] = c;
            } else {
                // Only add text after <body> and not in skip mode
                if (_inBody && _skipDepth == 0) {
                    addChar(c);
                }
            }
            break;
            
        case State::TAG_START:
            if (c == '/') {
                _isClosingTag = true;
                _state = State::TAG_NAME;
            } else if (c == '!') {
                // Could be comment or CDATA
                _state = State::COMMENT;
            } else if (c == '?') {
                // XML declaration - skip
                _state = State::TAG_ATTRS;
            } else if (isalpha(c)) {
                _tagName[0] = tolower(c);
                _tagNameLen = 1;
                _state = State::TAG_NAME;
            } else {
                // Invalid tag start, treat as text
                _state = State::TEXT;
                if (_inBody && _skipDepth == 0) {
                    addChar('<');
                    addChar(c);
                }
            }
            break;
            
        case State::TAG_NAME:
            if (isalnum(c) || c == '-' || c == '_' || c == ':') {
                if (_tagNameLen < HTML_MAX_TAG_NAME - 1) {
                    _tagName[_tagNameLen++] = tolower(c);
                }
            } else if (c == '>') {
                _tagName[_tagNameLen] = '\0';
                handleTagEnd();
                _state = State::TEXT;
            } else if (c == '/' && !_isClosingTag) {
                // Self-closing tag like <br/>
                _tagName[_tagNameLen] = '\0';
                _state = State::TAG_ATTRS;
            } else {
                // Entering attributes section
                _tagName[_tagNameLen] = '\0';
                _state = State::TAG_ATTRS;
            }
            break;
            
        case State::TAG_ATTRS:
            if (c == '>') {
                handleTagEnd();
                _state = State::TEXT;
            } else if (c == '/' && _tagNameLen > 0) {
                // Self-closing tag
                // Will close on next >
            }
            // Ignore attribute content
            break;
            
        case State::ENTITY:
            if (c == ';') {
                _entityBuffer[_entityLen++] = c;
                _entityBuffer[_entityLen] = '\0';
                char decoded = decodeEntity();
                if (_inBody && _skipDepth == 0) {
                    addChar(decoded);
                }
                _state = State::TEXT;
            } else if (_entityLen < HTML_MAX_ENTITY - 1 && (isalnum(c) || c == '#')) {
                _entityBuffer[_entityLen++] = c;
            } else {
                // Invalid entity, output as-is
                _entityBuffer[_entityLen] = '\0';
                if (_inBody && _skipDepth == 0) {
                    addText(_entityBuffer);
                    addChar(c);
                }
                _state = State::TEXT;
            }
            break;
            
        case State::COMMENT:
            // Simple comment handling - look for -->
            // Skip to end of comment
            if (c == '>') {
                _state = State::TEXT;
            }
            break;
            
        case State::CDATA:
            // Skip CDATA content
            if (c == '>') {
                _state = State::TEXT;
            }
            break;
    }
}

// =============================================================================
// Tag Handling
// =============================================================================

void StreamingHtmlParser::handleTagEnd() {
    if (_isClosingTag) {
        handleEndTag(_tagName);
    } else {
        handleStartTag(_tagName);
    }
}

void StreamingHtmlParser::handleStartTag(const char* name) {
    _depth++;
    
    // Check for body tag
    if (strcmp(name, "body") == 0) {
        _inBody = true;
        return;
    }
    
    // Track skip depth without processing content
    if (_skipDepth > 0) {
        return;
    }
    
    // Check for skip tags
    if (tagIsAny(SKIP_TAGS, NUM_SKIP_TAGS)) {
        _skipDepth = _depth;
        return;
    }
    
    // Check for header tags
    for (int i = 0; i < NUM_HEADER_TAGS; i++) {
        if (strcmp(name, HEADER_TAGS[i]) == 0) {
            flushParagraph();
            _inHeader = true;
            _headerLevel = i + 1;
            _inBold = true;  // Headers are bold
            return;
        }
    }
    
    // Check for block tags
    if (tagIsAny(BLOCK_TAGS, NUM_BLOCK_TAGS)) {
        if (strcmp(name, "br") == 0) {
            // <br> forces a new paragraph but keeps settings
            flushParagraph();
        } else {
            flushParagraph();
            _inParagraph = true;
        }
        return;
    }
    
    // Check for bold tags
    if (tagIsAny(BOLD_TAGS, NUM_BOLD_TAGS)) {
        _inBold = true;
        return;
    }
    
    // Check for italic tags
    if (tagIsAny(ITALIC_TAGS, NUM_ITALIC_TAGS)) {
        _inItalic = true;
        return;
    }
}

void StreamingHtmlParser::handleEndTag(const char* name) {
    _depth--;
    
    // Leaving skip mode
    if (_skipDepth > 0 && _depth < _skipDepth) {
        _skipDepth = 0;
        return;
    }
    
    // Check for header end
    for (int i = 0; i < NUM_HEADER_TAGS; i++) {
        if (strcmp(name, HEADER_TAGS[i]) == 0) {
            flushParagraph();
            _inHeader = false;
            _headerLevel = 0;
            _inBold = false;
            return;
        }
    }
    
    // Check for block tag end
    if (tagIsAny(BLOCK_TAGS, NUM_BLOCK_TAGS)) {
        flushParagraph();
        _inParagraph = false;
        return;
    }
    
    // Check for bold end
    if (tagIsAny(BOLD_TAGS, NUM_BOLD_TAGS)) {
        _inBold = false;
        return;
    }
    
    // Check for italic end
    if (tagIsAny(ITALIC_TAGS, NUM_ITALIC_TAGS)) {
        _inItalic = false;
        return;
    }
    
    // Check for body end
    if (strcmp(name, "body") == 0) {
        flushParagraph();
        _inBody = false;
    }
}

// =============================================================================
// Tag Matching Helpers
// =============================================================================

bool StreamingHtmlParser::tagIs(const char* name) const {
    return strcmp(_tagName, name) == 0;
}

bool StreamingHtmlParser::tagIsAny(const char** names, int count) const {
    for (int i = 0; i < count; i++) {
        if (strcmp(_tagName, names[i]) == 0) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Text Handling
// =============================================================================

void StreamingHtmlParser::addChar(char c) {
    // Whitespace normalization
    bool isSpace = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    
    if (isSpace) {
        if (!_lastWasSpace && _currentPara.length() > 0) {
            _currentPara += ' ';
        }
        _lastWasSpace = true;
    } else {
        _currentPara += c;
        _lastWasSpace = false;
        _characterCount++;
    }
}

void StreamingHtmlParser::addText(const char* text) {
    while (*text) {
        addChar(*text++);
    }
}

void StreamingHtmlParser::flushParagraph() {
    // Trim whitespace
    _currentPara.trim();
    
    if (_currentPara.length() > 0) {
        // Skip if too short (optional)
        if (_currentPara.length() >= (unsigned)_minParaLength) {
            if (_onParagraph) {
                _onParagraph(_currentPara, _inHeader);
            }
            _paragraphCount++;
        }
    }
    
    _currentPara = "";
    _lastWasSpace = true;
}

// =============================================================================
// Entity Decoding
// =============================================================================

char StreamingHtmlParser::decodeEntity() {
    // Handle numeric entities
    if (_entityBuffer[1] == '#') {
        int code = 0;
        if (_entityBuffer[2] == 'x' || _entityBuffer[2] == 'X') {
            // Hexadecimal
            code = strtol(_entityBuffer + 3, nullptr, 16);
        } else {
            // Decimal
            code = strtol(_entityBuffer + 2, nullptr, 10);
        }
        
        // Handle ASCII range only
        if (code > 0 && code < 128) {
            return (char)code;
        }
        // For extended characters, return space
        return ' ';
    }
    
    // Handle named entities (common ones)
    if (strcmp(_entityBuffer, "&amp;") == 0) return '&';
    if (strcmp(_entityBuffer, "&lt;") == 0) return '<';
    if (strcmp(_entityBuffer, "&gt;") == 0) return '>';
    if (strcmp(_entityBuffer, "&quot;") == 0) return '"';
    if (strcmp(_entityBuffer, "&apos;") == 0) return '\'';
    if (strcmp(_entityBuffer, "&nbsp;") == 0) return ' ';
    
    // Punctuation entities
    if (strcmp(_entityBuffer, "&ndash;") == 0) return '-';
    if (strcmp(_entityBuffer, "&mdash;") == 0) return '-';
    if (strcmp(_entityBuffer, "&lsquo;") == 0) return '\'';
    if (strcmp(_entityBuffer, "&rsquo;") == 0) return '\'';
    if (strcmp(_entityBuffer, "&ldquo;") == 0) return '"';
    if (strcmp(_entityBuffer, "&rdquo;") == 0) return '"';
    if (strcmp(_entityBuffer, "&hellip;") == 0) return '.';  // Simplified
    if (strcmp(_entityBuffer, "&bull;") == 0) return '*';
    if (strcmp(_entityBuffer, "&copy;") == 0) return 'c';    // Simplified
    if (strcmp(_entityBuffer, "&reg;") == 0) return 'r';     // Simplified
    if (strcmp(_entityBuffer, "&trade;") == 0) return 't';   // Simplified
    
    // Unknown entity - return space
    return ' ';
}

// =============================================================================
// Standalone Entity Decoding Functions
// =============================================================================

String decodeHtmlEntities(const String& text) {
    String result;
    result.reserve(text.length());
    
    int i = 0;
    while (i < (int)text.length()) {
        if (text[i] == '&') {
            // Find the end of entity
            int end = text.indexOf(';', i);
            if (end > i && end - i < HTML_MAX_ENTITY) {
                String entity = text.substring(i, end + 1);
                String decoded = decodeSingleEntity(entity);
                result += decoded;
                i = end + 1;
            } else {
                result += text[i++];
            }
        } else {
            result += text[i++];
        }
    }
    
    return result;
}

String decodeSingleEntity(const String& entity) {
    // Numeric entities
    if (entity.length() > 2 && entity[1] == '#') {
        int code = 0;
        if (entity[2] == 'x' || entity[2] == 'X') {
            code = strtol(entity.c_str() + 3, nullptr, 16);
        } else {
            code = strtol(entity.c_str() + 2, nullptr, 10);
        }
        if (code > 0 && code < 128) {
            return String((char)code);
        }
        return " ";
    }
    
    // Named entities
    if (entity == "&amp;") return "&";
    if (entity == "&lt;") return "<";
    if (entity == "&gt;") return ">";
    if (entity == "&quot;") return "\"";
    if (entity == "&apos;") return "'";
    if (entity == "&nbsp;") return " ";
    if (entity == "&ndash;") return "-";
    if (entity == "&mdash;") return "--";
    if (entity == "&lsquo;" || entity == "&rsquo;") return "'";
    if (entity == "&ldquo;" || entity == "&rdquo;") return "\"";
    if (entity == "&hellip;") return "...";
    if (entity == "&bull;") return "*";
    if (entity == "&copy;") return "(c)";
    if (entity == "&reg;") return "(R)";
    if (entity == "&trade;") return "(TM)";
    
    // Unknown - return original
    return entity;
}
