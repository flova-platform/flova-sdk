#define F(value) value
#define print println
#include <FlovaArduino.h>
#include <FlovaCustomCode.h>
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
    if (failedReadKey && strcmp(key, failedReadKey) == 0) return false;
    if (strstr(key, "_r_")) ++recordReads;
    auto found = records.find(key);
    if (found == records.end() || found->second.size() != length) return false;
    memcpy(output, found->second.data(), length);
    return true;
  }
  bool write(const char* key, const void* input, size_t length) override {
    if (failWrites || (failedWriteKey && strcmp(key, failedWriteKey) == 0)) return false;
    const auto* bytes = static_cast<const uint8_t*>(input);
    records[key] = std::vector<uint8_t>(bytes, bytes + length);
    return true;
  }
  flova::StorageInfo info() const override {
    flova::StorageInfo info;
    info.persistent = true;
    return info;
  }
  bool remove(const char* key) override { records.erase(key); return true; }
  bool clear() override { records.clear(); return true; }
  std::map<std::string, std::vector<uint8_t>> records;
  unsigned recordReads = 0;
  bool failWrites = false;
  const char* failedWriteKey = nullptr;
  const char* failedReadKey = nullptr;
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
  bool begin(FlovaProvisioningHandler handler, void* context) override {
    handler_ = handler;
    context_ = context;
    return true;
  }
  void clearHandler() override { handler_ = nullptr; context_ = nullptr; }
  bool startProvisioning() override { started = true; return true; }
  flova::detail::ManagedClaim::Submission submit(TestStorage& storage) {
    const flova::WifiRuntimeData wifi = {"test-network", "test-password"};
    const flova::ProvisioningHandoff handoff("mqtts://broker.test", "claim-token");
    return flova::detail::ManagedClaim::submit(storage, wifi, handoff, handler_, context_);
  }
  bool started = false;
 private:
  FlovaProvisioningHandler handler_ = nullptr;
  void* context_ = nullptr;
};

class TestHardware final : public flova::Hardware {
 public:
  void attach(flova::Device&) override {}
  bool apply(const flova::config::Unit&) override { return !failApply; }
  void failSafe() override { ++safeCalls; }
  void run() override {}
  void setConnected(bool value) override { connected = value; }
  bool connected = false;
  bool failApply = false;
  unsigned safeCalls = 0;
};

class TestLink final : public FlovaClientLink {
 public:
  bool begin() override { return beginAllowed; }
  bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override {
    hasBindings = count != 0;
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
  void schedulingDeclarations(flova::SchedulingReader reader, void*, size_t) override {
    hasScheduler = reader != nullptr;
  }
  void clearDatastreamBindings() override { hasBindings = false; }
  bool callbacksDetached() const { return !receiver_ && !context_ && !hasBindings && !hasScheduler; }
  uint32_t messageNonce() const override { return 1; }
  bool configure(const char*, const char*, const char*) override { return true; }
  bool beginBootstrap(const char*, const char*, const char*, const char*,
                      const char*) override { return true; }
  void pollBootstrap() override {}
  bool takeBootstrapCommitted(FlovaLinkBootstrapCommitted&) override {
    return false;
  }
  bool takeBootstrapError(char* output, size_t capacity) override {
    if (!bootstrapError || strlen(bootstrapError) >= capacity) return false;
    strcpy(output, bootstrapError);
    bootstrapError = nullptr;
    return true;
  }
  const char* bootstrapError = nullptr;
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
  bool publishLogEvent(const FlovaLinkLogEvent& event) override {
    customReports.push_back(event);
    return true;
  }
  bool takeLogEventCompletion(uint64_t id) override {
    if (!id || id != customAck) return false;
    customAck = 0;
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
    if (guardrailFixture) {
      if (payload[0] == 0) {
        output.kind = flova::config::UnitKind::Datastream;
        output.data.datastream.id = 1;
        output.data.datastream.valueType = 0;
        return flova::copyBounded("relay", output.data.datastream.key, true);
      }
      output.kind = flova::config::UnitKind::Safety;
      auto& safety = output.data.safety;
      safety.policy = flova::config::SafetyPolicy::GuardrailStop;
      safety.datastreamId = 1;
      safety.hasSafeValue = true;
      safety.safeValue.kind = flova::config::ValueKind::Boolean;
      safety.safeValue.data.boolean = false;
      return true;
    }
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
  bool beginAllowed = true;
  bool hasBindings = false;
  bool hasScheduler = false;
  bool logSupport = false;
  uint64_t logAck = 0;
  uint64_t customAck = 0;
  std::vector<FlovaLinkLogEvent> customReports;
  std::vector<FlovaLinkLifecycleReport> logReports;
  bool maintenance = false;
  bool maintenanceReadyNow = false;
  bool maintenanceFailedNow = false;
  unsigned maintenanceBegins = 0;
  unsigned maintenancePolls = 0;
  unsigned maintenanceEnds = 0;
  bool configurationPending = false;
  bool decoderAllowed = true;
  bool guardrailFixture = false;
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

void seedFailingGeneration(TestStorage& storage) {
  FlovaLinkConfigurationStorage banks(storage, 64);
  flova::config::Installer installer(banks, 64);
  flova::config::Record record;
  record.generation = 2;
  record.kind = flova::config::RecordKind::Datastream;
  record.length = 16;
  memset(record.body, 0x5a, record.length);
  flova::config::Digest digest;
  digest.addRecord(record);
  flova::config::Begin begin;
  begin.generation = 2;
  begin.schemaVersion = 1;
  begin.maximumRecordBytes = flova::config::kMaximumRecordBytes;
  begin.recordCount = 1;
  digest.finish(begin.checksum);
  assert(installer.begin(begin).accepted());
  assert(installer.record(record).accepted());
  flova::config::End end;
  end.generation = 2;
  end.recordCount = 1;
  end.checksum = begin.checksum;
  assert(installer.end(end).accepted());
  assert(installer.promote(2));
}

void verifyLinkReuse() {
  TestStorage storage; TestLink link; TestClock clock; TestLogger logger;
  {
    flova::Device first(link, storage, clock, logger);
    assert(first.datastream<bool>("relay").valid());
    link.beginAllowed = false;
    assert(!first.begin());
    assert(link.callbacksDetached());
    link.beginAllowed = true;
    assert(first.begin());
    assert(!link.callbacksDetached());
  }
  assert(link.callbacksDetached());
  {
    flova::Device replacement(link, storage, clock, logger);
    assert(replacement.begin());
    assert(!link.callbacksDetached());
  }
  assert(link.callbacksDetached());
}

void verifyApplyRollback(bool interrupted) {
  TestStorage storage;
  seedRuntimeConfiguration(storage);
  seedFailingGeneration(storage);
  TestLink link; TestClock clock; TestLogger logger; TestEntropy entropy;
  TestIdentity identity; TestNetwork network; TestTlsClock tlsClock;
  TestProvisioning provisioning; TestHardware hardware;
  if (interrupted) {
    const uint32_t marker[] = {2, ~uint32_t(2)};
    assert(storage.write("cfg_rejected", marker, sizeof(marker)));
  } else {
    hardware.failApply = true;
    FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                       clock, logger, entropy, hardware);
    assert(client.begin());
    for (unsigned i = 0; i < 64 && !client.restartRequired(); ++i) client.run();
    assert(client.restartRequired() && hardware.safeCalls != 0);
    assert(link.configurationGeneration() == 0);
    assert(storage.records.count("cfg_rejected") == 1);
  }

  hardware.failApply = false;
  network.online = tlsClock.clockReady = link.online = true;
  FlovaClient recovered(link, provisioning, network, tlsClock, identity, storage,
                        clock, logger, entropy, hardware);
  assert(recovered.begin());
  for (unsigned i = 0; i < 64 && !recovered.runtimeReady(); ++i) recovered.run();
  assert(recovered.runtimeReady());
  assert(link.configurationGeneration() == 1);
  link.configuration.phase = FlovaLinkConfigurationPhase::Begin;
  link.configuration.generation = 2;
  link.configurationPending = true;
  recovered.run();
  assert(link.lastReport.status == FlovaLinkResultStatus::Error);
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

bool verifyBootstrapRecovery() {
  for (bool rejected : {false, true}) {
    TestStorage storage;
    TestLink link; TestClock clock; TestLogger logger; TestEntropy entropy;
    TestIdentity identity; TestNetwork network; TestTlsClock tlsClock;
    TestProvisioning provisioning; TestHardware hardware;
    FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                       clock, logger, entropy, hardware);
    network.online = true;
    tlsClock.clockReady = true;
    if (!client.begin(true) ||
        client.provision(flova::ProvisioningHandoff("mqtts://broker.test", "old-token")) !=
            FlovaProvisioningResponse::Accepted)
      return false;

    client.run();  // Accept the staged handoff from setup.
    client.run();  // Start bootstrap once network and clock are ready.
    if (client.lifecycle() != FlovaLifecycle::Bootstrapping) return false;
    link.bootstrapError = rejected ? "mqtt_bootstrap_not_authorized" : "mqtt_handshake_failed";
    client.run();
    if (!rejected) {
      if (client.lifecycle() != FlovaLifecycle::Backoff || !storage.records.count("prov_pending"))
        return false;
      continue;
    }

    if (client.lifecycle() != FlovaLifecycle::Setup || storage.records.count("prov_pending"))
      return false;
    if (client.provision(flova::ProvisioningHandoff("mqtts://broker.test", "new-token")) !=
        FlovaProvisioningResponse::Accepted)
      return false;
    client.run();  // Accept the staged handoff from setup.
    client.run();  // Start bootstrap once network and clock are ready.
    if (client.lifecycle() != FlovaLifecycle::Bootstrapping) return false;
  }
  return true;
}

bool verifyManagedClaimAfterReset() {
  using Submission = flova::detail::ManagedClaim::Submission;
  for (int stage = 0; stage < 4; ++stage) {
    TestStorage storage;
    TestLink link; TestClock clock; TestLogger logger; TestEntropy entropy;
    TestIdentity identity; TestNetwork network; TestTlsClock tlsClock;
    TestProvisioning provisioning; TestHardware hardware;
    {
      FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                         clock, logger, entropy, hardware);
      if (!client.begin(true) || client.lifecycle() != FlovaLifecycle::Setup) return false;
      if (stage == 0) storage.failedWriteKey = "wifi";
      if (stage == 1) storage.failedWriteKey = "prov_pending";
      if (stage == 2) storage.failedReadKey = "prov_pending";
      const auto result = provisioning.submit(storage);
      const auto expected = stage == 0 ? Submission::WifiWriteFailed :
                            stage < 3 ? Submission::HandoffWriteFailed : Submission::Accepted;
      if (result != expected || (storage.records.count("wifi") != 0) != (stage > 0) ||
          (storage.records.count("prov_pending") != 0) != (stage >= 2)) return false;
      // Power loss before run() starts network association.
    }
    storage.failedWriteKey = storage.failedReadKey = nullptr;
    FlovaClient reboot(link, provisioning, network, tlsClock, identity, storage,
                       clock, logger, entropy, hardware);
    const auto expected = stage >= 2 ? FlovaLifecycle::WaitingForNetwork : FlovaLifecycle::Setup;
    if (!reboot.begin(true) || reboot.lifecycle() != expected) return false;
  }
  return true;
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

void verifyGuardrailConfiguration(bool writerAvailable, bool duplicate,
                                  bool interrupted) {
  TestStorage storage;
  seedRuntimeConfiguration(storage);
  TestLink link; TestClock clock; TestLogger logger; TestEntropy entropy;
  TestIdentity identity; TestNetwork network; TestTlsClock tlsClock;
  TestProvisioning provisioning; TestHardware hardware;
  link.guardrailFixture = true;
  link.online = network.online = tlsClock.clockReady = true;
  const unsigned count = duplicate ? 3 : 2;
  const uint32_t expectedGeneration = writerAvailable && !duplicate && !interrupted ? 2 : 1;
  bool applied = true;

  const auto registerWriter = [&](FlovaClient& client) {
    if (!writerAvailable) return;
    assert(client.device().datastream<bool>("relay").tryOnWrite(+[](void* context, bool value) {
      *static_cast<bool*>(context) = value;
      return flova::WriteResult::accept();
    }, &applied));
  };

  {
    FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                       clock, logger, entropy, hardware);
    registerWriter(client);
    assert(client.begin());
    for (unsigned i = 0; i < 128 && !client.runtimeReady(); ++i) client.run();
    assert(client.runtimeReady());

    flova::config::Digest digest;
    flova::config::Record records[3];
    for (unsigned i = 0; i < count; ++i) {
      auto& record = records[i];
      record.generation = 2;
      record.sequence = i;
      record.kind = i ? flova::config::RecordKind::Safety : flova::config::RecordKind::Datastream;
      record.length = 16;
      record.body[0] = i;
      digest.addRecord(record);
    }
    flova::config::Checksum checksum;
    digest.finish(checksum);
    auto& frame = link.configuration;
    frame.generation = 2;
    frame.recordCount = count;
    frame.schemaVersion = 1;
    frame.maximumRecordBytes = flova::config::kMaximumRecordBytes;
    memcpy(frame.checksum, checksum.bytes, sizeof(frame.checksum));

    const auto send = [&](FlovaLinkConfigurationPhase phase) {
      frame.phase = phase;
      ++frame.messageId;
      link.configurationPending = true;
      client.run();
      assert(!link.configurationPending);
    };

    send(FlovaLinkConfigurationPhase::Begin);
    assert(link.lastReport.status == FlovaLinkResultStatus::Ok);
    for (unsigned i = 0; i < count; ++i) {
      frame.sequence = i;
      frame.recordType = static_cast<uint8_t>(records[i].kind);
      frame.recordLength = records[i].length;
      memcpy(frame.record, records[i].body, frame.recordLength);
      send(FlovaLinkConfigurationPhase::Record);
      assert(link.lastReport.status == FlovaLinkResultStatus::Ok);
    }
    send(FlovaLinkConfigurationPhase::End);
    if (!interrupted) {
      for (unsigned i = 0; i < 128 && !client.restartRequired(); ++i) {
        ++testNow;
        client.run();
      }
      if (expectedGeneration == 1) assert(link.lastReport.status == FlovaLinkResultStatus::Error);
    }

    FlovaLinkConfigurationStorage banks(storage, 64);
    uint32_t active = 0, previous = 0;
    banks.generations(active, previous);
    assert(active == expectedGeneration);
  }

  link.online = network.online = false;
  FlovaClient reboot(link, provisioning, network, tlsClock, identity, storage,
                     clock, logger, entropy, hardware);
  registerWriter(reboot);
  assert(reboot.begin());
  for (unsigned i = 0; i < 128 && !reboot.runtimeReady(); ++i) reboot.run();
  assert(link.configurationGeneration() == expectedGeneration);
  if (expectedGeneration == 2) {
    uint64_t latch = 0;
    assert(!applied);
    assert(reboot.device().offlineGuardrailStatus(1, latch) == flova::GuardrailStatus::Disarmed);
  }
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
  assert(!client.logEvent("before_begin"));
  client.run();
  assert(!client.logEvent("still_before_begin"));
  assert(client.begin());
  assert(!client.logEvent("Not_a_code"));
  assert(!client.logEvent("bad_utf8", "\xC3"));
  assert(!client.logEvent("too_long", std::string(97, 'x').c_str()));
  char message[] = "Above 80 C";
  assert(FLOVA_LOG_EVENT(client, "temperature_high", message));
  message[0] = 'x';
  assert(!FLOVA_LOG_EVENT(client, "another_event"));
  client.run(); // no listener, network, clock, or server required for capture
  assert(link.logReports.empty());
  assert(link.customReports.empty());
  link.online = true;
  client.run(); // older Engine: no capability advertisement
  assert(link.logReports.empty());
  assert(link.customReports.size() == 1);
  const auto custom = link.customReports[0];
  assert(strcmp(custom.code, "temperature_high") == 0);
  assert(strcmp(custom.message, "Above 80 C") == 0);
  link.logSupport = true;
  client.run();
  assert(link.logReports.size() == 1 && link.logReports[0].code == 0);
  const auto boot = link.logReports[0];
  assert(boot.sequence == 1 && boot.uptimeMs == 1 && boot.messageId);
  assert(custom.bootId == boot.bootId && custom.sequence == 2 && custom.messageId != boot.messageId);
  testNow += 4999;
  client.run();
  assert(link.logReports.size() == 1);
  assert(link.customReports.size() == 1);
  testNow += 1;
  link.online = false;
  client.run();
  assert(link.customReports.size() == 1);
  link.online = true;
  client.run();
  assert(link.logReports.size() == 2 && link.logReports.back().messageId == boot.messageId);
  assert(link.logReports.back().sequence == boot.sequence);
  assert(link.customReports.size() == 2 && link.customReports.back().messageId == custom.messageId);
  link.logAck = boot.messageId;
  link.customAck = custom.messageId;
  client.run();
  client.run();
  // Internal stage/ready/link transitions do not create operational noise.
  assert(link.logReports.size() == 2);
  assert(FLOVA_LOG_EVENT(client, "another_event"));
  client.run();
  assert(link.customReports.size() == 3 && link.customReports.back().message[0] == '\0');
  testNow = 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--claim-recovery") == 0)
    return verifyManagedClaimAfterReset() ? 0 : 1;
  verifyLinkReuse();
  verifyApplyRollback(false);
  verifyApplyRollback(true);
  verifyOfflineLifecycleReplay();
  verifyConfigurationKeyIndex();
  verifyGuardrailConfiguration(true, false, false);
  verifyGuardrailConfiguration(false, false, false);
  verifyGuardrailConfiguration(true, true, false);
  verifyGuardrailConfiguration(true, false, true);
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
  assert(observer.count == 3);
  assert(observer.last.configurationGeneration == 0);

  link.online = false;
  client.run();
  assert(observer.count == 4);
  assert(observer.kinds[3] == FlovaStatusEventKind::LinkChanged);
  assert(!observer.last.linkConnected);

  assert(!client.setFirmwareTarget("invalid target"));
  client.run();
  assert(observer.count == 5);
  assert(observer.kinds[4] == FlovaStatusEventKind::ErrorChanged);
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
  if (!verifyBootstrapRecovery()) return 1;
  verifyNetworkSetup();
  return 0;
}
