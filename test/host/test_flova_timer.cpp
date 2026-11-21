#include <FlovaTimer.h>

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do {                              \
  if (!(condition)) {                                      \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
    abort();                                               \
  }                                                        \
} while (false)

namespace {

unsigned samples = 0;
unsigned timeouts = 0;
void sample() { ++samples; }
void timeout() { ++timeouts; }

void checkBoundedTimers() {
  flova::Timer<2> timers;
  CHECK(!FLOVA_INTERVAL(timers, 0, sample));
  CHECK(!FLOVA_TIMEOUT(timers, 0x80000000U, timeout));
  CHECK(!timers.setTimeout(1, nullptr));
  CHECK(!timers.cancel(0));
  const auto interval = FLOVA_INTERVAL(timers, 10, sample);
  const auto once = FLOVA_TIMEOUT(timers, 30, timeout);
  CHECK(interval && once && interval != once);
  CHECK(!FLOVA_INTERVAL(timers, 1, sample));

  // Arm just before rollover. The first dispatch is at the exact deadline.
  timers.run(UINT32_MAX - 8);
  timers.run(0);
  CHECK(samples == 0 && timeouts == 0);
  timers.run(1);
  CHECK(samples == 1 && timeouts == 0);

  timers.run(44);
  CHECK(samples == 2 && timeouts == 1);  // missed ticks collapsed into one
  timers.run(50);
  CHECK(samples == 2 && timeouts == 1);
  timers.run(51);
  CHECK(samples == 3 && timeouts == 1);  // original cadence retained

  const auto replacement = timers.setTimeout(INT32_MAX, timeout);
  CHECK(replacement && replacement != once);
  CHECK(!timers.cancel(once));
  CHECK(timers.cancel(interval));
  timers.run(52);
  timers.run(52U + INT32_MAX - 1U);
  CHECK(samples == 3 && timeouts == 1);
  timers.run(52U + INT32_MAX);
  CHECK(samples == 3 && timeouts == 2);
}

flova::Timer<2> callbackTimers;
flova::Timer<2>::Id canceled;

void replaceTimer() {
  CHECK(callbackTimers.cancel(canceled));
  CHECK(FLOVA_TIMEOUT(callbackTimers, 1, timeout));
  CHECK(FLOVA_TIMEOUT(callbackTimers, 1, timeout));
  callbackTimers.run(1000);  // a reentrant run must not arm the new entries
}

void checkCallbackMutation() {
  CHECK(callbackTimers.setTimeout(1, replaceTimer));
  canceled = callbackTimers.setTimeout(1, sample);
  CHECK(canceled);
  callbackTimers.run(0);
  callbackTimers.run(1);
  callbackTimers.run(2);
  CHECK(samples == 3 && timeouts == 2);
  callbackTimers.run(3);
  CHECK(samples == 3 && timeouts == 4);
}

void checkDefaultCapacity() {
  flova::Timer<> timers;
  for (unsigned i = 0; i < 16; ++i) {
    CHECK(FLOVA_INTERVAL(timers, 1, [] {
      const unsigned first = 1, second = 2;
      samples += first + second;
    }));
  }
  CHECK(!timers.setInterval(1, sample));
  timers.run(0);
  timers.run(500);
  CHECK(samples == 3 + 16 * 3);
}

}  // namespace

int main() {
  checkBoundedTimers();
  checkCallbackMutation();
  checkDefaultCapacity();
}
