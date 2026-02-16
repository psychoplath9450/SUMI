#include "platform_stubs.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>

#include "WString.h"

MockSerial Serial;
MockSPI SPI;
MockESP ESP;

void MockSerial::printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void MockSerial::println(const char* s) {
  if (s)
    fprintf(stderr, "%s\n", s);
  else
    fprintf(stderr, "\n");
}

void MockSerial::println() { fprintf(stderr, "\n"); }

void MockSerial::print(const char* s) {
  if (s) fprintf(stderr, "%s", s);
}

void MockSerial::println(int v) { fprintf(stderr, "%d\n", v); }
void MockSerial::println(unsigned long v) { fprintf(stderr, "%lu\n", v); }
void MockSerial::print(int v) { fprintf(stderr, "%d", v); }

void MockSerial::println(const String& s) {
  if (s.c_str())
    fprintf(stderr, "%s\n", s.c_str());
  else
    fprintf(stderr, "\n");
}

void MockSerial::print(const String& s) {
  if (s.c_str()) fprintf(stderr, "%s", s.c_str());
}

unsigned long millis() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}
