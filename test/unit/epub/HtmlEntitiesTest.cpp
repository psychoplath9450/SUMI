#include "test_utils.h"

#include <htmlEntities.h>

#include <cstring>

int main() {
  TestUtils::TestRunner runner("HtmlEntities Tests");

  // ============================================
  // Basic lookups
  // ============================================

  // Test 1: First entry in the table (AElig)
  {
    const char* result = lookupHtmlEntity("AElig", 5);
    runner.expectTrue(result != nullptr, "first_entry: AElig found");
    runner.expectEqual(std::string("\xC3\x86"), std::string(result), "first_entry: AElig correct UTF-8");
  }

  // Test 2: Last entry in the table (zwnj)
  {
    const char* result = lookupHtmlEntity("zwnj", 4);
    runner.expectTrue(result != nullptr, "last_entry: zwnj found");
    runner.expectEqual(std::string("\xE2\x80\x8C"), std::string(result), "last_entry: zwnj correct UTF-8");
  }

  // Test 3: Common entity - nbsp
  {
    const char* result = lookupHtmlEntity("nbsp", 4);
    runner.expectTrue(result != nullptr, "nbsp: found");
    runner.expectEqual(std::string("\xC2\xA0"), std::string(result), "nbsp: correct UTF-8");
  }

  // Test 4: Common entity - mdash
  {
    const char* result = lookupHtmlEntity("mdash", 5);
    runner.expectTrue(result != nullptr, "mdash: found");
    runner.expectEqual(std::string("\xE2\x80\x94"), std::string(result), "mdash: correct UTF-8");
  }

  // Test 5: Common entity - ndash
  {
    const char* result = lookupHtmlEntity("ndash", 5);
    runner.expectTrue(result != nullptr, "ndash: found");
    runner.expectEqual(std::string("\xE2\x80\x93"), std::string(result), "ndash: correct UTF-8");
  }

  // ============================================
  // Case sensitivity
  // ============================================

  // Test 6: Case matters - "Mu" vs "mu"
  {
    const char* upper = lookupHtmlEntity("Mu", 2);
    const char* lower = lookupHtmlEntity("mu", 2);
    runner.expectTrue(upper != nullptr, "case_Mu: uppercase found");
    runner.expectTrue(lower != nullptr, "case_mu: lowercase found");
    runner.expectTrue(strcmp(upper, lower) != 0, "case_Mu_mu: different UTF-8 values");
    runner.expectEqual(std::string("\xCE\x9C"), std::string(upper), "case_Mu: correct (Greek capital Mu)");
    runner.expectEqual(std::string("\xCE\xBC"), std::string(lower), "case_mu: correct (Greek small mu)");
  }

  // Test 7: Case matters - "ETH" vs "eth"
  {
    const char* upper = lookupHtmlEntity("ETH", 3);
    const char* lower = lookupHtmlEntity("eth", 3);
    runner.expectTrue(upper != nullptr, "case_ETH: found");
    runner.expectTrue(lower != nullptr, "case_eth: found");
    runner.expectTrue(strcmp(upper, lower) != 0, "case_ETH_eth: different values");
  }

  // ============================================
  // Not found / edge cases
  // ============================================

  // Test 8: Unknown entity returns nullptr
  {
    const char* result = lookupHtmlEntity("foobar", 6);
    runner.expectTrue(result == nullptr, "unknown: returns nullptr");
  }

  // Test 9: Empty name returns nullptr
  {
    const char* result = lookupHtmlEntity("", 0);
    runner.expectTrue(result == nullptr, "empty: returns nullptr");
  }

  // Test 10: Single character that doesn't match
  {
    const char* result = lookupHtmlEntity("z", 1);
    runner.expectTrue(result == nullptr, "single_z: returns nullptr");
  }

  // Test 11: Prefix of a valid entity should NOT match (e.g., "nbs" for "nbsp")
  {
    const char* result = lookupHtmlEntity("nbs", 3);
    runner.expectTrue(result == nullptr, "prefix_nbs: not found (not nbsp)");
  }

  // Test 12: Prefix "sup" should not match "sup1", "sup2", or "sup3"
  {
    const char* result = lookupHtmlEntity("sup", 3);
    runner.expectTrue(result == nullptr, "prefix_sup: not found (not sup1/2/3)");
  }

  // Test 13: "sup1" should match
  {
    const char* result = lookupHtmlEntity("sup1", 4);
    runner.expectTrue(result != nullptr, "sup1: found");
    runner.expectEqual(std::string("\xC2\xB9"), std::string(result), "sup1: correct UTF-8");
  }

  // Test 14: Longer string that starts with valid entity name
  // lookupHtmlEntity("nbspx", 5) should not match "nbsp" since nameLen is 5
  {
    const char* result = lookupHtmlEntity("nbspx", 5);
    runner.expectTrue(result == nullptr, "longer_nbspx: not found");
  }

  // ============================================
  // nameLen parameter correctness
  // ============================================

  // Test 15: nameLen shorter than actual string — should match substring
  // "nbsp..." with len=4 should find "nbsp"
  {
    const char* result = lookupHtmlEntity("nbspextra", 4);
    runner.expectTrue(result != nullptr, "namelen_short: finds nbsp with len=4");
    runner.expectEqual(std::string("\xC2\xA0"), std::string(result), "namelen_short: correct UTF-8");
  }

  // Test 16: nameLen=1 for a valid 1-char entity doesn't exist, returns nullptr
  {
    const char* result = lookupHtmlEntity("a", 1);
    runner.expectTrue(result == nullptr, "single_a: no 1-char entity 'a'");
  }

  // ============================================
  // Verify a sampling of entities across the table
  // ============================================

  // Test 17: euro
  {
    const char* result = lookupHtmlEntity("euro", 4);
    runner.expectTrue(result != nullptr, "euro: found");
    runner.expectEqual(std::string("\xE2\x82\xAC"), std::string(result), "euro: correct UTF-8");
  }

  // Test 18: trade
  {
    const char* result = lookupHtmlEntity("trade", 5);
    runner.expectTrue(result != nullptr, "trade: found");
    runner.expectEqual(std::string("\xE2\x84\xA2"), std::string(result), "trade: correct UTF-8");
  }

  // Test 19: ldquo / rdquo
  {
    const char* l = lookupHtmlEntity("ldquo", 5);
    const char* r = lookupHtmlEntity("rdquo", 5);
    runner.expectTrue(l != nullptr, "ldquo: found");
    runner.expectTrue(r != nullptr, "rdquo: found");
    runner.expectEqual(std::string("\xE2\x80\x9C"), std::string(l), "ldquo: correct");
    runner.expectEqual(std::string("\xE2\x80\x9D"), std::string(r), "rdquo: correct");
  }

  // Test 20: lsquo / rsquo
  {
    const char* l = lookupHtmlEntity("lsquo", 5);
    const char* r = lookupHtmlEntity("rsquo", 5);
    runner.expectTrue(l != nullptr, "lsquo: found");
    runner.expectTrue(r != nullptr, "rsquo: found");
    runner.expectEqual(std::string("\xE2\x80\x98"), std::string(l), "lsquo: correct");
    runner.expectEqual(std::string("\xE2\x80\x99"), std::string(r), "rsquo: correct");
  }

  // Test 21: hellip
  {
    const char* result = lookupHtmlEntity("hellip", 6);
    runner.expectTrue(result != nullptr, "hellip: found");
    runner.expectEqual(std::string("\xE2\x80\xA6"), std::string(result), "hellip: correct");
  }

  // Test 22: copy / reg
  {
    const char* c = lookupHtmlEntity("copy", 4);
    const char* r = lookupHtmlEntity("reg", 3);
    runner.expectTrue(c != nullptr, "copy: found");
    runner.expectTrue(r != nullptr, "reg: found");
    runner.expectEqual(std::string("\xC2\xA9"), std::string(c), "copy: correct");
    runner.expectEqual(std::string("\xC2\xAE"), std::string(r), "reg: correct");
  }

  // Test 23: bull (bullet)
  {
    const char* result = lookupHtmlEntity("bull", 4);
    runner.expectTrue(result != nullptr, "bull: found");
    runner.expectEqual(std::string("\xE2\x80\xA2"), std::string(result), "bull: correct");
  }

  // Test 24: OElig / oelig (both cases exist)
  {
    const char* upper = lookupHtmlEntity("OElig", 5);
    const char* lower = lookupHtmlEntity("oelig", 5);
    runner.expectTrue(upper != nullptr, "OElig: found");
    runner.expectTrue(lower != nullptr, "oelig: found");
    runner.expectEqual(std::string("\xC5\x92"), std::string(upper), "OElig: correct");
    runner.expectEqual(std::string("\xC5\x93"), std::string(lower), "oelig: correct");
  }

  // Test 25: Zero-width characters (zwj, zwnj, lrm, rlm)
  {
    runner.expectTrue(lookupHtmlEntity("zwj", 3) != nullptr, "zwj: found");
    runner.expectTrue(lookupHtmlEntity("zwnj", 4) != nullptr, "zwnj: found");
    runner.expectTrue(lookupHtmlEntity("lrm", 3) != nullptr, "lrm: found");
    runner.expectTrue(lookupHtmlEntity("rlm", 3) != nullptr, "rlm: found");
  }

  // ============================================
  // Currency entities
  // ============================================

  // Test 26: pound
  {
    const char* result = lookupHtmlEntity("pound", 5);
    runner.expectTrue(result != nullptr, "pound: found");
    runner.expectEqual(std::string("\xC2\xA3"), std::string(result), "pound: correct UTF-8");
  }

  // Test 27: yen
  {
    const char* result = lookupHtmlEntity("yen", 3);
    runner.expectTrue(result != nullptr, "yen: found");
    runner.expectEqual(std::string("\xC2\xA5"), std::string(result), "yen: correct UTF-8");
  }

  // Test 28: curren
  {
    const char* result = lookupHtmlEntity("curren", 6);
    runner.expectTrue(result != nullptr, "curren: found");
    runner.expectEqual(std::string("\xC2\xA4"), std::string(result), "curren: correct UTF-8");
  }

  // ============================================
  // Math entities
  // ============================================

  // Test 29: times
  {
    const char* result = lookupHtmlEntity("times", 5);
    runner.expectTrue(result != nullptr, "times: found");
    runner.expectEqual(std::string("\xC3\x97"), std::string(result), "times: correct UTF-8");
  }

  // Test 30: divide
  {
    const char* result = lookupHtmlEntity("divide", 6);
    runner.expectTrue(result != nullptr, "divide: found");
    runner.expectEqual(std::string("\xC3\xB7"), std::string(result), "divide: correct UTF-8");
  }

  // Test 31: plusmn
  {
    const char* result = lookupHtmlEntity("plusmn", 6);
    runner.expectTrue(result != nullptr, "plusmn: found");
    runner.expectEqual(std::string("\xC2\xB1"), std::string(result), "plusmn: correct UTF-8");
  }

  // ============================================
  // Arrow entities
  // ============================================

  // Test 32: arrows (larr, rarr, uarr, darr)
  {
    const char* l = lookupHtmlEntity("larr", 4);
    const char* r = lookupHtmlEntity("rarr", 4);
    const char* u = lookupHtmlEntity("uarr", 4);
    const char* d = lookupHtmlEntity("darr", 4);
    runner.expectTrue(l != nullptr, "larr: found");
    runner.expectTrue(r != nullptr, "rarr: found");
    runner.expectTrue(u != nullptr, "uarr: found");
    runner.expectTrue(d != nullptr, "darr: found");
    runner.expectEqual(std::string("\xE2\x86\x90"), std::string(l), "larr: correct UTF-8");
    runner.expectEqual(std::string("\xE2\x86\x92"), std::string(r), "rarr: correct UTF-8");
    runner.expectEqual(std::string("\xE2\x86\x91"), std::string(u), "uarr: correct UTF-8");
    runner.expectEqual(std::string("\xE2\x86\x93"), std::string(d), "darr: correct UTF-8");
  }

  // ============================================
  // Greek letter entities
  // ============================================

  // Test 33: lowercase Greek (alpha, beta, gamma, delta, pi)
  {
    const char* a = lookupHtmlEntity("alpha", 5);
    const char* b = lookupHtmlEntity("beta", 4);
    const char* g = lookupHtmlEntity("gamma", 5);
    const char* d = lookupHtmlEntity("delta", 5);
    const char* p = lookupHtmlEntity("pi", 2);
    runner.expectTrue(a != nullptr, "alpha: found");
    runner.expectTrue(b != nullptr, "beta: found");
    runner.expectTrue(g != nullptr, "gamma: found");
    runner.expectTrue(d != nullptr, "delta: found");
    runner.expectTrue(p != nullptr, "pi: found");
    runner.expectEqual(std::string("\xCE\xB1"), std::string(a), "alpha: correct UTF-8");
    runner.expectEqual(std::string("\xCE\xB2"), std::string(b), "beta: correct UTF-8");
    runner.expectEqual(std::string("\xCE\xB3"), std::string(g), "gamma: correct UTF-8");
    runner.expectEqual(std::string("\xCE\xB4"), std::string(d), "delta: correct UTF-8");
    runner.expectEqual(std::string("\xCF\x80"), std::string(p), "pi: correct UTF-8");
  }

  // Test 34: uppercase Omega
  {
    const char* result = lookupHtmlEntity("Omega", 5);
    runner.expectTrue(result != nullptr, "Omega: found");
    runner.expectEqual(std::string("\xCE\xA9"), std::string(result), "Omega: correct UTF-8");
  }

  // ============================================
  // Typographic entities
  // ============================================

  // Test 35: deg, sect, para
  {
    const char* deg = lookupHtmlEntity("deg", 3);
    const char* sect = lookupHtmlEntity("sect", 4);
    const char* para = lookupHtmlEntity("para", 4);
    runner.expectTrue(deg != nullptr, "deg: found");
    runner.expectTrue(sect != nullptr, "sect: found");
    runner.expectTrue(para != nullptr, "para: found");
    runner.expectEqual(std::string("\xC2\xB0"), std::string(deg), "deg: correct UTF-8");
    runner.expectEqual(std::string("\xC2\xA7"), std::string(sect), "sect: correct UTF-8");
    runner.expectEqual(std::string("\xC2\xB6"), std::string(para), "para: correct UTF-8");
  }

  // Test 36: frac12, frac14
  {
    const char* half = lookupHtmlEntity("frac12", 6);
    const char* quarter = lookupHtmlEntity("frac14", 6);
    runner.expectTrue(half != nullptr, "frac12: found");
    runner.expectTrue(quarter != nullptr, "frac14: found");
    runner.expectEqual(std::string("\xC2\xBD"), std::string(half), "frac12: correct UTF-8");
    runner.expectEqual(std::string("\xC2\xBC"), std::string(quarter), "frac14: correct UTF-8");
  }

  // Test 37: laquo, raquo (guillemets)
  {
    const char* l = lookupHtmlEntity("laquo", 5);
    const char* r = lookupHtmlEntity("raquo", 5);
    runner.expectTrue(l != nullptr, "laquo: found");
    runner.expectTrue(r != nullptr, "raquo: found");
    runner.expectEqual(std::string("\xC2\xAB"), std::string(l), "laquo: correct UTF-8");
    runner.expectEqual(std::string("\xC2\xBB"), std::string(r), "raquo: correct UTF-8");
  }

  // Test 38: sup2 (superscript 2)
  {
    const char* result = lookupHtmlEntity("sup2", 4);
    runner.expectTrue(result != nullptr, "sup2: found");
    runner.expectEqual(std::string("\xC2\xB2"), std::string(result), "sup2: correct UTF-8");
  }

  // Test 39: dagger, Dagger
  {
    const char* d = lookupHtmlEntity("dagger", 6);
    const char* dd = lookupHtmlEntity("Dagger", 6);
    runner.expectTrue(d != nullptr, "dagger: found");
    runner.expectTrue(dd != nullptr, "Dagger: found");
    runner.expectEqual(std::string("\xE2\x80\xA0"), std::string(d), "dagger: correct UTF-8");
    runner.expectEqual(std::string("\xE2\x80\xA1"), std::string(dd), "Dagger: correct UTF-8");
  }

  // Test 40: thinsp
  {
    const char* result = lookupHtmlEntity("thinsp", 6);
    runner.expectTrue(result != nullptr, "thinsp: found");
    runner.expectEqual(std::string("\xE2\x80\x89"), std::string(result), "thinsp: correct UTF-8");
  }

  return runner.allPassed() ? 0 : 1;
}
