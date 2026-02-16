#include "test_utils.h"

#include <map>
#include <string>

// Include mock dependencies
#include "HardwareSerial.h"

// CssStyle enum and struct definitions
enum class TextAlign {
  None,
  Left,
  Right,
  Center,
  Justify
};

enum class CssFontStyle {
  Normal,
  Italic
};

enum class CssFontWeight {
  Normal,
  Bold
};

enum class TextDirection {
  Ltr,
  Rtl
};

struct CssStyle {
  TextAlign textAlign = TextAlign::None;
  bool hasTextAlign = false;

  CssFontStyle fontStyle = CssFontStyle::Normal;
  bool hasFontStyle = false;

  CssFontWeight fontWeight = CssFontWeight::Normal;
  bool hasFontWeight = false;

  TextDirection direction = TextDirection::Ltr;
  bool hasDirection = false;

  void merge(const CssStyle& other) {
    if (other.hasTextAlign) {
      textAlign = other.textAlign;
      hasTextAlign = true;
    }
    if (other.hasFontStyle) {
      fontStyle = other.fontStyle;
      hasFontStyle = true;
    }
    if (other.hasFontWeight) {
      fontWeight = other.fontWeight;
      hasFontWeight = true;
    }
    if (other.hasDirection) {
      direction = other.direction;
      hasDirection = true;
    }
  }

  void reset() {
    textAlign = TextAlign::None;
    hasTextAlign = false;
    fontStyle = CssFontStyle::Normal;
    hasFontStyle = false;
    fontWeight = CssFontWeight::Normal;
    hasFontWeight = false;
    direction = TextDirection::Ltr;
    hasDirection = false;
  }
};

// Minimal CssParser implementation for testing (without file I/O)
namespace {

std::string trim(const std::string& str) {
  size_t start = 0;
  while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
    ++start;
  }
  if (start == str.size()) return "";

  size_t end = str.size() - 1;
  while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
    --end;
  }
  return str.substr(start, end - start + 1);
}

std::string toLower(const std::string& str) {
  std::string result = str;
  for (char& c : result) {
    if (c >= 'A' && c <= 'Z') {
      c = c - 'A' + 'a';
    }
  }
  return result;
}

TextAlign parseTextAlign(const std::string& value) {
  std::string v = toLower(trim(value));

  if (v == "left" || v == "start") {
    return TextAlign::Left;
  } else if (v == "right" || v == "end") {
    return TextAlign::Right;
  } else if (v == "center") {
    return TextAlign::Center;
  } else if (v == "justify") {
    return TextAlign::Justify;
  }

  return TextAlign::Left;
}

CssFontStyle parseFontStyle(const std::string& value) {
  std::string v = toLower(trim(value));

  if (v == "italic" || v == "oblique") {
    return CssFontStyle::Italic;
  }

  return CssFontStyle::Normal;
}

CssFontWeight parseFontWeight(const std::string& value) {
  std::string v = toLower(trim(value));

  if (v == "bold" || v == "bolder" || v == "700" || v == "800" || v == "900") {
    return CssFontWeight::Bold;
  }

  return CssFontWeight::Normal;
}

void parseProperty(const std::string& name, const std::string& value, CssStyle& style) {
  if (name == "text-align") {
    style.textAlign = parseTextAlign(value);
    style.hasTextAlign = true;
  } else if (name == "font-style") {
    style.fontStyle = parseFontStyle(value);
    style.hasFontStyle = true;
  } else if (name == "font-weight") {
    style.fontWeight = parseFontWeight(value);
    style.hasFontWeight = true;
  } else if (name == "direction") {
    std::string v = toLower(trim(value));
    if (v == "rtl") {
      style.direction = TextDirection::Rtl;
      style.hasDirection = true;
    } else if (v == "ltr") {
      style.direction = TextDirection::Ltr;
      style.hasDirection = true;
    }
  }
}

CssStyle parseInlineStyle(const std::string& styleAttr) {
  CssStyle style;

  if (styleAttr.empty()) {
    return style;
  }

  size_t propStart = 0;
  size_t propLen = styleAttr.length();

  while (propStart < propLen) {
    size_t propEnd = styleAttr.find(';', propStart);
    if (propEnd == std::string::npos) propEnd = propLen;

    std::string prop = trim(styleAttr.substr(propStart, propEnd - propStart));

    if (!prop.empty()) {
      size_t colonPos = prop.find(':');
      if (colonPos != std::string::npos && colonPos > 0) {
        std::string propName = trim(prop.substr(0, colonPos));
        std::string propValue = trim(prop.substr(colonPos + 1));
        propName = toLower(propName);
        parseProperty(propName, propValue, style);
      }
    }

    propStart = propEnd + 1;
  }

  return style;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("CSS Parser");

  // ============================================
  // parseTextAlign() tests
  // ============================================

  // Test 1: Standard values
  runner.expectTrue(parseTextAlign("left") == TextAlign::Left, "parseTextAlign: 'left'");
  runner.expectTrue(parseTextAlign("right") == TextAlign::Right, "parseTextAlign: 'right'");
  runner.expectTrue(parseTextAlign("center") == TextAlign::Center, "parseTextAlign: 'center'");
  runner.expectTrue(parseTextAlign("justify") == TextAlign::Justify, "parseTextAlign: 'justify'");

  // Test 2: Logical values
  runner.expectTrue(parseTextAlign("start") == TextAlign::Left, "parseTextAlign: 'start' maps to Left");
  runner.expectTrue(parseTextAlign("end") == TextAlign::Right, "parseTextAlign: 'end' maps to Right");

  // Test 3: Case insensitivity
  runner.expectTrue(parseTextAlign("LEFT") == TextAlign::Left, "parseTextAlign: 'LEFT' (uppercase)");
  runner.expectTrue(parseTextAlign("Center") == TextAlign::Center, "parseTextAlign: 'Center' (mixed case)");

  // Test 4: Whitespace trimming
  runner.expectTrue(parseTextAlign("  center  ") == TextAlign::Center, "parseTextAlign: '  center  ' (whitespace)");

  // Test 5: Unknown value defaults to Left
  runner.expectTrue(parseTextAlign("invalid") == TextAlign::Left, "parseTextAlign: unknown defaults to Left");
  runner.expectTrue(parseTextAlign("") == TextAlign::Left, "parseTextAlign: empty defaults to Left");

  // ============================================
  // parseFontStyle() tests
  // ============================================

  // Test 6: Standard values
  runner.expectTrue(parseFontStyle("normal") == CssFontStyle::Normal, "parseFontStyle: 'normal'");
  runner.expectTrue(parseFontStyle("italic") == CssFontStyle::Italic, "parseFontStyle: 'italic'");
  runner.expectTrue(parseFontStyle("oblique") == CssFontStyle::Italic, "parseFontStyle: 'oblique' maps to Italic");

  // Test 7: Case insensitivity
  runner.expectTrue(parseFontStyle("ITALIC") == CssFontStyle::Italic, "parseFontStyle: 'ITALIC' (uppercase)");

  // Test 8: Unknown defaults to Normal
  runner.expectTrue(parseFontStyle("invalid") == CssFontStyle::Normal, "parseFontStyle: unknown defaults to Normal");

  // ============================================
  // parseFontWeight() tests
  // ============================================

  // Test 9: Keyword values
  runner.expectTrue(parseFontWeight("normal") == CssFontWeight::Normal, "parseFontWeight: 'normal'");
  runner.expectTrue(parseFontWeight("bold") == CssFontWeight::Bold, "parseFontWeight: 'bold'");
  runner.expectTrue(parseFontWeight("bolder") == CssFontWeight::Bold, "parseFontWeight: 'bolder'");

  // Test 10: Numeric values
  runner.expectTrue(parseFontWeight("400") == CssFontWeight::Normal, "parseFontWeight: '400' is Normal");
  runner.expectTrue(parseFontWeight("700") == CssFontWeight::Bold, "parseFontWeight: '700' is Bold");
  runner.expectTrue(parseFontWeight("800") == CssFontWeight::Bold, "parseFontWeight: '800' is Bold");
  runner.expectTrue(parseFontWeight("900") == CssFontWeight::Bold, "parseFontWeight: '900' is Bold");

  // Test 11: Values below 700 are normal
  runner.expectTrue(parseFontWeight("500") == CssFontWeight::Normal, "parseFontWeight: '500' is Normal");
  runner.expectTrue(parseFontWeight("600") == CssFontWeight::Normal, "parseFontWeight: '600' is Normal");

  // ============================================
  // parseInlineStyle() tests
  // ============================================

  // Test 22: Single property
  {
    CssStyle style = parseInlineStyle("text-align: center");
    runner.expectTrue(style.hasTextAlign, "parseInlineStyle: single prop has text-align");
    runner.expectTrue(style.textAlign == TextAlign::Center, "parseInlineStyle: text-align is center");
  }

  // Test 23: Multiple properties
  {
    CssStyle style = parseInlineStyle("text-align: center; font-weight: bold");
    runner.expectTrue(style.hasTextAlign, "parseInlineStyle: multi-prop has text-align");
    runner.expectTrue(style.hasFontWeight, "parseInlineStyle: multi-prop has font-weight");
    runner.expectTrue(style.textAlign == TextAlign::Center, "parseInlineStyle: multi-prop text-align");
    runner.expectTrue(style.fontWeight == CssFontWeight::Bold, "parseInlineStyle: multi-prop font-weight");
  }

  // Test 24: With extra whitespace
  {
    CssStyle style = parseInlineStyle("  font-style :  italic  ;  font-weight : bold  ");
    runner.expectTrue(style.hasFontStyle, "parseInlineStyle: whitespace font-style");
    runner.expectTrue(style.fontStyle == CssFontStyle::Italic, "parseInlineStyle: whitespace font-style value");
    runner.expectTrue(style.hasFontWeight, "parseInlineStyle: whitespace font-weight");
    runner.expectTrue(style.fontWeight == CssFontWeight::Bold, "parseInlineStyle: whitespace font-weight value");
  }

  // Test 25: Empty string
  {
    CssStyle style = parseInlineStyle("");
    runner.expectFalse(style.hasTextAlign, "parseInlineStyle: empty has no properties");
    runner.expectFalse(style.hasFontStyle, "parseInlineStyle: empty has no font-style");
  }

  // Test 26: Missing semicolons (last property)
  {
    CssStyle style = parseInlineStyle("text-align: right");
    runner.expectTrue(style.textAlign == TextAlign::Right, "parseInlineStyle: no trailing semicolon");
  }

  // Test 27: Missing colon (property ignored)
  {
    CssStyle style = parseInlineStyle("text-align center; font-weight: bold");
    runner.expectFalse(style.hasTextAlign, "parseInlineStyle: missing colon ignored");
    runner.expectTrue(style.hasFontWeight, "parseInlineStyle: valid prop after invalid");
  }

  // Test 28: Unknown properties ignored
  {
    CssStyle style = parseInlineStyle("color: red; text-align: left; display: none");
    runner.expectTrue(style.hasTextAlign, "parseInlineStyle: known prop parsed");
    runner.expectFalse(style.hasFontWeight, "parseInlineStyle: unknown 'color' ignored");
  }

  // Test 29: Case insensitivity for property names
  {
    CssStyle style = parseInlineStyle("TEXT-ALIGN: center; FONT-WEIGHT: bold");
    runner.expectTrue(style.textAlign == TextAlign::Center, "parseInlineStyle: uppercase prop name");
    runner.expectTrue(style.fontWeight == CssFontWeight::Bold, "parseInlineStyle: uppercase prop name 2");
  }

  // ============================================
  // CssStyle::merge() tests
  // ============================================

  // Test 30: Merge overrides
  {
    CssStyle base;
    base.textAlign = TextAlign::Left;
    base.hasTextAlign = true;

    CssStyle override;
    override.textAlign = TextAlign::Center;
    override.hasTextAlign = true;
    override.fontWeight = CssFontWeight::Bold;
    override.hasFontWeight = true;

    base.merge(override);

    runner.expectTrue(base.textAlign == TextAlign::Center, "merge: override takes precedence");
    runner.expectTrue(base.fontWeight == CssFontWeight::Bold, "merge: new property added");
  }

  // Test 31: Merge preserves unset properties
  {
    CssStyle base;
    base.textAlign = TextAlign::Right;
    base.hasTextAlign = true;
    base.fontStyle = CssFontStyle::Italic;
    base.hasFontStyle = true;

    CssStyle override;
    override.fontWeight = CssFontWeight::Bold;
    override.hasFontWeight = true;
    // override.hasTextAlign is false

    base.merge(override);

    runner.expectTrue(base.textAlign == TextAlign::Right, "merge: unset property preserved");
    runner.expectTrue(base.fontStyle == CssFontStyle::Italic, "merge: unset property preserved 2");
    runner.expectTrue(base.fontWeight == CssFontWeight::Bold, "merge: new property added");
  }

  // ============================================
  // CssStyle::reset() tests
  // ============================================

  // Test 32: Reset clears all properties
  {
    CssStyle style;
    style.textAlign = TextAlign::Center;
    style.hasTextAlign = true;
    style.fontWeight = CssFontWeight::Bold;
    style.hasFontWeight = true;
    style.fontStyle = CssFontStyle::Italic;
    style.hasFontStyle = true;
    style.direction = TextDirection::Rtl;
    style.hasDirection = true;

    style.reset();

    runner.expectTrue(style.textAlign == TextAlign::None, "reset: textAlign to None");
    runner.expectFalse(style.hasTextAlign, "reset: hasTextAlign false");
    runner.expectTrue(style.fontWeight == CssFontWeight::Normal, "reset: fontWeight to Normal");
    runner.expectFalse(style.hasFontWeight, "reset: hasFontWeight false");
    runner.expectTrue(style.fontStyle == CssFontStyle::Normal, "reset: fontStyle to Normal");
    runner.expectFalse(style.hasFontStyle, "reset: hasFontStyle false");
    runner.expectTrue(style.direction == TextDirection::Ltr, "reset: direction to Ltr");
    runner.expectFalse(style.hasDirection, "reset: hasDirection false");
  }

  // ============================================
  // Direction property tests
  // ============================================

  // Test 33: Parse direction RTL
  {
    CssStyle style = parseInlineStyle("direction: rtl");
    runner.expectTrue(style.hasDirection, "direction: rtl sets hasDirection");
    runner.expectTrue(style.direction == TextDirection::Rtl, "direction: rtl value");
  }

  // Test 34: Parse direction LTR
  {
    CssStyle style = parseInlineStyle("direction: ltr");
    runner.expectTrue(style.hasDirection, "direction: ltr sets hasDirection");
    runner.expectTrue(style.direction == TextDirection::Ltr, "direction: ltr value");
  }

  // Test 35: Direction case insensitive
  {
    CssStyle style = parseInlineStyle("direction: RTL");
    runner.expectTrue(style.hasDirection, "direction: RTL uppercase");
    runner.expectTrue(style.direction == TextDirection::Rtl, "direction: RTL uppercase value");
  }

  // Test 36: Unknown direction value ignored
  {
    CssStyle style = parseInlineStyle("direction: auto");
    runner.expectFalse(style.hasDirection, "direction: unknown value not set");
  }

  // Test 37: Direction combined with other properties
  {
    CssStyle style = parseInlineStyle("text-align: right; direction: rtl; font-weight: bold");
    runner.expectTrue(style.hasDirection, "direction: combined has direction");
    runner.expectTrue(style.direction == TextDirection::Rtl, "direction: combined rtl value");
    runner.expectTrue(style.hasTextAlign, "direction: combined has text-align");
    runner.expectTrue(style.hasFontWeight, "direction: combined has font-weight");
  }

  // Test 38: Merge direction
  {
    CssStyle base;
    base.direction = TextDirection::Ltr;
    base.hasDirection = false;

    CssStyle override_;
    override_.direction = TextDirection::Rtl;
    override_.hasDirection = true;

    base.merge(override_);
    runner.expectTrue(base.hasDirection, "merge direction: override sets hasDirection");
    runner.expectTrue(base.direction == TextDirection::Rtl, "merge direction: override value");
  }

  // Test 39: Merge preserves direction when not overridden
  {
    CssStyle base;
    base.direction = TextDirection::Rtl;
    base.hasDirection = true;

    CssStyle override_;
    // hasDirection is false

    base.merge(override_);
    runner.expectTrue(base.direction == TextDirection::Rtl, "merge direction: preserved when not overridden");
  }

  return runner.allPassed() ? 0 : 1;
}
