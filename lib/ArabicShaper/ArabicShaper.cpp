#include "ArabicShaper.h"

// Inline UTF-8 decode to avoid header dependency
static uint32_t decodeUtf8(const unsigned char** ptr) {
  if (**ptr == 0) return 0;
  unsigned char c = **ptr;
  uint32_t cp;
  int bytes;
  if (c < 0x80) {
    cp = c;
    bytes = 1;
  } else if ((c >> 5) == 0x6) {
    cp = c & 0x1F;
    bytes = 2;
  } else if ((c >> 4) == 0xE) {
    cp = c & 0x0F;
    bytes = 3;
  } else if ((c >> 3) == 0x1E) {
    cp = c & 0x07;
    bytes = 4;
  } else {
    (*ptr)++;
    return 0xFFFD;
  }
  for (int i = 1; i < bytes; i++) {
    unsigned char cont = (*ptr)[i];
    if ((cont & 0xC0) != 0x80) {
      *ptr += i;
      return 0xFFFD;
    }
    cp = (cp << 6) | (cont & 0x3F);
  }
  *ptr += bytes;
  return cp;
}

namespace ArabicShaper {

static const ArabicFormEntry* findFormEntry(uint32_t cp) {
  for (size_t i = 0; i < ARABIC_FORMS_COUNT; i++) {
    if (ARABIC_FORMS[i].base == cp) return &ARABIC_FORMS[i];
  }
  return nullptr;
}

uint32_t getContextualForm(uint32_t cp, bool prevJoins, bool nextJoins) {
  const ArabicFormEntry* entry = findFormEntry(cp);
  if (!entry) return cp;

  if (prevJoins && nextJoins && entry->medial != 0) return entry->medial;
  if (prevJoins && entry->final_ != 0) return entry->final_;
  if (nextJoins && entry->initial != 0) return entry->initial;
  if (entry->isolated != 0) return entry->isolated;
  return cp;
}

uint32_t getLamAlefLigature(uint32_t alef, bool prevJoins) {
  for (size_t i = 0; i < LAM_ALEF_COUNT; i++) {
    if (LAM_ALEF_LIGATURES[i].alef == alef) {
      return prevJoins ? LAM_ALEF_LIGATURES[i].final_ : LAM_ALEF_LIGATURES[i].isolated;
    }
  }
  return 0;
}

// Check if a joining type can join to the left (has a connection on its left side)
static bool joinsToLeft(JoiningType jt) { return jt == JoiningType::DUAL_JOINING; }

// Check if a joining type can join to the right (has a connection on its right side)
static bool joinsToRight(JoiningType jt) { return jt == JoiningType::DUAL_JOINING || jt == JoiningType::RIGHT_JOINING; }

std::vector<uint32_t> shapeText(const char* text) {
  if (!text || !*text) return {};

  // Step 1: Decode UTF-8 to codepoints
  std::vector<uint32_t> codepoints;
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(text);
  uint32_t cp;
  while ((cp = decodeUtf8(&ptr))) {
    codepoints.push_back(cp);
  }

  if (codepoints.empty()) return {};

  // Step 2: Apply Lam-Alef ligatures
  std::vector<uint32_t> afterLigatures;
  afterLigatures.reserve(codepoints.size());
  for (size_t i = 0; i < codepoints.size(); i++) {
    if (codepoints[i] == 0x0644 && i + 1 < codepoints.size()) {  // Lam
      // Look ahead past any diacritics to find the Alef
      size_t alefIdx = i + 1;
      while (alefIdx < codepoints.size() && isArabicDiacritic(codepoints[alefIdx])) {
        alefIdx++;
      }
      if (alefIdx < codepoints.size()) {
        // Check if prev char joins to Lam
        bool prevJoins = false;
        for (int p = static_cast<int>(afterLigatures.size()) - 1; p >= 0; p--) {
          JoiningType pjt = getJoiningType(afterLigatures[p]);
          if (pjt == JoiningType::TRANSPARENT) continue;
          prevJoins = joinsToLeft(pjt);
          break;
        }
        uint32_t lig = getLamAlefLigature(codepoints[alefIdx], prevJoins);
        if (lig != 0) {
          afterLigatures.push_back(lig);
          // Copy diacritics between Lam and Alef
          for (size_t d = i + 1; d < alefIdx; d++) {
            afterLigatures.push_back(codepoints[d]);
          }
          i = alefIdx;  // Skip past the Alef
          continue;
        }
      }
    }
    afterLigatures.push_back(codepoints[i]);
  }

  // Step 3: Apply contextual forms
  std::vector<uint32_t> shaped;
  shaped.reserve(afterLigatures.size());

  for (size_t i = 0; i < afterLigatures.size(); i++) {
    uint32_t c = afterLigatures[i];

    // Skip non-Arabic and already-shaped (ligature) codepoints
    if (!isArabicBaseChar(c)) {
      shaped.push_back(c);
      continue;
    }

    // Find previous non-transparent joining type
    bool prevJoins = false;
    for (int p = static_cast<int>(i) - 1; p >= 0; p--) {
      JoiningType pjt = getJoiningType(afterLigatures[p]);
      if (pjt == JoiningType::TRANSPARENT) continue;
      prevJoins = joinsToLeft(pjt);
      break;
    }

    // Find next non-transparent joining type
    bool nextJoins = false;
    for (size_t n = i + 1; n < afterLigatures.size(); n++) {
      JoiningType njt = getJoiningType(afterLigatures[n]);
      if (njt == JoiningType::TRANSPARENT) continue;
      nextJoins = joinsToRight(njt);
      break;
    }

    shaped.push_back(getContextualForm(c, prevJoins, nextJoins));
  }

  // Step 4: Reverse for visual order (RTL to LTR rendering)
  std::vector<uint32_t> visual;
  visual.reserve(shaped.size());
  for (int i = static_cast<int>(shaped.size()) - 1; i >= 0; i--) {
    visual.push_back(shaped[i]);
  }

  return visual;
}

}  // namespace ArabicShaper
