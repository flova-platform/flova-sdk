#pragma once

#include "FlovaLinkMessages.h"

namespace flova {
// RAM-only, fixed capacity. Never writes flash or blocks runtime work. Preserve
// the oldest events (including boot); overflow is explicit in later uploads.
class LifecycleLog {
 public:
  static const size_t capacity = 16;
  void begin(uint64_t bootId) { bootId_ = bootId; }
  uint64_t bootId() const { return bootId_; }
  // Both log streams use Engine's (device, boot, sequence) deduplication key.
  uint32_t nextSequence() { return ++sequence_; }
  bool append(uint8_t code, uint32_t detail, uint32_t generation, uint64_t uptime) {
    // Internal stage, ready/link duplication and connection attempt counters
    // belong to local status callbacks, not the production operational log.
    if (code == 9 || code == 3 || code == 4 || code == 5 || (code == 1 && detail != 10)) return true;
    if ((code == 8 || code == 10) && warningSeen_[code == 10] &&
        static_cast<uint32_t>(uptime) - warningAt_[code == 10] < 60000) return true;
    if (code == 8 || code == 10) {
      warningSeen_[code == 10] = true;
      warningAt_[code == 10] = static_cast<uint32_t>(uptime);
    }
    const uint32_t sequence = nextSequence();
    if (count_ == capacity) { if (dropped_ != UINT32_MAX) ++dropped_; return false; }
    FlovaLinkLifecycleReport& event = entries_[(head_ + count_) % capacity];
    event = FlovaLinkLifecycleReport();
    event.bootId = bootId_; event.sequence = sequence; event.uptimeMs = uptime;
    event.code = code; event.detail = detail; event.generation = generation;
    ++count_;
    return true;
  }
  FlovaLinkLifecycleReport* front() {
    if (!count_) return nullptr;
    entries_[head_].dropped = dropped_;
    return &entries_[head_];
  }
  void acknowledge() { if (count_) { head_ = (head_ + 1) % capacity; --count_; } }
  uint32_t dropped() const { return dropped_; }
 private:
  FlovaLinkLifecycleReport entries_[capacity] = {};
  uint64_t bootId_ = 0;
  uint32_t sequence_ = 0, dropped_ = 0;
  size_t head_ = 0, count_ = 0;
  uint32_t warningAt_[2] = {};
  bool warningSeen_[2] = {};
};
}  // namespace flova
