#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <string.h>
#include <FlovaCustomCode.h>
#include <FlovaDevice.h>
#include <FlovaFactoryResetGesture.h>
#include <FlovaProvisioning.h>
#include <FlovaScheduler.h>

class TestLink : public flova::Link {
 public:
  bool online = true; int sends = 0, stateSends = 0, errorSends = 0; double stateValues[64]; flova::Message last, lastError, lastResult; flova::MessageReceiver receiver = 0; void* context = 0;
  bool begin() override { return true; }
  bool connected() const override { return online; }
  bool send(const flova::Message& message) override { sends++; last = message; if (message.kind == flova::MessageKind::Acknowledgement || message.kind == flova::MessageKind::Error) lastResult = message; if (message.kind == flova::MessageKind::StateUpdate) { if (message.value.type == flova::ValueType::Float) stateValues[stateSends] = message.value.scalar.floating; stateSends++; } if (message.kind == flova::MessageKind::Error) { errorSends++; lastError = message; } return online; }
  bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override { boundCount = count; for (size_t i = 0; i < count; ++i) ids[i] = static_cast<DatastreamId>(i + 1); return count <= FLOVA_MAX_ACTIVE_DATASTREAMS; }
  void poll() override {}
  void setReceiver(flova::MessageReceiver callback, void* value) override { receiver = callback; context = value; }
  uint32_t messageNonce() const override { return 0x12345678UL; }
  void inject(const flova::Message& message) { receiver(context, message); }
  void acknowledge(uint64_t messageId) {
    flova::Message message;
    message.kind = flova::MessageKind::Acknowledgement;
    message.messageId = messageId;
    inject(message);
  }
  size_t boundCount = 0;
};
class InvalidBindingLink : public TestLink { public: bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override { if (count) ids[0] = FLOVA_INVALID_DATASTREAM_ID; return true; } };
class DuplicateBindingLink : public TestLink { public: bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override { for (size_t i = 0; i < count; ++i) ids[i] = 1; return true; } };
class AsyncBindingLink : public TestLink {
 public:
  bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override {
    boundCount = count;
    for (size_t i = 0; i < count; ++i) { ids[i] = FLOVA_INVALID_DATASTREAM_ID; pendingIds[i] = static_cast<DatastreamId>(i + 10); }
    return count <= FLOVA_MAX_ACTIVE_DATASTREAMS;
  }
  bool bindingReady() const override { return ready; }
  bool readDatastreamBinding(DatastreamId* ids, size_t count) const override {
    if (!ready || count != boundCount) return false;
    for (size_t i = 0; i < count; ++i) ids[i] = pendingIds[i];
    return true;
  }
  void poll() override { if (autoReady) ready = true; }
  bool ready = false;
  bool autoReady = true;
  DatastreamId pendingIds[FLOVA_MAX_ACTIVE_DATASTREAMS] = {};
};
class TestStorage : public flova::Storage { public:
  std::map<std::string, std::vector<uint8_t>> records;
  std::map<std::string, unsigned> reads, writes, removals;
  bool failWrites = false;
  bool read(const char* key, void* out, size_t size) override {
    ++reads[key];
    auto found = records.find(key);
    if (found == records.end() || found->second.size() != size) return false;
    memcpy(out, found->second.data(), size); return true;
  }
  bool write(const char* key, const void* data, size_t size) override {
    ++writes[key];
    if (failWrites) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    records[key] = std::vector<uint8_t>(bytes, bytes + size); return true;
  }
  bool remove(const char* key) override { ++removals[key]; records.erase(key); return true; } flova::StorageCapabilities capabilities() const override { flova::StorageCapabilities value; value.usableBytes = 65536; value.availableBytes = 65536; value.persistent = true; return value; } };
class PersistentTestStorage : public TestStorage {
 public:
  bool read(const char* key, void* output, size_t size) override {
    if (strcmp(key, "dsid:1")) return TestStorage::read(key, output, size);
    if (!present || size != recordSize) return false;
    memcpy(output, record, size);
    return true;
  }

  bool write(const char* key, const void* input, size_t size) override {
    if (strcmp(key, "dsid:1")) return TestStorage::write(key, input, size);
    if (!strcmp(key, "dsid:1") && size <= sizeof(record)) {
      memcpy(record, input, size);
      recordSize = size;
      present = true;
    }
    return true;
  }

 private:
  uint8_t record[256] = {};
  size_t recordSize = 0;
  bool present = false;
};
class TestClock : public flova::Clock { public: uint64_t now = 1, utc = 0; uint64_t milliseconds() const override { return now; } bool utcValid() const override { return utc != 0; } uint64_t utcMilliseconds() const override { return utc + now; } void setUtc(uint64_t value, uint64_t) override { utc = value - now; } };
class TestLogger : public flova::Logger { public: void log(const char*) override {} };

static int writes;
static int factoryResets;
static flova::WriteResult requestFactoryReset(void*, const char*) { factoryResets++; return flova::WriteResult::accept(); }
static int numericWrites;
static int voidWrites;
static bool reject;
static bool noChange;
static flova::WriteResult writeRelay(bool) { writes++; return reject ? flova::WriteResult::reject("safety_lock") : noChange ? flova::WriteResult::noChange() : flova::WriteResult::accept(); }
static flova::WriteResult writeSetpoint(double) { numericWrites++; return flova::WriteResult::accept(); }
  static void writeCounter(int64_t) { voidWrites++; }

class SessionStorage : public flova::Storage { public: flova::ProvisioningSession session; bool present = false; bool read(const char* key, void* out, size_t size) override { if (!present || strcmp(key, "session") || size != sizeof(session)) return false; memcpy(out, &session, size); return true; } bool write(const char* key, const void* value, size_t size) override { if (!strcmp(key, "session") && size == sizeof(session)) { memcpy(&session, value, size); present = true; } return true; } bool remove(const char*) override { return true; } };
class TestEngine : public flova::LinkBootstrapClient { public: bool beginBootstrap(const flova::ProvisioningRequest&) override { return true; } flova::ProvisioningPoll poll(flova::ProvisioningSession& out) override { out.schemaVersion = 1; flova::Value::copy(out.deviceId, "device-1"); flova::Value::copy(out.linkUrl, "wss://link.example/api/device-link"); flova::Value::copy(out.secret, "device-secret"); out.serverUtcMs = 100000; return flova::ProvisioningPoll::Complete; } };
class TestWallClock : public flova::WallClock { public: bool valid = false; flova::LocalTime now = {2026, 7, 15, 3, 20, 0}; bool synchronized() const override { return valid; } bool localTime(const char*, flova::LocalTime& out) const override { out = now; return true; } };
static int scheduledRuns;
static void scheduledWrite() { scheduledRuns++; }
static void verifyPersistencePolicies() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto immediate = device.datastream<int64_t>("immediate");
  auto changed = device.datastream<int64_t>("changed");
  auto deferred = device.datastream<int64_t>("deferred");
  auto transient = device.datastream<int64_t>("transient");
  immediate.persist(flova::PersistencePolicy::Persistent);
  changed.persist(flova::PersistencePolicy::OnChange);
  deferred.persist(flova::PersistencePolicy::Deferred, 1000);
  assert(device.begin());
  link.online = false;
  for (unsigned i = 0; i < 2; ++i) {
    assert(immediate.report(7).accepted());
    assert(changed.report(7).accepted());
    assert(transient.report(7).accepted());
  }
  assert(storage.writes["dsid:1"] == 2); // Legacy immediate durability.
  assert(storage.writes["dsid:2"] == 1);
  assert(storage.writes["dsid:4"] == 0);
  assert(changed.report(8).accepted() && storage.writes["dsid:2"] == 2);
  for (unsigned i = 0; i < 10; ++i) {
    clock.now = 1 + i * 100;
    assert(deferred.report(i).accepted());
    device.run();
    assert(storage.writes["dsid:3"] == 0);
  }
  clock.now = 1001; device.run();
  assert(storage.writes["dsid:3"] == 1 && deferred.value() == 9);
  assert(deferred.report(9).accepted());
  clock.now = 5001; device.run();
  assert(storage.writes["dsid:3"] == 1);
  assert(deferred.report(10).accepted() && deferred.flush());
  assert(storage.writes["dsid:3"] == 2);
  assert(deferred.report(11).accepted() && device.flushPersistence());
  assert(storage.writes["dsid:3"] == 3);

  storage.failWrites = true;
  assert(changed.report(9).accepted());
  assert(changed.snapshot().quality == flova::Quality::HardwareError);
  const unsigned failedAttempts = storage.writes["dsid:2"];
  device.run();
  assert(storage.writes["dsid:2"] == failedAttempts);
  storage.failWrites = false;
  clock.now += 1000; device.run();
  assert(storage.writes["dsid:2"] == failedAttempts + 1);
  assert(changed.snapshot().quality == flova::Quality::Good);

  // Deferred state restores from the last completed flush, not unsaved RAM.
  assert(deferred.report(12).accepted());
  TestLink restoredLink; flova::Device restored(restoredLink, storage, clock, logger);
  restored.datastream<int64_t>("immediate");
  auto restoredChanged = restored.datastream<int64_t>("changed");
  auto restoredDeferred = restored.datastream<int64_t>("deferred");
  restoredChanged.persist(flova::PersistencePolicy::OnChange);
  restoredDeferred.persist(flova::PersistencePolicy::Deferred, 1000);
  assert(restored.begin());
  assert(restoredDeferred.hasValue() && restoredDeferred.value() == 11);
  const unsigned previous = storage.writes["dsid:2"];
  assert(restoredChanged.report(9).accepted());
  assert(storage.writes["dsid:2"] == previous);

  deferred.persist(flova::PersistencePolicy::None);
  clock.now += 2000; device.run();
  assert(storage.writes["dsid:3"] == 3);
  clock.now = 0xfffffff0ULL;
  deferred.persist(flova::PersistencePolicy::Deferred, 100);
  assert(deferred.report(13).accepted());
  clock.now += 99; device.run();
  assert(storage.writes["dsid:3"] == 3);
  ++clock.now; device.run();
  assert(storage.writes["dsid:3"] == 4);
  auto invalid = device.datastream<int64_t>("deferred");
  invalid.persist(flova::PersistencePolicy::Deferred, 0);
  assert(!invalid.valid());
}

static void verifyHistoryMaintenance() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto sample = device.datastream<float>("history");
  flova::HistoryRetentionPolicy retention;
  retention.maximumRecords = 2;
  retention.minimumIntervalMs = 100;
  retention.maximumAgeSeconds = 10;
  sample.offline(flova::OfflinePolicy::StoreHistory).retention(retention);
  clock.utc = 100000;
  assert(device.begin());
  link.online = false;
  assert(sample.report(1).accepted());
  clock.now = 50; assert(sample.report(2).accepted());
  assert(storage.writes["history.meta"] == 1);
  clock.now = 101; assert(sample.report(3).accepted());
  clock.now = 201; assert(sample.report(4).accepted());
  assert(storage.records.count("history:0") == 0);
  assert(storage.writes["history.meta"] == 4);
  link.online = true;
  device.run();
  const unsigned headReads = storage.reads["history:1"];
  for (unsigned i = 0; i < 100; ++i) device.run();
  assert(storage.reads["history:1"] == headReads);
  assert(link.last.kind == flova::MessageKind::StateUpdate);
  const uint64_t headId = link.last.messageId;
  storage.failWrites = true;
  link.acknowledge(headId);
  assert(storage.records.count("history:1") == 1);
  storage.failWrites = false;
  link.acknowledge(headId);
  assert(storage.records.count("history:1") == 0);
  clock.now += 20000;
  const int sent = link.stateSends;
  device.run();
  assert(device.diagnostics().expiredHistory == 1);
  assert(link.stateSends == sent); // Never send the next expired record.
  assert(storage.records.count("history:2") == 0);
}

static void verifyLocalTimers() {
  TestWallClock clock;
  flova::Scheduler scheduler(clock);
  const int before = scheduledRuns;
  assert(!scheduler.daily(24, 0, "UTC0", scheduledWrite));
  assert(!scheduler.daily(20, 60, "UTC0", scheduledWrite));
  assert(!scheduler.daily(20, 0, "", scheduledWrite));
  assert(!scheduler.daily(20, 0, "UTC0", scheduledWrite, 0x80));
  assert(scheduler.daily(20, 0, "UTC0", scheduledWrite));
  scheduler.run(); assert(scheduledRuns == before);
  clock.valid = true;
  scheduler.run(); scheduler.run(); assert(scheduledRuns == before + 1);
  clock.now.day++; clock.now.minute = 1;
  scheduler.run(); assert(scheduledRuns == before + 1); // no catch-up
  clock.now.day++; clock.now.minute = 0;
  scheduler.run(); assert(scheduledRuns == before + 2);
  clock.now.day -= 2;
  scheduler.run(); assert(scheduledRuns == before + 2); // clock correction
  flova::Scheduler reset(clock);
  reset.run(); assert(scheduledRuns == before + 2); // no persisted entries
  for (size_t i = 1; i < flova::Scheduler::kCapacity; ++i)
    assert(scheduler.daily(21, 0, "UTC0", scheduledWrite));
  assert(!scheduler.daily(21, 0, "UTC0", scheduledWrite));
  scheduledRuns = before;
}

static void verifySchedulingRules() {
  TestStorage storage;
  TestClock clock;
  TestLogger logger;
  TestLink link;
  flova::Device device(link, storage, clock, logger);
  auto relay = device.datastream<bool>("relay");
  assert(device.begin());
  assert(device.scheduling(1) == flova::Scheduling::Disabled);
  relay.scheduling(flova::Scheduling::CloudOnly);
  assert(!relay.valid()); // declarations are immutable after startup

  TestLink cloudLink;
  flova::Device cloud(cloudLink, storage, clock, logger);
  cloud.datastream<bool>("relay").scheduling(flova::Scheduling::CloudOnly);
  assert(cloud.begin());
  assert(cloud.scheduling(1) == flova::Scheduling::CloudOnly);

  TestLink sensorLink;
  flova::Device sensor(sensorLink, storage, clock, logger);
  sensor.datastream<double>("temperature").mode(flova::Mode::Sample)
      .scheduling(flova::Scheduling::CloudOnly);
  assert(!sensor.begin());

  TestLink universalLink;
  flova::Device universal(universalLink, storage, clock, logger);
  assert(universal.allowCloudSchedulingForOutputs(true));
  flova::config::Unit output = {};
  output.kind = flova::config::UnitKind::Datastream;
  output.data.datastream.id = 10;
  strcpy(output.data.datastream.key, "output");
  output.data.datastream.hasMapping = true;
  output.data.datastream.mapping.kind = flova::config::MappingKind::DigitalOutput;
  assert(universal.applyConfigurationUnit(output));
  assert(universal.scheduling(10) == flova::Scheduling::CloudOnly);
  output.data.datastream.id = 11;
  strcpy(output.data.datastream.key, "input");
  output.data.datastream.mapping.kind = flova::config::MappingKind::DigitalInput;
  assert(universal.applyConfigurationUnit(output));
  assert(universal.scheduling(11) == flova::Scheduling::Disabled);
  assert(universal.begin());
  assert(!universal.allowCloudSchedulingForOutputs(false));

  // Removed offline modes are rejected rather than widening the declaration.
  TestLink invalidLink;
  flova::Device invalid(invalidLink, storage, clock, logger);
  auto invalidStream = invalid.datastream<bool>("relay");
  invalidStream.scheduling(static_cast<flova::Scheduling>(2));
  assert(!invalidStream.valid());
}

static void verifyFullCapacityAndIntegerSafety() {
  TestLink link;
  TestStorage storage;
  TestClock clock;
  TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  char key[49] = {};
  char value[flova::kMaxText] = {};
  memset(value, 'v', sizeof(value) - 1);
  for (unsigned i = 0; i < 64; ++i) {
    memset(key, 'k', 48);
    key[0] = 'A' + i / 26; key[1] = 'a' + i % 26;
    auto stream = device.datastream<flova::Text>(key);
    assert(stream.report(flova::Text(value)).accepted());
    assert(strcmp(stream.value().c_str(), value) == 0);
  }
  assert(device.datastreamCount() == 64 && device.begin());
  assert(!device.datastream<bool>("overflow").write(true).accepted());
  device.run();
  assert(link.boundCount == 64);

  flova::Device numeric(link, storage, clock, logger);
  auto integer = numeric.datastream<int64_t>("integer");
  auto humidityLimit = numeric.setting<float>("humidity_limit", 60.0f);
  assert(humidityLimit.valid() && humidityLimit.hasValue() && humidityLimit.value() == 60.0f);
  integer.onWrite(+[](int64_t) { return flova::accept(); });
  assert(numeric.begin());
  flova::config::Unit safety = {};
  safety.kind = flova::config::UnitKind::Safety;
  safety.data.safety.datastreamId = 1;
  safety.data.safety.policy = flova::config::SafetyPolicy::Range;
  safety.data.safety.hasMinimum = safety.data.safety.hasMaximum = true;
  safety.data.safety.minimum.kind = safety.data.safety.maximum.kind = flova::config::ValueKind::Int64;
  safety.data.safety.minimum.data.integer = INT64_MAX - 1;
  safety.data.safety.maximum.data.integer = INT64_MAX - 1;
  assert(numeric.applyConfigurationUnit(safety));
  assert(integer.write(INT64_MAX - 1).accepted());
  const auto before = integer.snapshot();
  assert(!integer.write(INT64_MAX).accepted());
  assert(integer.value() == INT64_MAX - 1 && integer.snapshot().revision == before.revision);
  assert(!integer.write(INT64_MAX - 2).accepted());

  flova::config::Unit parameter = {};
  parameter.kind = flova::config::UnitKind::Parameter;
  flova::Value::copy(parameter.data.parameter.key, "humidity_limit");
  parameter.data.parameter.valueType = static_cast<uint8_t>(flova::config::ValueKind::Float32);
  parameter.data.parameter.hasValue = true;
  parameter.data.parameter.value.kind = flova::config::ValueKind::Float32;
  parameter.data.parameter.value.data.float32 = 72.5f;
  assert(numeric.validateConfigurationUnit(parameter));
  assert(numeric.applyConfigurationUnit(parameter));
  assert(humidityLimit.value() == 72.5f);
}

static void verifyMixedNumericSafety() {
  TestLink link; TestStorage storage; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto number = device.datastream<double>("mixed-number");
  auto integer = device.datastream<int64_t>("mixed-integer");
  number.onWrite(+[](double) { return flova::accept(); });
  integer.onWrite(+[](int64_t) { return flova::accept(); });
  assert(device.begin());
  using flova::config::SafetyPolicy;
  struct Case { int64_t integer; double number; SafetyPolicy policy; bool accepted; };
  const Case numberCases[] = {
    {INT64_MAX, 9223372036854775808.0, SafetyPolicy::Maximum, false},
    {INT64_MAX, 9223372036854774784.0, SafetyPolicy::Maximum, true},
    {INT64_MAX, 9223372036854774784.0, SafetyPolicy::Minimum, false},
    {INT64_MAX, 9223372036854775808.0, SafetyPolicy::Minimum, true},
    {9007199254740993LL, 9007199254740992.0, SafetyPolicy::Minimum, false},
    {9007199254740993LL, 9007199254740994.0, SafetyPolicy::Maximum, false},
    {INT64_MIN, -9223372036854775808.0, SafetyPolicy::Minimum, true},
    {INT64_MIN, -9223372036854777856.0, SafetyPolicy::Minimum, false},
  };
  const Case integerCases[] = {
    {INT64_MAX, 9223372036854775808.0, SafetyPolicy::Minimum, false},
    {INT64_MAX, 9223372036854775808.0, SafetyPolicy::Maximum, true},
    {INT64_MIN, -9223372036854775808.0, SafetyPolicy::Minimum, true},
    {INT64_MIN, -9223372036854777856.0, SafetyPolicy::Maximum, false},
    {1, 1.5, SafetyPolicy::Minimum, false},
    {2, 1.5, SafetyPolicy::Minimum, true},
    {-1, -1.5, SafetyPolicy::Maximum, false},
    {-2, -1.5, SafetyPolicy::Maximum, true},
    {9007199254740993LL, 9007199254740992.0, SafetyPolicy::Maximum, false},
  };
  for (const auto& test : numberCases) {
    flova::config::Unit unit = {}; unit.kind = flova::config::UnitKind::Safety;
    unit.data.safety.datastreamId = 1; unit.data.safety.policy = test.policy;
    unit.data.safety.hasMinimum = test.policy == SafetyPolicy::Minimum;
    unit.data.safety.hasMaximum = test.policy == SafetyPolicy::Maximum;
    auto& bound = unit.data.safety.hasMinimum ? unit.data.safety.minimum : unit.data.safety.maximum;
    bound.kind = flova::config::ValueKind::Int64; bound.data.integer = test.integer;
    assert(device.applyConfigurationUnit(unit));
    const auto before = number.snapshot();
    assert(number.write(test.number).accepted() == test.accepted);
    if (!test.accepted) assert(number.snapshot().revision == before.revision);
  }
  for (const auto& test : integerCases) {
    flova::config::Unit unit = {}; unit.kind = flova::config::UnitKind::Safety;
    unit.data.safety.datastreamId = 2; unit.data.safety.policy = test.policy;
    unit.data.safety.hasMinimum = test.policy == SafetyPolicy::Minimum;
    unit.data.safety.hasMaximum = test.policy == SafetyPolicy::Maximum;
    auto& bound = unit.data.safety.hasMinimum ? unit.data.safety.minimum : unit.data.safety.maximum;
    bound.kind = flova::config::ValueKind::Float64; bound.data.float64 = test.number;
    assert(device.applyConfigurationUnit(unit));
    const auto before = integer.snapshot();
    assert(integer.write(test.integer).accepted() == test.accepted);
    if (!test.accepted) assert(integer.snapshot().revision == before.revision);
  }
}

static void verifyCustomCodeHelpers() {
  TestLink link;
  TestStorage storage;
  TestClock clock;
  TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto relay = FLOVA_DATASTREAM(device, bool, "relay");
  auto status = FLOVA_DATASTREAM(device, flova::Text, "status");

  FLOVA_ON_WRITE(relay, writeRelay);
  assert(device.begin());
  assert(!FLOVA_HAS_VALUE(relay));
  assert(FLOVA_WRITE(relay, true).accepted());
  assert(FLOVA_HAS_VALUE(relay));
  assert(FLOVA_READ(relay));
  assert(FLOVA_REPORT(status, flova::Text("ready")).accepted());
  assert(strcmp(FLOVA_READ(status).c_str(), "ready") == 0);
}

static void verifyCommandLedger() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto relay = device.datastream<bool>("relay"); relay.onWrite(writeRelay);
  assert(device.begin());
  flova::Message command;
  command.kind = flova::MessageKind::WriteRequest;
  command.datastreamId = 1; command.value = flova::Value::from(true);
  strcpy(command.commandId, "rejected");
  reject = true;
  const int before = writes;
  link.inject(command);
  const uint64_t rejectedId = link.lastResult.messageId;
  assert(writes == before + 1 && link.lastResult.kind == flova::MessageKind::Error);
  reject = false;
  link.acknowledge(rejectedId);
  // More than four other commands must not erase a retained outcome.
  for (unsigned i = 0; i < 6; ++i) {
    snprintf(command.commandId, sizeof(command.commandId), "other-%u", i);
    command.value = flova::Value::from((i % 2) == 0);
    link.inject(command); link.acknowledge(link.lastResult.messageId);
  }
  const int after = writes;
  strcpy(command.commandId, "rejected"); command.value = flova::Value::from(true);
  link.inject(command);
  assert(writes == after && link.lastResult.messageId == rejectedId);
  assert(link.lastResult.kind == flova::MessageKind::Error && !strcmp(link.lastResult.reason, "safety_lock"));
  TestLink restoredLink;
  flova::Device restored(restoredLink, storage, clock, logger);
  restored.datastream<bool>("relay").onWrite(writeRelay);
  assert(restored.begin()); restoredLink.inject(command);
  assert(writes == after && restoredLink.lastResult.messageId == rejectedId);
  assert(restoredLink.lastResult.kind == flova::MessageKind::Error);
  strcpy(command.commandId, "versioned"); command.revision = 10;
  restoredLink.inject(command); restoredLink.acknowledge(restoredLink.lastResult.messageId);
  TestLink fencedLink; flova::Device fenced(fencedLink, storage, clock, logger);
  fenced.datastream<bool>("relay").onWrite(writeRelay); assert(fenced.begin());
  const int fencedWrites = writes;
  strcpy(command.commandId, "older-version"); command.revision = 9;
  command.value = flova::Value::from(false); fencedLink.inject(command);
  assert(writes == fencedWrites && fencedLink.lastResult.kind == flova::MessageKind::Error);
  assert(!strcmp(fencedLink.lastResult.reason, "state_unavailable"));
  command.revision = 0;

  TestStorage fullStorage; TestLink fullLink;
  flova::Device full(fullLink, fullStorage, clock, logger);
  full.datastream<bool>("relay").onWrite(writeRelay); assert(full.begin());
  for (unsigned i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i) {
    snprintf(command.commandId, sizeof(command.commandId), "pending-%u", i);
    command.value = flova::Value::from((i % 2) == 0); fullLink.inject(command);
  }
  const int saturated = writes;
  strcpy(command.commandId, "overflow"); fullLink.inject(command);
  assert(writes == saturated && full.diagnostics().queueOverflow == 1);
  assert(fullLink.lastResult.kind == flova::MessageKind::Error);
  fullLink.acknowledge(fullLink.lastResult.messageId); // Best effort failure has no ledger slot.

  TestStorage failedStorage; failedStorage.failWrites = true; TestLink failedLink;
  flova::Device failed(failedLink, failedStorage, clock, logger);
  failed.datastream<bool>("relay").onWrite(writeRelay); assert(failed.begin());
  failedLink.inject(command);
  assert(writes == saturated && failedLink.lastResult.kind == flova::MessageKind::Error);
}

class InterruptedResultStorage : public TestStorage {
 public:
  bool write(const char* key, const void* value, size_t size) override {
    if (!strncmp(key, "cmd:", 4) && ++commandWrites == 2) return false;
    return TestStorage::write(key, value, size);
  }
  unsigned commandWrites = 0;
};

static void verifyInterruptedCommand() {
  InterruptedResultStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  device.datastream<bool>("relay").onWrite(writeRelay); assert(device.begin());
  flova::Message command; command.kind = flova::MessageKind::WriteRequest;
  command.datastreamId = 1; command.value = flova::Value::from(true);
  strcpy(command.commandId, "interrupted");
  const int before = writes; link.inject(command); assert(writes == before + 1);
  TestLink rebootLink; flova::Device reboot(rebootLink, storage, clock, logger);
  reboot.datastream<bool>("relay").onWrite(writeRelay); assert(reboot.begin());
  rebootLink.inject(command);
  assert(writes == before + 1 && rebootLink.lastResult.kind == flova::MessageKind::Error);
  assert(!strcmp(rebootLink.lastResult.reason, "command_outcome_unknown"));
}

static void verifyFiniteSafetyAndRestore() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  assert(!device.setting<double>("invalid", NAN).valid());
  auto setpoint = device.datastream<double>("setpoint");
  setpoint.onWrite(writeSetpoint).persist(flova::PersistencePolicy::OnChange);
  assert(device.begin());
  flova::config::Unit safety = {}; safety.kind = flova::config::UnitKind::Safety;
  safety.data.safety.datastreamId = 1; safety.data.safety.policy = flova::config::SafetyPolicy::Maximum;
  safety.data.safety.hasMaximum = true; safety.data.safety.maximum.kind = flova::config::ValueKind::Float64;
  safety.data.safety.maximum.data.float64 = 100;
  assert(device.applyConfigurationUnit(safety));
  assert(setpoint.write(90).accepted());
  const int before = numericWrites;
  const uint32_t revision = setpoint.snapshot().revision;
  assert(!setpoint.write(NAN).accepted() && !setpoint.write(INFINITY).accepted());
  assert(!setpoint.report(NAN).accepted() && setpoint.snapshot().revision == revision);
  assert(numericWrites == before && setpoint.value() == 90);
  safety.data.safety.maximum.data.float64 = NAN;
  assert(!device.validateConfigurationUnit(safety) && !device.applyConfigurationUnit(safety));

  TestLink rebootLink; flova::Device reboot(rebootLink, storage, clock, logger);
  auto restored = reboot.datastream<double>("setpoint");
  restored.onWrite(writeSetpoint).persist(flova::PersistencePolicy::OnChange);
  assert(reboot.begin());
  safety.data.safety.maximum.data.float64 = 50;
  assert(reboot.applyConfigurationUnit(safety)); reboot.run();
  assert(numericWrites == before && restored.snapshot().quality == flova::Quality::HardwareError);
  // A rejected hardware restore is not hammered on every loop iteration.
  TestStorage relayStorage; TestLink firstLink; flova::Device first(firstLink, relayStorage, clock, logger);
  first.datastream<bool>("relay").onWrite(writeRelay).persist(flova::PersistencePolicy::OnChange);
  assert(first.begin()); assert(first.write(1, flova::Value::from(true)).accepted());
  TestLink secondLink; flova::Device second(secondLink, relayStorage, clock, logger);
  second.datastream<bool>("relay").onWrite(writeRelay).persist(flova::PersistencePolicy::OnChange);
  assert(second.begin()); reject = true;
  const int restores = writes; second.run(); second.run();
  assert(writes == restores + 1); clock.now += 1000; second.run();
  assert(writes == restores + 2); reject = false;
}

static void verifyFlowControlBoundary() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto sample = device.datastream<double>("sample"); assert(device.begin());
  flova::Message control; control.kind = flova::MessageKind::FlowControl; control.retryAfterMs = 1000;
  link.inject(control); const int sent = link.stateSends;
  assert(sample.report(1).accepted()); device.run(); assert(link.stateSends == sent);
  clock.now += 1000; device.run(); assert(link.stateSends == sent + 1);
}

class BatchTestLink : public TestLink {
 public:
  bool batchesState() const override { return true; }
  size_t sendStateBatch(flova::StateUpdateReader reader, void* context, size_t count, uint64_t id) override {
    if (busy) return 0;
    ++batches; batchId = id; accepted = count < prefix ? count : prefix;
    for (size_t i = 0; i < accepted; ++i) assert(reader(context, i, ids[i], revisions[i], values[i]));
    return accepted;
  }
  bool busy = false; size_t prefix = 4, accepted = 0; unsigned batches = 0;
  uint64_t batchId = 0; DatastreamId ids[4] = {}; uint32_t revisions[4] = {}; flova::Value values[4];
};

static void verifyBatchedDelivery() {
  TestStorage storage; BatchTestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto a = device.datastream<double>("a"); auto b = device.datastream<double>("b");
  auto c = device.datastream<double>("c"); assert(device.begin());
  flova::config::Unit system = {}; system.kind = flova::config::UnitKind::System;
  system.data.system.hasBatchFlushMs = true; system.data.system.batchFlushMs = 100;
  assert(device.applyConfigurationUnit(system));
  a.report(1); b.report(2); c.report(3); device.run(); assert(link.batches == 0);
  clock.now += 100; link.busy = true; device.run(); assert(link.batches == 0);
  link.busy = false; link.prefix = 2; device.run();
  assert(link.batches == 1 && link.accepted == 2 && link.ids[0] == 1 && link.ids[1] == 2);
  const uint64_t old = link.batchId;
  a.report(4); link.acknowledge(old);
  assert(a.snapshot().dirty && !b.snapshot().dirty && c.snapshot().dirty);
  clock.now += 100; device.run(); assert(link.batches == 2 && link.accepted == 2);
  link.acknowledge(old); assert(a.snapshot().dirty && c.snapshot().dirty);
  link.acknowledge(link.batchId); assert(!a.snapshot().dirty && !c.snapshot().dirty);
}

static void verifyHistoryRebinding() {
  TestStorage storage; TestLink firstLink; TestClock clock; TestLogger logger;
  clock.utc = 100000;
  flova::Device first(firstLink, storage, clock, logger);
  auto sample = first.datastream<double>("sample"); sample.offline(flova::OfflinePolicy::StoreHistory);
  assert(first.begin()); firstLink.online = false; sample.report(1); sample.report(2);
  AsyncBindingLink link; link.autoReady = false; link.pendingIds[0] = 1;
  flova::Device restored(link, storage, clock, logger);
  restored.datastream<double>("sample").offline(flova::OfflinePolicy::StoreHistory);
  assert(restored.begin()); link.pendingIds[0] = 1; link.ready = true;
  restored.run(); assert(link.stateSends == 1); link.acknowledge(link.last.messageId);
  restored.run(); assert(link.stateSends == 2 && restored.diagnostics().storageFailures == 0);
}

static void verifyTerminalResultRejection() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  device.datastream<bool>("relay").onWrite(writeRelay);
  assert(device.begin());
  flova::Message request;
  request.kind = flova::MessageKind::WriteRequest;
  request.datastreamId = 1;
  request.value = flova::Value::from(true);
  for (unsigned i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY + 2; ++i) {
    snprintf(request.commandId, sizeof(request.commandId), "terminal-%u", i);
    request.revision = i + 1;
    link.inject(request);
    assert(device.commandResultPending(request.commandId));
    flova::Message rejected;
    rejected.kind = flova::MessageKind::Rejection;
    rejected.messageId = link.lastResult.messageId;
    storage.failWrites = true;
    link.inject(rejected);
    assert(device.commandResultPending(request.commandId));
    storage.failWrites = false;
    link.inject(rejected);
    assert(!device.commandResultPending(request.commandId));
  }
  const int before = writes;
  link.inject(request);
  assert(writes == before && !device.commandResultPending(request.commandId));
  TestLink restoredLink;
  flova::Device restored(restoredLink, storage, clock, logger);
  restored.datastream<bool>("relay").onWrite(writeRelay);
  assert(restored.begin() && !restored.commandResultPending(request.commandId));
  restoredLink.inject(request);
  assert(writes == before);
}

static void verifyResultFlowControl() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  device.datastream<bool>("relay").onWrite(writeRelay);
  assert(device.begin());
  flova::Message request;
  request.kind = flova::MessageKind::WriteRequest;
  request.datastreamId = 1; request.value = flova::Value::from(true);
  flova::Value::copy(request.commandId, "backoff-result");
  link.inject(request);
  const uint64_t resultId = link.lastResult.messageId;
  flova::Message control;
  control.kind = flova::MessageKind::FlowControl;
  control.messageId = resultId; control.retryAfterMs = 60000;
  link.inject(control);
  const int before = link.sends;
  clock.now += 5001; device.run(); link.inject(request);
  assert(link.sends == before);
  clock.now += 55000; device.run();
  assert(link.sends > before && link.lastResult.messageId == resultId);
}

static void verifyBatchRetryIdentity() {
  TestStorage storage; BatchTestLink link; TestClock clock; TestLogger logger;
  flova::Device device(link, storage, clock, logger);
  auto a = device.datastream<double>("a");
  auto b = device.datastream<double>("b");
  assert(device.begin()); a.report(1); b.report(2);
  device.run(); clock.now += 100; device.run();
  const uint64_t first = link.batchId;
  assert(first && link.accepted == 2);
  clock.now += 5001; device.run();
  assert(link.batchId == first && link.accepted == 2);
  assert(link.values[0].scalar.number == 1 && link.values[1].scalar.number == 2);
  link.acknowledge(first);
  assert(!a.snapshot().dirty && !b.snapshot().dirty);
  a.report(3); b.report(4); device.run(); clock.now += 100; device.run();
  const uint64_t second = link.batchId;
  a.report(5); clock.now += 5001; device.run(); clock.now += 100; device.run();
  assert(link.batchId != second);
  link.acknowledge(second);
  assert(a.snapshot().dirty && b.snapshot().dirty);
  link.acknowledge(link.batchId);
  assert(!a.snapshot().dirty && !b.snapshot().dirty);
}

int main() {
  verifyMixedNumericSafety();
  verifyCommandLedger();
  verifyInterruptedCommand();
  verifyFiniteSafetyAndRestore();
  verifyFlowControlBoundary();
  verifyTerminalResultRejection();
  verifyResultFlowControl();
  verifyBatchRetryIdentity();
  verifyBatchedDelivery();
  verifyHistoryRebinding();
  writes = numericWrites = 0;
  verifyPersistencePolicies();
  verifyHistoryMaintenance();
  verifySchedulingRules();
  verifyLocalTimers();
  flova::RgbColor rgb = {0, 0, 0};
  assert(flova::parseRgbHex(flova::Text("#fF8000"), rgb));
  assert(rgb.r == 255 && rgb.g == 128 && rgb.b == 0);
  assert(strcmp(flova::formatRgbHex(rgb).c_str(), "#FF8000") == 0);
  assert(!flova::parseRgbHex(flova::Text("#GG8000"), rgb));
  assert(!flova::parseRgbHex(flova::Text("#12345"), rgb));
  assert(!flova::parseRgbHex(flova::Text("#1234567"), rgb));
  assert(!flova::parseRgbHex(flova::Text("1234567"), rgb));
  verifyFullCapacityAndIntegerSafety();
  verifyCustomCodeHelpers();
  FlovaFactoryResetGesture resetGesture;
  resetGesture.configure();
  uint32_t gestureNow = 100;
  assert(resetGesture.update(false, gestureNow) == FlovaFactoryResetGesture::None);
  gestureNow += 500;
  assert(resetGesture.update(false, gestureNow) == FlovaFactoryResetGesture::Armed);
  for (int tap = 0; tap < 3; tap++) {
    gestureNow += 10; resetGesture.update(true, gestureNow);
    gestureNow += 50; resetGesture.update(true, gestureNow);
    gestureNow += 10; resetGesture.update(false, gestureNow);
    gestureNow += 50;
    assert(resetGesture.update(false, gestureNow) == FlovaFactoryResetGesture::TapAccepted);
  }
  gestureNow += 10; resetGesture.update(true, gestureNow);
  gestureNow += 50;
  assert(resetGesture.update(true, gestureNow) == FlovaFactoryResetGesture::HoldStarted);
  gestureNow += 10000;
  assert(resetGesture.update(true, gestureNow) == FlovaFactoryResetGesture::ReleaseRequested);
  gestureNow += 10; resetGesture.update(false, gestureNow);
  gestureNow += 50;
  assert(resetGesture.update(false, gestureNow) == FlovaFactoryResetGesture::Confirmed);

  FlovaFactoryResetGesture stuckGesture;
  stuckGesture.configure();
  assert(stuckGesture.update(true, 100) == FlovaFactoryResetGesture::None);
  assert(stuckGesture.update(true, 60100) == FlovaFactoryResetGesture::WindowClosed);

  FlovaFactoryResetGesture holdGesture;
  holdGesture.configure(3000, FlovaFactoryResetGesture::Hold, 1, 0, 20, false);
  uint32_t holdNow = 100;
  holdGesture.update(false, holdNow);
  holdNow += 500;
  assert(holdGesture.update(false, holdNow) == FlovaFactoryResetGesture::Armed);
  holdNow += 20;
  holdGesture.update(true, holdNow);
  holdNow += 20;
  assert(holdGesture.update(true, holdNow) == FlovaFactoryResetGesture::HoldStarted);
  holdNow += 3000;
  assert(holdGesture.update(true, holdNow) == FlovaFactoryResetGesture::Confirmed);

  FlovaFactoryResetGesture wrapGesture;
  wrapGesture.configure();
  uint32_t wrapNow = 0xFFFFFF00UL;
  wrapGesture.update(false, wrapNow);
  wrapNow += 500;
  assert(wrapGesture.update(false, wrapNow) == FlovaFactoryResetGesture::Armed);

  PersistentTestStorage persistentStorage;
  TestLink seedLink;
  TestClock seedClock;
  TestLogger seedLogger;
  flova::Device seedDevice(seedLink, persistentStorage, seedClock, seedLogger);
  flova::Datastream<bool> seedRelay = seedDevice.datastream<bool>("relay");
  seedRelay.onWrite(writeRelay)
      .persist(flova::PersistencePolicy::Persistent);
  assert(seedDevice.begin());
  reject = false;
  noChange = false;
  const int writesBeforePersist = writes;
  assert(seedRelay.write(true).accepted());
  assert(writes == writesBeforePersist + 1);

  TestLink restoreLink;
  restoreLink.online = false;
  TestClock restoreClock;
  TestLogger restoreLogger;
  flova::Device restoreDevice(restoreLink, persistentStorage, restoreClock,
                              restoreLogger);
  flova::Datastream<bool> restoredRelay = restoreDevice.datastream<bool>("relay");
  restoredRelay.onWrite(writeRelay)
      .persist(flova::PersistencePolicy::Persistent);
  assert(restoreDevice.begin());
  const int writesBeforeRestore = writes;
  restoreDevice.run();
  assert(writes == writesBeforeRestore + 1);
  assert(restoredRelay.hasValue() && restoredRelay.value());
  assert(restoredRelay.snapshot().quality == flova::Quality::Good);

  AsyncBindingLink delayedRestoreLink;
  delayedRestoreLink.online = false;
  delayedRestoreLink.autoReady = false;
  TestClock delayedRestoreClock;
  TestLogger delayedRestoreLogger;
  flova::Device delayedRestoreDevice(delayedRestoreLink, persistentStorage,
                                     delayedRestoreClock,
                                     delayedRestoreLogger);
  flova::Datastream<bool> delayedRelay =
      delayedRestoreDevice.datastream<bool>("relay");
  delayedRelay.onWrite(writeRelay)
      .persist(flova::PersistencePolicy::Persistent);
  flova::config::Unit delayedUnit;
  delayedUnit.kind = flova::config::UnitKind::Datastream;
  delayedUnit.data.datastream.id = 1;
  delayedUnit.data.datastream.valueType = 0;
  strcpy(delayedUnit.data.datastream.key, "relay");
  assert(delayedRestoreDevice.applyConfigurationUnit(delayedUnit));
  assert(delayedRestoreDevice.begin());
  const int writesBeforeDelayedRestore = writes;
  delayedRestoreDevice.run();
  assert(writes == writesBeforeDelayedRestore + 1);
  assert(delayedRelay.hasValue() && delayedRelay.value());
  assert(!delayedRestoreDevice.ready());
  delayedRestoreLink.ready = true;
  delayedRestoreLink.online = true;
  delayedRestoreDevice.run();
  assert(writes == writesBeforeDelayedRestore + 1);
  assert(delayedRestoreDevice.ready());

  TestLink failedRestoreLink;
  failedRestoreLink.online = false;
  TestClock failedRestoreClock;
  TestLogger failedRestoreLogger;
  flova::Device failedRestoreDevice(failedRestoreLink, persistentStorage,
                                    failedRestoreClock, failedRestoreLogger);
  flova::Datastream<bool> failedRelay =
      failedRestoreDevice.datastream<bool>("relay");
  failedRelay.onWrite(writeRelay)
      .persist(flova::PersistencePolicy::Persistent);
  assert(failedRestoreDevice.begin());
  reject = true;
  const int writesBeforeFailedRestore = writes;
  failedRestoreDevice.run();
  assert(writes == writesBeforeFailedRestore + 1);
  assert(failedRelay.snapshot().quality == flova::Quality::HardwareError);
  failedRestoreLink.online = true;
  reject = false;
  flova::Message retryRestore;
  retryRestore.kind = flova::MessageKind::WriteRequest;
  retryRestore.datastreamId = 1;
  retryRestore.value = flova::Value::from(true);
  retryRestore.revision = 1;
  flova::Value::copy(retryRestore.commandId, "retry-restore");
  failedRestoreLink.inject(retryRestore);
  assert(writes == writesBeforeFailedRestore + 2);

  TestLink link; TestStorage storage; TestClock clock; TestLogger logger; flova::Device device(link, storage, clock, logger);
  flova::Datastream<bool> relay = device.datastream<bool>("relay");
  flova::Datastream<float> temperature = device.datastream<float>("temperature");
  flova::Datastream<double> setpoint = device.datastream<double>("setpoint");
  flova::Datastream<int64_t> counter = device.datastream<int64_t>("counter");
  flova::Datastream<flova::Text> status = device.datastream<flova::Text>("status");
  setpoint.onWrite(writeSetpoint);
  counter.onWrite(writeCounter);
  relay.onWrite(writeRelay);
  assert(!relay.bound());
  assert(temperature.report(31.5f).accepted() && link.sends == 0);
  assert(device.begin());
  assert(relay.bound() && counter.bound());

  device.setFactoryResetHandler(requestFactoryReset, nullptr);
  flova::Message resetCommand;
  resetCommand.kind = flova::MessageKind::WriteRequest;
  resetCommand.datastreamId = FLOVA_FACTORY_RESET_DATASTREAM_ID;
  resetCommand.value = flova::Value::from(true);
  flova::Value::copy(resetCommand.commandId, "factory-reset-1");
  link.inject(resetCommand);
  assert(factoryResets == 1 && device.commandResultPending("factory-reset-1"));
  device.run();
  assert(link.lastResult.kind == flova::MessageKind::Acknowledgement);
  link.acknowledge(link.lastResult.messageId);
  assert(!device.commandResultPending("factory-reset-1"));

  assert(temperature.hasValue() && temperature.value() == 31.5f);
  assert(counter.write(INT64_MAX).accepted() && counter.value() == INT64_MAX && voidWrites == 1);
  assert(status.report("ready").accepted() && !strcmp(status.value().c_str(), "ready"));
  char longText[flova::kMaxText + 8]; memset(longText, 'x', sizeof(longText)); longText[sizeof(longText) - 1] = 0;
  assert(!status.report(longText).accepted());
  assert(relay.write(false).accepted()); reject = true; assert(!relay.write(true).accepted() && relay.value() == false);
  reject = false; noChange = true; assert(relay.write(true).accepted() && relay.value()); noChange = false; assert(relay.write(false).accepted());

  flova::Message command; command.kind = flova::MessageKind::WriteRequest; command.datastreamId = 1; flova::Value::copy(command.commandId, "cmd-1"); command.value = flova::Value::from(true); command.revision = 1; command.origin = flova::Origin::UserCommand;
  int before = writes; link.inject(command); assert(writes == before + 1 && relay.value()); assert(link.last.kind == flova::MessageKind::Acknowledgement);
  link.inject(command); assert(writes == before + 1);  // stale retry is acknowledged without hardware.
  command.revision = 0; link.inject(command); assert(writes == before + 1);  // command ID also deduplicates imperative writes.
  command.datastreamId = 99; flova::Value::copy(command.commandId, "cmd-2"); link.inject(command); assert(link.last.kind == flova::MessageKind::Error && strcmp(link.last.reason, "unknown_datastream") == 0);
  command.datastreamId = 1; command.revision = 2; command.value = flova::Value::from(1.0f); flova::Value::copy(command.commandId, "cmd-wrong-type"); link.inject(command); assert(writes == before + 1 && link.last.kind == flova::MessageKind::Error && strcmp(link.last.reason, "type_mismatch") == 0);

  link.online = false; relay.offline(flova::OfflinePolicy::Reject); assert(!relay.write(false).accepted()); assert(!relay.write(true).accepted()); int sends = link.sends;
  assert(relay.value());
  link.online = true; clock.now += 5000; device.run(); assert(link.sends > sends);
  link.acknowledge(link.last.messageId);

  flova::HistoryRetentionPolicy retention; retention.maximumRecords = 2;
  temperature.offline(flova::OfflinePolicy::StoreHistory).retention(retention); link.online = false; temperature.report(5.0f); temperature.report(10.0f); temperature.report(20.0f); assert(device.diagnostics().droppedHistory == 1); int historical = link.stateSends; link.online = true; device.run(); assert(link.stateSends == historical + 1 && link.stateValues[historical] == 10.0); link.acknowledge(link.last.messageId); device.run(); assert(link.stateSends == historical + 2 && link.stateValues[historical + 1] == 20.0); link.acknowledge(link.last.messageId);
  flova::Message time; time.kind = flova::MessageKind::TimeResponse; time.timestamp = 200000; flova::Value::copy(time.commandId, "time-1"); link.inject(time); assert(clock.utcValid());

  TestLink maximumLink; TestStorage maximumStorage; TestClock maximumClock; TestLogger maximumLogger;
  flova::Device maximumDevice(maximumLink, maximumStorage, maximumClock, maximumLogger);
  char maximumKeys[FLOVA_MAX_ACTIVE_DATASTREAMS][8] = {};
  for (size_t i = 0; i < FLOVA_MAX_ACTIVE_DATASTREAMS; ++i) { snprintf(maximumKeys[i], sizeof(maximumKeys[i]), "d%u", static_cast<unsigned>(i)); maximumDevice.datastream<bool>(maximumKeys[i]); }
  assert(maximumDevice.begin() && maximumLink.boundCount == FLOVA_MAX_ACTIVE_DATASTREAMS);
  DatastreamId overLimitIds[FLOVA_MAX_ACTIVE_DATASTREAMS + 1] = {};
  assert(!maximumLink.bindDatastreams(nullptr, FLOVA_MAX_ACTIVE_DATASTREAMS + 1, overLimitIds));
  InvalidBindingLink invalidLink; flova::Device invalidDevice(invalidLink, maximumStorage, maximumClock, maximumLogger); invalidDevice.datastream<bool>("invalid"); assert(!invalidDevice.begin());
  DuplicateBindingLink duplicateLink; flova::Device duplicateDevice(duplicateLink, maximumStorage, maximumClock, maximumLogger); duplicateDevice.datastream<bool>("duplicate"); duplicateDevice.datastream<bool>("duplicate-two"); assert(!duplicateDevice.begin());
  AsyncBindingLink asyncLink; flova::Device asyncDevice(asyncLink, maximumStorage, maximumClock, maximumLogger); flova::Datastream<bool> asyncDatastream = asyncDevice.datastream<bool>("async"); asyncDatastream.onWrite(writeRelay); assert(asyncDevice.begin() && !asyncDevice.ready()); asyncDevice.run(); assert(asyncDevice.ready() && asyncDatastream.write(true).accepted());

  TestLink defaultHistoryLink; TestStorage defaultHistoryStorage;
  TestClock defaultHistoryClock; TestLogger defaultHistoryLogger;
  flova::Device defaultHistoryDevice(defaultHistoryLink, defaultHistoryStorage,
                                     defaultHistoryClock,
                                     defaultHistoryLogger);
  flova::Datastream<float> defaultHistory =
      defaultHistoryDevice.datastream<float>("default-history");
  defaultHistory.offline(flova::OfflinePolicy::StoreHistory);
  assert(defaultHistoryDevice.begin());
  defaultHistoryLink.online = false;
  assert(defaultHistory.report(42.0f).accepted());
  assert(defaultHistoryDevice.diagnostics().droppedHistory == 0);
  assert(defaultHistoryDevice.diagnostics().storageFailures == 0);

  flova::config::Unit configuredDatastream;
  configuredDatastream.kind = flova::config::UnitKind::Datastream;
  configuredDatastream.data.datastream.id = 9;
  configuredDatastream.data.datastream.valueType = 3;
  flova::Value::copy(configuredDatastream.data.datastream.key, "setpoint");
  assert(device.applyConfigurationUnit(configuredDatastream));
  flova::config::Unit safety;
  safety.kind = flova::config::UnitKind::Safety;
  safety.data.safety.datastreamId = 9;
  safety.data.safety.policy = flova::config::SafetyPolicy::Range;
  safety.data.safety.hasMinimum = true; safety.data.safety.minimum.kind = flova::config::ValueKind::Float64; safety.data.safety.minimum.data.float64 = 10;
  safety.data.safety.hasMaximum = true; safety.data.safety.maximum.kind = flova::config::ValueKind::Float64; safety.data.safety.maximum.data.float64 = 20;
  assert(device.applyConfigurationUnit(safety));
  assert(!setpoint.write(5).accepted() && numericWrites == 0);
  assert(setpoint.write(15).accepted() && numericWrites == 1);
  flova::Message expired = command; expired.datastreamId = 1; expired.value = flova::Value::from(true); expired.expiresAtUtcMs = clock.utcMilliseconds() - 1; flova::Value::copy(expired.commandId, "cmd-expired"); link.inject(expired); assert(link.last.kind == flova::MessageKind::Error && strcmp(link.last.reason, "command_expired") == 0);

  SessionStorage sessionStorage; TestEngine engine; flova::Provisioner provisioner(engine, sessionStorage, clock); flova::ProvisioningRequest request; flova::Value::copy(request.engineUrl, "https://engine.example"); flova::Value::copy(request.token, "one-time-token"); flova::Value::copy(request.hardwareId, "stm32-123"); assert(provisioner.begin(request)); provisioner.run(); flova::ProvisioningSession restored; assert(provisioner.status() == flova::ProvisioningStatus::Ready && provisioner.load(restored) && !strcmp(restored.deviceId, "device-1") && clock.utcValid());
  TestWallClock wallClock; flova::Scheduler scheduler(wallClock); assert(scheduler.daily(20, 0, "IRST-3:30", scheduledWrite)); scheduler.run(); assert(scheduledRuns == 0); wallClock.valid = true; scheduler.run(); scheduler.run(); assert(scheduledRuns == 1); wallClock.now.day++; scheduler.run(); assert(scheduledRuns == 2);

  return 0;
}
