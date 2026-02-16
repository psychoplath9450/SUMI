#pragma once

#include <ctype.h>

#ifdef __cplusplus
#include <algorithm>

#include "Print.h"
#include "WString.h"
#include "platform_stubs.h"

using std::max;
using std::min;

inline bool isPrintable(char c) { return std::isprint(static_cast<unsigned char>(c)); }
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#endif

#define HEX 16
#define PROGMEM
