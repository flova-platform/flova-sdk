#define F(value) value
#define print println
#include <FlovaArduino.h>
#undef print
#undef F

#include <assert.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

HardwareSerial Serial;
static uint32_t testNow = 1;
uint32_t millis() { return testNow; }
unsigned long micros() { return 1; }
void delay(unsigned long) {}
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return 0; }
int analogRead(uint8_t) { return 0; }
void analogWrite(uint8_t, int) {}

namespace {

class TestStorage final : public flova::Storage {
 public:
  bool read(const char* key, void* output, size_t length) override {
    if (strstr(key, "_r_")) ++recordReads;
    auto found = records.find(key);
    if (found == records.end() || found->second.size() != length) return false;
    memcpy(output, found->second.data(), length);
    return true;
  }
  bool write(const char* key, const void* input, size_t length) override {
    if (failWrites) return false;
    const auto* bytes = static_cast<const uint8_t*>(input);
    records[key] = std::vector<uint8_t>(bytes, bytes + length);
    return true;
  }
  bool remove(const char* key) override { records.erase(key); return true; }
  bool clear() override { records.clear(); return true; }
  std::map<std::string, std::vector<uint8_t>> records;
  unsigned recordReads = 0;
  bool failWrites = false;
};

class TestClock final : public flova::Clock {
 public:
  uint64_t milliseconds() const override { return testNow; }
};

class TestLogger final : public flova::Logger {
 public:
  void log(const char*) override {}
};

class TestEntropy final : public FlovaEntropySource {
 public:
  uint8_t byte() override { return 1; }
};

class TestIdentity final : public FlovaBoardIdentity {
 public:
  bool hardwareId(char* output, size_t capacity) const override {
    static const char id[] = "test-board";
    if (capacity < sizeof(id)) return false;
    memcpy(output, id, sizeof(id));
    return true;
  }
  const char* firmwareTarget() const override { return "test-target"; }
};

class TestNetwork final : public FlovaNetworkRuntime {
 public:
  bool connected() const override { return online; }
  bool online = false;
};

class TestTlsClock final : public FlovaTlsClockBootstrap {
 public:
  bool ready() const override { return clockReady; }
  bool clockReady = false;
};

class TestProvisioning final : public FlovaProvisioningAdapter {
 public:
  bool begin(FlovaProvisioningHandler, void*) override { return true; }
  bool startProvisioning() override { started = true; return true; }
  bool started = false;
};

class TestHardware final : public flova::Hardware {
 public:
  void attach(flova::Device&) override {}
  bool apply(const flova::config::Unit&) override { return true; }
  void run() override {}
  void setConnected(bool value) override { connected = value; }
  bool connected = false;
};

class TestLink final : public FlovaClientLink {
 public:
  bool begin() override { return true; }
  bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override {
    for (size_t i = 0; i < count; ++i) ids[i] = static_cast<DatastreamId>(i + 1);
    return true;
  }
  bool connected() const override { return online; }
  bool applicationPaused() const override { return maintenance; }
  bool send(const flova::Message&) override { return online && !maintenance; }
  void poll() override {}
  void setReceiver(flova::MessageReceiver receiver, void* context) override {
    receiver_ = receiver;
    context_ = context;
  }
  uint32_t messageNonce() const override { return 1; }
  bool configure(const char*, const char*, const char*) override { return true; }
  bool beginBootstrap(const char*, const char*, const char*, const char*,
                      const char*) override { return true; }
  void pollBootstrap() override {}
  bool takeBootstrapCommitted(FlovaLinkBootstrapCommitted&) override {
    return false;
  }
  bool takeBootstrapError(char*, size_t) override { return false; }
  bool takeConfigurationRecord(FlovaLinkConfigurationRecord& output) override {
    if (!configurationPending) return false;
    output = configuration;
    configurationPending = false;
    return true;
  }
  bool publishConfigurationReport(
      const FlovaLinkConfigurationReport& report) override {
    lastReport = report; ++reports; return true;
  }
  bool publishConfigurationState(
      const FlovaLinkConfigurationState&) override { return true; }
  bool publishHeartbeat(const FlovaLinkHeartbeat&) override { return true; }
  bool lifecycleLogsSupported() const override { return logSupport; }
  bool publishLifecycle(const FlovaLinkLifecycleReport& report) override {
    logReports.push_back(report);
    return true;
  }
  bool takeLifecycleAck(uint64_t id) override {
    if (!id || id != logAck) return false;
    logAck = 0;
    return true;
  }
  bool takeOtaOffer(FlovaLinkOtaOffer&) override { return false; }
  bool publishOtaReport(const FlovaLinkOtaReport&) override { return true; }
  flova::OtaInstallResult installOta(const FlovaLinkOtaOffer&) override {
    return flova::OtaInstallResult::DownloadFailed;
  }
  bool decodeStoredConfigurationUnit(
      const uint8_t* payload, size_t length, flova::config::Unit& output) override {
    assert(length == 16);
    if (!configurationKeys.empty()) {
      const size_t index = payload[0];
      assert(index < configurationKeys.size());
      output.kind = flova::config::UnitKind::Datastream;
      output.data.datastream.id = static_cast<DatastreamId>(index + 1);
      output.data.datastream.valueType = 0;
      assert(flova::copyBounded(configurationKeys[index].c_str(), output.data.datastream.key, true));
      return true;
    }
    // Writing the typed alternative must not destroy the installer's copy.
    output = flova::config::Unit();
    for (size_t i = 0; i < length; ++i) assert(payload[i] == 0x5a);
    output.kind = flova::config::UnitKind::Datastream;
    output.data.datastream.id = decoderAllowed ? 1 : 0;
    output.data.datastream.valueType = 0;
    memcpy(output.data.datastream.key, "relay", 6);
    return true;
  }
  void setConfigurationGeneration(uint32_t generation) override {
    generation_ = generation;
  }
  uint32_t configurationGeneration() const override { return generation_; }
  void setHardwareCapabilities(
      const flova::HardwareCapabilities&) override {}
  void disconnect() override { online = false; }
  void beginMaintenance() override {
    ++maintenanceBegins;
    maintenance = true;
  }
  bool maintenanceReady() override {
    ++maintenancePolls;
    return maintenanceReadyNow;
  }
  bool maintenanceFailed() const override { return maintenanceFailedNow; }
  void endMaintenance() override {
    ++maintenanceEnds;
    maintenance = false;
  }

  bool online = false;
  bool logSupport = false;
  uint64_t logAck = 0;
  std::vector<FlovaLinkLifecycleReport> logReports;
  bool maintenance = false;
  bool maintenanceReadyNow = false;
  bool maintenanceFailedNow = false;
  unsigned maintenanceBegins = 0;
  unsigned maintenancePolls = 0;
  unsigned maintenanceEnds = 0;
  bool configurationPending = false;
  bool decoderAllowed = true;
  unsigned reports = 0;
  FlovaLinkConfigurationRecord configuration = {};
  FlovaLinkConfigurationReport lastReport = {};
  std::vector<std::string> configurationKeys;

 private:
  flova::MessageReceiver receiver_ = nullptr;
  void* context_ = nullptr;
  uint32_t generation_ = 0;
};

struct Observer {
  unsigned count = 0;
  FlovaStatusEventKind kinds[8] = {};
  FlovaStatusSnapshot last = {};
};

void observe(void* context, const FlovaStatusEvent& event) {
  Observer& observer = *static_cast<Observer*>(context);
  assert(observer.count < sizeof(observer.kinds) / sizeof(observer.kinds[0]));
  observer.kinds[observer.count++] = event.kind;
  observer.last = event.current;
}

void seedRuntimeConfiguration(TestStorage& storage) {
  flova::DeviceConfiguration configuration = {};
  assert(flova::copyBounded("device-1", configuration.deviceId, true));
  assert(flova::copyBounded("mqtts://mqtt.example:8883",
                            configuration.linkUrl, true));
  assert(flova::copyBounded("secret", configuration.linkSecret, true));
  configuration.generation = 1;

  flova::ConfigurationImage image = {};
  flova::makeConfigurationImage(configuration, image);
  storage.records["config"] = std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(&image),
      reinterpret_cast<const uint8_t*>(&image) + sizeof(image));

  FlovaLinkConfigurationStorage configurationStorage(storage, 64);
  flova::config::Installer installer(configurationStorage, 64);
  flova::config::Begin begin;
  begin.generation = 1;
  begin.schemaVersion = 1;
  begin.maximumRecordBytes = flova::config::kMaximumRecordBytes;
  flova::config::Digest digest;
  digest.finish(begin.checksum);
  assert(installer.begin(begin).accepted());
  flova::config::End end;
  end.generation = begin.generation;
  end.recordCount = 0;
  end.checksum = begin.checksum;
  assert(installer.end(end).accepted());
  assert(installer.promote(begin.generation));
}

void verifyFreshSetup() {
  TestStorage storage;
  TestLink link; TestClock clock; TestLogger logger; TestEntropy entropy;
  TestIdentity identity; TestNetwork network; TestTlsClock tlsClock;
  TestProvisioning provisioning; TestHardware hardware;
  FlovaClient passive(link, provisioning, network, tlsClock, identity, storage,
                      clock, logger, entropy, hardware);
  assert(passive.begin(false));
  assert(passive.lifecycle() == FlovaLifecycle::AwaitingProvisioning);
  assert(!provisioning.started);
  TestStorage managedStorage;
  FlovaClient managed(link, provisioning, network, tlsClock, identity, managedStorage,
                      clock, logger, entropy, hardware);
  assert(managed.begin(true));
  assert(managed.lifecycle() == FlovaLifecycle::Setup);
  assert(provisioning.started);
  assert(!managed.beginNetworkSetup());
}

void verifyNetworkSetup() {
  TestStorage storage;
  seedRuntimeConfiguration(storage);
  TestLink link;
  TestClock clock; TestLogger logger; TestEntropy entropy; TestIdentity identity;
  TestNetwork network; TestTlsClock tlsClock; TestProvisioning provisioning;
  TestHardware hardware;
  network.online = tlsClock.clockReady = link.online = true;
  FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                     clock, logger, entropy, hardware);
  assert(!client.beginNetworkSetup());
  assert(client.begin(true));
  for (unsigned i = 0; i < 128 && !client.runtimeReady(); ++i) client.run();
  assert(client.runtimeReady());
  const auto original = storage.records;
  assert(client.beginNetworkSetup());
  assert(!client.beginNetworkSetup());
  assert(!client.startProvisioning());
  assert(client.prepareForPowerDown() == FlovaPowerDownStatus::Busy);
  client.run();
  assert(!client.networkSetupReady());
  link.maintenanceReadyNow = true;
  client.run();
  assert(client.networkSetupReady());
  assert(!link.maintenance);
  assert(storage.records == original);
  client.endNetworkSetup();
  assert(!client.networkSetupActive());
  // Offline devices can still enter local recovery; no cloud-ready prerequisite.
  network.online = link.online = false;
  assert(client.beginNetworkSetup());
  link.maintenanceFailedNow = true;
  client.run();
  assert(!client.networkSetupActive());
  assert(!strcmp(client.lastError(), "network_setup_drain_failed"));
  assert(storage.records == original);
}

void verifyPowerDownPreparation() {
  TestStorage storage;
  seedRuntimeConfiguration(storage);
  TestLink link;
  TestClock clock;
  TestLogger logger;
  TestEntropy entropy;
  TestIdentity identity;
  TestNetwork network;
  TestTlsClock tlsClock;
  TestProvisioning provisioning;
  TestHardware hardware;
  network.online = true;
  tlsClock.clockReady = true;
  link.online = true;
  FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                     clock, logger, entropy, hardware);
  auto deferred = client.stream<int64_t>("deferred");
  deferred.persist(flova::PersistencePolicy::Deferred, 1000);
  assert(client.begin(false));
  for (unsigned i = 0; i < 128 && !client.runtimeReady(); ++i) client.run();
  assert(client.ready());

  assert(deferred.report(42).accepted());
  assert(storage.records.count("dsid:1") == 0);

  assert(client.prepareForPowerDown() == FlovaPowerDownStatus::Draining);
  assert(link.maintenanceBegins == 1 && link.maintenancePolls == 1);
  assert(client.prepareForPowerDown() == FlovaPowerDownStatus::Draining);
  assert(link.maintenanceBegins == 1);
  link.maintenanceReadyNow = true;
  assert(client.prepareForPowerDown() == FlovaPowerDownStatus::Ready);
  assert(storage.records.count("dsid:1") == 1);
  assert(client.prepareForPowerDown() == FlovaPowerDownStatus::Ready);
  assert(link.maintenanceBegins == 1 && link.maintenanceEnds == 0);

  TestStorage failedStorage;
  seedRuntimeConfiguration(failedStorage);
  TestLink failedLink;
  TestNetwork failedNetwork;
  TestTlsClock failedTlsClock;
  TestProvisioning failedProvisioning;
  TestHardware failedHardware;
  failedNetwork.online = true;
  failedTlsClock.clockReady = true;
  failedLink.online = true;
  FlovaClient failedClient(failedLink, failedProvisioning, failedNetwork,
                           failedTlsClock, identity, failedStorage, clock,
                           logger, entropy, failedHardware);
  assert(failedClient.begin(false));
  for (unsigned i = 0; i < 128 && !failedClient.runtimeReady(); ++i)
    failedClient.run();
  assert(failedClient.ready());
  assert(failedClient.prepareForPowerDown() == FlovaPowerDownStatus::Draining);
  failedLink.maintenanceReadyNow = true;
  failedLink.maintenanceFailedNow = true;
  assert(failedClient.prepareForPowerDown() == FlovaPowerDownStatus::Failed);
  assert(failedLink.maintenanceEnds == 1 && !failedLink.maintenance);
}

void verifyIndexedRestore(const std::vector<std::string>& keys, bool valid) {
  TestStorage storage; seedRuntimeConfiguration(storage);
  FlovaLinkConfigurationStorage configStorage(storage, 64);
  flova::config::Installer installer(configStorage, 64);
  flova::config::Begin begin;
  begin.generation = 2; begin.schemaVersion = 1;
  begin.maximumRecordBytes = flova::config::kMaximumRecordBytes;
  begin.recordCount = keys.size();
  std::vector<flova::config::Record> records(keys.size());
  flova::config::Digest digest;
  for (size_t i = 0; i < keys.size(); ++i) {
    auto& record = records[i];
    record.generation = 2; record.sequence = i; record.length = 16;
    record.body[0] = i;
    digest.addRecord(record);
  }
  digest.finish(begin.checksum);
  assert(installer.begin(begin).accepted());
  for (const auto& record : records) assert(installer.record(record).accepted());
  flova::config::End end;
  end.generation = 2; end.recordCount = keys.size(); end.checksum = begin.checksum;
  assert(installer.end(end).accepted() && installer.promote(2));
  storage.recordReads = 0;
  TestLink link; TestClock clock; TestLogger logger; TestEntropy entropy;
  TestIdentity identity; TestNetwork network; TestTlsClock tlsClock;
  TestProvisioning provisioning; TestHardware hardware;
  link.configurationKeys = keys;
  link.online = network.online = tlsClock.clockReady = true;
  FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                     clock, logger, entropy, hardware);
  assert(client.begin(false));
  for (unsigned i = 0; i < 4096 && !client.runtimeReady(); ++i) client.run();
  assert(client.runtimeReady());
  assert(link.configurationGeneration() == (valid ? 2U : 1U));
  assert(client.device().datastreamCount() == (valid ? keys.size() : 0));
  if (valid) assert(storage.recordReads == keys.size() * 4 + 1);
  else assert(storage.recordReads == 8);
}

void verifyConfigurationKeyIndex() {
  // These distinct keys have the same FNV-1a fingerprint (0x5e4daa9d).
  std::vector<std::string> keys = {"costarring", "liquid"};
  for (unsigned i = 2; i < 64; ++i) keys.push_back("stream-" + std::to_string(i));
  verifyIndexedRestore(keys, true);
  verifyIndexedRestore({"costarring", "liquid", "costarring"}, false);
}

void verifyOfflineLifecycleReplay() {
  TestStorage storage;
  TestLink link;
  TestClock clock;
  TestLogger logger;
  TestEntropy entropy;
  TestIdentity identity;
  TestNetwork network;
  TestTlsClock tlsClock;
  TestProvisioning provisioning;
  TestHardware hardware;
  FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                     clock, logger, entropy, hardware);
  assert(client.begin());
  client.run(); // no listener, network, clock, or server required for capture
  assert(link.logReports.empty());
  link.online = true;
  client.run(); // older Engine: no capability advertisement
  assert(link.logReports.empty());
  link.logSupport = true;
  client.run();
  assert(link.logReports.size() == 1 && link.logReports[0].code == 0);
  const auto boot = link.logReports[0];
  assert(boot.sequence == 1 && boot.uptimeMs == 1 && boot.messageId);
  testNow += 4999;
  client.run();
  assert(link.logReports.size() == 1);
  testNow += 1;
  client.run();
  assert(link.logReports.size() == 2 && link.logReports.back().messageId == boot.messageId);
  assert(link.logReports.back().sequence == boot.sequence);
  link.logAck = boot.messageId;
  client.run();
  client.run();
  // Internal stage/ready/link transitions do not create operational noise.
  assert(link.logReports.size() == 2);
  testNow = 1;
}

}  // namespace

int main() {
  verifyOfflineLifecycleReplay();
  verifyConfigurationKeyIndex();
  TestLink link;
  TestStorage storage;
  TestClock clock;
  TestLogger logger;
  TestEntropy entropy;
  TestIdentity identity;
  TestNetwork network;
  TestTlsClock tlsClock;
  TestProvisioning provisioning;
  TestHardware hardware;
  FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                     clock, logger, entropy, hardware);
  Observer observer;

  client.setStatusListener(observe, &observer);
  assert(client.begin(false));
  client.run();
  assert(observer.count == 1);
  assert(observer.kinds[0] == FlovaStatusEventKind::LifecycleChanged);
  assert(observer.last.lifecycle == FlovaLifecycle::AwaitingProvisioning);
  assert(!observer.last.linkConnected);
  assert(!observer.last.ready);

  network.online = true;
  tlsClock.clockReady = true;
  link.online = true;
  client.run();
  assert(observer.count == 3);
  assert(observer.kinds[1] == FlovaStatusEventKind::NetworkChanged);
  assert(observer.kinds[2] == FlovaStatusEventKind::LinkChanged);
  assert(observer.last.linkConnected);
  assert(!observer.last.ready);

  client.run();
  assert(observer.count == 3);

  link.setConfigurationGeneration(5);
  client.run();
  assert(observer.count == 4);
  assert(observer.kinds[3] == FlovaStatusEventKind::ConfigurationChanged);
  assert(observer.last.configurationGeneration == 5);

  link.online = false;
  client.run();
  assert(observer.count == 5);
  assert(observer.kinds[4] == FlovaStatusEventKind::LinkChanged);
  assert(!observer.last.linkConnected);

  assert(!client.setFirmwareTarget("invalid target"));
  client.run();
  assert(observer.count == 6);
  assert(observer.kinds[5] == FlovaStatusEventKind::ErrorChanged);
  assert(strcmp(observer.last.errorCode, "invalid_firmware_target") == 0);

  FlovaStatusSnapshot snapshot = {};
  client.status(snapshot);
  assert(snapshot.lifecycle == FlovaLifecycle::AwaitingProvisioning);
  assert(strcmp(snapshot.errorCode, "invalid_firmware_target") == 0);
  // Exercise transfer -> typed workspace reuse through the real lifecycle.
  client.setStatusListener(nullptr);
  assert(client.provision(flova::ProvisioningHandoff(
      "mqtts://mqtt.example:8883",
      "ttttttttttttttttttttttttttttttttttttttttttt")) == FlovaProvisioningResponse::Accepted);
  network.online = tlsClock.clockReady = link.online = true;
  for (unsigned i = 0; i < 4; ++i) client.run();
  assert(client.lifecycle() == FlovaLifecycle::Bootstrapping);
  link.configuration.phase = FlovaLinkConfigurationPhase::Begin;
  link.configuration.messageId = 41;
  link.configuration.generation = 7;
  link.configuration.recordCount = 1;
  link.configuration.schemaVersion = 1;
  link.configuration.maximumRecordBytes = 448;
  link.configurationPending = true;
  client.run();
  assert(link.reports == 1 && link.lastReport.status == FlovaLinkResultStatus::Ok);
  link.configuration.phase = FlovaLinkConfigurationPhase::Record;
  link.configuration.messageId = 42;
  link.configuration.sequence = 0;
  link.configuration.recordType = 0;
  link.configuration.recordLength = 16;
  memset(link.configuration.record, 0x5a, 16);
  link.configurationPending = true;
  client.run();
  assert(link.reports == 2 && link.lastReport.status == FlovaLinkResultStatus::Ok);
  assert(link.lastReport.messageId == 42 && link.lastReport.generation == 7);
  const auto saved = storage.records;
  link.decoderAllowed = false;
  link.configuration.messageId = 43;
  link.configurationPending = true;
  client.run();
  assert(link.reports == 3 && link.lastReport.status == FlovaLinkResultStatus::Error);
  assert(link.lastReport.messageId == 43 && link.lastReport.generation == 7);
  assert(storage.records == saved);
  assert(client.prepareForPowerDown() == FlovaPowerDownStatus::Busy);
  verifyPowerDownPreparation();
  verifyFreshSetup();
  verifyNetworkSetup();
  return 0;
}
