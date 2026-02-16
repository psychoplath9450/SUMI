#pragma once

// Lookup an HTML entity name (without & and ;) and return its UTF-8 string.
// Returns nullptr if the entity is not found.
// nameLen is the length of the name (not null-terminated from expat).
const char* lookupHtmlEntity(const char* name, int nameLen);
