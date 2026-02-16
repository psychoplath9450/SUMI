#include "test_utils.h"

#include <cstdint>
#include <vector>

#include "ArabicCharacter.h"
#include "ArabicShaper.h"
#include "ArabicShapingTables.h"

using namespace ArabicShaper;

int main() {
  TestUtils::TestRunner runner("Arabic Shaper");

  // ============================================
  // Character classification tests
  // ============================================

  // Test 1: Diacritics (harakat)
  runner.expectTrue(isArabicDiacritic(0x064B), "diacritic: fathatan U+064B");
  runner.expectTrue(isArabicDiacritic(0x0650), "diacritic: kasra U+0650");
  runner.expectTrue(isArabicDiacritic(0x065F), "diacritic: wavy hamza U+065F");
  runner.expectTrue(isArabicDiacritic(0x0670), "diacritic: superscript alef U+0670");
  runner.expectFalse(isArabicDiacritic(0x0628), "diacritic: Beh is not diacritic");
  runner.expectFalse(isArabicDiacritic(0x0041), "diacritic: ASCII 'A' is not diacritic");

  // Test 2: Base characters
  runner.expectTrue(isArabicBaseChar(0x0621), "baseChar: Hamza U+0621");
  runner.expectTrue(isArabicBaseChar(0x0628), "baseChar: Beh U+0628");
  runner.expectTrue(isArabicBaseChar(0x064A), "baseChar: Yeh U+064A");
  runner.expectFalse(isArabicBaseChar(0x064B), "baseChar: diacritic not base");
  runner.expectFalse(isArabicBaseChar(0x0041), "baseChar: ASCII not base");

  // Test 3: Joining types
  runner.expectTrue(getJoiningType(0x0621) == JoiningType::NON_JOINING, "joining: Hamza is NON_JOINING");
  runner.expectTrue(getJoiningType(0x0627) == JoiningType::RIGHT_JOINING, "joining: Alef is RIGHT_JOINING");
  runner.expectTrue(getJoiningType(0x062F) == JoiningType::RIGHT_JOINING, "joining: Dal is RIGHT_JOINING");
  runner.expectTrue(getJoiningType(0x0631) == JoiningType::RIGHT_JOINING, "joining: Ra is RIGHT_JOINING");
  runner.expectTrue(getJoiningType(0x0648) == JoiningType::RIGHT_JOINING, "joining: Waw is RIGHT_JOINING");
  runner.expectTrue(getJoiningType(0x0628) == JoiningType::DUAL_JOINING, "joining: Beh is DUAL_JOINING");
  runner.expectTrue(getJoiningType(0x0633) == JoiningType::DUAL_JOINING, "joining: Seen is DUAL_JOINING");
  runner.expectTrue(getJoiningType(0x064A) == JoiningType::DUAL_JOINING, "joining: Yeh is DUAL_JOINING");
  runner.expectTrue(getJoiningType(0x064B) == JoiningType::TRANSPARENT, "joining: diacritic is TRANSPARENT");
  runner.expectTrue(getJoiningType(0x0041) == JoiningType::NON_JOINING, "joining: ASCII is NON_JOINING");

  // ============================================
  // Contextual form tests
  // ============================================

  // Test 4: Isolated form (no neighbors join)
  {
    uint32_t form = getContextualForm(0x0628, false, false);  // Beh isolated
    runner.expectEq(static_cast<uint32_t>(0xFE8F), form, "contextual: Beh isolated");
  }

  // Test 5: Initial form (next joins, prev doesn't)
  {
    uint32_t form = getContextualForm(0x0628, false, true);  // Beh initial
    runner.expectEq(static_cast<uint32_t>(0xFE91), form, "contextual: Beh initial");
  }

  // Test 6: Medial form (both join)
  {
    uint32_t form = getContextualForm(0x0628, true, true);  // Beh medial
    runner.expectEq(static_cast<uint32_t>(0xFE92), form, "contextual: Beh medial");
  }

  // Test 7: Final form (prev joins, next doesn't)
  {
    uint32_t form = getContextualForm(0x0628, true, false);  // Beh final
    runner.expectEq(static_cast<uint32_t>(0xFE90), form, "contextual: Beh final");
  }

  // Test 8: Right-joining char only has isolated and final
  {
    uint32_t isolated = getContextualForm(0x0627, false, false);  // Alef isolated
    runner.expectEq(static_cast<uint32_t>(0xFE8D), isolated, "contextual: Alef isolated");

    uint32_t final_ = getContextualForm(0x0627, true, false);  // Alef final
    runner.expectEq(static_cast<uint32_t>(0xFE8E), final_, "contextual: Alef final");

    // Alef has no initial/medial - should fall back to isolated/final
    uint32_t noInitial = getContextualForm(0x0627, false, true);
    runner.expectEq(static_cast<uint32_t>(0xFE8D), noInitial, "contextual: Alef no initial form");
  }

  // Test 9: Non-Arabic codepoint returns unchanged
  {
    uint32_t form = getContextualForm(0x0041, true, true);  // ASCII 'A'
    runner.expectEq(static_cast<uint32_t>(0x0041), form, "contextual: non-Arabic unchanged");
  }

  // ============================================
  // Lam-Alef ligature tests
  // ============================================

  // Test 10: Lam + Alef isolated
  {
    uint32_t lig = getLamAlefLigature(0x0627, false);  // Alef, no prev join
    runner.expectEq(static_cast<uint32_t>(0xFEFB), lig, "lamAlef: Lam+Alef isolated");
  }

  // Test 11: Lam + Alef final (prev joins)
  {
    uint32_t lig = getLamAlefLigature(0x0627, true);
    runner.expectEq(static_cast<uint32_t>(0xFEFC), lig, "lamAlef: Lam+Alef final");
  }

  // Test 12: Lam + Alef Madda
  {
    uint32_t lig = getLamAlefLigature(0x0622, false);
    runner.expectEq(static_cast<uint32_t>(0xFEF5), lig, "lamAlef: Lam+AlefMadda isolated");
  }

  // Test 13: Lam + Alef Hamza Above
  {
    uint32_t lig = getLamAlefLigature(0x0623, false);
    runner.expectEq(static_cast<uint32_t>(0xFEF7), lig, "lamAlef: Lam+AlefHamzaAbove isolated");
  }

  // Test 14: Non-Alef returns 0
  {
    uint32_t lig = getLamAlefLigature(0x0628, false);  // Beh is not Alef
    runner.expectEq(static_cast<uint32_t>(0), lig, "lamAlef: non-Alef returns 0");
  }

  // ============================================
  // Full shapeText() tests
  // ============================================

  // Test 15: Empty/null input
  {
    auto result = shapeText(nullptr);
    runner.expectTrue(result.empty(), "shapeText: null returns empty");

    auto result2 = shapeText("");
    runner.expectTrue(result2.empty(), "shapeText: empty returns empty");
  }

  // Test 16: Single isolated character (Beh)
  {
    // U+0628 Beh = UTF-8: 0xD8 0xA8
    auto result = shapeText("\xD8\xA8");
    runner.expectEq(static_cast<size_t>(1), result.size(), "shapeText: single char count");
    runner.expectEq(static_cast<uint32_t>(0xFE8F), result[0], "shapeText: Beh isolated");
  }

  // Test 17: Two connected letters (Ba-Alef = "ba")
  // Logical: Beh(0x0628) + Alef(0x0627) → visual (reversed): Alef-final, Beh-initial
  {
    auto result = shapeText("\xD8\xA8\xD8\xA7");  // Beh + Alef
    runner.expectEq(static_cast<size_t>(2), result.size(), "shapeText: two chars count");
    // Visual order is reversed: Alef comes first (was last in logical order)
    runner.expectEq(static_cast<uint32_t>(0xFE8E), result[0], "shapeText: Alef final (visual first)");
    runner.expectEq(static_cast<uint32_t>(0xFE91), result[1], "shapeText: Beh initial (visual second)");
  }

  // Test 18: Three-letter word with medial form
  // Logical: Beh(0628) + Seen(0633) + Meem(0645)
  // Joining: Beh(D) + Seen(D) + Meem(D) → initial + medial + final (isolated end)
  // Wait: Meem is last so nextJoins=false → Meem is final? No, there's nothing after.
  // Beh: prev=none, next=Seen(D) → initial
  // Seen: prev=Beh(D)→joinsLeft=true, next=Meem(D)→joinsRight=true → medial
  // Meem: prev=Seen(D)→joinsLeft=true, next=none → final
  // Visual (reversed): Meem-final, Seen-medial, Beh-initial
  {
    auto result = shapeText("\xD8\xA8\xD8\xB3\xD9\x85");  // Beh + Seen + Meem
    runner.expectEq(static_cast<size_t>(3), result.size(), "shapeText: three chars count");
    runner.expectEq(static_cast<uint32_t>(0xFEE2), result[0], "shapeText: Meem final");
    runner.expectEq(static_cast<uint32_t>(0xFEB4), result[1], "shapeText: Seen medial");
    runner.expectEq(static_cast<uint32_t>(0xFE91), result[2], "shapeText: Beh initial");
  }

  // Test 19: Lam-Alef ligature in shapeText
  // Logical: Lam(0644) + Alef(0627) → single ligature codepoint
  {
    auto result = shapeText("\xD9\x84\xD8\xA7");  // Lam + Alef
    runner.expectEq(static_cast<size_t>(1), result.size(), "shapeText: LamAlef ligature count");
    runner.expectEq(static_cast<uint32_t>(0xFEFB), result[0], "shapeText: LamAlef isolated");
  }

  // Test 20: Lam-Alef with preceding letter (final form)
  // Logical: Beh(0628) + Lam(0644) + Alef(0627)
  // Beh is dual-joining, so it joins left → LamAlef gets final form
  // Beh: prev=none, next=LamAlef-ligature... After ligature, Beh has next joining
  // Result should be: Beh-initial + LamAlef-final
  {
    auto result = shapeText("\xD8\xA8\xD9\x84\xD8\xA7");  // Beh + Lam + Alef
    runner.expectEq(static_cast<size_t>(2), result.size(), "shapeText: Beh+LamAlef count");
    runner.expectEq(static_cast<uint32_t>(0xFEFC), result[0], "shapeText: LamAlef final (visual first)");
    runner.expectEq(static_cast<uint32_t>(0xFE91), result[1], "shapeText: Beh initial (visual second)");
  }

  // Test 21: Non-joining character (Hamza)
  {
    auto result = shapeText("\xD8\xA1");  // Hamza U+0621
    runner.expectEq(static_cast<size_t>(1), result.size(), "shapeText: Hamza count");
    runner.expectEq(static_cast<uint32_t>(0xFE80), result[0], "shapeText: Hamza isolated");
  }

  // Test 22: ASCII passes through unchanged (reversed)
  {
    auto result = shapeText("AB");
    runner.expectEq(static_cast<size_t>(2), result.size(), "shapeText: ASCII count");
    runner.expectEq(static_cast<uint32_t>('B'), result[0], "shapeText: B first (reversed)");
    runner.expectEq(static_cast<uint32_t>('A'), result[1], "shapeText: A second (reversed)");
  }

  // Test 23: Diacritics preserved
  // Logical: Beh(0628) + Fatha(064E) + Alef(0627)
  // Fatha is transparent, doesn't affect joining
  {
    auto result = shapeText("\xD8\xA8\xD9\x8E\xD8\xA7");  // Beh + Fatha + Alef
    runner.expectEq(static_cast<size_t>(3), result.size(), "shapeText: diacritic preserved count");
    // Visual: Alef-final, Fatha, Beh-initial
    runner.expectEq(static_cast<uint32_t>(0xFE8E), result[0], "shapeText: Alef final with diacritic");
    runner.expectEq(static_cast<uint32_t>(0x064E), result[1], "shapeText: Fatha preserved");
    runner.expectEq(static_cast<uint32_t>(0xFE91), result[2], "shapeText: Beh initial with diacritic");
  }

  return runner.allPassed() ? 0 : 1;
}
