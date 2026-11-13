// Development-only non-GPIO acceptance. Back up and restore full flash around
// upload. This firmware has no filesystem backend or network credentials.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FlovaEsp8266BuildProfile.h>
#include <FlovaDevice.h>
#include <FlovaScheduler.h>
#include <FlovaLinkCodec.h>

namespace {
class CountingStorage : public flova::Storage {
 public:
  unsigned calls = 0;
  bool read(const char*, void*, size_t) override { ++calls; return false; }
  bool write(const char*, const void*, size_t) override { ++calls; return false; }
  bool remove(const char*) override { ++calls; return false; }
};
class TestClock : public flova::Clock, public flova::WallClock {
 public:
  bool valid = false;
  flova::LocalTime now = {2026, 11, 13, 5, 12, 0};
  uint64_t milliseconds() const override { return millis(); }
  bool utcValid() const override { return valid; }
  uint64_t utcMilliseconds() const override { return 1800000000000ULL + millis(); }
  bool synchronized() const override { return valid; }
  bool localTime(const char*, flova::LocalTime& out) const override { out = now; return valid; }
};
class LocalLink : public flova::Link {
 public:
  bool begin() override { return true; }
  bool connected() const override { return false; }
  bool send(const flova::Message&) override { return false; }
  void poll() override {}
  void setReceiver(flova::MessageReceiver, void*) override {}
  uint32_t messageNonce() const override { return 1; }
  bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override {
    for (size_t i = 0; i < count; ++i) ids[i] = i + 1;
    return true;
  }
};
class Logger : public flova::Logger { void log(const char*) override {} };
CountingStorage storage;
TestClock clockSource;
LocalLink link;
Logger logger;
flova::Device device(link, storage, clockSource, logger);
flova::Scheduler timers(clockSource);
unsigned callbacks = 0, acceptedWrites = 0;
bool rejectWrite = false, nextValue = true;
flova::WriteResult onWrite(bool) {
  if (rejectWrite) return flova::WriteResult::reject("test_rejection");
  ++acceptedWrites;
  return flova::WriteResult::accept();
}
void timerAction() {
  ++callbacks;
  device.write(1, flova::Value::from(nextValue), flova::Origin::LocalLogic);
}
bool check(bool passed, const char* name) {
  Serial.printf("FLOVA_SCHEDULE_TEST {\"check\":\"%s\",\"ok\":%u}\n", name, passed);
  return passed;
}
bool runChecks() {
  auto relay = device.datastream<bool>("memory_only");
  relay.onWrite(onWrite).scheduling(flova::Scheduling::CloudOnly);
  if (!check(device.begin() && device.scheduling(1) == flova::Scheduling::CloudOnly,
             "cloud_permission_is_explicit")) return false;
  storage.calls = 0;
  if (!check(timers.daily(12, 0, "UTC0", timerAction), "local_timer_registered")) return false;
  timers.run();
  if (!check(callbacks == 0, "invalid_clock_blocks_execution")) return false;
  clockSource.valid = true;
  timers.run(); timers.run();
  if (!check(callbacks == 1 && acceptedWrites == 1 && relay.value() && !link.connected(),
             "local_timer_once_without_network")) return false;
  clockSource.now.day = 14;
  rejectWrite = true; nextValue = false;
  timers.run(); timers.run();
  rejectWrite = false;
  if (!check(callbacks == 2 && acceptedWrites == 1 && relay.value(),
             "safety_rejection_preserves_state")) return false;
  clockSource.now.day = 15;
  clockSource.now.minute = 1;
  timers.run();
  if (!check(callbacks == 2, "missed_minute_not_replayed")) return false;
  clockSource.now.day = 16;
  clockSource.now.minute = 0;
  timers.run();
  if (!check(callbacks == 3 && acceptedWrites == 2, "next_day_runs")) return false;
  flova::Scheduler fresh(clockSource);
  fresh.run();
  if (!check(callbacks == 3, "new_runtime_has_no_restored_timers")) return false;
  for (uint8_t code : {0x15, 0x16, 0x24, 0x2b, 0x2c, 0x30, 0x31, 0x32})
    if (!check(!flova::link::isKnownMessageType(code), "schedule_transfer_opcode_retired")) return false;
  return check(storage.calls == 0, "local_timers_zero_storage_operations");
}
}

void setup() {
  Serial.begin(115200); delay(300);
  WiFi.persistent(false); WiFi.mode(WIFI_OFF);
  Serial.println("FLOVA_SCHEDULE_TEST {\"event\":\"boot\"}");
  const bool passed = runChecks();
  Serial.printf("FLOVA_SCHEDULE_TEST {\"event\":\"%s\",\"heap\":%u,\"largest\":%u,\"stack\":%u}\n",
                passed ? "passed" : "failed", ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), ESP.getFreeContStack());
}
void loop() { delay(100); }
