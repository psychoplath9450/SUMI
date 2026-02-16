#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t c) {
    (void)c;
    return 1;
  }
  virtual size_t write(const uint8_t* buf, size_t size) {
    for (size_t i = 0; i < size; i++) write(buf[i]);
    return size;
  }
  size_t print(const char* s) {
    if (!s) return 0;
    return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
  }
  size_t println(const char* s = "") {
    size_t n = print(s);
    n += write('\n');
    return n;
  }
  size_t printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) write(reinterpret_cast<const uint8_t*>(buf), len);
    return len > 0 ? len : 0;
  }
};
