#include "ParsedText.h"

#include <GfxRenderer.h>
#include <Utf8.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "../../src/core/MemoryArena.h"
#include "hyphenation/Hyphenator.h"

constexpr int MAX_COST = std::numeric_limits<int>::max();

// Soft hyphen (U+00AD) as UTF-8 bytes
constexpr unsigned char SOFT_HYPHEN_BYTE1 = 0xC2;
constexpr unsigned char SOFT_HYPHEN_BYTE2 = 0xAD;

// Known attaching punctuation (including UTF-8 sequences)
const std::vector<std::string> punctuation = {
    ".",
    ",",
    "!",
    "?",
    ";",
    ":",
    "\"",
    "'",
    "\xE2\x80\x99",  // ' (U+2019 right single quote)
    "\xE2\x80\x9D"   // " (U+201D right double quote)
};

// Check if a word consists entirely of attaching punctuation
// These should attach to the previous word without extra spacing
bool isAttachingPunctuationWord(const std::string& word) {
  if (word.empty()) return false;
  size_t pos = 0;
  while (pos < word.size()) {
    bool matched = false;
    for (const auto& p : punctuation) {
      if (word.compare(pos, p.size(), p) == 0) {
        pos += p.size();
        matched = true;
        break;
      }
    }
    if (!matched) return false;
  }
  return true;
}

namespace {

// Remove all soft hyphens (U+00AD) from a string
std::string stripSoftHyphens(const std::string& word) {
  std::string result;
  result.reserve(word.size());
  size_t i = 0;
  while (i < word.size()) {
    if (i + 1 < word.size() && static_cast<unsigned char>(word[i]) == SOFT_HYPHEN_BYTE1 &&
        static_cast<unsigned char>(word[i + 1]) == SOFT_HYPHEN_BYTE2) {
      i += 2;  // Skip soft hyphen
    } else {
      result += word[i++];
    }
  }
  return result;
}

// Check if codepoint is CJK ideograph (Unicode Line Break Class ID)
// Based on UAX #14 - allows line break before/after these characters
bool isCjkCodepoint(uint32_t cp) {
  // CJK Unified Ideographs
  if (cp >= 0x4E00 && cp <= 0x9FFF) return true;
  // CJK Extension A
  if (cp >= 0x3400 && cp <= 0x4DBF) return true;
  // CJK Compatibility Ideographs
  if (cp >= 0xF900 && cp <= 0xFAFF) return true;
  // Hiragana
  if (cp >= 0x3040 && cp <= 0x309F) return true;
  // Katakana
  if (cp >= 0x30A0 && cp <= 0x30FF) return true;
  // Hangul Syllables
  if (cp >= 0xAC00 && cp <= 0xD7AF) return true;
  // CJK Extension B and beyond (Plane 2)
  if (cp >= 0x20000 && cp <= 0x2A6DF) return true;
  // Fullwidth ASCII variants (often used in CJK context)
  if (cp >= 0xFF00 && cp <= 0xFFEF) return true;
  return false;
}

// Measure a word's width, stripping soft hyphens and optionally appending a visible hyphen.
uint16_t measureWordWidth(const GfxRenderer& renderer, const int fontId, const std::string& word,
                          const EpdFontFamily::Style style, const bool appendHyphen = false) {
  const bool hasShy = word.find("\xC2\xAD") != std::string::npos;
  if (!hasShy && !appendHyphen) {
    return renderer.getTextWidth(fontId, word.c_str(), style);
  }

  std::string sanitized = hasShy ? stripSoftHyphens(word) : word;
  if (appendHyphen) {
    sanitized.push_back('-');
  }
  return renderer.getTextWidth(fontId, sanitized.c_str(), style);
}

}  // namespace

void ParsedText::addWord(std::string word, const EpdFontFamily::Style fontStyle, const uint8_t decorations) {
  if (word.empty()) return;

  // Check if word contains any CJK characters
  bool hasCjk = false;
  const unsigned char* check = reinterpret_cast<const unsigned char*>(word.c_str());
  uint32_t cp;
  while ((cp = utf8NextCodepoint(&check))) {
    if (isCjkCodepoint(cp)) {
      hasCjk = true;
      break;
    }
  }

  if (!hasCjk) {
    // No CJK - keep as single word (Latin, accented Latin, Cyrillic, etc.)
    words.push_back(std::move(word));
    wordStyles.push_back(fontStyle);
    wordDecorations.push_back(decorations);
    return;
  }

  // Mixed content: group non-CJK runs together, split CJK individually
  const unsigned char* p = reinterpret_cast<const unsigned char*>(word.c_str());
  std::string nonCjkBuf;

  while ((cp = utf8NextCodepoint(&p))) {
    if (isCjkCodepoint(cp)) {
      // CJK character - flush non-CJK buffer first, then add this char alone
      if (!nonCjkBuf.empty()) {
        words.push_back(std::move(nonCjkBuf));
        wordStyles.push_back(fontStyle);
        wordDecorations.push_back(decorations);
        nonCjkBuf.clear();
      }

      // Re-encode CJK codepoint to UTF-8
      std::string buf;
      if (cp < 0x10000) {
        buf += static_cast<char>(0xE0 | (cp >> 12));
        buf += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        buf += static_cast<char>(0x80 | (cp & 0x3F));
      } else {
        buf += static_cast<char>(0xF0 | (cp >> 18));
        buf += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        buf += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        buf += static_cast<char>(0x80 | (cp & 0x3F));
      }
      words.push_back(buf);
      wordStyles.push_back(fontStyle);
      wordDecorations.push_back(decorations);
    } else {
      // Non-CJK character - accumulate into buffer
      if (cp < 0x80) {
        nonCjkBuf += static_cast<char>(cp);
      } else if (cp < 0x800) {
        nonCjkBuf += static_cast<char>(0xC0 | (cp >> 6));
        nonCjkBuf += static_cast<char>(0x80 | (cp & 0x3F));
      } else if (cp < 0x10000) {
        nonCjkBuf += static_cast<char>(0xE0 | (cp >> 12));
        nonCjkBuf += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        nonCjkBuf += static_cast<char>(0x80 | (cp & 0x3F));
      } else {
        nonCjkBuf += static_cast<char>(0xF0 | (cp >> 18));
        nonCjkBuf += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        nonCjkBuf += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        nonCjkBuf += static_cast<char>(0x80 | (cp & 0x3F));
      }
    }
  }

  // Flush any remaining non-CJK buffer
  if (!nonCjkBuf.empty()) {
    words.push_back(std::move(nonCjkBuf));
    wordStyles.push_back(fontStyle);
    wordDecorations.push_back(decorations);
  }
}

// Consumes data to minimize memory usage
// Returns false if aborted, true otherwise
bool ParsedText::layoutAndExtractLines(const GfxRenderer& renderer, const int fontId, const uint16_t viewportWidth,
                                       const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                                       const bool includeLastLine, const AbortCallback& shouldAbort) {
  if (words.empty()) {
    return true;
  }

  // Check for abort before starting
  if (shouldAbort && shouldAbort()) {
    return false;
  }

  const int pageWidth = viewportWidth;
  const int spaceWidth = renderer.getSpaceWidth(fontId);

  auto wordWidths = calculateWordWidths(renderer, fontId);

  std::vector<size_t> lineBreakIndices;
  if (hyphenationEnabled) {
    // Greedy layout with opportunistic Liang hyphenation at overflow points
    lineBreakIndices = computeHyphenatedLineBreaks(renderer, fontId, pageWidth, spaceWidth, wordWidths, shouldAbort);
  } else if (useGreedyBreaking) {
    lineBreakIndices = computeLineBreaksGreedy(pageWidth, spaceWidth, wordWidths, shouldAbort);
  } else {
    lineBreakIndices = computeLineBreaks(pageWidth, spaceWidth, wordWidths, shouldAbort);
  }

  // Check if we were aborted during line break computation
  if (shouldAbort && shouldAbort()) {
    return false;
  }

  const size_t lineCount = includeLastLine ? lineBreakIndices.size() : lineBreakIndices.size() - 1;

  for (size_t i = 0; i < lineCount; ++i) {
    if (shouldAbort && shouldAbort()) {
      return false;
    }
    extractLine(i, pageWidth, spaceWidth, wordWidths, lineBreakIndices, processLine);
  }
  return true;
}

std::vector<uint16_t> ParsedText::calculateWordWidths(const GfxRenderer& renderer, const int fontId) {
  const size_t totalWordCount = words.size();

  std::vector<uint16_t> wordWidths;
  wordWidths.reserve(totalWordCount);

  // Add indentation at the beginning of first word in paragraph
  if (indentLevel > 0 && !words.empty()) {
    std::string& first_word = words.front();
    switch (indentLevel) {
      case 2:  // Normal - em-space (U+2003)
        first_word.insert(0, "\xe2\x80\x83");
        break;
      case 3:  // Large - em-space + en-space (U+2003 + U+2002)
        first_word.insert(0, "\xe2\x80\x83\xe2\x80\x82");
        break;
      default:  // Fallback for unexpected values: single en-space (U+2002)
        first_word.insert(0, "\xe2\x80\x82");
        break;
    }
  }

  auto wordsIt = words.begin();
  auto wordStylesIt = wordStyles.begin();

  while (wordsIt != words.end()) {
    // Strip soft hyphens before measuring (they should be invisible)
    std::string displayWord = stripSoftHyphens(*wordsIt);
    wordWidths.push_back(renderer.getTextWidth(fontId, displayWord.c_str(), *wordStylesIt));
    // Update the word in the list with the stripped version for rendering
    *wordsIt = std::move(displayWord);

    std::advance(wordsIt, 1);
    std::advance(wordStylesIt, 1);
  }

  return wordWidths;
}

std::vector<size_t> ParsedText::computeLineBreaks(const int pageWidth, const int spaceWidth,
                                                  const std::vector<uint16_t>& wordWidths,
                                                  const AbortCallback& shouldAbort) const {
  const size_t n = words.size();

  if (n == 0) {
    return {};
  }

  // Build an indexed snapshot of which words are attaching punctuation for O(1) lookup.
  // Attaching punctuation words don't get space before them, mirroring crosspoint's "continues" concept.
  std::vector<bool> isAttaching(n, false);
  {
    auto it = words.begin();
    for (size_t i = 0; i < n; ++i, ++it) {
      isAttaching[i] = isAttachingPunctuationWord(*it);
    }
  }

  // Minimum-raggedness backward DP (crosspoint algorithm).
  // dp[i] = minimum cost for lines starting at word i through to the end.
  // ans[i] = index of the last word on the optimal line starting at word i.
  //
  // Allocate dp[] and ans[] from the memory arena bump allocator when available.
  // This avoids heap fragmentation from large temporary arrays (n*4 bytes each).
  // Falls back to std::vector if arena is not initialized.
  sumi::ArenaScratch arena;
  int* dp = nullptr;
  size_t* ans = nullptr;
  std::vector<int> dpVec;
  std::vector<size_t> ansVec;

  if (arena.isValid()) {
    dp = arena.alloc<int>(n);
    ans = arena.alloc<size_t>(n);
  }
  if (!dp || !ans) {
    // Arena exhausted or not available — fall back to heap
    dp = nullptr;
    ans = nullptr;
    dpVec.resize(n);
    ansVec.resize(n);
    dp = dpVec.data();
    ans = ansVec.data();
  }

  // Base case: last word alone on a line has zero cost (last line is free)
  dp[n - 1] = 0;
  ans[n - 1] = n - 1;

  for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
    // Check for abort periodically (every 100 words)
    if (shouldAbort && (static_cast<size_t>(-i) % 100 == 0) && shouldAbort()) {
      return {};
    }

    int currlen = 0;
    dp[i] = MAX_COST;

    for (size_t j = static_cast<size_t>(i); j < n; ++j) {
      // Add space before word j unless it's the first word on the line or attaching punctuation
      const int gap = (j > static_cast<size_t>(i) && !isAttaching[j]) ? spaceWidth : 0;
      currlen += wordWidths[j] + gap;

      if (currlen > pageWidth) {
        break;
      }

      int cost;
      if (j == n - 1) {
        cost = 0;  // Last line is free
      } else {
        const int remainingSpace = pageWidth - currlen;
        // Use long long for the square to prevent overflow on ESP32
        const long long cost_ll = static_cast<long long>(remainingSpace) * remainingSpace + dp[j + 1];
        cost = (cost_ll > MAX_COST) ? MAX_COST : static_cast<int>(cost_ll);
      }

      if (cost < dp[i]) {
        dp[i] = cost;
        ans[i] = j;  // j is the index of the last word on this line
      }
    }

    // Handle oversized word: if no valid configuration found, force single-word line
    if (dp[i] == MAX_COST) {
      ans[i] = static_cast<size_t>(i);
      if (i + 1 < static_cast<int>(n)) {
        dp[i] = dp[i + 1];
      } else {
        dp[i] = 0;
      }
    }
  }

  // Forward scan to reconstruct line break indices
  std::vector<size_t> lineBreakIndices;
  size_t currentWordIndex = 0;

  while (currentWordIndex < n) {
    size_t nextBreakIndex = ans[currentWordIndex] + 1;

    // Safety check: prevent infinite loop if nextBreakIndex doesn't advance
    if (nextBreakIndex <= currentWordIndex) {
      nextBreakIndex = currentWordIndex + 1;
    }

    lineBreakIndices.push_back(nextBreakIndex);
    currentWordIndex = nextBreakIndex;
  }

  return lineBreakIndices;
  // ArenaScratch destructor resets bump watermark here — dp/ans memory is reclaimed
}

std::vector<size_t> ParsedText::computeLineBreaksGreedy(const int pageWidth, const int spaceWidth,
                                                        const std::vector<uint16_t>& wordWidths,
                                                        const AbortCallback& shouldAbort) const {
  std::vector<size_t> breaks;
  const size_t n = wordWidths.size();

  if (n == 0) {
    return breaks;
  }

  int lineWidth = -spaceWidth;  // First word won't have preceding space
  for (size_t i = 0; i < n; i++) {
    // Check for abort periodically (every 200 words)
    if (shouldAbort && (i % 200 == 0) && shouldAbort()) {
      return {};  // Return empty to signal abort
    }

    const int wordWidth = wordWidths[i];

    // Check if adding this word would overflow the line
    if (lineWidth + wordWidth + spaceWidth > pageWidth && lineWidth > 0) {
      // Start a new line at this word
      breaks.push_back(i);
      lineWidth = wordWidth;
    } else {
      lineWidth += wordWidth + spaceWidth;
    }
  }

  // Final break at end of all words
  breaks.push_back(n);
  return breaks;
}

void ParsedText::extractLine(const size_t breakIndex, const int pageWidth, const int spaceWidth,
                             const std::vector<uint16_t>& wordWidths, const std::vector<size_t>& lineBreakIndices,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine) {
  const size_t lineBreak = lineBreakIndices[breakIndex];
  const size_t lastBreakAt = breakIndex > 0 ? lineBreakIndices[breakIndex - 1] : 0;
  const size_t lineWordCount = lineBreak - lastBreakAt;

  // Calculate total word width for this line and count actual word gaps
  // (punctuation that attaches to previous word doesn't count as a gap)
  int lineWordWidthSum = 0;
  size_t actualGapCount = 0;
  auto countWordIt = words.begin();

  for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
    lineWordWidthSum += wordWidths[lastBreakAt + wordIdx];
    // Count gaps: each word after the first creates a gap, unless it's attaching punctuation
    if (wordIdx > 0 && !isAttachingPunctuationWord(*countWordIt)) {
      actualGapCount++;
    }
    ++countWordIt;
  }

  // Calculate spacing
  const int spareSpace = pageWidth - lineWordWidthSum;

  int spacing = spaceWidth;
  const bool isLastLine = breakIndex == lineBreakIndices.size() - 1;

  // For justified text, calculate spacing based on actual gap count
  if (style == TextBlock::JUSTIFIED && !isLastLine && actualGapCount >= 1) {
    spacing = spareSpace / static_cast<int>(actualGapCount);
  }

  // For RTL text, default to right alignment
  const auto effectiveStyle = (isRtl && style == TextBlock::LEFT_ALIGN) ? TextBlock::RIGHT_ALIGN : style;

  // Build WordData vector directly, consuming from front of lists
  // Punctuation that attaches to the previous word doesn't get space before it
  std::vector<TextBlock::WordData> lineData;
  lineData.reserve(lineWordCount);

  auto wordIt = words.begin();
  auto styleIt = wordStyles.begin();
  auto decoIt = wordDecorations.begin();

  if (isRtl) {
    // RTL: Position words from right to left
    uint16_t xpos;
    if (effectiveStyle == TextBlock::CENTER_ALIGN) {
      xpos = pageWidth - (spareSpace - static_cast<int>(actualGapCount) * spacing) / 2;
    } else {
      xpos = pageWidth;  // RIGHT_ALIGN and JUSTIFIED start from right edge
    }

    for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
      const uint16_t currentWordWidth = wordWidths[lastBreakAt + wordIdx];
      xpos -= currentWordWidth;
      lineData.push_back({std::move(*wordIt), xpos, *styleIt, *decoIt});

      auto nextWordIt = wordIt;
      ++nextWordIt;
      const bool nextIsAttachingPunctuation = wordIdx + 1 < lineWordCount && isAttachingPunctuationWord(*nextWordIt);
      xpos -= (nextIsAttachingPunctuation ? 0 : spacing);
      ++wordIt;
      ++styleIt;
      ++decoIt;
    }
  } else {
    // LTR: Position words from left to right
    uint16_t xpos = 0;
    if (effectiveStyle == TextBlock::RIGHT_ALIGN) {
      xpos = spareSpace - static_cast<int>(actualGapCount) * spaceWidth;
    } else if (effectiveStyle == TextBlock::CENTER_ALIGN) {
      xpos = (spareSpace - static_cast<int>(actualGapCount) * spaceWidth) / 2;
    }

    for (size_t wordIdx = 0; wordIdx < lineWordCount; wordIdx++) {
      const uint16_t currentWordWidth = wordWidths[lastBreakAt + wordIdx];
      lineData.push_back({std::move(*wordIt), xpos, *styleIt, *decoIt});

      auto nextWordIt = wordIt;
      ++nextWordIt;
      const bool nextIsAttachingPunctuation = wordIdx + 1 < lineWordCount && isAttachingPunctuationWord(*nextWordIt);
      xpos += currentWordWidth + (nextIsAttachingPunctuation ? 0 : spacing);
      ++wordIt;
      ++styleIt;
      ++decoIt;
    }
  }

  // Remove consumed elements from lists
  words.erase(words.begin(), wordIt);
  wordStyles.erase(wordStyles.begin(), styleIt);
  wordDecorations.erase(wordDecorations.begin(), decoIt);

  processLine(std::make_shared<TextBlock>(std::move(lineData), effectiveStyle));
}

// Splits words[wordIndex] into prefix + remainder when a legal breakpoint fits the available width.
// Uses Liang hyphenation (via Hyphenator) for linguistically correct break points.
// Returns true if the word was successfully split, false if no split fits.
bool ParsedText::hyphenateWordAtIndex(const size_t wordIndex, const int availableWidth, const GfxRenderer& renderer,
                                      const int fontId, std::vector<uint16_t>& wordWidths,
                                      const bool allowFallbackBreaks) {
  if (availableWidth <= 0 || wordIndex >= words.size()) {
    return false;
  }

  // Get iterators to target word and style
  auto wordIt = words.begin();
  auto styleIt = wordStyles.begin();
  auto decoIt = wordDecorations.begin();
  std::advance(wordIt, wordIndex);
  std::advance(styleIt, wordIndex);
  std::advance(decoIt, wordIndex);

  const std::string& word = *wordIt;
  const auto wordStyle = *styleIt;
  const auto wordDeco = *decoIt;

  // Collect candidate breakpoints from the Liang hyphenator
  auto breakInfos = Hyphenator::breakOffsets(word, allowFallbackBreaks);
  if (breakInfos.empty()) {
    return false;
  }

  size_t chosenOffset = 0;
  int chosenWidth = -1;
  bool chosenNeedsHyphen = true;

  // Find the widest prefix that still fits the available width
  for (const auto& info : breakInfos) {
    const size_t offset = info.byteOffset;
    if (offset == 0 || offset >= word.size()) {
      continue;
    }

    const bool needsHyphen = info.requiresInsertedHyphen;
    const int prefixWidth = measureWordWidth(renderer, fontId, word.substr(0, offset), wordStyle, needsHyphen);
    if (prefixWidth > availableWidth || prefixWidth <= chosenWidth) {
      continue;
    }

    chosenWidth = prefixWidth;
    chosenOffset = offset;
    chosenNeedsHyphen = needsHyphen;
  }

  if (chosenWidth < 0) {
    return false;
  }

  // Split the word: prefix gets a hyphen if needed, remainder is inserted after
  std::string remainder = word.substr(chosenOffset);
  wordIt->resize(chosenOffset);
  if (chosenNeedsHyphen) {
    wordIt->push_back('-');
  }

  // Insert the remainder word with matching style and decorations
  auto insertWordIt = std::next(wordIt);
  auto insertStyleIt = std::next(styleIt);
  auto insertDecoIt = std::next(decoIt);
  words.insert(insertWordIt, remainder);
  wordStyles.insert(insertStyleIt, wordStyle);
  wordDecorations.insert(insertDecoIt, wordDeco);

  // Update cached widths
  wordWidths[wordIndex] = static_cast<uint16_t>(chosenWidth);
  const uint16_t remainderWidth = measureWordWidth(renderer, fontId, remainder, wordStyle);
  wordWidths.insert(wordWidths.begin() + wordIndex + 1, remainderWidth);
  return true;
}

// Greedy line breaking with opportunistic Liang hyphenation at overflow points.
// When a word overflows the current line, we try to hyphenate it and fit the prefix.
std::vector<size_t> ParsedText::computeHyphenatedLineBreaks(const GfxRenderer& renderer, const int fontId,
                                                            const int pageWidth, const int spaceWidth,
                                                            std::vector<uint16_t>& wordWidths,
                                                            const AbortCallback& shouldAbort) {
  // Build isAttaching snapshot for O(1) lookup (may grow as words are split)
  std::vector<bool> isAttaching;
  isAttaching.reserve(wordWidths.size());
  for (auto it = words.begin(); it != words.end(); ++it) {
    isAttaching.push_back(isAttachingPunctuationWord(*it));
  }

  // Pre-split oversized words that can't fit even on an empty line
  for (size_t i = 0; i < wordWidths.size(); ++i) {
    while (wordWidths[i] > static_cast<uint16_t>(pageWidth)) {
      if (!hyphenateWordAtIndex(i, pageWidth, renderer, fontId, wordWidths, /*allowFallbackBreaks=*/true)) {
        break;
      }
      // Update isAttaching for the newly inserted remainder
      isAttaching.insert(isAttaching.begin() + i + 1, false);
    }
  }

  std::vector<size_t> lineBreakIndices;
  size_t currentIndex = 0;

  while (currentIndex < wordWidths.size()) {
    // Check for abort periodically
    if (shouldAbort && (currentIndex % 200 == 0) && shouldAbort()) {
      return {};
    }

    const size_t lineStart = currentIndex;
    int lineWidth = 0;

    // Consume as many words as possible for the current line
    while (currentIndex < wordWidths.size()) {
      const bool isFirstWord = currentIndex == lineStart;
      const int spacing = (isFirstWord || isAttaching[currentIndex]) ? 0 : spaceWidth;
      const int candidateWidth = spacing + wordWidths[currentIndex];

      // Word fits on current line
      if (lineWidth + candidateWidth <= pageWidth) {
        lineWidth += candidateWidth;
        ++currentIndex;
        continue;
      }

      // Word would overflow - try to hyphenate and fit a prefix
      const int availWidth = pageWidth - lineWidth - spacing;
      const bool fallback = isFirstWord;  // Only allow fallback for first word on line

      if (availWidth > 0 &&
          hyphenateWordAtIndex(currentIndex, availWidth, renderer, fontId, wordWidths, fallback)) {
        // Update isAttaching for the newly inserted remainder
        isAttaching.insert(isAttaching.begin() + currentIndex + 1, false);
        // Prefix fits, append it to this line and move to next line
        lineWidth += spacing + wordWidths[currentIndex];
        ++currentIndex;
        break;
      }

      // Could not split: force at least one word per line to avoid infinite loop
      if (currentIndex == lineStart) {
        lineWidth += candidateWidth;
        ++currentIndex;
      }
      break;
    }

    lineBreakIndices.push_back(currentIndex);
  }

  return lineBreakIndices;
}
