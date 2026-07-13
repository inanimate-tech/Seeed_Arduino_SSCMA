// Arduino.h — host (macOS/Linux) stub of the Arduino core, sufficient to
// compile the UNMODIFIED Seeed_Arduino_SSCMA library and drive it from tests.
//
// Deliberately does NOT #define ARDUINO, so ArduinoJson keeps
// ARDUINOJSON_ENABLE_ARDUINO_{STREAM,PRINT} and PROGMEM at 0. The build defines
// -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 explicitly so that
// `variant.as<String>()` resolves to our String below.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deque>
#include <string>
#include <vector>

#include "WString.h"

// ---------------------------------------------------------------------------
// Digital I/O constants
// ---------------------------------------------------------------------------
#define HIGH 0x1
#define LOW 0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

#define LSBFIRST 0
#define MSBFIRST 1

#ifndef PROGMEM
#  define PROGMEM
#endif

typedef uint8_t byte;
typedef bool boolean;

// ---------------------------------------------------------------------------
// Fake clock. millis() advances 1ms per call by default, so the library's
// `while (millis() - start <= timeout)` spin loops terminate instantly instead
// of burning real wall-clock seconds. delay() also advances it.
// ---------------------------------------------------------------------------
namespace fake_arduino {

struct Clock {
  unsigned long now = 0;
  unsigned long tick_per_call = 1;  // ms added on each millis() call
};
inline Clock& clock_state() {
  static Clock c;
  return c;
}

// Records of pin writes, so tests can assert on reset sequencing if needed.
struct PinState {
  std::vector<int> modes;   // pin -> last mode
  std::vector<int> values;  // pin -> last value
  void ensure(int pin) {
    if (pin < 0) return;
    if (static_cast<size_t>(pin) >= modes.size()) {
      modes.resize(pin + 1, -1);
      values.resize(pin + 1, -1);
    }
  }
};
inline PinState& pins() {
  static PinState p;
  return p;
}

inline void reset_clock() { clock_state().now = 0; }

}  // namespace fake_arduino

inline unsigned long millis() {
  auto& c = fake_arduino::clock_state();
  unsigned long v = c.now;
  c.now += c.tick_per_call;
  return v;
}
inline unsigned long micros() { return millis() * 1000UL; }

inline void delay(unsigned long ms) { fake_arduino::clock_state().now += ms; }
inline void delayMicroseconds(unsigned int us) {
  fake_arduino::clock_state().now += (us / 1000);
}

inline void pinMode(int pin, int mode) {
  auto& p = fake_arduino::pins();
  p.ensure(pin);
  if (pin >= 0) p.modes[pin] = mode;
}
inline void digitalWrite(int pin, int value) {
  auto& p = fake_arduino::pins();
  p.ensure(pin);
  if (pin >= 0) p.values[pin] = value;
}
inline int digitalRead(int pin) {
  auto& p = fake_arduino::pins();
  p.ensure(pin);
  return (pin >= 0 && p.values[pin] >= 0) ? p.values[pin] : HIGH;
}

inline void yield() {}

// ---------------------------------------------------------------------------
// Print / Stream — just enough of the Arduino shape that the integer-typed
// write() calls in the library (e.g. `_wire->write(len >> 8)`, which is `int`)
// bind unambiguously, exactly as they do against the real Arduino core.
// ---------------------------------------------------------------------------
class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
  size_t write(const char* str) {
    return str ? write(reinterpret_cast<const uint8_t*>(str), strlen(str)) : 0;
  }
  size_t write(const char* buffer, size_t size) {
    return write(reinterpret_cast<const uint8_t*>(buffer), size);
  }
  // Arduino's Print provides these so that plain `int` literals/expressions
  // don't become ambiguous between write(uint8_t) and write(const char*).
  size_t write(long n) { return write(static_cast<uint8_t>(n)); }
  size_t write(unsigned long n) { return write(static_cast<uint8_t>(n)); }
  size_t write(int n) { return write(static_cast<uint8_t>(n)); }
  size_t write(unsigned int n) { return write(static_cast<uint8_t>(n)); }
  virtual void flush() {}
};

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  void setTimeout(unsigned long t) { timeout_ = t; }
  unsigned long getTimeout() const { return timeout_; }

  virtual size_t readBytes(char* buffer, size_t length) {
    size_t n = 0;
    while (n < length) {
      int c = read();
      if (c < 0) break;
      buffer[n++] = static_cast<char>(c);
    }
    return n;
  }
  size_t readBytes(uint8_t* buffer, size_t length) {
    return readBytes(reinterpret_cast<char*>(buffer), length);
  }

 protected:
  unsigned long timeout_ = 1000;
};

// ---------------------------------------------------------------------------
// HardwareSerial — the controllable fake transport.
//
//   * RX is a deque of "chunks". available() reports the size of the FRONT
//     chunk only, and readBytes() drains only the front chunk. This models the
//     module answering one message at a time, and (importantly) it is what lets
//     SSCMA::begin() work: begin() issues AT+ID? then AT+NAME?, and each wait()
//     only re-scans rx_buf after a fresh read. If both replies were handed over
//     in a single available()/read(), wait() would consume both into rx_buf,
//     match ID?, and then NAME?'s wait() would spin on available()==0 forever
//     because it never re-scans the already-buffered bytes. Feed one chunk per
//     reply.
//   * TX is captured into `tx` for assertions.
// ---------------------------------------------------------------------------
class HardwareSerial : public Stream {
 public:
  // ---- test-facing API ----------------------------------------------------
  // Queue one "read burst". available() will report exactly this many bytes
  // until it is drained.
  void feed(const std::string& chunk) {
    if (!chunk.empty()) rx_.push_back(chunk);
  }
  // Frame a payload the way the SSCMA module does: "\r" + json + "}\n" already
  // included by the caller. Convenience wrapper below builds the whole frame.
  void feedFrame(const std::string& json_body) {
    feed("\r" + json_body + "\n");
  }
  // Build+queue a complete reply: {"type":T,"name":"N","code":C,"data":D}
  void feedReply(int type, const std::string& name, int code,
                 const std::string& data_json) {
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "{\"type\":%d,\"name\":\"%s\",\"code\":%d,\"data\":",
             type, name.c_str(), code);
    feedFrame(std::string(hdr) + data_json + "}");
  }

  std::string tx;  // everything the library wrote

  void clearTx() { tx.clear(); }
  void clearRx() { rx_.clear(); }
  size_t pendingChunks() const { return rx_.size(); }

  bool begun = false;
  unsigned long baud = 0;
  int flush_count = 0;

  // ---- Arduino-facing API -------------------------------------------------
  void begin(unsigned long b) {
    begun = true;
    baud = b;
  }
  void begin(unsigned long b, uint32_t /*config*/) { begin(b); }
  void end() { begun = false; }

  int available() override {
    return rx_.empty() ? 0 : static_cast<int>(rx_.front().size());
  }

  int read() override {
    if (rx_.empty()) return -1;
    std::string& f = rx_.front();
    char c = f.front();
    f.erase(f.begin());
    if (f.empty()) rx_.pop_front();
    return static_cast<unsigned char>(c);
  }

  int peek() override {
    if (rx_.empty()) return -1;
    return static_cast<unsigned char>(rx_.front().front());
  }

  size_t readBytes(char* buffer, size_t length) override {
    size_t n = 0;
    while (n < length && !rx_.empty()) {
      std::string& f = rx_.front();
      size_t take = f.size() < (length - n) ? f.size() : (length - n);
      memcpy(buffer + n, f.data(), take);
      f.erase(0, take);
      n += take;
      if (f.empty()) rx_.pop_front();
      // Only ever drain one chunk per call: mirrors "one burst per read".
      break;
    }
    return n;
  }

  size_t write(uint8_t c) override {
    tx.push_back(static_cast<char>(c));
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) override {
    tx.append(reinterpret_cast<const char*>(buffer), size);
    return size;
  }
  using Print::write;  // keep the int/long/const char* overloads visible

  void flush() override { flush_count++; }

 private:
  std::deque<std::string> rx_;
};

// Not used by the library on the host path, but examples reference it.
extern HardwareSerial Serial;
