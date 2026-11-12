#pragma once

#include <Arduino.h>
#include <time.h>

#include <FlovaDevice.h>

class ArduinoFlovaClock : public flova::Clock {
 public:
  uint64_t milliseconds() const override {
    const uint32_t now = millis();
    // Board-loop owned; sample at least once per 32-bit tick cycle.
    if (now < previousMs_) wrapMs_ += (1ULL << 32);
    previousMs_ = now;
    return wrapMs_ + now;
  }
  bool utcValid() const override { return utcBaseMs_ != 0; }
  uint64_t utcMilliseconds() const override {
    return utcValid() ? utcBaseMs_ + (milliseconds() - monotonicBaseMs_) : 0;
  }
  void setUtc(uint64_t value, uint64_t) override {
    utcBaseMs_ = value;
    monotonicBaseMs_ = milliseconds();
  }

 private:
  uint64_t utcBaseMs_ = 0;
  uint64_t monotonicBaseMs_ = 0;
  mutable uint64_t wrapMs_ = 0;
  mutable uint32_t previousMs_ = 0;
};

// The default client is intentionally volatile. Applications that need local
// persistence inject their own flova::Storage implementation instead of
// making the SDK guess which filesystem or wear policy their board uses.
class ArduinoFlovaStorage : public flova::Storage {
 public:
  bool read(const char*, void*, size_t) override { return false; }
  bool write(const char*, const void*, size_t) override { return false; }
  bool remove(const char*) override { return true; }
};

class ArduinoFlovaLogger : public flova::Logger {
 public:
  void log(const char* message) override {
    if (message) Serial.println(message);
  }
};
