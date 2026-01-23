/**
 * @file StreamingHtmlParser.cpp
 * @brief Streaming HTML Parser Implementation
 * @version 2.4.0
 *
 * Features:
 * - Per-word font style tracking using depth-based state machine
 * - Handles nested <b><i>text</i></b> correctly
 * - HTML entity decoding
 * - List bullet points
 * - Scene breaks (<hr>)
 *
 * Based on professional e-reader parsing architecture.
 */

#include "core/StreamingHtmlParser.h"

// =============================================================================
// Tag Lists
// =============================================================================

static const char* HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
static const int NUM_HEADER_TAGS = 6;

static const char* BLOCK_TAGS[] = {"p", "li", "div", "br", "blockquote", "article", "section", "hr"};
static const int NUM_BLOCK_TAGS = 8;

static const char* BOLD_TAGS[] = {"b", "strong"};
static const int NUM_BOLD_TAGS = 2;

static const char* ITALIC_TAGS[] = {"i", "em", "cite"};
static const int NUM_ITALIC_TAGS = 3;

static const char* SKIP_TAGS[] = {"head", "script", "style", "table", "nav", "aside", "footer"};
static const int NUM_SKIP_TAGS = 7;

// UTF-8 bullet character (•)
static const char BULLET_UTF8[] = "\xe2\x80\xa2";

// Check if character is whitespace
static bool isWhitespace(char c) {
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

StreamingHtmlParser::StreamingHtmlParser() 
    : _onParagraph(nullptr),
      _onStyledParagraph(nullptr),
      _useStyledCallback(false),
      _preserveFormatting(true),
      _minParaLength(0),
      _extraParagraphSpacing(true),
      _defaultAlignment(TextAlign::JUSTIFY),
      _state(State::TEXT),
      _tagNameLen(0),
      _isClosingTag(false),
      _inBody(false),
      _inParagraph(false),
      _inHeader(false),
      _headerLevel(0),
      _boldUntilDepth(INT_MAX),
      _italicUntilDepth(INT_MAX),
      _skipUntilDepth(INT_MAX),
      _depth(0),
      _inBold(false),
      _inItalic(false),
      _skipDepth(0),
      _entityLen(0),
      _wordBufferLen(0),
      _currentBlock(nullptr),
      _lastWasSpace(true),
      _paragraphCount(0),
      _characterCount(0),
      _bytesProcessed(0) {
    memset(_tagName, 0, sizeof(_tagName));
    memset(_entityBuffer, 0, sizeof(_entityBuffer));
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
}

StreamingHtmlParser::~StreamingHtmlParser() {
    if (_currentBlock) {
        delete _currentBlock;
        _currentBlock = nullptr;
    }
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
    _useStyledCallback = false;
    
    char buffer[HTML_PARSE_BUFFER_SIZE];
    
    while (file.available()) {
        size_t bytesRead = file.readBytes(buffer, sizeof(buffer));
        if (bytesRead == 0) break;
        
        processChunk(buffer, bytesRead);
        _bytesProcessed += bytesRead;
    }
    
    flushParagraph();
    
    Serial.printf("[HTML] Parsed: %d paragraphs, %d chars, %d bytes\n",
                  _paragraphCount, _characterCount, _bytesProcessed);
    
    return true;
}

bool StreamingHtmlParser::parseStyled(File& file, StyledParagraphCallback onStyledParagraph) {
    if (!file || !onStyledParagraph) {
        return false;
    }
    
    reset();
    _onStyledParagraph = onStyledParagraph;
    _useStyledCallback = true;
    
    // Create initial text block
    _currentBlock = new TextBlock(_defaultAlignment, _extraParagraphSpacing, false);
    
    char buffer[HTML_PARSE_BUFFER_SIZE];
    
    while (file.available()) {
        size_t bytesRead = file.readBytes(buffer, sizeof(buffer));
        if (bytesRead == 0) break;
        
        processChunk(buffer, bytesRead);
        _bytesProcessed += bytesRead;
    }
    
    flushStyledParagraph();
    
    if (_currentBlock) {
        delete _currentBlock;
        _currentBlock = nullptr;
    }
    
    Serial.printf("[HTML] Styled parse: %d paragraphs, %d chars, %d bytes\n",
                  _paragraphCount, _characterCount, _bytesProcessed);
    
    return true;
}

bool StreamingHtmlParser::parseString(const String& html, ParagraphCallback onParagraph) {
    if (html.length() == 0 || !onParagraph) {
        return false;
    }
    
    reset();
    _onParagraph = onParagraph;
    _useStyledCallback = false;
    
    processChunk(html.c_str(), html.length());
    _bytesProcessed = html.length();
    
    flushParagraph();
    
    return true;
}

bool StreamingHtmlParser::parseStringStyled(const String& html, StyledParagraphCallback onStyledParagraph) {
    if (html.length() == 0 || !onStyledParagraph) {
        return false;
    }
    
    reset();
    _onStyledParagraph = onStyledParagraph;
    _useStyledCallback = true;
    
    _currentBlock = new TextBlock(_defaultAlignment, _extraParagraphSpacing, false);
    
    processChunk(html.c_str(), html.length());
    _bytesProcessed = html.length();
    
    flushStyledParagraph();
    
    if (_currentBlock) {
        delete _currentBlock;
        _currentBlock = nullptr;
    }
    
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
    _boldUntilDepth = INT_MAX;
    _italicUntilDepth = INT_MAX;
    _skipUntilDepth = INT_MAX;
    _depth = 0;
    _inBold = false;
    _inItalic = false;
    _skipDepth = 0;
    _entityLen = 0;
    _wordBufferLen = 0;
    _currentPara = "";
    _lastWasSpace = true;
    _paragraphCount = 0;
    _characterCount = 0;
    _bytesProcessed = 0;
    
    if (_currentBlock) {
        delete _currentBlock;
        _currentBlock = nullptr;
    }
}

// =============================================================================
// Font Style Determination
// =============================================================================

FontStyle StreamingHtmlParser::getCurrentStyle() const {
    bool isBold = (_boldUntilDepth < _depth);
    bool isItalic = (_italicUntilDepth < _depth);
    
    if (isBold && isItalic) {
        return FontStyle::BOLD_ITALIC;
    } else if (isBold) {
        return FontStyle::BOLD;
    } else if (isItalic) {
        return FontStyle::ITALIC;
    }
    return FontStyle::REGULAR;
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
                if (_inBody && _skipUntilDepth == INT_MAX) {
                    if (_useStyledCallback) {
                        addCharToWord(c);
                    } else {
                        addChar(c);
                    }
                }
            }
            break;
            
        case State::TAG_START:
            if (c == '/') {
                _isClosingTag = true;
                _state = State::TAG_NAME;
            } else if (c == '!') {
                _state = State::COMMENT;
            } else if (c == '?') {
                _state = State::TAG_ATTRS;
            } else if (isalpha(c)) {
                _tagName[0] = tolower(c);
                _tagNameLen = 1;
                _state = State::TAG_NAME;
            } else {
                _state = State::TEXT;
                if (_inBody && _skipUntilDepth == INT_MAX) {
                    if (_useStyledCallback) {
                        addCharToWord('<');
                        addCharToWord(c);
                    } else {
                        addChar('<');
                        addChar(c);
                    }
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
                _tagName[_tagNameLen] = '\0';
                _state = State::TAG_ATTRS;
            } else {
                _tagName[_tagNameLen] = '\0';
                _state = State::TAG_ATTRS;
            }
            break;
            
        case State::TAG_ATTRS:
            if (c == '>') {
                handleTagEnd();
                _state = State::TEXT;
            }
            break;
            
        case State::ENTITY:
            if (c == ';') {
                _entityBuffer[_entityLen++] = c;
                _entityBuffer[_entityLen] = '\0';
                char decoded = decodeEntity();
                if (_inBody && _skipUntilDepth == INT_MAX) {
                    if (_useStyledCallback) {
                        addCharToWord(decoded);
                    } else {
                        addChar(decoded);
                    }
                }
                _state = State::TEXT;
            } else if (_entityLen < HTML_MAX_ENTITY - 1 && (isalnum(c) || c == '#')) {
                _entityBuffer[_entityLen++] = c;
            } else {
                _entityBuffer[_entityLen] = '\0';
                if (_inBody && _skipUntilDepth == INT_MAX) {
                    if (_useStyledCallback) {
                        for (int i = 0; i < _entityLen; i++) {
                            addCharToWord(_entityBuffer[i]);
                        }
                        addCharToWord(c);
                    } else {
                        addText(_entityBuffer);
                        addChar(c);
                    }
                }
                _state = State::TEXT;
            }
            break;
            
        case State::COMMENT:
            if (c == '>') {
                _state = State::TEXT;
            }
            break;
            
        case State::CDATA:
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
    
    // Inside skip mode?
    if (_skipUntilDepth < _depth) {
        return;
    }
    
    // Table handling - show placeholder
    if (strcmp(name, "table") == 0) {
        if (_useStyledCallback) {
            flushWord();
            startNewBlock(TextAlign::CENTER);
            _currentBlock->addWord("[Table omitted]", FontStyle::ITALIC);
        } else {
            flushParagraph();
            _currentPara = "[Table omitted]";
            flushParagraph();
        }
        _skipUntilDepth = _depth - 1;
        return;
    }
    
    // Skip tags
    if (tagIsAny(SKIP_TAGS, NUM_SKIP_TAGS)) {
        _skipUntilDepth = _depth - 1;
        return;
    }
    
    // Body tag
    if (strcmp(name, "body") == 0) {
        _inBody = true;
        return;
    }
    
    // Header tags
    for (int i = 0; i < NUM_HEADER_TAGS; i++) {
        if (strcmp(name, HEADER_TAGS[i]) == 0) {
            if (_useStyledCallback) {
                flushWord();
                flushStyledParagraph();
                startNewBlock(TextAlign::CENTER);
            } else {
                flushParagraph();
            }
            _inHeader = true;
            _headerLevel = i + 1;
            _boldUntilDepth = _depth - 1;  // Headers are bold
            return;
        }
    }
    
    // Block tags
    if (tagIsAny(BLOCK_TAGS, NUM_BLOCK_TAGS)) {
        if (strcmp(name, "br") == 0) {
            // <br> forces new paragraph but keeps settings
            if (_useStyledCallback) {
                flushWord();
                flushStyledParagraph();
                TextAlign align = _currentBlock ? _currentBlock->alignment : _defaultAlignment;
                startNewBlock(align);
            } else {
                flushParagraph();
            }
        } else if (strcmp(name, "hr") == 0) {
            // <hr> is scene break
            if (_useStyledCallback) {
                flushWord();
                flushStyledParagraph();
                if (_onStyledParagraph) {
                    TextBlock sceneBreak(TextAlign::CENTER, _extraParagraphSpacing, false);
                    sceneBreak.addWord("~~~SCENE_BREAK~~~", FontStyle::REGULAR);
                    _onStyledParagraph(sceneBreak, false);
                }
                startNewBlock(_defaultAlignment);
            } else {
                flushParagraph();
                if (_onParagraph) {
                    _onParagraph("~~~SCENE_BREAK~~~", false);
                }
            }
            _paragraphCount++;
        } else {
            if (_useStyledCallback) {
                flushWord();
                flushStyledParagraph();
                startNewBlock(_defaultAlignment);
                
                // List items get bullet point
                if (strcmp(name, "li") == 0) {
                    _currentBlock->addWord(BULLET_UTF8, FontStyle::REGULAR);
                }
            } else {
                flushParagraph();
                _inParagraph = true;
            }
        }
        return;
    }
    
    // Bold tags
    if (tagIsAny(BOLD_TAGS, NUM_BOLD_TAGS)) {
        if (_useStyledCallback) {
            flushWord();  // End current word before style change
        }
        _boldUntilDepth = min(_boldUntilDepth, _depth - 1);
        _inBold = true;
        return;
    }
    
    // Italic tags
    if (tagIsAny(ITALIC_TAGS, NUM_ITALIC_TAGS)) {
        if (_useStyledCallback) {
            flushWord();  // End current word before style change
        }
        _italicUntilDepth = min(_italicUntilDepth, _depth - 1);
        _inItalic = true;
        return;
    }
}

void StreamingHtmlParser::handleEndTag(const char* name) {
    // Flush word on style tag close
    if (tagIsAny(BOLD_TAGS, NUM_BOLD_TAGS) || tagIsAny(ITALIC_TAGS, NUM_ITALIC_TAGS)) {
        if (_useStyledCallback) {
            flushWord();
        }
    }
    
    _depth--;
    
    // Leaving skip mode
    if (_skipUntilDepth == _depth) {
        _skipUntilDepth = INT_MAX;
        return;
    }
    
    // Leaving bold
    if (_boldUntilDepth == _depth) {
        _boldUntilDepth = INT_MAX;
        _inBold = false;
    }
    
    // Leaving italic
    if (_italicUntilDepth == _depth) {
        _italicUntilDepth = INT_MAX;
        _inItalic = false;
    }
    
    // Header end
    for (int i = 0; i < NUM_HEADER_TAGS; i++) {
        if (strcmp(name, HEADER_TAGS[i]) == 0) {
            if (_useStyledCallback) {
                flushWord();
                flushStyledParagraph();
                startNewBlock(_defaultAlignment);
            } else {
                flushParagraph();
            }
            _inHeader = false;
            _headerLevel = 0;
            return;
        }
    }
    
    // Block tag end
    if (tagIsAny(BLOCK_TAGS, NUM_BLOCK_TAGS)) {
        if (_useStyledCallback) {
            flushWord();
            flushStyledParagraph();
            startNewBlock(_defaultAlignment);
        } else {
            flushParagraph();
            _inParagraph = false;
        }
        return;
    }
    
    // Body end
    if (strcmp(name, "body") == 0) {
        if (_useStyledCallback) {
            flushWord();
            flushStyledParagraph();
        } else {
            flushParagraph();
        }
        _inBody = false;
    }
}

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
// Word Handling (Styled Mode)
// =============================================================================

void StreamingHtmlParser::addCharToWord(char c) {
    bool isSpace = isWhitespace(c);
    
    if (isSpace) {
        // Whitespace ends current word
        if (_wordBufferLen > 0) {
            flushWord();
        }
        _lastWasSpace = true;
    } else {
        // Skip BOM (U+FEFF)
        if ((unsigned char)c == 0xEF && _wordBufferLen == 0) {
            // Could be start of BOM - would need 2 more bytes
            // For simplicity, just add it
        }
        
        // Add to word buffer
        if (_wordBufferLen < HTML_MAX_WORD - 1) {
            _wordBuffer[_wordBufferLen++] = c;
        }
        _lastWasSpace = false;
        _characterCount++;
    }
}

void StreamingHtmlParser::flushWord() {
    if (_wordBufferLen > 0 && _currentBlock) {
        _wordBuffer[_wordBufferLen] = '\0';
        FontStyle style = getCurrentStyle();
        _currentBlock->addWord(String(_wordBuffer), style);
        _wordBufferLen = 0;
    }
}

// =============================================================================
// Legacy Text Handling
// =============================================================================

void StreamingHtmlParser::addChar(char c) {
    bool isSpace = isWhitespace(c);
    
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

// =============================================================================
// Paragraph Handling
// =============================================================================

void StreamingHtmlParser::startNewBlock(TextAlign alignment) {
    if (_currentBlock) {
        // Reuse if empty
        if (_currentBlock->isEmpty()) {
            _currentBlock->setAlignment(alignment);
            return;
        }
        delete _currentBlock;
    }
    _currentBlock = new TextBlock(alignment, _extraParagraphSpacing, false);
}

void StreamingHtmlParser::flushParagraph() {
    _currentPara.trim();
    
    if (_currentPara.length() > 0) {
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

void StreamingHtmlParser::flushStyledParagraph() {
    if (!_currentBlock || _currentBlock->isEmpty()) {
        return;
    }
    
    if (_onStyledParagraph) {
        _onStyledParagraph(*_currentBlock, _inHeader);
        _paragraphCount++;
    }
    
    // Clear the block for reuse
    _currentBlock->clear();
}

// =============================================================================
// Entity Decoding
// =============================================================================

char StreamingHtmlParser::decodeEntity() {
    // Numeric entities
    if (_entityBuffer[1] == '#') {
        int code = 0;
        if (_entityBuffer[2] == 'x' || _entityBuffer[2] == 'X') {
            code = strtol(_entityBuffer + 3, nullptr, 16);
        } else {
            code = strtol(_entityBuffer + 2, nullptr, 10);
        }
        
        if (code > 0 && code < 128) {
            return (char)code;
        }
        return ' ';
    }
    
    // Named entities
    if (strcmp(_entityBuffer, "&amp;") == 0) return '&';
    if (strcmp(_entityBuffer, "&lt;") == 0) return '<';
    if (strcmp(_entityBuffer, "&gt;") == 0) return '>';
    if (strcmp(_entityBuffer, "&quot;") == 0) return '"';
    if (strcmp(_entityBuffer, "&apos;") == 0) return '\'';
    if (strcmp(_entityBuffer, "&nbsp;") == 0) return ' ';
    if (strcmp(_entityBuffer, "&ndash;") == 0) return '-';
    if (strcmp(_entityBuffer, "&mdash;") == 0) return '-';
    if (strcmp(_entityBuffer, "&lsquo;") == 0) return '\'';
    if (strcmp(_entityBuffer, "&rsquo;") == 0) return '\'';
    if (strcmp(_entityBuffer, "&ldquo;") == 0) return '"';
    if (strcmp(_entityBuffer, "&rdquo;") == 0) return '"';
    if (strcmp(_entityBuffer, "&hellip;") == 0) return '.';
    if (strcmp(_entityBuffer, "&bull;") == 0) return '*';
    if (strcmp(_entityBuffer, "&copy;") == 0) return 'c';
    if (strcmp(_entityBuffer, "&reg;") == 0) return 'r';
    if (strcmp(_entityBuffer, "&trade;") == 0) return 't';
    
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
    
    return entity;
}
