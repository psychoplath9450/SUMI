#pragma once

#include <cstdint>
#include <utility>

namespace sumi {

enum class Error : uint8_t {
  None = 0,

  // Storage errors
  SdCardNotFound,
  FileNotFound,
  FileCorrupted,
  CacheFull,

  // Content errors
  InvalidFormat,
  UnsupportedVersion,
  ParseFailed,

  // Hardware errors
  DisplayFailed,
  NetworkFailed,
  OutOfMemory,

  // Generic
  InvalidState,
  InvalidOperation,
  IOError,
  Timeout,
};

template <typename T>
struct Result {
  T value;
  Error err;

  bool ok() const { return err == Error::None; }
  explicit operator bool() const { return ok(); }

  const T& operator*() const { return value; }
  T& operator*() { return value; }

  const T* operator->() const { return &value; }
  T* operator->() { return &value; }
};

template <>
struct Result<void> {
  Error err;

  bool ok() const { return err == Error::None; }
  explicit operator bool() const { return ok(); }
};

template <typename T>
Result<T> Ok(T val) {
  return {std::move(val), Error::None};
}

inline Result<void> Ok() { return {Error::None}; }

template <typename T = void>
Result<T> Err(Error e) {
  return {{}, e};
}

inline Result<void> ErrVoid(Error e) { return {e}; }

const char* errorToString(Error e);

}  // namespace sumi

// Helper macros for unique variable names (uses __LINE__ for uniqueness)
#define TRY_PASTE_(a, b) a##b
#define TRY_PASTE(a, b) TRY_PASTE_(a, b)

#define TRY(expr)                                                                                            \
  do {                                                                                                       \
    auto TRY_PASTE(_try_res_, __LINE__) = (expr);                                                            \
    if (!TRY_PASTE(_try_res_, __LINE__).ok()) return ::sumi::ErrVoid(TRY_PASTE(_try_res_, __LINE__).err); \
  } while (0)

#define TRY_VAL(var, expr)                                               \
  auto _result_##var = (expr);                                           \
  if (!_result_##var.ok()) return ::sumi::ErrVoid(_result_##var.err); \
  auto var = std::move(*_result_##var)
