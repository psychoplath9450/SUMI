#include "htmlEntities.h"

#include <cstring>

struct HtmlEntity {
  const char* name;
  const char* utf8;
};

// Sorted by name for binary search. Covers the HTML entities commonly found in EPUBs.
// Expat already handles the 5 XML built-in entities (&amp; &lt; &gt; &quot; &apos;).
static const HtmlEntity entities[] = {
    {"AElig", "\xC3\x86"},      {"Aacute", "\xC3\x81"},     {"Acirc", "\xC3\x82"},      {"Agrave", "\xC3\x80"},
    {"Alpha", "\xCE\x91"},      {"Aring", "\xC3\x85"},      {"Atilde", "\xC3\x83"},     {"Auml", "\xC3\x84"},
    {"Beta", "\xCE\x92"},       {"Ccedil", "\xC3\x87"},     {"Chi", "\xCE\xA7"},        {"Dagger", "\xE2\x80\xA1"},
    {"Delta", "\xCE\x94"},      {"ETH", "\xC3\x90"},        {"Eacute", "\xC3\x89"},     {"Ecirc", "\xC3\x8A"},
    {"Egrave", "\xC3\x88"},     {"Epsilon", "\xCE\x95"},    {"Eta", "\xCE\x97"},        {"Euml", "\xC3\x8B"},
    {"Gamma", "\xCE\x93"},      {"Iacute", "\xC3\x8D"},     {"Icirc", "\xC3\x8E"},      {"Igrave", "\xC3\x8C"},
    {"Iota", "\xCE\x99"},       {"Iuml", "\xC3\x8F"},       {"Kappa", "\xCE\x9A"},      {"Lambda", "\xCE\x9B"},
    {"Mu", "\xCE\x9C"},         {"Ntilde", "\xC3\x91"},     {"Nu", "\xCE\x9D"},         {"OElig", "\xC5\x92"},
    {"Oacute", "\xC3\x93"},     {"Ocirc", "\xC3\x94"},      {"Ograve", "\xC3\x92"},     {"Omega", "\xCE\xA9"},
    {"Omicron", "\xCE\x9F"},    {"Oslash", "\xC3\x98"},     {"Otilde", "\xC3\x95"},     {"Ouml", "\xC3\x96"},
    {"Phi", "\xCE\xA6"},        {"Pi", "\xCE\xA0"},         {"Prime", "\xE2\x80\xB3"},  {"Psi", "\xCE\xA8"},
    {"Rho", "\xCE\xA1"},        {"Scaron", "\xC5\xA0"},     {"Sigma", "\xCE\xA3"},      {"THORN", "\xC3\x9E"},
    {"Tau", "\xCE\xA4"},        {"Theta", "\xCE\x98"},      {"Uacute", "\xC3\x9A"},     {"Ucirc", "\xC3\x9B"},
    {"Ugrave", "\xC3\x99"},     {"Upsilon", "\xCE\xA5"},    {"Uuml", "\xC3\x9C"},       {"Xi", "\xCE\x9E"},
    {"Yacute", "\xC3\x9D"},     {"Yuml", "\xC5\xB8"},       {"Zeta", "\xCE\x96"},       {"aacute", "\xC3\xA1"},
    {"acirc", "\xC3\xA2"},      {"acute", "\xC2\xB4"},      {"aelig", "\xC3\xA6"},      {"agrave", "\xC3\xA0"},
    {"alpha", "\xCE\xB1"},      {"aring", "\xC3\xA5"},      {"atilde", "\xC3\xA3"},     {"auml", "\xC3\xA4"},
    {"bdquo", "\xE2\x80\x9E"},  {"beta", "\xCE\xB2"},       {"brvbar", "\xC2\xA6"},     {"bull", "\xE2\x80\xA2"},
    {"ccedil", "\xC3\xA7"},     {"cedil", "\xC2\xB8"},      {"cent", "\xC2\xA2"},       {"chi", "\xCF\x87"},
    {"circ", "\xCB\x86"},       {"copy", "\xC2\xA9"},       {"curren", "\xC2\xA4"},     {"dagger", "\xE2\x80\xA0"},
    {"darr", "\xE2\x86\x93"},   {"deg", "\xC2\xB0"},        {"delta", "\xCE\xB4"},      {"divide", "\xC3\xB7"},
    {"eacute", "\xC3\xA9"},     {"ecirc", "\xC3\xAA"},      {"egrave", "\xC3\xA8"},     {"emsp", "\xE2\x80\x83"},
    {"ensp", "\xE2\x80\x82"},   {"epsilon", "\xCE\xB5"},    {"eta", "\xCE\xB7"},        {"eth", "\xC3\xB0"},
    {"euml", "\xC3\xAB"},       {"euro", "\xE2\x82\xAC"},   {"fnof", "\xC6\x92"},       {"frac12", "\xC2\xBD"},
    {"frac14", "\xC2\xBC"},     {"frac34", "\xC2\xBE"},     {"gamma", "\xCE\xB3"},      {"harr", "\xE2\x86\x94"},
    {"hellip", "\xE2\x80\xA6"}, {"iacute", "\xC3\xAD"},     {"icirc", "\xC3\xAE"},      {"iexcl", "\xC2\xA1"},
    {"igrave", "\xC3\xAC"},     {"infin", "\xE2\x88\x9E"},  {"iota", "\xCE\xB9"},       {"iquest", "\xC2\xBF"},
    {"iuml", "\xC3\xAF"},       {"kappa", "\xCE\xBA"},      {"lambda", "\xCE\xBB"},     {"laquo", "\xC2\xAB"},
    {"larr", "\xE2\x86\x90"},   {"ldquo", "\xE2\x80\x9C"},  {"loz", "\xE2\x97\x8A"},    {"lrm", "\xE2\x80\x8E"},
    {"lsaquo", "\xE2\x80\xB9"}, {"lsquo", "\xE2\x80\x98"},  {"macr", "\xC2\xAF"},       {"mdash", "\xE2\x80\x94"},
    {"micro", "\xC2\xB5"},      {"middot", "\xC2\xB7"},     {"minus", "\xE2\x88\x92"},  {"mu", "\xCE\xBC"},
    {"nbsp", "\xC2\xA0"},       {"ndash", "\xE2\x80\x93"},  {"not", "\xC2\xAC"},        {"ntilde", "\xC3\xB1"},
    {"nu", "\xCE\xBD"},         {"oacute", "\xC3\xB3"},     {"ocirc", "\xC3\xB4"},      {"oelig", "\xC5\x93"},
    {"ograve", "\xC3\xB2"},     {"omega", "\xCF\x89"},      {"omicron", "\xCE\xBF"},    {"ordf", "\xC2\xAA"},
    {"ordm", "\xC2\xBA"},       {"oslash", "\xC3\xB8"},     {"otilde", "\xC3\xB5"},     {"ouml", "\xC3\xB6"},
    {"para", "\xC2\xB6"},       {"permil", "\xE2\x80\xB0"}, {"phi", "\xCF\x86"},        {"pi", "\xCF\x80"},
    {"plusmn", "\xC2\xB1"},     {"pound", "\xC2\xA3"},      {"prime", "\xE2\x80\xB2"},  {"psi", "\xCF\x88"},
    {"raquo", "\xC2\xBB"},      {"rarr", "\xE2\x86\x92"},   {"rdquo", "\xE2\x80\x9D"},  {"reg", "\xC2\xAE"},
    {"rho", "\xCF\x81"},        {"rlm", "\xE2\x80\x8F"},    {"rsaquo", "\xE2\x80\xBA"}, {"rsquo", "\xE2\x80\x99"},
    {"sbquo", "\xE2\x80\x9A"},  {"scaron", "\xC5\xA1"},     {"sect", "\xC2\xA7"},       {"shy", "\xC2\xAD"},
    {"sigma", "\xCF\x83"},      {"sigmaf", "\xCF\x82"},     {"sup1", "\xC2\xB9"},       {"sup2", "\xC2\xB2"},
    {"sup3", "\xC2\xB3"},       {"szlig", "\xC3\x9F"},      {"tau", "\xCF\x84"},        {"theta", "\xCE\xB8"},
    {"thinsp", "\xE2\x80\x89"}, {"thorn", "\xC3\xBE"},      {"tilde", "\xCB\x9C"},      {"times", "\xC3\x97"},
    {"trade", "\xE2\x84\xA2"},  {"uacute", "\xC3\xBA"},     {"uarr", "\xE2\x86\x91"},   {"ucirc", "\xC3\xBB"},
    {"ugrave", "\xC3\xB9"},     {"uml", "\xC2\xA8"},        {"upsilon", "\xCF\x85"},    {"uuml", "\xC3\xBC"},
    {"xi", "\xCE\xBE"},         {"yacute", "\xC3\xBD"},     {"yen", "\xC2\xA5"},        {"yuml", "\xC3\xBF"},
    {"zeta", "\xCE\xB6"},       {"zwj", "\xE2\x80\x8D"},    {"zwnj", "\xE2\x80\x8C"},
};

static constexpr int NUM_ENTITIES = sizeof(entities) / sizeof(entities[0]);

const char* lookupHtmlEntity(const char* name, int nameLen) {
  // Binary search on sorted table
  int lo = 0;
  int hi = NUM_ENTITIES - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const int cmp = strncmp(name, entities[mid].name, nameLen);
    if (cmp == 0) {
      // strncmp matched for nameLen chars — check the table entry isn't longer
      if (entities[mid].name[nameLen] == '\0') {
        return entities[mid].utf8;
      }
      // Table entry is longer (e.g. searching "sup" matched "sup1" prefix) — go left
      hi = mid - 1;
    } else if (cmp < 0) {
      hi = mid - 1;
    } else {
      lo = mid + 1;
    }
  }
  return nullptr;
}
