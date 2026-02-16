#include "test_utils.h"

#include <cstddef>
#include <cstring>

// Extract the validation logic from ThemeManager::listAvailableThemes() for direct testing.
// Must stay in sync with the lambda in ThemeManager.cpp.
static bool isValidThemeName(const char* name, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
      return false;
    }
  }
  return len > 0;
}

int main() {
  TestUtils::TestRunner runner("ThemeManager - isValidThemeName");

  // Valid names
  runner.expectTrue(isValidThemeName("light", 5), "simple lowercase name");
  runner.expectTrue(isValidThemeName("dark", 4), "another simple name");
  runner.expectTrue(isValidThemeName("my-custom", 9), "hyphenated name");
  runner.expectTrue(isValidThemeName("dark_v2", 7), "underscore name");
  runner.expectTrue(isValidThemeName("MyTheme", 7), "mixed case name");
  runner.expectTrue(isValidThemeName("ALLCAPS", 7), "uppercase name");
  runner.expectTrue(isValidThemeName("theme123", 8), "name with digits");
  runner.expectTrue(isValidThemeName("a", 1), "single character");
  runner.expectTrue(isValidThemeName("A", 1), "single uppercase");
  runner.expectTrue(isValidThemeName("0", 1), "single digit");
  runner.expectTrue(isValidThemeName("-", 1), "single hyphen");
  runner.expectTrue(isValidThemeName("_", 1), "single underscore");
  runner.expectTrue(isValidThemeName("my-dark-theme-v2", 16), "complex valid name");
  runner.expectTrue(isValidThemeName("a_b-c", 5), "mixed separators");

  // Invalid names - special characters
  runner.expectFalse(isValidThemeName("my theme", 8), "space in name");
  runner.expectFalse(isValidThemeName("my.theme", 8), "dot in name");
  runner.expectFalse(isValidThemeName("theme!", 6), "exclamation mark");
  runner.expectFalse(isValidThemeName("theme@home", 10), "at sign");
  runner.expectFalse(isValidThemeName("theme#1", 7), "hash");
  runner.expectFalse(isValidThemeName("100%", 4), "percent");
  runner.expectFalse(isValidThemeName("a/b", 3), "forward slash");
  runner.expectFalse(isValidThemeName("a\\b", 3), "backslash");
  runner.expectFalse(isValidThemeName("theme(1)", 8), "parentheses");
  runner.expectFalse(isValidThemeName("theme+dark", 10), "plus sign");
  runner.expectFalse(isValidThemeName("theme=dark", 10), "equals sign");
  runner.expectFalse(isValidThemeName("th\xC3\xA9me", 6), "non-ASCII (UTF-8 e-acute)");
  runner.expectFalse(isValidThemeName("\t", 1), "tab character");
  runner.expectFalse(isValidThemeName("\n", 1), "newline character");

  // Empty name
  runner.expectFalse(isValidThemeName("", 0), "empty name");
  runner.expectFalse(isValidThemeName("anything", 0), "zero length");

  // Boundary: invalid character at different positions
  runner.expectFalse(isValidThemeName(".abc", 4), "invalid at start");
  runner.expectFalse(isValidThemeName("ab.c", 4), "invalid in middle");
  runner.expectFalse(isValidThemeName("abc.", 4), "invalid at end");

  return runner.allPassed() ? 0 : 1;
}
