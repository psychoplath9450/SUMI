#pragma once

#include <functional>
#include <string>

class FsFile;
class Print;

struct ImageConvertConfig {
  int maxWidth = 480;
  int maxHeight = 800;
  bool oneBit = false;
  bool quickMode = false;
  const char* logTag = "IMG";
  std::function<bool()> shouldAbort = nullptr;
};

class ImageConverter {
 public:
  virtual ~ImageConverter() = default;
  virtual bool convert(FsFile&, Print&, const ImageConvertConfig&) = 0;
  virtual const char* formatName() const = 0;
};

class ImageConverterFactory {
 public:
  static ImageConverter* getConverter(const std::string&) { return nullptr; }
  static bool convertToBmp(const std::string&, const std::string&, const ImageConvertConfig& = {}) { return false; }
  static bool isSupported(const std::string&) { return false; }
};
