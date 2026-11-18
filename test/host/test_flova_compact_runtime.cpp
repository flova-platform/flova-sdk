#include <FlovaCustomCode.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#define CHECK(condition)                                                         \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);         \
      std::abort();                                                              \
    }                                                                           \
  } while (false)

struct Storage : flova::Storage {
  std::map<std::string, std::vector<uint8_t>> records;
  bool failRead = false, failWrite = false;
  unsigned writes = 0, reads = 0;
  bool read(const char* key, void* out, size_t size) override {
    ++reads;
    const auto it = records.find(key);
    if (failRead || it == records.end() || it->second.size() != size) return false;
    memcpy(out, it->second.data(), size); return true;
  }
  bool write(const char* key, const void* data, size_t size) override {
    if (failWrite) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    records[key] = std::vector<uint8_t>(bytes, bytes + size); ++writes; return true;
  }
  bool remove(const char* key) override { records.erase(key); return true; }
};
struct Clock : flova::Clock {
  uint64_t now = 1;
  uint64_t milliseconds() const override { return now; }
};
struct Logger : flova::Logger { void log(const char*) override {} };
struct Link : flova::Link {
  flova::MessageReceiver receiver = nullptr;
  void* context = nullptr;
  std::vector<flova::Message> sent;
  bool online = true, paused = false;
  bool applicationPaused() const override { return paused; }
  bool begin() override { return true; }
  bool connected() const override { return online; }
  void poll() override {}
  uint32_t messageNonce() const override { return 123; }
  void setReceiver(flova::MessageReceiver r, void* c) override { receiver = r; context = c; }
  bool send(const flova::Message& m) override { if (!online) return false; if (m.kind == flova::MessageKind::StateUpdate) sent.push_back(m); return true; }
  bool bindDatastreamKeys(flova::DatastreamKeyReader reader, void* ctx,
                          size_t count, DatastreamId* ids) override {
    char key[49];
    for (size_t i = 0; i < count; ++i) {
      if (!reader(ctx, i, key, sizeof(key)) || strlen(key) != 48) return false;
      CHECK(key[0] == static_cast<char>('a' + i / 26) && key[1] == static_cast<char>('a' + i % 26));
      ids[i] = static_cast<DatastreamId>(i + 1);
    }
    return true;
  }
  void ack(uint64_t id) { flova::Message m; m.kind = flova::MessageKind::Acknowledgement; m.messageId = id; receiver(context, m); }
};
static unsigned hardwareWrites;
static flova::WriteResult writeText(flova::Text) { ++hardwareWrites; return flova::accept(); }
static flova::WriteResult writeInteger(int64_t) { ++hardwareWrites; return flova::accept(); }
static void keyFor(unsigned i, char (&key)[49]) {
  memset(key, 'k', 48); key[48] = 0; key[0] = 'a' + i / 26; key[1] = 'a' + i % 26;
}
struct Facade {
  flova::Device& device;
  template <typename T> flova::Datastream<T> flovaDatastream(const char* key) {
    return device.datastream<T>(key);
  }
  template <typename T> flova::WriteResult flovaWrite(const char* key, const T& value) {
    return device.datastream<T>(key).write(value);
  }
  template <typename T> flova::WriteResult flovaReport(const char* key, const T& value, flova::Origin origin) {
    return device.datastream<T>(key).report(value, origin);
  }
  template <typename T> bool flovaHasValue(const char* key) {
    return device.datastream<T>(key).hasValue();
  }
  template <typename T> T flovaValue(const char* key) {
    return device.datastream<T>(key).value();
  }
  template <typename T>
  bool flovaOnWrite(const char* key, flova::ValueType, FlovaDatastreamBinding<T>& binding) {
    return binding.attach(device.datastream<T>(key));
  }
};

template <>
struct flova::detail::UseResolvedDatastream<Facade> : std::true_type {};

struct LegacyFacade {
  FlovaDatastreamBinding<int64_t>* callback = nullptr;
  bool rejectBinding = false;
  unsigned fallbackWrites = 0;

  template <typename T> flova::Datastream<T> flovaDatastream(const char*) { return {}; }
  flova::WriteResult flovaWrite(const char*, int64_t) {
    ++fallbackWrites;
    return flova::accept();
  }
  bool flovaOnWrite(const char*, flova::ValueType, FlovaDatastreamBinding<int64_t>& binding) {
    if (rejectBinding) return false;
    callback = &binding;
    return true;
  }
};

static void verifyCheckedSetup() {
  Storage storage;
  Link link;
  Clock clock;
  Logger logger;
  flova::Device device(link, storage, clock, logger);
  Facade facade{device};
  char key[49];
  keyFor(0, key);
  FlovaDatastream<int64_t, Facade> helper(facade, key);
  CHECK(storage.reads == 0 && storage.writes == 0);

  flova::Datastream<int64_t> invalid;
  CHECK(!invalid.tryOnWrite(writeInteger));
  CHECK(!invalid.tryMode(flova::Mode::Sample));
  CHECK(helper.tryOnWrite(writeInteger));
  auto stream = device.datastream<int64_t>(key);
  using Handler = flova::WriteResult (*)(int64_t);
  CHECK(!stream.tryOnWrite(static_cast<Handler>(nullptr)));
  CHECK(!helper.tryOnWrite(static_cast<Handler>(nullptr)));
  stream.onWrite(static_cast<Handler>(nullptr));
  unsigned before = hardwareWrites;
  CHECK(helper.write(1).accepted());
  CHECK(hardwareWrites == before + 1);

  CHECK(stream.tryMode(flova::Mode::Command));
  CHECK(!stream.tryMode(static_cast<flova::Mode>(255)));
  CHECK(stream.tryOffline(flova::OfflinePolicy::Reject));
  CHECK(!stream.tryOffline(static_cast<flova::OfflinePolicy>(255)));
  link.online = false;
  CHECK(!helper.write(2).accepted() && helper.value() == 1);
  CHECK(stream.tryOffline(flova::OfflinePolicy::KeepLatest));
  CHECK(helper.write(2).accepted());
  CHECK(stream.tryPersist(flova::PersistencePolicy::None));
  CHECK(!stream.tryPersist(flova::PersistencePolicy::Deferred, 0));
  CHECK(!stream.tryPersist(flova::PersistencePolicy::Deferred, 0x80000000UL));
  const unsigned writes = storage.writes;
  CHECK(helper.write(3).accepted() && storage.writes == writes);
  flova::HistoryRetentionPolicy retention;
  retention.overflow = static_cast<flova::HistoryOverflow>(255);
  CHECK(!stream.tryRetention(retention) && storage.writes == writes);
  retention.overflow = flova::HistoryOverflow::DropNewest;
  storage.failWrite = true;
  CHECK(!stream.tryRetention(retention) && stream.valid());

  FlovaDatastream<bool, Facade> conflicting(facade, key);
  CHECK(!conflicting.tryOnWrite(+[](bool) {}));
  storage.failWrite = false;
  char otherKey[49];
  keyFor(1, otherKey);
  CHECK(device.datastream<bool>(otherKey).valid());
  keyFor(2, otherKey);
  CHECK(device.datastream<bool>(otherKey).valid());
  FlovaDatastream<int64_t, Facade> missing(facade, "missing");
  storage.failRead = true;
  const unsigned reads = storage.reads;
  CHECK(!missing.report(4).accepted());
  CHECK(storage.reads == reads + 1);
  CHECK(helper.write(4).accepted() && storage.reads == reads + 1);
}

static void verifyLegacyFallback() {
  LegacyFacade facade;
  FlovaDatastream<int64_t, LegacyFacade> helper(facade, "legacy");
  CHECK(helper.write(1).accepted() && facade.fallbackWrites == 1);
  CHECK(helper.tryOnWrite(writeInteger));
  CHECK(facade.callback);
  facade.rejectBinding = true;
  CHECK(!helper.tryOnWrite(+[](int64_t) { return flova::reject("replacement"); }));
  FlovaDatastreamCommand command = {"legacy", "command", 0, flova::Value::from(int64_t(1))};
  const unsigned before = hardwareWrites;
  CHECK(FlovaDatastreamBinding<int64_t>::dispatch(facade.callback, command).accepted());
  CHECK(hardwareWrites == before + 1);
}

static void verifyCachedHelpers() {
  Storage storage; Link link; Clock clock; Logger logger;
  flova::Device device(link, storage, clock, logger);
  Facade facade{device};
  char firstKey[49], secondKey[49]; keyFor(0, firstKey); keyFor(1, secondKey);
  FlovaDatastream<int64_t, Facade> first(facade, firstKey), second(facade, secondKey);
  CHECK(storage.reads == 0 && storage.writes == 0);
  first.mode(flova::Mode::Sample); second.mode(flova::Mode::Sample);
  CHECK(device.begin());
  CHECK(first.report(1).accepted() && second.report(2).accepted());
  const unsigned reads = storage.reads, writes = storage.writes;
  storage.failRead = true;
  for (unsigned i = 0; i < 100; ++i) {
    CHECK(first.hasValue() && first.value() == 1);
    CHECK(second.hasValue() && second.value() == 2);
  }
  CHECK(storage.reads == reads && storage.writes == writes);
  CHECK(device.datastreamCount() == 2);
}

static void verifyAlternatingSafetyChecks() {
  Storage storage;
  Link link;
  Clock clock;
  Logger logger;
  flova::Device device(link, storage, clock, logger);
  char key[49];
  keyFor(0, key);
  auto first = device.datastream<int64_t>(key);
  keyFor(1, key);
  auto second = device.datastream<int64_t>(key);
  CHECK(first.tryOnWrite(writeInteger));
  CHECK(second.tryOnWrite(writeInteger));
  CHECK(device.begin());

  flova::config::Unit unit;
  unit.kind = flova::config::UnitKind::Safety;
  unit.data.safety.policy = static_cast<flova::config::SafetyPolicy>(1);
  unit.data.safety.hasMinimum = true;
  unit.data.safety.minimum.kind = flova::config::ValueKind::Int64;
  unit.data.safety.datastreamId = 1;
  unit.data.safety.minimum.data.integer = 10;
  CHECK(device.applyConfigurationUnit(unit));
  unit.data.safety.datastreamId = 2;
  unit.data.safety.minimum.data.integer = 100;
  CHECK(device.applyConfigurationUnit(unit));

  const unsigned reads = storage.reads;
  const unsigned writes = storage.writes;
  const unsigned calls = hardwareWrites;
  for (int64_t offset = 0; offset < 100; ++offset) {
    CHECK(first.write(10 + offset).accepted());
    CHECK(second.write(100 + offset).accepted());
  }
  if (storage.reads != reads)
    std::fprintf(stderr, "100 alternating numeric safety checks per stream used %u reads\n",
                 storage.reads - reads);
  CHECK(storage.reads == reads);
  CHECK(storage.writes == writes);
  CHECK(!first.write(9).accepted());
  CHECK(!second.write(99).accepted());
  CHECK(hardwareWrites == calls + 200);
  CHECK(storage.reads == reads);
}

int main() {
  verifyAlternatingSafetyChecks();
  verifyCheckedSetup();
  verifyLegacyFallback();
  hardwareWrites = 0;
  verifyCachedHelpers();
  Storage storage; Link link; Clock clock; Logger logger;
  flova::Device device(link, storage, clock, logger);
  char key[49]; char text[flova::kMaxText]; memset(text, 'v', sizeof(text)); text[sizeof(text) - 1] = 0;
  for (unsigned i = 0; i < 64; ++i) {
    keyFor(i, key);
    auto stream = device.datastream<flova::Text>(key);
    CHECK(stream.valid()); stream.onWrite(writeText);
    CHECK(stream.write(flova::Text(text)).accepted());
  }
  CHECK(device.datastreamCount() == 64 && hardwareWrites == 64);
  CHECK(!device.datastream<bool>("overflow").valid());
  CHECK(device.begin());
  device.run();
  CHECK(link.sent.size() == 4); // Window size, independent of stream count.
  const unsigned registrationWrites = storage.writes;
  keyFor(0, key);
  auto first = device.datastream<flova::Text>(key);
  const auto old = link.sent.front();
  CHECK(first.write(flova::Text("newer")).accepted());
  link.ack(old.messageId);
  CHECK(first.snapshot().dirty); // Old revision cannot acknowledge the new one.
  size_t acknowledged = 1;
  bool observed[65] = {}; observed[1] = true;
  for (unsigned turn = 0; turn < 100 && acknowledged < 65; ++turn) {
    while (acknowledged < link.sent.size()) {
      const auto message = link.sent[acknowledged++];
      observed[message.datastreamId] = true;
      link.ack(message.messageId);
    }
    device.run();
  }
  for (unsigned i = 1; i <= 64; ++i) CHECK(observed[i]);
  CHECK(!first.snapshot().dirty);
  link.paused = true;
  const unsigned pausedWrites = hardwareWrites;
  CHECK(!first.write(flova::Text("paused")).accepted());
  CHECK(hardwareWrites == pausedWrites && strcmp(first.value().c_str(), "newer") == 0);
  link.paused = false;
  CHECK(storage.writes == registrationWrites); // No flash writes per live sample.
  // Reading other descriptors evicts the cache. A failed read cannot create a
  // duplicate registration or discard the already registered live values.
  storage.failRead = true;
  keyFor(32, key);
  CHECK(!device.datastream<flova::Text>(key).valid());
  CHECK(device.datastreamCount() == 64);
  CHECK(strcmp(first.value().c_str(), "newer") == 0);

  Storage safetyStorage; Link safetyLink; Clock safetyClock;
  flova::Device safety(safetyLink, safetyStorage, safetyClock, logger);
  keyFor(0, key); auto number = safety.datastream<int64_t>(key); number.onWrite(writeInteger);
  keyFor(1, key); CHECK(safety.datastream<bool>(key).valid());
  keyFor(2, key); CHECK(safety.datastream<bool>(key).valid());
  CHECK(safety.begin());
  flova::config::Unit unit; unit.kind = flova::config::UnitKind::Safety;
  unit.data.safety.datastreamId = 1; unit.data.safety.policy = static_cast<flova::config::SafetyPolicy>(1);
  unit.data.safety.hasMinimum = true;
  unit.data.safety.minimum.kind = flova::config::ValueKind::Int64;
  unit.data.safety.minimum.data.integer = 10;
  CHECK(safety.applyConfigurationUnit(unit));
  CHECK(number.write(10).accepted());
  const unsigned before = hardwareWrites;
  CHECK(!number.write(9).accepted() && hardwareWrites == before);
  safetyStorage.failWrite = true;
  unit.data.safety.minimum.data.integer = 0;
  CHECK(!safety.applyConfigurationUnit(unit));
  CHECK(!number.write(9).accepted() && hardwareWrites == before);
  keyFor(2, key); CHECK(safety.datastream<bool>(key).valid());
  safetyStorage.failRead = true;
  CHECK(!number.write(11).accepted() && hardwareWrites == before);
  CHECK(number.value() == 10);
}
