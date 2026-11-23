#pragma once

#include <stddef.h>
#include <stdint.h>

namespace flova {

// Cooperative timers. Own an instance and drive it from one application loop
// with a monotonic millisecond count (for example, Arduino millis()).
template <size_t Capacity = 16>
class Timer {
  static_assert(Capacity >= 1 && Capacity <= 16, "Flova Timer capacity must be 1..16");

 public:
  using Id = uint32_t;
  using Callback = void (*)();

  Timer() = default;
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  // Zero means invalid input or no available slot/ID. Timing starts on the
  // first run after registration, not in the call that registers the timer.
  Id setInterval(uint32_t periodMs, Callback callback) {
    return add(periodMs, callback, Mode::Interval);
  }

  Id setTimeout(uint32_t delayMs, Callback callback) {
    return add(delayMs, callback, Mode::Timeout);
  }

  bool cancel(Id id) {
    if (!id) return false;
    for (auto& entry : entries_) {
      if (entry.id != id) continue;
      entry.id = 0;
      return true;
    }
    return false;
  }

  // No network or wall clock required. Call regularly (at least once per
  // INT32_MAX milliseconds) so unsigned elapsed time survives rollover.
  // Callbacks run synchronously and may register/cancel timers, but not throw
  // or destroy this Timer. Registrations from a callback start on the next run.
  void run(uint32_t nowMs) {
    if (running_) return;
    running_ = true;
    for (auto& entry : entries_) {
      if (entry.id && !entry.armed) {
        entry.lastMs = nowMs;
        entry.armed = true;
      }
    }
    for (auto& entry : entries_) {
      if (!entry.id || !entry.armed) continue;
      const uint32_t elapsed = nowMs - entry.lastMs;
      if (elapsed < entry.periodMs) continue;

      const auto callback = entry.callback;
      if (entry.mode == Mode::Timeout) {
        entry.id = 0;
      } else {
        // Keep the original cadence without replaying missed ticks.
        entry.lastMs = nowMs - elapsed % entry.periodMs;
      }
      callback();
    }
    running_ = false;
  }

 private:
  enum class Mode : uint8_t { Interval, Timeout };

  struct Entry {
    Callback callback;
    Id id;
    uint32_t periodMs;
    uint32_t lastMs;
    Mode mode;
    bool armed;
  };

  Id add(uint32_t periodMs, Callback callback, Mode mode) {
    if (!periodMs || periodMs > INT32_MAX || !callback || nextId_ == UINT32_MAX) return 0;
    for (auto& entry : entries_) {
      if (entry.id) continue;
      // Do not recycle IDs when a slot is reused: a stale ID cannot cancel a
      // different timer.
      entry = {callback, ++nextId_, periodMs, 0, mode, false};
      return entry.id;
    }
    return 0;
  }

  Entry entries_[Capacity] = {};
  Id nextId_ = 0;
  bool running_ = false;
};

}  // namespace flova

#define FLOVA_INTERVAL(timer, period_ms, ...) ((timer).setInterval((period_ms), (__VA_ARGS__)))
#define FLOVA_TIMEOUT(timer, delay_ms, ...) ((timer).setTimeout((delay_ms), (__VA_ARGS__)))
