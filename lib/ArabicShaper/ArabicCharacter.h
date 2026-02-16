#pragma once
#include <cstdint>

namespace ArabicShaper {

enum class JoiningType : uint8_t {
  NON_JOINING,    // Does not join (e.g. non-Arabic chars)
  RIGHT_JOINING,  // Joins only to the right (Alef, Dal, Thal, Ra, Zain, Waw, Teh Marbuta)
  DUAL_JOINING,   // Joins on both sides (most letters)
  TRANSPARENT     // Diacritics - don't affect joining
};

JoiningType getJoiningType(uint32_t cp);
bool isArabicBaseChar(uint32_t cp);
bool isArabicDiacritic(uint32_t cp);

}  // namespace ArabicShaper
