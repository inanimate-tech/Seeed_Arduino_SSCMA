// test_harness.h — tiny assert framework + SSCMA fixture helpers.
#pragma once

#include <stdio.h>
#include <stdlib.h>

#include <string>

// ---------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------
namespace th {
inline int& failures() {
  static int f = 0;
  return f;
}
inline int& checks() {
  static int c = 0;
  return c;
}
}  // namespace th

#define TH_CHECK(cond, ...)                                              \
  do {                                                                   \
    th::checks()++;                                                      \
    if (!(cond)) {                                                       \
      th::failures()++;                                                  \
      fprintf(stderr, "  FAIL %s:%d: %s\n        ", __FILE__, __LINE__,  \
              #cond);                                                    \
      fprintf(stderr, __VA_ARGS__);                                      \
      fprintf(stderr, "\n");                                             \
    } else {                                                             \
      fprintf(stderr, "  ok   %s\n", #cond);                             \
    }                                                                    \
  } while (0)

#define TH_EQ_INT(actual, expected)                                          \
  do {                                                                       \
    long long a_ = (long long)(actual);                                       \
    long long e_ = (long long)(expected);                                     \
    th::checks()++;                                                          \
    if (a_ != e_) {                                                          \
      th::failures()++;                                                      \
      fprintf(stderr, "  FAIL %s:%d: %s == %s  (got %lld, want %lld)\n",     \
              __FILE__, __LINE__, #actual, #expected, a_, e_);               \
    } else {                                                                 \
      fprintf(stderr, "  ok   %s == %lld\n", #actual, e_);                   \
    }                                                                        \
  } while (0)

#define TH_EQ_STR(actual, expected)                                          \
  do {                                                                       \
    std::string a_((actual));                                                \
    std::string e_((expected));                                              \
    th::checks()++;                                                          \
    if (a_ != e_) {                                                          \
      th::failures()++;                                                      \
      fprintf(stderr, "  FAIL %s:%d: %s == \"%s\"  (got \"%s\")\n",          \
              __FILE__, __LINE__, #actual, e_.c_str(), a_.c_str());          \
    } else {                                                                 \
      fprintf(stderr, "  ok   %s == \"%s\"\n", #actual, e_.c_str());         \
    }                                                                        \
  } while (0)

#define TH_REPORT()                                                        \
  do {                                                                     \
    fprintf(stderr, "\n[%s] %d checks, %d failures\n",                     \
            th::failures() ? "FAILED" : "PASSED", th::checks(),            \
            th::failures());                                               \
    return th::failures() ? 1 : 0;                                         \
  } while (0)

// ---------------------------------------------------------------------------
// Fixture: feed the two replies SSCMA::begin() demands (AT+ID? then AT+NAME?).
// One chunk each — see the note in Arduino.h HardwareSerial.
// ---------------------------------------------------------------------------
inline void feed_begin(HardwareSerial& s,
                       const std::string& id = "0000000012345678",
                       const std::string& name = "sscma") {
  s.feedReply(0, "ID?", 0, "\"" + id + "\"");
  s.feedReply(0, "NAME?", 0, "\"" + name + "\"");
}
