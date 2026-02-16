#include "ArabicCharacter.h"

namespace ArabicShaper {

bool isArabicDiacritic(uint32_t cp) { return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670; }

bool isArabicBaseChar(uint32_t cp) { return cp >= 0x0621 && cp <= 0x064A && !isArabicDiacritic(cp); }

JoiningType getJoiningType(uint32_t cp) {
  if (isArabicDiacritic(cp)) return JoiningType::TRANSPARENT;

  // Right-joining only characters
  switch (cp) {
    case 0x0622:  // Alef with Madda
    case 0x0623:  // Alef with Hamza Above
    case 0x0624:  // Waw with Hamza Above
    case 0x0625:  // Alef with Hamza Below
    case 0x0627:  // Alef
    case 0x0629:  // Teh Marbuta
    case 0x062F:  // Dal
    case 0x0630:  // Thal
    case 0x0631:  // Ra
    case 0x0632:  // Zain
    case 0x0648:  // Waw
    // Lam-Alef ligatures (Presentation Forms-B) - right-joining
    case 0xFEF5:  // Lam+Alef Madda isolated
    case 0xFEF6:  // Lam+Alef Madda final
    case 0xFEF7:  // Lam+Alef Hamza Above isolated
    case 0xFEF8:  // Lam+Alef Hamza Above final
    case 0xFEF9:  // Lam+Alef Hamza Below isolated
    case 0xFEFA:  // Lam+Alef Hamza Below final
    case 0xFEFB:  // Lam+Alef isolated
    case 0xFEFC:  // Lam+Alef final
      return JoiningType::RIGHT_JOINING;
    default:
      break;
  }

  // Dual-joining Arabic letters
  if (isArabicBaseChar(cp) && cp != 0x0621) {  // Hamza is non-joining
    return JoiningType::DUAL_JOINING;
  }

  // Hamza (isolated only)
  if (cp == 0x0621) return JoiningType::NON_JOINING;

  return JoiningType::NON_JOINING;
}

}  // namespace ArabicShaper
