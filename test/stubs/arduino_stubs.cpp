// arduino_stubs.cpp — definitions for the global instances the Arduino core
// normally provides. SSCMA::begin(TwoWire*) defaults its first argument to
// `&Wire`, so `Wire` must exist even for serial-only tests.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

HardwareSerial Serial;
TwoWire Wire;
SPIClass SPI;
