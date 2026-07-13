// Wire.h — host stub of Arduino TwoWire.
//
// Recording fake: every transmission is captured, and the RX side is fed from a
// deque of chunks so i2c tests can script the module's answers.
#pragma once

#include <Arduino.h>

#include <deque>
#include <string>
#include <vector>

class TwoWire : public Stream {
 public:
  // ---- test-facing ---------------------------------------------------------
  struct Transmission {
    uint16_t address = 0;
    std::string data;
    uint8_t result = 0;
  };

  std::vector<Transmission> transmissions;  // completed endTransmission()s
  std::deque<std::string> rx_chunks;        // scripted slave responses
  uint8_t next_end_result = 0;              // what endTransmission() returns

  bool begun = false;
  uint32_t clock_hz = 0;
  size_t buffer_size = 128;

  void feed(const std::string& chunk) {
    if (!chunk.empty()) rx_chunks.push_back(chunk);
  }

  // ---- Arduino-facing ------------------------------------------------------
  void begin() { begun = true; }
  void begin(int /*sda*/, int /*scl*/) { begun = true; }
  void end() { begun = false; }

  void setClock(uint32_t hz) { clock_hz = hz; }
  bool setBufferSize(size_t size) {
    buffer_size = size;
    return true;
  }

  void beginTransmission(uint16_t address) {
    cur_.address = address;
    cur_.data.clear();
    in_transmission_ = true;
  }
  void beginTransmission(int address) {
    beginTransmission(static_cast<uint16_t>(address));
  }

  uint8_t endTransmission(bool /*stop*/ = true) {
    cur_.result = next_end_result;
    transmissions.push_back(cur_);
    cur_ = Transmission{};
    in_transmission_ = false;
    return next_end_result;
  }

  size_t requestFrom(uint16_t address, uint8_t quantity) {
    last_request_address = address;
    last_request_len = quantity;
    return available() < quantity ? available() : quantity;
  }
  size_t requestFrom(int address, int quantity) {
    return requestFrom(static_cast<uint16_t>(address),
                       static_cast<uint8_t>(quantity));
  }

  uint16_t last_request_address = 0;
  size_t last_request_len = 0;

  // Stream
  int available() override {
    return rx_chunks.empty() ? 0 : static_cast<int>(rx_chunks.front().size());
  }
  int read() override {
    if (rx_chunks.empty()) return -1;
    std::string& f = rx_chunks.front();
    char c = f.front();
    f.erase(f.begin());
    if (f.empty()) rx_chunks.pop_front();
    return static_cast<unsigned char>(c);
  }
  int peek() override {
    if (rx_chunks.empty()) return -1;
    return static_cast<unsigned char>(rx_chunks.front().front());
  }

  // Print
  size_t write(uint8_t c) override {
    if (in_transmission_) cur_.data.push_back(static_cast<char>(c));
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) override {
    if (in_transmission_)
      cur_.data.append(reinterpret_cast<const char*>(buffer), size);
    return size;
  }
  using Print::write;

 private:
  Transmission cur_;
  bool in_transmission_ = false;
};

extern TwoWire Wire;
