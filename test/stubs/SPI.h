// SPI.h — host stub of Arduino SPIClass.
//
// transfer(void*, size_t) is full-duplex in Arduino: the buffer is overwritten
// in place with the bytes clocked in. The fake captures the outgoing bytes into
// `mosi` and overwrites the buffer from the scripted `miso` stream, so SPI
// tests can script module replies.
#pragma once

#include <Arduino.h>

#include <string>

#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C

class SPISettings {
 public:
  SPISettings() = default;
  SPISettings(uint32_t clock, uint8_t bit_order, uint8_t data_mode)
      : clock(clock), bit_order(bit_order), data_mode(data_mode) {}
  uint32_t clock = 1000000;
  uint8_t bit_order = MSBFIRST;
  uint8_t data_mode = SPI_MODE0;
};

class SPIClass {
 public:
  bool begun = false;
  int transaction_depth = 0;
  SPISettings last_settings;

  std::string mosi;  // everything clocked out
  std::string miso;  // scripted bytes clocked in (consumed front-first)

  void feed(const std::string& bytes) { miso += bytes; }

  void begin() { begun = true; }
  void begin(int /*sck*/, int /*miso*/, int /*mosi*/, int /*ss*/) {
    begun = true;
  }
  void end() { begun = false; }

  void beginTransaction(SPISettings settings) {
    last_settings = settings;
    transaction_depth++;
  }
  void endTransaction() { transaction_depth--; }

  uint8_t transfer(uint8_t data) {
    mosi.push_back(static_cast<char>(data));
    return shiftIn();
  }

  uint16_t transfer16(uint16_t data) {
    mosi.push_back(static_cast<char>(data >> 8));
    mosi.push_back(static_cast<char>(data & 0xFF));
    uint16_t hi = shiftIn();
    uint16_t lo = shiftIn();
    return static_cast<uint16_t>((hi << 8) | lo);
  }

  void transfer(void* buf, size_t count) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    mosi.append(reinterpret_cast<const char*>(p), count);
    for (size_t i = 0; i < count; i++) p[i] = shiftIn();
  }

  void transfer(const void* tx, void* rx, size_t count) {
    const uint8_t* t = static_cast<const uint8_t*>(tx);
    uint8_t* r = static_cast<uint8_t*>(rx);
    mosi.append(reinterpret_cast<const char*>(t), count);
    for (size_t i = 0; i < count; i++) r[i] = shiftIn();
  }

 private:
  size_t miso_pos_ = 0;
  uint8_t shiftIn() {
    if (miso_pos_ < miso.size())
      return static_cast<uint8_t>(miso[miso_pos_++]);
    return 0xFF;  // idle line
  }
};

extern SPIClass SPI;
