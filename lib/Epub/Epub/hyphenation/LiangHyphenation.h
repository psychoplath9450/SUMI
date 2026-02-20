#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "HyphenationCommon.h"
#include "SerializedHyphenationTrie.h"

// Encapsulates every language-specific dial the Liang algorithm needs at runtime.
struct LiangWordConfig {
  static constexpr size_t kDefaultMinPrefix = 2;
  static constexpr size_t kDefaultMinSuffix = 2;
  bool (*isLetter)(uint32_t);
  uint32_t (*toLower)(uint32_t);
  size_t minPrefix;
  size_t minSuffix;

  LiangWordConfig(bool (*letterFn)(uint32_t), uint32_t (*lowerFn)(uint32_t), size_t prefix = kDefaultMinPrefix,
                  size_t suffix = kDefaultMinSuffix)
      : isLetter(letterFn), toLower(lowerFn), minPrefix(prefix), minSuffix(suffix) {}
};

// Shared Liang pattern evaluator used by every language-specific hyphenator.
std::vector<size_t> liangBreakIndexes(const std::vector<CodepointInfo>& cps,
                                      const SerializedHyphenationPatterns& patterns, const LiangWordConfig& config);
