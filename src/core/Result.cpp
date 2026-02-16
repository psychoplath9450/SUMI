#include "Result.h"

namespace sumi {

const char* errorToString(Error e) {
  switch (e) {
    case Error::None:
      return "None";
    case Error::SdCardNotFound:
      return "SD card not found";
    case Error::FileNotFound:
      return "File not found";
    case Error::FileCorrupted:
      return "File corrupted";
    case Error::CacheFull:
      return "Cache full";
    case Error::InvalidFormat:
      return "Invalid format";
    case Error::UnsupportedVersion:
      return "Unsupported version";
    case Error::ParseFailed:
      return "Parse failed";
    case Error::DisplayFailed:
      return "Display failed";
    case Error::NetworkFailed:
      return "Network failed";
    case Error::OutOfMemory:
      return "Out of memory";
    case Error::InvalidState:
      return "Invalid state";
    case Error::InvalidOperation:
      return "Invalid operation";
    case Error::IOError:
      return "I/O error";
    case Error::Timeout:
      return "Timeout";
  }
  return "Unknown error";
}

}  // namespace sumi
