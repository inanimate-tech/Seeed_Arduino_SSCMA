// WString.h — host stub of the Arduino `String` class.
//
// ArduinoJson (with ARDUINOJSON_ENABLE_ARDUINO_STRING=1) requires EXACTLY this
// surface from ::String:
//   * const char* c_str() const                    -> string_traits::has_cstr
//   * <unsigned> length() const                    -> string_traits::has_length
//   * String& operator=(const char*)               -> convertFromJson()
//     ...and it MUST tolerate a nullptr, because Writer<::String>'s ctor does
//     `str = static_cast<const char*>(0);`
//   * bool concat(const char*)                     -> Writer<::String>::flush()
//   * copy ctor / copy assign / default ctor
// Everything else below is convenience for the library + tests.
#pragma once

#include <stddef.h>

#include <string>

class String {
 public:
  String() = default;
  String(const char* s) {
    if (s) s_ = s;
  }
  String(const std::string& s) : s_(s) {}
  String(const String&) = default;
  String(String&&) = default;
  String& operator=(const String&) = default;
  String& operator=(String&&) = default;

  // Must accept nullptr: ArduinoJson's Writer<::String> clears via `= (const
  // char*)0`.
  String& operator=(const char* s) {
    if (s)
      s_ = s;
    else
      s_.clear();
    return *this;
  }

  // ArduinoJson's Writer<::String>::flush() calls concat() and treats the
  // return value as "did it fit".
  bool concat(const char* s) {
    if (s) s_ += s;
    return true;
  }
  bool concat(const String& s) {
    s_ += s.s_;
    return true;
  }

  String& operator+=(const char* s) {
    concat(s);
    return *this;
  }
  String& operator+=(const String& s) {
    concat(s);
    return *this;
  }

  const char* c_str() const { return s_.c_str(); }
  // NOTE: must be an *unsigned* type for ArduinoJson's has_length trait.
  unsigned int length() const { return static_cast<unsigned int>(s_.length()); }
  bool isEmpty() const { return s_.empty(); }

  char operator[](size_t i) const { return s_[i]; }

  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return o && s_ == o; }
  bool operator!=(const String& o) const { return !(*this == o); }
  bool operator!=(const char* o) const { return !(*this == o); }

  // Test-side escape hatch.
  const std::string& std_str() const { return s_; }

 private:
  std::string s_;
};
