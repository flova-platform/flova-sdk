#pragma once

#include <stddef.h>
#include <stdint.h>

namespace flova {

// ESP8266 Update.end(false) commits a complete image. Keep its last byte out
// of flash until the caller has verified the digest, so abort always cancels.
template <typename Updater>
class Esp8266OtaWriter {
 public:
  Esp8266OtaWriter(Updater& updater, size_t size)
      : updater_(updater), remaining_(size), failed_(size == 0) {}

  bool write(uint8_t* data, size_t length) {
    if (failed_ || !data || !length || length > remaining_) {
      failed_ = true;
      return false;
    }
    const size_t flashLength = length == remaining_ ? length - 1 : length;
    if (flashLength && updater_.write(data, flashLength) != flashLength) {
      failed_ = true;
      return false;
    }
    if (length == remaining_) lastByte_ = data[length - 1];
    remaining_ -= length;
    return true;
  }

  void abort() {
    failed_ = true;
    updater_.end(false);
  }

  bool finish(bool verified) {
    if (!verified || failed_ || remaining_) {
      abort();
      return false;
    }
    if (updater_.write(&lastByte_, 1) != 1) {
      abort();
      return false;
    }
    return updater_.end(false);
  }

 private:
  Updater& updater_;
  size_t remaining_;
  uint8_t lastByte_ = 0;
  bool failed_ = false;
};

}  // namespace flova
