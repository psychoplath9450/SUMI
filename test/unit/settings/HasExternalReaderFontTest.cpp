#include "test_utils.h"

#include <cstdint>
#include <cstring>

// Inline the necessary types to avoid firmware dependencies

struct Theme {
  char readerFontFamilyXSmall[32];
  char readerFontFamilySmall[32];
  char readerFontFamilyMedium[32];
  char readerFontFamilyLarge[32];
};

namespace sumi {
struct Settings {
  enum FontSize : uint8_t { FontXSmall = 0, FontSmall = 1, FontMedium = 2, FontLarge = 3 };

  uint8_t fontSize = FontMedium;

  bool hasExternalReaderFont(const Theme& theme) const {
    const char* family = nullptr;
    switch (fontSize) {
      case FontXSmall:
        family = theme.readerFontFamilyXSmall;
        break;
      case FontMedium:
        family = theme.readerFontFamilyMedium;
        break;
      case FontLarge:
        family = theme.readerFontFamilyLarge;
        break;
      default:
        family = theme.readerFontFamilySmall;
        break;
    }
    return family && *family;
  }
};
}  // namespace sumi

static Theme makeTheme(const char* xsmall = "", const char* small = "", const char* medium = "",
                        const char* large = "") {
  Theme theme = {};
  strncpy(theme.readerFontFamilyXSmall, xsmall, sizeof(theme.readerFontFamilyXSmall) - 1);
  strncpy(theme.readerFontFamilySmall, small, sizeof(theme.readerFontFamilySmall) - 1);
  strncpy(theme.readerFontFamilyMedium, medium, sizeof(theme.readerFontFamilyMedium) - 1);
  strncpy(theme.readerFontFamilyLarge, large, sizeof(theme.readerFontFamilyLarge) - 1);
  return theme;
}

int main() {
  TestUtils::TestRunner runner("HasExternalReaderFontTest");

  // === No external fonts (all empty) ===
  {
    Theme theme = makeTheme();
    sumi::Settings settings;

    settings.fontSize = sumi::Settings::FontXSmall;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "XSmall: empty family returns false");

    settings.fontSize = sumi::Settings::FontSmall;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "Small: empty family returns false");

    settings.fontSize = sumi::Settings::FontMedium;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "Medium: empty family returns false");

    settings.fontSize = sumi::Settings::FontLarge;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "Large: empty family returns false");
  }

  // === All external fonts set ===
  {
    Theme theme = makeTheme("NotoSans", "NotoSans", "NotoSans", "NotoSans");
    sumi::Settings settings;

    settings.fontSize = sumi::Settings::FontXSmall;
    runner.expectTrue(settings.hasExternalReaderFont(theme), "XSmall: non-empty family returns true");

    settings.fontSize = sumi::Settings::FontSmall;
    runner.expectTrue(settings.hasExternalReaderFont(theme), "Small: non-empty family returns true");

    settings.fontSize = sumi::Settings::FontMedium;
    runner.expectTrue(settings.hasExternalReaderFont(theme), "Medium: non-empty family returns true");

    settings.fontSize = sumi::Settings::FontLarge;
    runner.expectTrue(settings.hasExternalReaderFont(theme), "Large: non-empty family returns true");
  }

  // === Only some sizes have external fonts ===
  {
    Theme theme = makeTheme("", "", "NotoSans", "");
    sumi::Settings settings;

    settings.fontSize = sumi::Settings::FontXSmall;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "XSmall: no external font when only Medium set");

    settings.fontSize = sumi::Settings::FontSmall;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "Small: no external font when only Medium set");

    settings.fontSize = sumi::Settings::FontMedium;
    runner.expectTrue(settings.hasExternalReaderFont(theme), "Medium: has external font");

    settings.fontSize = sumi::Settings::FontLarge;
    runner.expectFalse(settings.hasExternalReaderFont(theme), "Large: no external font when only Medium set");
  }

  // === Default fontSize is FontMedium ===
  {
    sumi::Settings settings;
    runner.expectEq(uint8_t(sumi::Settings::FontMedium), settings.fontSize, "default fontSize is FontMedium");
  }

  // === Unknown fontSize falls through to Small (default case) ===
  {
    Theme theme = makeTheme("", "ThaiFont", "", "");
    sumi::Settings settings;
    settings.fontSize = 99;  // invalid value
    runner.expectTrue(settings.hasExternalReaderFont(theme), "invalid fontSize falls to default (Small)");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
