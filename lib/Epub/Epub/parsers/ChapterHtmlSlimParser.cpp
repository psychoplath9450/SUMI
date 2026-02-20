#include "ChapterHtmlSlimParser.h"

#include <algorithm>
#include <Bitmap.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <ImageConverter.h>
#include <SDCardManager.h>
#include <esp_heap_caps.h>
#include <expat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../Page.h"
#include "../htmlEntities.h"

const char* HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
constexpr int NUM_HEADER_TAGS = sizeof(HEADER_TAGS) / sizeof(HEADER_TAGS[0]);

// Minimum file size (in bytes) to show progress bar - smaller chapters don't benefit from it
constexpr size_t MIN_SIZE_FOR_PROGRESS = 50 * 1024;  // 50KB

const char* BLOCK_TAGS[] = {"p", "li", "div", "br", "blockquote", "question", "answer", "quotation",
                            "figure", "figcaption", "section", "article", "aside", "header", "footer", "details",
                            "summary", "main"};
constexpr int NUM_BLOCK_TAGS = sizeof(BLOCK_TAGS) / sizeof(BLOCK_TAGS[0]);

const char* BOLD_TAGS[] = {"b", "strong"};
constexpr int NUM_BOLD_TAGS = sizeof(BOLD_TAGS) / sizeof(BOLD_TAGS[0]);

const char* ITALIC_TAGS[] = {"i", "em"};
constexpr int NUM_ITALIC_TAGS = sizeof(ITALIC_TAGS) / sizeof(ITALIC_TAGS[0]);

const char* UNDERLINE_TAGS[] = {"u", "ins"};
constexpr int NUM_UNDERLINE_TAGS = sizeof(UNDERLINE_TAGS) / sizeof(UNDERLINE_TAGS[0]);

const char* STRIKETHROUGH_TAGS[] = {"s", "strike", "del"};
constexpr int NUM_STRIKETHROUGH_TAGS = sizeof(STRIKETHROUGH_TAGS) / sizeof(STRIKETHROUGH_TAGS[0]);

const char* IMAGE_TAGS[] = {"img"};
constexpr int NUM_IMAGE_TAGS = sizeof(IMAGE_TAGS) / sizeof(IMAGE_TAGS[0]);

const char* SKIP_TAGS[] = {"head"};
constexpr int NUM_SKIP_TAGS = sizeof(SKIP_TAGS) / sizeof(SKIP_TAGS[0]);

bool isWhitespace(const char c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t'; }

// given the start and end of a tag, check to see if it matches a known tag
bool matches(const char* tag_name, const char* possible_tags[], const int possible_tag_count) {
  for (int i = 0; i < possible_tag_count; i++) {
    if (strcmp(tag_name, possible_tags[i]) == 0) {
      return true;
    }
  }
  return false;
}

void ChapterHtmlSlimParser::flushPartWordBuffer() {
  if (!currentTextBlock || partWordBufferIndex == 0) {
    partWordBufferIndex = 0;
    return;
  }

  // Determine font style from HTML tags and CSS
  const bool isBold = boldUntilDepth < depth || cssBoldUntilDepth < depth;
  const bool isItalic = italicUntilDepth < depth || cssItalicUntilDepth < depth;

  EpdFontFamily::Style fontStyle = EpdFontFamily::REGULAR;
  if (isBold && isItalic) {
    fontStyle = EpdFontFamily::BOLD_ITALIC;
  } else if (isBold) {
    fontStyle = EpdFontFamily::BOLD;
  } else if (isItalic) {
    fontStyle = EpdFontFamily::ITALIC;
  }

  // Determine text decorations from HTML tags and CSS
  uint8_t decorations = TextBlock::DECO_NONE;
  if (underlineUntilDepth < depth || cssUnderlineUntilDepth < depth) {
    decorations |= TextBlock::DECO_UNDERLINE;
  }
  if (strikethroughUntilDepth < depth || cssStrikethroughUntilDepth < depth) {
    decorations |= TextBlock::DECO_STRIKETHROUGH;
  }

  partWordBuffer[partWordBufferIndex] = '\0';
  currentTextBlock->addWord(partWordBuffer, fontStyle, decorations);
  partWordBufferIndex = 0;
}

// start a new text block if needed
void ChapterHtmlSlimParser::startNewTextBlock(const TextBlock::BLOCK_STYLE style) {
  if (currentTextBlock) {
    // already have a text block running and it is empty - just reuse it
    if (currentTextBlock->isEmpty()) {
      currentTextBlock->setStyle(style);
      return;
    }

    makePages();
    pendingEmergencySplit_ = false;
  }
  currentTextBlock.reset(new ParsedText(style, config.indentLevel, config.hyphenation, false, pendingRtl_));
}

void XMLCALL ChapterHtmlSlimParser::startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);
  (void)atts;

  // Prevent stack overflow from deeply nested XML
  if (self->depth >= MAX_XML_DEPTH) {
    XML_StopParser(self->xmlParser_, XML_FALSE);
    return;
  }

  // Middle of skip
  if (self->skipUntilDepth < self->depth) {
    self->depth += 1;
    return;
  }

  // Track when we enter <body> - ignore all text content before it
  if (strcasecmp(name, "body") == 0) {
    self->insideBody_ = true;
  }

  if (matches(name, IMAGE_TAGS, NUM_IMAGE_TAGS)) {
    // Skip all images when memory is critically low (avoid OOM crash during conversion)
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 40000) {
      Serial.printf("[%lu] [EHP] Skipping image - low memory (%zu bytes)\n", millis(), freeHeap);
      self->depth += 1;
      return;
    }

    std::string srcAttr;
    std::string altText;
    if (atts != nullptr) {
      for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "src") == 0 && atts[i + 1][0] != '\0') {
          srcAttr = atts[i + 1];
        } else if (strcmp(atts[i], "alt") == 0 && atts[i + 1][0] != '\0') {
          altText = atts[i + 1];
        }
      }
    }

    Serial.printf("[%lu] [EHP] Found image: src=%s\n", millis(), srcAttr.empty() ? "(empty)" : srcAttr.c_str());

    // Silently skip unsupported image formats (GIF, SVG, WebP, etc.)
    if (!srcAttr.empty() && !ImageConverterFactory::isSupported(srcAttr)) {
      Serial.printf("[%lu] [EHP] Skipping unsupported format: %s\n", millis(), srcAttr.c_str());
      self->depth += 1;
      return;
    }

    // Try to cache and display the image if we have image support configured
    if (!srcAttr.empty() && self->readItemFn && !self->imageCachePath.empty()) {
      // Check abort before and after image caching (conversion can take 10+ seconds for large JPEGs)
      if (self->externalAbortCallback_ && self->externalAbortCallback_()) {
        self->depth += 1;
        return;
      }
      std::string cachedPath = self->cacheImage(srcAttr);
      if (self->externalAbortCallback_ && self->externalAbortCallback_()) {
        self->depth += 1;
        return;
      }
      if (!cachedPath.empty()) {
        // Read image dimensions from cached BMP
        FsFile bmpFile;
        if (SdMan.openFileForRead("EHP", cachedPath, bmpFile)) {
          Bitmap bitmap(bmpFile, false);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            // Skip tiny decorative images (e.g. 1px-tall line separators) - invisible on e-paper
            if (bitmap.getWidth() < 20 || bitmap.getHeight() < 20) {
              bmpFile.close();
              self->depth += 1;
              return;
            }
            Serial.printf("[%lu] [EHP] Image loaded: %dx%d\n", millis(), bitmap.getWidth(), bitmap.getHeight());
            auto imageBlock = std::make_shared<ImageBlock>(cachedPath, bitmap.getWidth(), bitmap.getHeight());
            bmpFile.close();

            // Flush any pending text block before adding image
            if (self->currentTextBlock && !self->currentTextBlock->isEmpty()) {
              self->makePages();
            }

            self->addImageToPage(imageBlock);
            self->depth += 1;
            return;
          } else {
            Serial.printf("[%lu] [EHP] BMP parse failed for cached image\n", millis());
          }
          bmpFile.close();
        } else {
          Serial.printf("[%lu] [EHP] Failed to open cached BMP: %s\n", millis(), cachedPath.c_str());
        }
      }
    } else {
      Serial.printf("[%lu] [EHP] Image skipped: src=%d, readItemFn=%d, imageCachePath=%d\n", millis(), !srcAttr.empty(),
                    self->readItemFn != nullptr, !self->imageCachePath.empty());
    }

    // Fallback: show placeholder with alt text if image processing failed
    self->startNewTextBlock(TextBlock::CENTER_ALIGN);
    if (self->currentTextBlock) {
      if (!altText.empty()) {
        std::string placeholder = "[Image: " + altText + "]";
        self->currentTextBlock->addWord(placeholder.c_str(), EpdFontFamily::ITALIC);
      } else {
        self->currentTextBlock->addWord("[Image]", EpdFontFamily::ITALIC);
      }
    }

    self->depth += 1;
    return;
  }

  // Table handling — collect cell data for rendering on </table>
  if (strcmp(name, "table") == 0) {
    if (self->inTable_) {
      // Nested table — skip contents
      self->nestedTableDepth_++;
    } else {
      // Flush any pending text before table
      if (self->currentTextBlock && !self->currentTextBlock->isEmpty()) {
        self->makePages();
      }
      self->inTable_ = true;
      self->inTableCell_ = false;
      self->nestedTableDepth_ = 0;
      self->tableRows_.clear();
    }
    self->depth += 1;
    return;
  }

  // Inside table — collect structure instead of rendering
  if (self->inTable_) {
    if (self->nestedTableDepth_ > 0) {
      // Inside nested table — skip everything
      self->depth += 1;
      return;
    }
    if (strcmp(name, "tr") == 0) {
      self->tableRows_.push_back({});
    } else if (strcmp(name, "td") == 0 || strcmp(name, "th") == 0) {
      self->inTableCell_ = true;
      ChapterHtmlSlimParser::TableCell cell;
      cell.isHeader = (strcmp(name, "th") == 0);
      if (!self->tableRows_.empty()) {
        self->tableRows_.back().push_back(cell);
      }
    } else if (strcmp(name, "caption") == 0) {
      self->inTableCaption_ = true;
    }
    // thead, tbody, tfoot — just track depth
    self->depth += 1;
    return;
  }

  if (matches(name, SKIP_TAGS, NUM_SKIP_TAGS)) {
    // start skip
    self->skipUntilDepth = self->depth;
    self->depth += 1;
    return;
  }

  // Skip blocks with role="doc-pagebreak" and epub:type="pagebreak"
  if (atts != nullptr) {
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "role") == 0 && strcmp(atts[i + 1], "doc-pagebreak") == 0 ||
          strcmp(atts[i], "epub:type") == 0 && strcmp(atts[i + 1], "pagebreak") == 0) {
        self->skipUntilDepth = self->depth;
        self->depth += 1;
        return;
      }
    }
  }

  // Skip empty anchor tags with aria-hidden (Pandoc line number anchors)
  // These appear as: <a href="#cb1-1" aria-hidden="true" tabindex="-1"></a>
  if (strcmp(name, "a") == 0 && atts != nullptr) {
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "aria-hidden") == 0 && strcmp(atts[i + 1], "true") == 0) {
        self->skipUntilDepth = self->depth;
        self->depth += 1;
        return;
      }
    }
  }

  // Extract class, style, dir, and id attributes
  std::string classAttr;
  std::string styleAttr;
  std::string dirAttr;
  std::string idAttr;
  if (atts != nullptr) {
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "class") == 0) {
        classAttr = atts[i + 1];
      } else if (strcmp(atts[i], "style") == 0) {
        styleAttr = atts[i + 1];
      } else if (strcmp(atts[i], "dir") == 0) {
        dirAttr = atts[i + 1];
      } else if (strcmp(atts[i], "id") == 0 && atts[i + 1][0] != '\0') {
        idAttr = atts[i + 1];
      }
    }
  }

  // Query CSS for combined style (tag + classes + inline)
  CssStyle cssStyle;
  if (self->cssParser_) {
    if (++self->elementCounter_ % CSS_HEAP_CHECK_INTERVAL == 0) {
      self->cssHeapOk_ = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= MIN_FREE_HEAP;
      if (!self->cssHeapOk_) {
        Serial.printf("[%lu] [EHP] Low memory, skipping CSS lookups\n", millis());
      }
    }
    if (self->cssHeapOk_) {
      cssStyle = self->cssParser_->getCombinedStyle(name, classAttr);
    }
  }
  // Inline styles override stylesheet rules (static method, no instance needed)
  if (!styleAttr.empty()) {
    cssStyle.merge(CssParser::parseInlineStyle(styleAttr));
  }
  // HTML dir attribute overrides CSS direction (case-insensitive per HTML spec)
  if (!dirAttr.empty() && strcasecmp(dirAttr.c_str(), "rtl") == 0) {
    cssStyle.direction = TextDirection::Rtl;
    cssStyle.hasDirection = true;
  } else if (!dirAttr.empty() && strcasecmp(dirAttr.c_str(), "ltr") == 0) {
    cssStyle.direction = TextDirection::Ltr;
    cssStyle.hasDirection = true;
  }

  // Apply CSS font-weight and font-style
  if (cssStyle.hasFontWeight && cssStyle.fontWeight == CssFontWeight::Bold) {
    self->cssBoldUntilDepth = min(self->cssBoldUntilDepth, self->depth);
  }
  if (cssStyle.hasFontStyle && cssStyle.fontStyle == CssFontStyle::Italic) {
    self->cssItalicUntilDepth = min(self->cssItalicUntilDepth, self->depth);
  }

  // CSS display:none — skip this element and all children
  if (cssStyle.hasDisplay && cssStyle.display == CssDisplay::None) {
    self->skipUntilDepth = self->depth;
    self->depth += 1;
    return;
  }

  // CSS text-decoration
  if (cssStyle.hasTextDecoration) {
    if (cssStyle.textDecoration == CssTextDecoration::Underline) {
      self->cssUnderlineUntilDepth = min(self->cssUnderlineUntilDepth, self->depth);
    } else if (cssStyle.textDecoration == CssTextDecoration::LineThrough) {
      self->cssStrikethroughUntilDepth = min(self->cssStrikethroughUntilDepth, self->depth);
    }
  }

  // Track direction for next text block creation
  if (cssStyle.hasDirection) {
    self->pendingRtl_ = (cssStyle.direction == TextDirection::Rtl);
    self->rtlUntilDepth_ = min(self->rtlUntilDepth_, self->depth);
  }

  if (matches(name, HEADER_TAGS, NUM_HEADER_TAGS)) {
    self->startNewTextBlock(TextBlock::CENTER_ALIGN);
    self->boldUntilDepth = min(self->boldUntilDepth, self->depth);
  } else if (matches(name, BLOCK_TAGS, NUM_BLOCK_TAGS)) {
    if (strcmp(name, "br") == 0) {
      self->flushPartWordBuffer();
      const auto style = self->currentTextBlock ? self->currentTextBlock->getStyle()
                                                : static_cast<TextBlock::BLOCK_STYLE>(self->config.paragraphAlignment);
      self->startNewTextBlock(style);
    } else {
      // Determine block style: CSS text-align takes precedence
      TextBlock::BLOCK_STYLE blockStyle = static_cast<TextBlock::BLOCK_STYLE>(self->config.paragraphAlignment);
      if (cssStyle.hasTextAlign) {
        switch (cssStyle.textAlign) {
          case TextAlign::Left:
            blockStyle = TextBlock::LEFT_ALIGN;
            break;
          case TextAlign::Right:
            blockStyle = TextBlock::RIGHT_ALIGN;
            break;
          case TextAlign::Center:
            blockStyle = TextBlock::CENTER_ALIGN;
            break;
          case TextAlign::Justify:
            blockStyle = TextBlock::JUSTIFIED;
            break;
          default:
            break;
        }
      }
      self->startNewTextBlock(blockStyle);
    }
  } else if (matches(name, BOLD_TAGS, NUM_BOLD_TAGS)) {
    self->boldUntilDepth = min(self->boldUntilDepth, self->depth);
  } else if (matches(name, ITALIC_TAGS, NUM_ITALIC_TAGS)) {
    self->italicUntilDepth = min(self->italicUntilDepth, self->depth);
  } else if (matches(name, UNDERLINE_TAGS, NUM_UNDERLINE_TAGS)) {
    self->underlineUntilDepth = min(self->underlineUntilDepth, self->depth);
  } else if (matches(name, STRIKETHROUGH_TAGS, NUM_STRIKETHROUGH_TAGS)) {
    self->strikethroughUntilDepth = min(self->strikethroughUntilDepth, self->depth);
  } else if (strcmp(name, "hr") == 0) {
    // Horizontal rule: flush text, draw a centered line separator
    self->flushPartWordBuffer();
    if (self->currentTextBlock && !self->currentTextBlock->isEmpty()) {
      self->makePages();
    }
    self->startNewTextBlock(TextBlock::CENTER_ALIGN);
    if (self->currentTextBlock) {
      // Use a visual separator that works with any font
      self->currentTextBlock->addWord("\xE2\x80\x95\xE2\x80\x95\xE2\x80\x95\xE2\x80\x95\xE2\x80\x95",
                                      EpdFontFamily::REGULAR);  // Five horizontal bars (U+2015)
    }
  }

  // Record anchor-to-page mapping (after block handling so pagesCreated_ reflects current page)
  if (!idAttr.empty()) {
    self->anchorMap_.emplace_back(std::move(idAttr), self->pagesCreated_);
  }

  self->depth += 1;
}

void XMLCALL ChapterHtmlSlimParser::characterData(void* userData, const XML_Char* s, const int len) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);

  // Don't emit text before <body>
  if (!self->insideBody_) {
    return;
  }

  // Middle of skip
  if (self->skipUntilDepth < self->depth) {
    return;
  }

  // Inside table cell — collect text into cell buffer
  if (self->inTable_ && self->inTableCell_ && self->nestedTableDepth_ == 0) {
    if (!self->tableRows_.empty() && !self->tableRows_.back().empty()) {
      auto& cell = self->tableRows_.back().back();
      for (int i = 0; i < len; i++) {
        if (isWhitespace(s[i])) {
          // Collapse whitespace to single space
          if (!cell.text.empty() && cell.text.back() != ' ') {
            cell.text += ' ';
          }
        } else {
          cell.text += s[i];
        }
      }
    }
    return;
  }

  // Inside table caption — collect text
  if (self->inTable_ && self->inTableCaption_ && self->nestedTableDepth_ == 0) {
    for (int i = 0; i < len; i++) {
      if (isWhitespace(s[i])) {
        if (!self->tableCaption_.empty() && self->tableCaption_.back() != ' ') {
          self->tableCaption_ += ' ';
        }
      } else {
        self->tableCaption_ += s[i];
      }
    }
    return;
  }

  // Inside table but not in a cell or caption — skip (thead text, etc.)
  if (self->inTable_) {
    return;
  }

  // Zero Width No-Break Space / BOM (U+FEFF) = 0xEF 0xBB 0xBF
  const XML_Char FEFF_BYTE_1 = static_cast<XML_Char>(0xEF);
  const XML_Char FEFF_BYTE_2 = static_cast<XML_Char>(0xBB);
  const XML_Char FEFF_BYTE_3 = static_cast<XML_Char>(0xBF);

  for (int i = 0; i < len; i++) {
    if (isWhitespace(s[i])) {
      // Currently looking at whitespace, if there's anything in the partWordBuffer, flush it
      if (self->partWordBufferIndex > 0) {
        self->flushPartWordBuffer();
      }
      // Skip the whitespace char
      continue;
    }

    // Skip BOM character (sometimes appears before em-dashes in EPUBs)
    if (s[i] == FEFF_BYTE_1) {
      // Check if the next two bytes complete the 3-byte sequence
      if ((i + 2 < len) && (s[i + 1] == FEFF_BYTE_2) && (s[i + 2] == FEFF_BYTE_3)) {
        // Sequence 0xEF 0xBB 0xBF found!
        i += 2;    // Skip the next two bytes
        continue;  // Move to the next iteration
      }
    }

    // If we're about to run out of space, then cut the word off and start a new one
    if (self->partWordBufferIndex >= MAX_WORD_SIZE) {
      self->flushPartWordBuffer();
    }

    self->partWordBuffer[self->partWordBufferIndex++] = s[i];
  }

  // Flag for deferred split - handled outside XML callback to avoid stack overflow
  if (self->currentTextBlock && self->currentTextBlock->size() > 750) {
    self->pendingEmergencySplit_ = true;
  }
}

void XMLCALL ChapterHtmlSlimParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);

  // Table element handling — manage state before normal processing
  if (self->inTable_) {
    if (strcmp(name, "table") == 0) {
      if (self->nestedTableDepth_ > 0) {
        self->nestedTableDepth_--;
      } else {
        self->renderTable();
        self->inTable_ = false;
        self->inTableCell_ = false;
      }
    } else if (strcmp(name, "td") == 0 || strcmp(name, "th") == 0) {
      self->inTableCell_ = false;
    } else if (strcmp(name, "caption") == 0) {
      self->inTableCaption_ = false;
    }
    self->depth -= 1;
    return;  // Skip normal endElement processing for table internals
  }

  (void)name;

  if (self->partWordBufferIndex > 0) {
    // Only flush out part word buffer if we're closing a block tag or are at the top of the HTML file.
    // We don't want to flush out content when closing inline tags like <span>.
    // Currently this also flushes out on closing <b> and <i> tags, but they are line tags so that shouldn't happen,
    // text styling needs to be overhauled to fix it.
    const bool shouldBreakText =
        matches(name, BLOCK_TAGS, NUM_BLOCK_TAGS) || matches(name, HEADER_TAGS, NUM_HEADER_TAGS) ||
        matches(name, BOLD_TAGS, NUM_BOLD_TAGS) || matches(name, ITALIC_TAGS, NUM_ITALIC_TAGS) ||
        matches(name, UNDERLINE_TAGS, NUM_UNDERLINE_TAGS) ||
        matches(name, STRIKETHROUGH_TAGS, NUM_STRIKETHROUGH_TAGS) || self->depth == 1;

    if (shouldBreakText) {
      self->flushPartWordBuffer();
    }
  }

  self->depth -= 1;

  if (self->skipUntilDepth == self->depth) {
    self->skipUntilDepth = INT_MAX;
  }
  if (self->boldUntilDepth == self->depth) {
    self->boldUntilDepth = INT_MAX;
  }
  if (self->italicUntilDepth == self->depth) {
    self->italicUntilDepth = INT_MAX;
  }
  if (self->cssBoldUntilDepth == self->depth) {
    self->cssBoldUntilDepth = INT_MAX;
  }
  if (self->cssItalicUntilDepth == self->depth) {
    self->cssItalicUntilDepth = INT_MAX;
  }
  if (self->underlineUntilDepth == self->depth) {
    self->underlineUntilDepth = INT_MAX;
  }
  if (self->strikethroughUntilDepth == self->depth) {
    self->strikethroughUntilDepth = INT_MAX;
  }
  if (self->cssUnderlineUntilDepth == self->depth) {
    self->cssUnderlineUntilDepth = INT_MAX;
  }
  if (self->cssStrikethroughUntilDepth == self->depth) {
    self->cssStrikethroughUntilDepth = INT_MAX;
  }
  if (self->rtlUntilDepth_ == self->depth) {
    self->rtlUntilDepth_ = INT_MAX;
    self->pendingRtl_ = false;
  }
}

void XMLCALL ChapterHtmlSlimParser::defaultHandler(void* userData, const XML_Char* s, int len) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);

  // Don't emit anything before <body>
  if (!self->insideBody_) {
    return;
  }

  // Called for text that expat doesn't handle — primarily undeclared entities.
  // Expat handles the 5 built-in XML entities (&amp; &lt; &gt; &quot; &apos;) and any
  // entities declared in the document's DTD. This catches HTML entities like &nbsp;,
  // &mdash;, &ldquo; etc. that many EPUBs use without proper DTD declarations.
  if (len >= 3 && s[0] == '&' && s[len - 1] == ';') {
    const char* utf8 = lookupHtmlEntity(s + 1, len - 2);
    if (utf8) {
      characterData(userData, utf8, static_cast<int>(strlen(utf8)));
      return;
    }
  }
  // Not a recognized entity — pass through as raw text
  characterData(userData, s, len);
}

bool ChapterHtmlSlimParser::shouldAbort() const {
  // Check external abort callback first (cooperative cancellation)
  if (externalAbortCallback_ && externalAbortCallback_()) {
    Serial.printf("[%lu] [EHP] External abort requested\n", millis());
    return true;
  }

  // Check timeout
  if (millis() - parseStartTime_ > MAX_PARSE_TIME_MS) {
    Serial.printf("[%lu] [EHP] Parse timeout exceeded (%u ms)\n", millis(), MAX_PARSE_TIME_MS);
    return true;
  }

  // Check memory pressure
  const size_t freeHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (freeHeap < MIN_FREE_HEAP) {
    Serial.printf("[%lu] [EHP] Low memory (%zu bytes free)\n", millis(), freeHeap);
    return true;
  }

  return false;
}

ChapterHtmlSlimParser::~ChapterHtmlSlimParser() { cleanupParser(); }

void ChapterHtmlSlimParser::cleanupParser() {
  if (xmlParser_) {
    XML_SetElementHandler(xmlParser_, nullptr, nullptr);
    XML_SetCharacterDataHandler(xmlParser_, nullptr);
    XML_SetDefaultHandlerExpand(xmlParser_, nullptr);
    XML_ParserFree(xmlParser_);
    xmlParser_ = nullptr;
  }
  if (file_) {
    file_.close();
  }
  currentPage.reset();
  currentTextBlock.reset();
  suspended_ = false;
}

bool ChapterHtmlSlimParser::initParser() {
  parseStartTime_ = millis();
  loopCounter_ = 0;
  elementCounter_ = 0;
  cssHeapOk_ = true;
  pendingEmergencySplit_ = false;
  aborted_ = false;
  stopRequested_ = false;
  suspended_ = false;
  insideBody_ = false;
  depth = 0;
  skipUntilDepth = INT_MAX;
  boldUntilDepth = INT_MAX;
  italicUntilDepth = INT_MAX;
  cssBoldUntilDepth = INT_MAX;
  cssItalicUntilDepth = INT_MAX;
  underlineUntilDepth = INT_MAX;
  strikethroughUntilDepth = INT_MAX;
  cssUnderlineUntilDepth = INT_MAX;
  cssStrikethroughUntilDepth = INT_MAX;
  inTable_ = false;
  inTableCell_ = false;
  inTableCaption_ = false;
  tableCaption_.clear();
  nestedTableDepth_ = 0;
  tableRows_.clear();
  dataUriStripper_.reset();
  startNewTextBlock(static_cast<TextBlock::BLOCK_STYLE>(config.paragraphAlignment));

  xmlParser_ = XML_ParserCreate(nullptr);
  if (!xmlParser_) {
    Serial.printf("[%lu] [EHP] Couldn't allocate memory for parser\n", millis());
    return false;
  }

  if (!SdMan.openFileForRead("EHP", filepath, file_)) {
    XML_ParserFree(xmlParser_);
    xmlParser_ = nullptr;
    return false;
  }

  totalSize_ = file_.size();
  bytesRead_ = 0;
  lastProgress_ = -1;
  pagesCreated_ = 0;

  // Allow parsing documents with undeclared HTML entities (e.g. &nbsp;, &mdash;).
  // Without this, Expat returns XML_ERROR_UNDEFINED_ENTITY for any entity
  // not declared in the document's DTD. UseForeignDTD makes Expat treat
  // undeclared entities as "skipped" rather than errors, passing them to
  // our default handler for resolution via the HTML entity lookup table.
  XML_UseForeignDTD(xmlParser_, XML_TRUE);

  XML_SetUserData(xmlParser_, this);
  XML_SetElementHandler(xmlParser_, startElement, endElement);
  XML_SetCharacterDataHandler(xmlParser_, characterData);
  XML_SetDefaultHandlerExpand(xmlParser_, defaultHandler);

  return true;
}

bool ChapterHtmlSlimParser::parseLoop() {
  int done;

  do {
    // Periodic safety check and yield
    if (++loopCounter_ % YIELD_CHECK_INTERVAL == 0) {
      if (shouldAbort()) {
        Serial.printf("[%lu] [EHP] Aborting parse, pages created: %u\n", millis(), pagesCreated_);
        aborted_ = true;
        break;
      }
      vTaskDelay(1);  // Yield to prevent watchdog reset
    }

    constexpr size_t kReadChunkSize = 1024;
    constexpr size_t kDataUriPrefixSize = 10;  // max partial saved by DataUriStripper: "src=\"data:"
    void* const buf = XML_GetBuffer(xmlParser_, kReadChunkSize + kDataUriPrefixSize);
    if (!buf) {
      Serial.printf("[%lu] [EHP] Couldn't allocate memory for buffer\n", millis());
      cleanupParser();
      return false;
    }

    size_t len = file_.read(static_cast<uint8_t*>(buf), kReadChunkSize);

    if (len == 0) {
      // len==0 is normal EOF — finalize the XML parser
      XML_ParseBuffer(xmlParser_, 0, 1);
      done = 1;
      break;
    }

    // Strip data URIs BEFORE expat parses the buffer to prevent OOM on large embedded images.
    // This replaces src="data:image/..." with src="#" so expat never sees the huge base64 string.
    const size_t originalLen = len;
    len = dataUriStripper_.strip(static_cast<char*>(buf), len, kReadChunkSize + kDataUriPrefixSize);

    // Update progress (call every 10% change to avoid too frequent updates)
    // Only show progress for larger chapters where rendering overhead is worth it
    bytesRead_ += originalLen;
    if (progressFn && totalSize_ >= MIN_SIZE_FOR_PROGRESS) {
      const int progress = static_cast<int>((bytesRead_ * 100) / totalSize_);
      if (lastProgress_ / 10 != progress / 10) {
        lastProgress_ = progress;
        progressFn(progress);
      }
    }

    done = file_.available() == 0;

    const auto status = XML_ParseBuffer(xmlParser_, static_cast<int>(len), done);
    if (status == XML_STATUS_ERROR) {
      Serial.printf("[%lu] [EHP] Parse error at line %lu:\n%s\n", millis(), XML_GetCurrentLineNumber(xmlParser_),
                    XML_ErrorString(XML_GetErrorCode(xmlParser_)));
      cleanupParser();
      return false;
    }

    // XML_STATUS_SUSPENDED means completePageFn returned false (maxPages hit).
    // Parser state is preserved for resume. Close file to free handle.
    if (status == XML_STATUS_SUSPENDED) {
      suspended_ = true;
      file_.close();
      return true;
    }

    // Deferred emergency split - runs outside XML callback to avoid stack overflow.
    // Inside characterData(), the call chain includes expat internal frames (~1-2KB).
    // By splitting here, we save that stack space - critical for external fonts which
    // add extra frames through getExternalGlyphWidth() → ExternalFont::getGlyph() (SD I/O).
    if (pendingEmergencySplit_ && currentTextBlock && !currentTextBlock->isEmpty()) {
      pendingEmergencySplit_ = false;
      const size_t freeHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      if (freeHeap < MIN_FREE_HEAP + (MIN_FREE_HEAP / 4)) {
        Serial.printf("[%lu] [EHP] Low memory (%zu), aborting parse\n", millis(), freeHeap);
        aborted_ = true;
        break;
      }
      Serial.printf("[%lu] [EHP] Text block too long (%zu words), splitting\n", millis(), currentTextBlock->size());
      currentTextBlock->setUseGreedyBreaking(true);
      currentTextBlock->layoutAndExtractLines(
          renderer, config.fontId, config.viewportWidth,
          [this](const std::shared_ptr<TextBlock>& textBlock) { addLineToPage(textBlock); }, false,
          [this]() -> bool { return shouldAbort(); });
    }
  } while (!done);

  // Reached end of file or aborted — finalize
  // Process last page if there is still text
  if (currentTextBlock && !stopRequested_) {
    makePages();
    if (!stopRequested_ && currentPage) {
      completePageFn(std::move(currentPage));
    }
    currentPage.reset();
    currentTextBlock.reset();
  }

  cleanupParser();
  return true;
}

bool ChapterHtmlSlimParser::parseAndBuildPages() {
  if (!initParser()) {
    return false;
  }
  return parseLoop();
}

bool ChapterHtmlSlimParser::resumeParsing() {
  if (!suspended_ || !xmlParser_) {
    return false;
  }

  // Reopen file at saved position (closed on suspend to free file handle)
  if (!SdMan.openFileForRead("EHP", filepath, file_)) {
    Serial.printf("[%lu] [EHP] Failed to reopen file for resume\n", millis());
    cleanupParser();
    return false;
  }
  file_.seek(bytesRead_);

  // Reset per-extend state
  parseStartTime_ = millis();
  loopCounter_ = 0;
  elementCounter_ = 0;
  stopRequested_ = false;
  suspended_ = false;

  const auto status = XML_ResumeParser(xmlParser_);
  if (status == XML_STATUS_ERROR) {
    Serial.printf("[%lu] [EHP] Resume error: %s\n", millis(), XML_ErrorString(XML_GetErrorCode(xmlParser_)));
    cleanupParser();
    return false;
  }

  // If resume itself caused a suspend (maxPages hit again immediately), we're done.
  // Close file to free handle (same as the suspend path inside parseLoop).
  if (status == XML_STATUS_SUSPENDED) {
    suspended_ = true;
    file_.close();
    return true;
  }

  // Continue the file-reading loop
  return parseLoop();
}

void ChapterHtmlSlimParser::addLineToPage(std::shared_ptr<TextBlock> line) {
  if (stopRequested_) return;

  const int lineHeight = renderer.getLineHeight(config.fontId) * config.lineCompression;

  if (currentPageNextY + lineHeight > config.viewportHeight) {
    ++pagesCreated_;
    if (!completePageFn(std::move(currentPage))) {
      // Preserve this line for the next batch — it's already been
      // extracted from the text block and would be lost otherwise
      currentPage.reset(new Page());
      currentPageNextY = 0;
      currentPage->elements.push_back(std::make_shared<PageLine>(line, 0, currentPageNextY));
      currentPageNextY += lineHeight;
      stopRequested_ = true;
      if (xmlParser_) {
        XML_StopParser(xmlParser_, XML_TRUE);  // Resumable suspend
      }
      return;
    }
    parseStartTime_ = millis();
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  currentPage->elements.push_back(std::make_shared<PageLine>(line, 0, currentPageNextY));
  currentPageNextY += lineHeight;
}

void ChapterHtmlSlimParser::makePages() {
  if (!currentTextBlock) {
    Serial.printf("[%lu] [EHP] !! No text block to make pages for !!\n", millis());
    return;
  }

  flushPartWordBuffer();

  // Check memory before expensive layout operation
  // Layout needs ~4-6KB for text processing; allow operation if we have 1.25× MIN_FREE_HEAP
  const size_t freeHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (freeHeap < MIN_FREE_HEAP + (MIN_FREE_HEAP / 4)) {
    Serial.printf("[%lu] [EHP] Insufficient memory for layout (%zu bytes)\n", millis(), freeHeap);
    currentTextBlock.reset();
    aborted_ = true;
    return;
  }

  if (!currentPage) {
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  const int lineHeight = renderer.getLineHeight(config.fontId) * config.lineCompression;
  currentTextBlock->layoutAndExtractLines(
      renderer, config.fontId, config.viewportWidth,
      [this](const std::shared_ptr<TextBlock>& textBlock) { addLineToPage(textBlock); }, true,
      [this]() -> bool { return stopRequested_; });
  // Extra paragraph spacing based on spacingLevel (0=none, 1=small, 3=large)
  // Skip if aborted mid-block — spacing between paragraphs, not mid-paragraph
  if (!stopRequested_) {
    switch (config.spacingLevel) {
      case 1:
        currentPageNextY += lineHeight / 4;  // Small (1/4 line)
        break;
      case 3:
        currentPageNextY += lineHeight;  // Large (full line)
        break;
    }
  }
}

std::string ChapterHtmlSlimParser::cacheImage(const std::string& src) {
  // Check abort before starting image processing
  if (externalAbortCallback_ && externalAbortCallback_()) {
    Serial.printf("[%lu] [EHP] Abort requested, skipping image\n", millis());
    return "";
  }

  // Skip data URIs - embedded base64 images can't be extracted and waste memory
  if (src.length() >= 5 && strncasecmp(src.c_str(), "data:", 5) == 0) {
    Serial.printf("[%lu] [EHP] Skipping embedded data URI image\n", millis());
    return "";
  }

  // Skip remaining images after too many consecutive failures
  if (consecutiveImageFailures_ >= MAX_CONSECUTIVE_IMAGE_FAILURES) {
    Serial.printf("[%lu] [EHP] Skipping image - too many failures\n", millis());
    return "";
  }

  // Skip image conversion if heap is critically low (prevents abort from malloc failure)
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 20000) {
    Serial.printf("[%lu] [EHP] Skipping image - low heap (%zu bytes)\n", millis(), freeHeap);
    consecutiveImageFailures_++;
    return "";
  }

  // Resolve relative path from chapter base
  std::string resolvedPath = FsHelpers::normalisePath(chapterBasePath + src);

  // Generate cache filename from hash
  size_t srcHash = std::hash<std::string>{}(resolvedPath);
  std::string cachedBmpPath = imageCachePath + "/" + std::to_string(srcHash) + ".bmp";

  // Check if already cached
  if (SdMan.exists(cachedBmpPath.c_str())) {
    consecutiveImageFailures_ = 0;  // Reset on success
    return cachedBmpPath;
  }

  // Check for failed marker
  std::string failedMarker = imageCachePath + "/" + std::to_string(srcHash) + ".failed";
  if (SdMan.exists(failedMarker.c_str())) {
    consecutiveImageFailures_++;
    return "";
  }

  // Check if format is supported
  if (!ImageConverterFactory::isSupported(src)) {
    Serial.printf("[%lu] [EHP] Unsupported image format: %s\n", millis(), src.c_str());
    FsFile marker;
    if (SdMan.openFileForWrite("EHP", failedMarker, marker)) {
      marker.close();
    }
    consecutiveImageFailures_++;
    return "";
  }

  // BMP source: already device-native — extract directly to cache, zero conversion
  if (FsHelpers::isBmpFile(src)) {
    FsFile bmpFile;
    if (!SdMan.openFileForWrite("EHP", cachedBmpPath, bmpFile)) {
      Serial.printf("[%lu] [EHP] Failed to create cache file for BMP\n", millis());
      return "";
    }
    if (!readItemFn(resolvedPath, bmpFile, 1024)) {
      Serial.printf("[%lu] [EHP] Failed to extract BMP: %s\n", millis(), resolvedPath.c_str());
      bmpFile.close();
      SdMan.remove(cachedBmpPath.c_str());
      FsFile marker;
      if (SdMan.openFileForWrite("EHP", failedMarker, marker)) {
        marker.close();
      }
      consecutiveImageFailures_++;
      return "";
    }
    bmpFile.close();
    consecutiveImageFailures_ = 0;
    Serial.printf("[%lu] [EHP] Cached BMP direct: %s\n", millis(), cachedBmpPath.c_str());
    return cachedBmpPath;
  }

  // JPEG/PNG source: extract to temp, convert to BMP with dithering
  const std::string tempExt = FsHelpers::isPngFile(src) ? ".png" : ".jpg";
  std::string tempPath = imageCachePath + "/.tmp_" + std::to_string(srcHash) + tempExt;
  FsFile tempFile;
  if (!SdMan.openFileForWrite("EHP", tempPath, tempFile)) {
    Serial.printf("[%lu] [EHP] Failed to create temp file for image\n", millis());
    return "";
  }

  if (!readItemFn(resolvedPath, tempFile, 1024)) {
    Serial.printf("[%lu] [EHP] Failed to extract image: %s\n", millis(), resolvedPath.c_str());
    tempFile.close();
    SdMan.remove(tempPath.c_str());
    FsFile marker;
    if (SdMan.openFileForWrite("EHP", failedMarker, marker)) {
      marker.close();
    }
    consecutiveImageFailures_++;
    return "";
  }
  tempFile.close();

  const int maxImageHeight = config.allowTallImages ? 2000 : config.viewportHeight;
  ImageConvertConfig convertConfig;
  convertConfig.maxWidth = static_cast<int>(config.viewportWidth);
  convertConfig.maxHeight = maxImageHeight;
  convertConfig.logTag = "EHP";
  convertConfig.shouldAbort = externalAbortCallback_;

  const bool success = ImageConverterFactory::convertToBmp(tempPath, cachedBmpPath, convertConfig);
  SdMan.remove(tempPath.c_str());

  if (!success) {
    Serial.printf("[%lu] [EHP] Failed to convert image to BMP: %s\n", millis(), resolvedPath.c_str());
    SdMan.remove(cachedBmpPath.c_str());
    FsFile marker;
    if (SdMan.openFileForWrite("EHP", failedMarker, marker)) {
      marker.close();
    }
    consecutiveImageFailures_++;
    return "";
  }

  consecutiveImageFailures_ = 0;  // Reset on success
  Serial.printf("[%lu] [EHP] Cached image: %s\n", millis(), cachedBmpPath.c_str());
  return cachedBmpPath;
}

void ChapterHtmlSlimParser::addImageToPage(std::shared_ptr<ImageBlock> image) {
  if (stopRequested_) return;

  const int imageHeight = image->getHeight();
  const int lineHeight = renderer.getLineHeight(config.fontId) * config.lineCompression;
  const bool isTallImage = imageHeight > config.viewportHeight / 2;

  if (!currentPage) {
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  // In allowTallImages mode (landscape scroll), don't split — one image per page
  if (config.allowTallImages) {
    // Flush current page if it has any content
    if (currentPageNextY > 0) {
      if (!completePageFn(std::move(currentPage))) {
        stopRequested_ = true;
        if (xmlParser_) XML_StopParser(xmlParser_, XML_TRUE);
        return;
      }
      parseStartTime_ = millis();
      currentPage.reset(new Page());
      currentPageNextY = 0;
    }

    // Center image horizontally
    int xPos = (static_cast<int>(config.viewportWidth) - static_cast<int>(image->getWidth())) / 2;
    if (xPos < 0) xPos = 0;

    currentPage->elements.push_back(std::make_shared<PageImage>(image, xPos, 0));
    currentPageNextY = imageHeight + lineHeight;

    // Always complete the page — each image gets its own scrollable page
    if (!completePageFn(std::move(currentPage))) {
      stopRequested_ = true;
      if (xmlParser_) XML_StopParser(xmlParser_, XML_TRUE);
      return;
    }
    parseStartTime_ = millis();
    currentPage.reset(new Page());
    currentPageNextY = 0;
    return;
  }

  // Normal mode: tall images get a dedicated page, flush current page if it has content
  if (isTallImage && currentPageNextY > 0) {
    if (!completePageFn(std::move(currentPage))) {
      stopRequested_ = true;
      if (xmlParser_) {
        XML_StopParser(xmlParser_, XML_TRUE);  // Resumable suspend
      }
      return;
    }
    parseStartTime_ = millis();
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  // Check if image fits on current page
  if (currentPageNextY + imageHeight > config.viewportHeight) {
    if (!completePageFn(std::move(currentPage))) {
      stopRequested_ = true;
      if (xmlParser_) {
        XML_StopParser(xmlParser_, XML_TRUE);  // Resumable suspend
      }
      return;
    }
    parseStartTime_ = millis();
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  // Center image horizontally (cast to signed to handle images wider than viewport)
  int xPos = (static_cast<int>(config.viewportWidth) - static_cast<int>(image->getWidth())) / 2;
  if (xPos < 0) xPos = 0;

  // Center tall images vertically on their dedicated page
  int yPos = currentPageNextY;
  if (isTallImage && currentPageNextY == 0 && imageHeight < config.viewportHeight) {
    yPos = (config.viewportHeight - imageHeight) / 2;
  }

  currentPage->elements.push_back(std::make_shared<PageImage>(image, xPos, yPos));
  currentPageNextY = yPos + imageHeight + lineHeight;

  // Complete the page after a tall image so text continues on the next page
  if (isTallImage) {
    if (!completePageFn(std::move(currentPage))) {
      stopRequested_ = true;
      if (xmlParser_) {
        XML_StopParser(xmlParser_, XML_TRUE);  // Resumable suspend
      }
      return;
    }
    parseStartTime_ = millis();
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }
}

// ── Table Rendering ─────────────────────────────────────────────────────────

std::string ChapterHtmlSlimParser::trimWhitespace(const std::string& s) {
  size_t start = 0, end = s.size();
  while (start < end && (s[start] == ' ' || s[start] == '\n' || s[start] == '\r' || s[start] == '\t')) start++;
  while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\n' || s[end - 1] == '\r' || s[end - 1] == '\t')) end--;
  return s.substr(start, end - start);
}

std::string ChapterHtmlSlimParser::truncateToFit(const std::string& text, int maxWidth, EpdFontFamily::Style style) {
  if (text.empty() || maxWidth <= 0) return "";
  int fullWidth = renderer.getTextWidth(config.fontId, text.c_str(), style);
  if (fullWidth <= maxWidth) return text;

  // Binary search for the longest prefix that fits with ellipsis
  const int ellipsisWidth = renderer.getTextWidth(config.fontId, "..", style);
  const int targetWidth = maxWidth - ellipsisWidth;
  if (targetWidth <= 0) return "..";

  // Walk forward byte-by-byte, respecting UTF-8 boundaries
  std::string result;
  size_t i = 0;
  while (i < text.size()) {
    // Determine UTF-8 char length
    uint8_t ch = static_cast<uint8_t>(text[i]);
    size_t charLen = 1;
    if (ch >= 0xF0) charLen = 4;
    else if (ch >= 0xE0) charLen = 3;
    else if (ch >= 0xC0) charLen = 2;
    if (i + charLen > text.size()) break;

    std::string candidate = result + text.substr(i, charLen);
    int w = renderer.getTextWidth(config.fontId, candidate.c_str(), style);
    if (w > targetWidth) break;
    result = candidate;
    i += charLen;
  }
  return result + "..";
}

void ChapterHtmlSlimParser::renderTable() {
  if (tableRows_.empty() || stopRequested_) return;

  // Skip table rendering if disabled in settings
  if (!config.showTables) {
    tableRows_.clear();
    return;
  }

  // Remove empty rows
  tableRows_.erase(
      std::remove_if(tableRows_.begin(), tableRows_.end(),
                     [](const std::vector<TableCell>& row) { return row.empty(); }),
      tableRows_.end());
  if (tableRows_.empty()) return;

  // Find max column count
  size_t maxCols = 0;
  for (auto& row : tableRows_) {
    if (row.size() > maxCols) maxCols = row.size();
  }
  // Limit columns to prevent absurd tables from eating all memory
  if (maxCols > 8) maxCols = 8;
  if (maxCols == 0) return;

  // Trim cell text
  for (auto& row : tableRows_) {
    for (auto& cell : row) {
      cell.text = trimWhitespace(cell.text);
    }
  }

  const int vw = config.viewportWidth;
  const int lineHeight = renderer.getLineHeight(config.fontId) * config.lineCompression;
  const int cellPad = 4;                             // pixels padding each side of cell text
  const int sepW = renderer.getTextWidth(config.fontId, "|", EpdFontFamily::REGULAR);
  const int totalSepWidth = (maxCols + 1) * sepW;    // separators: |col1|col2|col3|
  const int totalPadWidth = maxCols * cellPad * 2;    // padding on both sides of each cell
  const int availWidth = vw - totalSepWidth - totalPadWidth;

  if (availWidth < (int)(maxCols * 10)) {
    // Too narrow to render — fall back to placeholder
    auto words = std::vector<TextBlock::WordData>();
    words.emplace_back("[Table: too wide]", 0, EpdFontFamily::ITALIC);
    auto block = std::make_shared<TextBlock>(std::move(words), TextBlock::CENTER_ALIGN);
    addLineToPage(block);
    return;
  }

  // Measure max content width per column
  std::vector<int> colMaxW(maxCols, 0);
  for (auto& row : tableRows_) {
    for (size_t c = 0; c < row.size() && c < maxCols; c++) {
      auto style = row[c].isHeader ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
      int w = renderer.getTextWidth(config.fontId, row[c].text.c_str(), style);
      if (w > colMaxW[c]) colMaxW[c] = w;
    }
  }

  // Distribute width: proportional to content, with minimum per column
  int totalContentW = 0;
  for (auto w : colMaxW) totalContentW += std::max(w, 10);

  std::vector<int> colW(maxCols);
  if (totalContentW <= availWidth) {
    // Everything fits — use natural widths
    for (size_t c = 0; c < maxCols; c++) {
      colW[c] = std::max(colMaxW[c], 10);
    }
  } else {
    // Scale proportionally to fit
    for (size_t c = 0; c < maxCols; c++) {
      colW[c] = std::max(10, (int)((long)std::max(colMaxW[c], 10) * availWidth / totalContentW));
    }
  }

  // Ensure page exists
  if (!currentPage) {
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  // Helper: create a horizontal border row (dashes spanning full width)
  auto makeBorderRow = [&]() {
    std::vector<TextBlock::WordData> words;
    int x = 0;
    // Build border string: +---+---+---+
    words.emplace_back("+", x, EpdFontFamily::REGULAR);
    x += sepW;
    for (size_t c = 0; c < maxCols; c++) {
      int dashWidth = colW[c] + cellPad * 2;
      // Create a string of dashes that fills the column width
      std::string dashes;
      int dashCharW = renderer.getTextWidth(config.fontId, "-", EpdFontFamily::REGULAR);
      if (dashCharW > 0) {
        int count = dashWidth / dashCharW;
        for (int d = 0; d < count; d++) dashes += '-';
      }
      words.emplace_back(dashes, x, EpdFontFamily::REGULAR);
      x += dashWidth;
      words.emplace_back("+", x, EpdFontFamily::REGULAR);
      x += sepW;
    }
    auto block = std::make_shared<TextBlock>(std::move(words), TextBlock::LEFT_ALIGN);
    addLineToPage(block);
  };

  // Render caption above table if present
  if (!tableCaption_.empty()) {
    std::string cap = trimWhitespace(tableCaption_);
    if (!cap.empty()) {
      auto capBlock = std::unique_ptr<ParsedText>(new ParsedText(TextBlock::CENTER_ALIGN, config.indentLevel, false, true, false));
      capBlock->addWord(cap.c_str(), EpdFontFamily::ITALIC);
      currentTextBlock = std::move(capBlock);
      makePages();
    }
    tableCaption_.clear();
  }

  // Render top border
  makeBorderRow();

  bool headerDone = false;
  for (size_t r = 0; r < tableRows_.size() && !stopRequested_; r++) {
    auto& row = tableRows_[r];

    // Build row: |cell1|cell2|cell3|
    std::vector<TextBlock::WordData> words;
    int x = 0;
    words.emplace_back("|", x, EpdFontFamily::REGULAR);
    x += sepW;

    for (size_t c = 0; c < maxCols; c++) {
      x += cellPad;

      if (c < row.size()) {
        auto style = row[c].isHeader ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
        std::string text = truncateToFit(row[c].text, colW[c], style);
        if (!text.empty()) {
          words.emplace_back(text, x, style);
        }
      }

      x += colW[c] + cellPad;
      words.emplace_back("|", x, EpdFontFamily::REGULAR);
      x += sepW;
    }

    auto block = std::make_shared<TextBlock>(std::move(words), TextBlock::LEFT_ALIGN);
    addLineToPage(block);

    // Draw border after header row(s)
    if (!headerDone && !row.empty() && row[0].isHeader) {
      // Check if next row is NOT a header
      bool nextIsData = (r + 1 >= tableRows_.size()) || (!tableRows_[r + 1].empty() && !tableRows_[r + 1][0].isHeader);
      if (nextIsData) {
        makeBorderRow();
        headerDone = true;
      }
    }
  }

  // Bottom border
  makeBorderRow();

  // Resume normal text block for content after table
  startNewTextBlock(static_cast<TextBlock::BLOCK_STYLE>(config.paragraphAlignment));
}
