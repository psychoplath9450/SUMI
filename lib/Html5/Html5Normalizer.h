#pragma once
#include <string>

namespace html5 {

// Normalize HTML5 void elements to XHTML self-closing format
// Converts <img src="x"> to <img src="x" />
// Processes file in streaming mode for memory efficiency
// Returns true on success
bool normalizeVoidElements(const std::string& inputPath, const std::string& outputPath);

}  // namespace html5
