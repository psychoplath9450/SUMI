/**
 * @file ExpatHtmlParser.cpp
 * @brief Expat-based Streaming HTML/XML Parser Implementation
 * 
 * Uses expat XML library for robust parsing.
 * 
 * Version: 2.1.16
 */

// Include expat first so XMLCALL is properly defined
#include <expat.h>
#include "core/ExpatHtmlParser.h"
#include <SD.h>

// =============================================================================
// Tag Lists
// =============================================================================

static const char* BLOCK_TAGS[] = {"p", "li", "div", "br", "blockquote", "article", "section"};
static const int NUM_BLOCK_TAGS = 7;

static const char* HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
static const int NUM_HEADER_TAGS = 6;

static const char* BOLD_TAGS[] = {"b", "strong"};
static const int NUM_BOLD_TAGS = 2;

static const char* ITALIC_TAGS[] = {"i", "em", "cite"};
static const int NUM_ITALIC_TAGS = 3;

static const char* SKIP_TAGS[] = {"head", "script", "style", "table", "nav", "aside", "footer"};
static const int NUM_SKIP_TAGS = 7;

// =============================================================================
// Helper: Check whitespace
// =============================================================================

static bool isWhitespace(char c) {
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

ExpatHtmlParser::ExpatHtmlParser()
    : _callback(nullptr),
      _depth(0),
      _skipUntilDepth(INT_MAX),
      _boldUntilDepth(INT_MAX),
      _italicUntilDepth(INT_MAX),
      _inBody(false),
      _inHeader(false),
      _lastWasSpace(true),
      _wordBufferIndex(0),
      _paragraphCount(0),
      _characterCount(0) {
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
}

ExpatHtmlParser::~ExpatHtmlParser() {
}

// =============================================================================
// Main Parse Method
// =============================================================================

bool ExpatHtmlParser::parseFile(const String& filepath, ExpatParagraphCallback callback) {
    if (!callback) {
        _error = "No callback provided";
        return false;
    }
    
    // Reset state
    _callback = callback;
    _depth = 0;
    _skipUntilDepth = INT_MAX;
    _boldUntilDepth = INT_MAX;
    _italicUntilDepth = INT_MAX;
    _inBody = false;
    _inHeader = false;
    _currentText = "";
    _lastWasSpace = true;
    _wordBufferIndex = 0;
    _paragraphCount = 0;
    _characterCount = 0;
    _error = "";
    
    // Create expat parser
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser) {
        _error = "Failed to create XML parser";
        Serial.printf("[EXPAT] %s\n", _error.c_str());
        return false;
    }
    
    // Open file
    File file = SD.open(filepath, FILE_READ);
    if (!file) {
        _error = "Failed to open file: " + filepath;
        Serial.printf("[EXPAT] %s\n", _error.c_str());
        XML_ParserFree(parser);
        return false;
    }
    
    Serial.printf("[EXPAT] Parsing: %s (%d bytes)\n", filepath.c_str(), file.size());
    
    // Set up expat callbacks
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetCharacterDataHandler(parser, characterData);
    
    // Parse file in chunks for memory efficiency
    bool success = true;
    bool done = false;
    
    while (!done) {
        // Get buffer from expat
        void* buf = XML_GetBuffer(parser, EXPAT_PARSE_BUFFER_SIZE);
        if (!buf) {
            _error = "Failed to get parse buffer";
            Serial.printf("[EXPAT] %s\n", _error.c_str());
            success = false;
            break;
        }
        
        // Read chunk from file
        size_t len = file.read((uint8_t*)buf, EXPAT_PARSE_BUFFER_SIZE);
        
        if (len == 0 && file.available() > 0) {
            _error = "File read error";
            Serial.printf("[EXPAT] %s\n", _error.c_str());
            success = false;
            break;
        }
        
        done = (file.available() == 0);
        
        // Parse the chunk
        if (XML_ParseBuffer(parser, (int)len, done) == XML_STATUS_ERROR) {
            _error = "Parse error at line ";
            _error += String((int)XML_GetCurrentLineNumber(parser));
            _error += ": ";
            _error += XML_ErrorString(XML_GetErrorCode(parser));
            Serial.printf("[EXPAT] %s\n", _error.c_str());
            success = false;
            break;
        }
    }
    
    // Clean up
    file.close();
    
    // Clear callbacks before freeing parser
    XML_SetElementHandler(parser, nullptr, nullptr);
    XML_SetCharacterDataHandler(parser, nullptr);
    XML_ParserFree(parser);
    
    // Flush any remaining content
    if (success) {
        flushWordBuffer();
        flushParagraph(_inHeader);
    }
    
    Serial.printf("[EXPAT] Done: %d paragraphs, %d chars\n", _paragraphCount, _characterCount);
    
    return success;
}

// =============================================================================
// Expat Callbacks (static)
// =============================================================================

void XMLCALL ExpatHtmlParser::startElement(void* userData, const char* name, const char** atts) {
    ExpatHtmlParser* self = static_cast<ExpatHtmlParser*>(userData);
    self->handleStartTag(name, atts);
}

void XMLCALL ExpatHtmlParser::endElement(void* userData, const char* name) {
    ExpatHtmlParser* self = static_cast<ExpatHtmlParser*>(userData);
    self->handleEndTag(name);
}

void XMLCALL ExpatHtmlParser::characterData(void* userData, const char* s, int len) {
    ExpatHtmlParser* self = static_cast<ExpatHtmlParser*>(userData);
    
    // Skip if in skip mode
    if (self->_skipUntilDepth < self->_depth) {
        return;
    }
    
    // Process each character
    for (int i = 0; i < len; i++) {
        self->handleCharacter(s[i]);
    }
}

// =============================================================================
// Tag Handlers
// =============================================================================

void ExpatHtmlParser::handleStartTag(const char* name, const char** atts) {
    _depth++;
    
    // Convert to lowercase for comparison
    char lowerName[32];
    int i = 0;
    while (name[i] && i < 31) {
        lowerName[i] = tolower(name[i]);
        i++;
    }
    lowerName[i] = '\0';
    
    // Check for body tag
    if (strcmp(lowerName, "body") == 0) {
        _inBody = true;
        return;
    }
    
    // If not in body yet, ignore
    if (!_inBody) return;
    
    // Middle of skip
    if (_skipUntilDepth < _depth) {
        return;
    }
    
    // Check for skip tags
    if (isSkipTag(lowerName)) {
        _skipUntilDepth = _depth;
        return;
    }
    
    // Skip blocks with role="doc-pagebreak"
    if (atts) {
        for (int i = 0; atts[i]; i += 2) {
            if ((strcmp(atts[i], "role") == 0 && strcmp(atts[i + 1], "doc-pagebreak") == 0) ||
                (strcmp(atts[i], "epub:type") == 0 && strcmp(atts[i + 1], "pagebreak") == 0)) {
                _skipUntilDepth = _depth;
                return;
            }
        }
    }
    
    // Check for header tags
    if (isHeaderTag(lowerName)) {
        flushWordBuffer();
        flushParagraph(false);
        _inHeader = true;
        _boldUntilDepth = min(_boldUntilDepth, _depth);
        return;
    }
    
    // Check for block tags
    if (isBlockTag(lowerName)) {
        flushWordBuffer();
        if (strcmp(lowerName, "br") == 0) {
            // br element starts new paragraph
            flushParagraph(_inHeader);
        } else {
            flushParagraph(_inHeader);
        }
        return;
    }
    
    // Check for bold tags
    if (isBoldTag(lowerName)) {
        _boldUntilDepth = min(_boldUntilDepth, _depth);
        return;
    }
    
    // Check for italic tags
    if (isItalicTag(lowerName)) {
        _italicUntilDepth = min(_italicUntilDepth, _depth);
        return;
    }
}

void ExpatHtmlParser::handleEndTag(const char* name) {
    // Convert to lowercase
    char lowerName[32];
    int i = 0;
    while (name[i] && i < 31) {
        lowerName[i] = tolower(name[i]);
        i++;
    }
    lowerName[i] = '\0';
    
    // ALWAYS flush word buffer on ANY closing tag
    if (_wordBufferIndex > 0) {
        flushWordBuffer();
    }
    
    _depth--;
    
    // Leaving skip
    if (_skipUntilDepth == _depth) {
        _skipUntilDepth = INT_MAX;
        return;
    }
    
    // Leaving bold
    if (_boldUntilDepth == _depth) {
        _boldUntilDepth = INT_MAX;
    }
    
    // Leaving italic
    if (_italicUntilDepth == _depth) {
        _italicUntilDepth = INT_MAX;
    }
    
    // Check for header end
    if (isHeaderTag(lowerName)) {
        flushParagraph(true);  // This was a header
        _inHeader = false;
        return;
    }
    
    // Check for block tag end
    if (isBlockTag(lowerName)) {
        flushParagraph(_inHeader);
        return;
    }
    
    // Check for body end
    if (strcmp(lowerName, "body") == 0) {
        flushParagraph(_inHeader);
        _inBody = false;
    }
}

void ExpatHtmlParser::handleCharacter(char c) {
    if (!_inBody || _skipUntilDepth < _depth) {
        return;
    }
    
    if (isWhitespace(c)) {
        // Flush any accumulated word
        if (_wordBufferIndex > 0) {
            flushWordBuffer();
        }
        // Mark that we've seen whitespace (flushWordBuffer will add space if needed)
        _lastWasSpace = true;
    } else {
        // Check if word buffer is full
        if (_wordBufferIndex >= EXPAT_MAX_WORD_SIZE) {
            flushWordBuffer();
        }
        _wordBuffer[_wordBufferIndex++] = c;
        _lastWasSpace = false;
        _characterCount++;
    }
}

// =============================================================================
// Buffer Management
// =============================================================================

void ExpatHtmlParser::flushWordBuffer() {
    if (_wordBufferIndex > 0) {
        _wordBuffer[_wordBufferIndex] = '\0';
        
        // ALWAYS ensure space before word if text exists
        if (_currentText.length() > 0) {
            char lastChar = _currentText.charAt(_currentText.length() - 1);
            if (lastChar != ' ' && lastChar != '\n' && lastChar != '\t' && 
                lastChar != '*') {  // Don't add space after opening marker
                _currentText += ' ';
            }
        }
        
        // Decode entities in word
        String word = replaceHtmlEntities(_wordBuffer);
        
        // Check if we're in bold or italic - wrap word with markers
        bool inBold = (_boldUntilDepth < INT_MAX);
        bool inItalic = (_italicUntilDepth < INT_MAX);
        
        if (inBold && inItalic) {
            _currentText += "***";
            _currentText += word;
            _currentText += "*** ";
        } else if (inBold) {
            _currentText += "**";
            _currentText += word;
            _currentText += "** ";
        } else if (inItalic) {
            _currentText += "*";
            _currentText += word;
            _currentText += "* ";
        } else {
            _currentText += word;
        }
        
        _wordBufferIndex = 0;
        _lastWasSpace = false;
    }
}

void ExpatHtmlParser::flushParagraph(bool isHeader) {
    _currentText.trim();
    
    if (_currentText.length() > 0) {
        // Add header marker if this is a header
        String output;
        if (isHeader) {
            output = "# ";
            output += _currentText;
        } else {
            output = _currentText;
        }
        
        if (_callback) {
            _callback(output, isHeader);
        }
        _paragraphCount++;
    }
    
    _currentText = "";
    _lastWasSpace = true;
}

// =============================================================================
// Tag Matching
// =============================================================================

bool ExpatHtmlParser::isBlockTag(const char* name) {
    for (int i = 0; i < NUM_BLOCK_TAGS; i++) {
        if (strcmp(name, BLOCK_TAGS[i]) == 0) return true;
    }
    return false;
}

bool ExpatHtmlParser::isHeaderTag(const char* name) {
    for (int i = 0; i < NUM_HEADER_TAGS; i++) {
        if (strcmp(name, HEADER_TAGS[i]) == 0) return true;
    }
    return false;
}

bool ExpatHtmlParser::isBoldTag(const char* name) {
    for (int i = 0; i < NUM_BOLD_TAGS; i++) {
        if (strcmp(name, BOLD_TAGS[i]) == 0) return true;
    }
    return false;
}

bool ExpatHtmlParser::isItalicTag(const char* name) {
    for (int i = 0; i < NUM_ITALIC_TAGS; i++) {
        if (strcmp(name, ITALIC_TAGS[i]) == 0) return true;
    }
    return false;
}

bool ExpatHtmlParser::isSkipTag(const char* name) {
    for (int i = 0; i < NUM_SKIP_TAGS; i++) {
        if (strcmp(name, SKIP_TAGS[i]) == 0) return true;
    }
    return false;
}

// =============================================================================
// Entity Decoding
// =============================================================================

String replaceHtmlEntities(const char* text) {
    String result;
    result.reserve(strlen(text));
    
    int i = 0;
    while (text[i]) {
        if (text[i] == '&') {
            // Find end of entity
            int end = i + 1;
            while (text[end] && text[end] != ';' && (end - i) < 12) {
                end++;
            }
            
            if (text[end] == ';') {
                // Extract entity
                char entity[16];
                int len = end - i + 1;
                if (len < 16) {
                    strncpy(entity, text + i, len);
                    entity[len] = '\0';
                    
                    // Decode
                    if (strcmp(entity, "&amp;") == 0) result += '&';
                    else if (strcmp(entity, "&lt;") == 0) result += '<';
                    else if (strcmp(entity, "&gt;") == 0) result += '>';
                    else if (strcmp(entity, "&quot;") == 0) result += '"';
                    else if (strcmp(entity, "&apos;") == 0) result += '\'';
                    else if (strcmp(entity, "&nbsp;") == 0) result += ' ';
                    else if (strcmp(entity, "&ndash;") == 0) result += '-';
                    else if (strcmp(entity, "&mdash;") == 0) result += '-';
                    else if (strcmp(entity, "&lsquo;") == 0) result += '\'';
                    else if (strcmp(entity, "&rsquo;") == 0) result += '\'';
                    else if (strcmp(entity, "&ldquo;") == 0) result += '"';
                    else if (strcmp(entity, "&rdquo;") == 0) result += '"';
                    else if (strcmp(entity, "&hellip;") == 0) result += "...";
                    else if (strcmp(entity, "&bull;") == 0) result += '*';
                    else if (strncmp(entity, "&#", 2) == 0) {
                        // Numeric entity
                        int code = 0;
                        if (entity[2] == 'x' || entity[2] == 'X') {
                            code = strtol(entity + 3, nullptr, 16);
                        } else {
                            code = strtol(entity + 2, nullptr, 10);
                        }
                        if (code > 0 && code < 128) {
                            result += (char)code;
                        } else {
                            result += ' ';
                        }
                    } else {
                        // Unknown entity - keep as-is or use space
                        result += ' ';
                    }
                    
                    i = end + 1;
                    continue;
                }
            }
        }
        
        result += text[i];
        i++;
    }
    
    return result;
}
