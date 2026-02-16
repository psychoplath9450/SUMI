#pragma once

#include <map>
#include <string>

#include "SdFat.h"

class SDCardManager {
 public:
  SDCardManager() = default;
  bool begin() { return true; }
  bool ready() const { return true; }

  void registerFile(const std::string& path, const std::string& data) { files_[path] = data; }

  void clearFiles() { files_.clear(); }

  bool exists(const char* path) { return files_.find(path) != files_.end(); }

  FsFile open(const char* path, int mode = O_RDONLY) {
    (void)mode;
    FsFile file;
    auto it = files_.find(path);
    if (it != files_.end()) {
      file.setBuffer(it->second);
    }
    return file;
  }

  bool openFileForRead(const char* moduleName, const char* path, FsFile& file) {
    (void)moduleName;
    auto it = files_.find(path);
    if (it != files_.end()) {
      file.setBuffer(it->second);
      return true;
    }
    return false;
  }

  bool openFileForRead(const char* moduleName, const std::string& path, FsFile& file) {
    return openFileForRead(moduleName, path.c_str(), file);
  }

  static SDCardManager& getInstance() {
    static SDCardManager instance;
    return instance;
  }

 private:
  std::map<std::string, std::string> files_;
};

#define SdMan SDCardManager::getInstance()
