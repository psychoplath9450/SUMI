#pragma once

#include <cstdint>
#include <vector>

#include "ArabicCharacter.h"
#include "ArabicShapingTables.h"

namespace ArabicShaper {

// Shape Arabic text: apply contextual forms and Lam-Alef ligatures.
// Input: logical order UTF-8 string
// Output: visual order (reversed) shaped codepoints ready for left-to-right rendering
std::vector<uint32_t> shapeText(const char* text);

// Get the contextual form for a base Arabic codepoint.
// prevJoins: whether the previous character can join to this one
// nextJoins: whether the next character can join from this one
uint32_t getContextualForm(uint32_t cp, bool prevJoins, bool nextJoins);

// Check for Lam-Alef ligature. Returns ligature codepoint or 0 if not a ligature pair.
// prevJoins: whether the character before Lam can join
uint32_t getLamAlefLigature(uint32_t alef, bool prevJoins);

}  // namespace ArabicShaper
