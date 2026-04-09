#include "Hyphenator.h"

#include <algorithm>
#include <vector>

#include "HyphenationCommon.h"
#include "LanguageRegistry.h"

const LanguageHyphenator* Hyphenator::cachedHyphenator_ = nullptr;

namespace {

// ISO 639-2 (three-letter) to ISO 639-1 (two-letter) mapping.
// EPUBs may use either form in dc:language metadata (CrossPoint #1037).
struct Iso639Mapping {
  const char* iso639_2;
  const char* iso639_1;
};
static constexpr Iso639Mapping kIso639Mappings[] = {
    {"eng", "en"}, {"fra", "fr"}, {"fre", "fr"}, {"deu", "de"}, {"ger", "de"},
    {"rus", "ru"}, {"spa", "es"}, {"ita", "it"}, {"ukr", "uk"},
};

// Maps a BCP-47 or ISO 639-2 language tag to a language-specific hyphenator.
const LanguageHyphenator* hyphenatorForLanguage(const std::string& langTag) {
  if (langTag.empty()) return nullptr;

  // Extract primary subtag and normalize to lowercase (e.g., "en-US" -> "en").
  std::string primary;
  primary.reserve(langTag.size());
  for (char c : langTag) {
    if (c == '-' || c == '_') break;
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    primary.push_back(c);
  }
  if (primary.empty()) return nullptr;

  // Normalize ISO 639-2 three-letter codes to two-letter equivalents.
  for (const auto& mapping : kIso639Mappings) {
    if (primary == mapping.iso639_2) {
      primary = mapping.iso639_1;
      break;
    }
  }

  return getLanguageHyphenatorForPrimaryTag(primary);
}

// Maps a codepoint index back to its byte offset inside the source word.
size_t byteOffsetForIndex(const std::vector<CodepointInfo>& cps, const size_t index) {
  return (index < cps.size()) ? cps[index].byteOffset : (cps.empty() ? 0 : cps.back().byteOffset);
}

// Builds a vector of break information from explicit hyphen markers in the given codepoints.
std::vector<Hyphenator::BreakInfo> buildExplicitBreakInfos(const std::vector<CodepointInfo>& cps) {
  std::vector<Hyphenator::BreakInfo> breaks;

  for (size_t i = 1; i + 1 < cps.size(); ++i) {
    const uint32_t cp = cps[i].value;
    if (!isExplicitHyphen(cp) || !isAlphabetic(cps[i - 1].value) || !isAlphabetic(cps[i + 1].value)) {
      continue;
    }
    breaks.push_back({cps[i + 1].byteOffset, isSoftHyphen(cp)});
  }

  return breaks;
}

// Checks if a codepoint is a segment separator (hyphen or apostrophe).
// Used to split compound words into segments for per-segment hyphenation.
bool isSegmentSeparator(const uint32_t cp) { return isExplicitHyphen(cp) || isApostrophe(cp); }

// Runs language-specific hyphenation on each alphabetic segment between separators.
// e.g. "US-Satellitensystems" → segments ["US", "Satellitensystems"], each hyphenated independently.
void appendSegmentPatternBreaks(const std::vector<CodepointInfo>& cps, const LanguageHyphenator& hyphenator,
                                const bool includeFallback, std::vector<Hyphenator::BreakInfo>& outBreaks) {
  size_t segStart = 0;

  for (size_t i = 0; i <= cps.size(); ++i) {
    const bool atEnd = i == cps.size();
    const bool atSeparator = !atEnd && isSegmentSeparator(cps[i].value);
    if (!atEnd && !atSeparator) continue;

    if (i > segStart) {
      std::vector<CodepointInfo> segment(cps.begin() + segStart, cps.begin() + i);
      auto segIndexes = hyphenator.breakIndexes(segment);

      if (includeFallback && segIndexes.empty()) {
        const size_t minPrefix = hyphenator.minPrefix();
        const size_t minSuffix = hyphenator.minSuffix();
        for (size_t idx = minPrefix; idx + minSuffix <= segment.size(); ++idx) {
          segIndexes.push_back(idx);
        }
      }

      for (const size_t idx : segIndexes) {
        if (idx == 0 || idx >= segment.size()) continue;
        const size_t cpIdx = segStart + idx;
        if (cpIdx < cps.size()) {
          outBreaks.push_back({cps[cpIdx].byteOffset, true});
        }
      }
    }

    segStart = i + 1;
  }
}

// Adds break points at apostrophe positions in contractions, but only when both sides
// are long enough to avoid stranding short clitics (CrossPoint #1318, #1405).
// e.g. "don't" → no break (right segment "t" too short), "all'improvviso" → break after apostrophe.
void appendApostropheContractionBreaks(const std::vector<CodepointInfo>& cps,
                                       std::vector<Hyphenator::BreakInfo>& outBreaks) {
  constexpr size_t kMinLeftSegmentLen = 3;
  constexpr size_t kMinRightSegmentLen = 3;
  size_t segmentStart = 0;

  for (size_t i = 0; i < cps.size(); ++i) {
    if (isSegmentSeparator(cps[i].value)) {
      if (isApostrophe(cps[i].value) && i > 0 && i + 1 < cps.size() &&
          isAlphabetic(cps[i - 1].value) && isAlphabetic(cps[i + 1].value)) {
        size_t leftLen = 0;
        for (size_t j = segmentStart; j < i; ++j) {
          if (isAlphabetic(cps[j].value)) ++leftLen;
        }
        size_t rightLen = 0;
        for (size_t j = i + 1; j < cps.size() && !isSegmentSeparator(cps[j].value); ++j) {
          if (isAlphabetic(cps[j].value)) ++rightLen;
        }
        if (leftLen >= kMinLeftSegmentLen && rightLen >= kMinRightSegmentLen) {
          outBreaks.push_back({cps[i + 1].byteOffset, false});
        }
      }
      segmentStart = i + 1;
    }
  }
}

void sortAndDedupeBreakInfos(std::vector<Hyphenator::BreakInfo>& infos) {
  std::sort(infos.begin(), infos.end(), [](const Hyphenator::BreakInfo& a, const Hyphenator::BreakInfo& b) {
    if (a.byteOffset != b.byteOffset) return a.byteOffset < b.byteOffset;
    return a.requiresInsertedHyphen < b.requiresInsertedHyphen;
  });
  infos.erase(std::unique(infos.begin(), infos.end(),
                          [](const Hyphenator::BreakInfo& a, const Hyphenator::BreakInfo& b) {
                            return a.byteOffset == b.byteOffset;
                          }),
              infos.end());
}

}  // namespace

std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string& word, const bool includeFallback) {
  if (word.empty()) {
    return {};
  }

  auto cps = collectCodepoints(word);
  trimSurroundingPunctuationAndFootnote(cps);
  const auto* hyphenator = cachedHyphenator_;

  // Detect apostrophe-like separators early (CrossPoint #1318).
  bool hasApostrophe = false;
  for (const auto& cp : cps) {
    if (isApostrophe(cp.value)) {
      hasApostrophe = true;
      break;
    }
  }

  // Explicit hyphen markers (soft or hard) take precedence over language breaks.
  auto explicitBreakInfos = buildExplicitBreakInfos(cps);
  if (!explicitBreakInfos.empty()) {
    // Also run patterns on each alphabetic segment between explicit hyphens,
    // so "US-Satellitensystems" can break within "Satellitensystems".
    if (hyphenator) {
      appendSegmentPatternBreaks(cps, *hyphenator, false, explicitBreakInfos);
    }
    if (hasApostrophe) {
      appendApostropheContractionBreaks(cps, explicitBreakInfos);
    }
    sortAndDedupeBreakInfos(explicitBreakInfos);
    return explicitBreakInfos;
  }

  // Apostrophe compounds: split into segments and hyphenate each independently.
  if (hasApostrophe) {
    std::vector<BreakInfo> segmentedBreaks;
    if (hyphenator) {
      appendSegmentPatternBreaks(cps, *hyphenator, includeFallback, segmentedBreaks);
    }
    appendApostropheContractionBreaks(cps, segmentedBreaks);
    sortAndDedupeBreakInfos(segmentedBreaks);
    return segmentedBreaks;
  }

  // Ask language hyphenator for legal break points.
  std::vector<size_t> indexes;
  if (hyphenator) {
    indexes = hyphenator->breakIndexes(cps);
  }

  // Only add fallback breaks if needed
  if (includeFallback && indexes.empty()) {
    const size_t minPrefix = hyphenator ? hyphenator->minPrefix() : LiangWordConfig::kDefaultMinPrefix;
    const size_t minSuffix = hyphenator ? hyphenator->minSuffix() : LiangWordConfig::kDefaultMinSuffix;
    for (size_t idx = minPrefix; idx + minSuffix <= cps.size(); ++idx) {
      indexes.push_back(idx);
    }
  }

  if (indexes.empty()) {
    return {};
  }

  std::vector<Hyphenator::BreakInfo> breaks;
  breaks.reserve(indexes.size());
  for (const size_t idx : indexes) {
    breaks.push_back({byteOffsetForIndex(cps, idx), true});
  }

  return breaks;
}

void Hyphenator::setPreferredLanguage(const std::string& lang) { cachedHyphenator_ = hyphenatorForLanguage(lang); }
