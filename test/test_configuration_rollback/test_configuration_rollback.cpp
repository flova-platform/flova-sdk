#include <Arduino.h>
#include <FlovaEsp8266.h>
#include <FlovaMqtt.h>
#include <unity.h>

namespace {

class TestHardware final : public flova::Hardware {
 public:
  void attach(flova::Device&) override {}
  bool apply(const flova::config::Unit&) override { return false; }
  void failSafe() override { safe = true; }
  void run() override {}
  void setConnected(bool) override {}
  bool safe = false;
};

class TestLink final : public ArduinoFlovaLink {
 public:
  using ArduinoFlovaLink::ArduinoFlovaLink;
  bool decodeStoredConfigurationUnit(const uint8_t*, size_t length,
                                     flova::config::Unit& unit) override {
    if (length != 1) return false;
    unit.kind = flova::config::UnitKind::Datastream;
    unit.data.datastream.id = 1;
    unit.data.datastream.valueType = 0;
    return flova::copyBounded("relay", unit.data.datastream.key, true);
  }
};

class IdleNetwork final : public FlovaNetworkRuntime {
 public:
  bool connected() const override { return false; }
};

class ClosedClock final : public FlovaTlsClockBootstrap {
 public:
  bool ready() const override { return false; }
};

FlovaEsp8266Storage storage;
FlovaEsp8266Platform platform;
FlovaEsp8266Entropy entropy;
FlovaEsp8266Identity identity("architecture_test");
ArduinoFlovaClock clock;
ArduinoFlovaLogger logger;
TestLink link(platform, entropy);
TestHardware hardware;
IdleNetwork network;
ClosedClock tls;
FlovaProvisioningAdapter provisioning;
FlovaClient client(link, provisioning, network, tls, identity, storage, clock,
                   logger, entropy, hardware);

void seedBanks() {
  flova::DeviceConfiguration configuration = {};
  TEST_ASSERT_TRUE(flova::copyBounded("a28cbe21-fd34-4cf6-8f28-8264e90192c7",
                                     configuration.deviceId, true));
  TEST_ASSERT_TRUE(flova::copyBounded("mqtts://fixture.invalid:8883",
                                     configuration.linkUrl, true));
  TEST_ASSERT_TRUE(flova::copyBounded("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                                     configuration.linkSecret, true));
  configuration.generation = 1;
  flova::ConfigurationImage image = {};
  flova::makeConfigurationImage(configuration, image);
  TEST_ASSERT_TRUE(storage.write("config", &image, sizeof(image)));

  // These workspaces are used during fixture seeding only.
  static FlovaLinkConfigurationStorage banks(storage, 4);
  static flova::config::Installer installer(banks, 4);
  static flova::config::Record record;
  record.generation = 2;
  record.kind = flova::config::RecordKind::Datastream;
  record.length = 1;
  for (uint32_t generation = 1; generation <= 2; ++generation) {
    flova::config::Begin begin;
    begin.generation = generation;
    begin.schemaVersion = 1;
    begin.maximumRecordBytes = flova::config::kMaximumRecordBytes;
    begin.recordCount = generation - 1;
    flova::config::Digest digest;
    if (begin.recordCount) digest.addRecord(record);
    digest.finish(begin.checksum);
    TEST_ASSERT_TRUE(installer.begin(begin).accepted());
    if (begin.recordCount) TEST_ASSERT_TRUE(installer.record(record).accepted());
    flova::config::End end;
    end.generation = generation;
    end.recordCount = begin.recordCount;
    end.checksum = begin.checksum;
    TEST_ASSERT_TRUE(installer.end(end).accepted());
    TEST_ASSERT_TRUE(installer.promote(generation));
  }
}

void configurationFallsBackAfterReset() {
  TEST_ASSERT_TRUE(storage.begin());
  uint32_t marker[2] = {};
  const bool recovering = storage.read("cfg_rejected", marker, sizeof(marker));
  if (!recovering) seedBanks();
  TEST_ASSERT_TRUE(client.begin());
  for (unsigned i = 0; i < 64 && client.lifecycle() == FlovaLifecycle::RestoringConfiguration; ++i) {
    client.run();
    yield();
  }
  if (!recovering) {
    TEST_ASSERT_TRUE(client.restartRequired());
    TEST_ASSERT_TRUE(hardware.safe);
    TEST_ASSERT_EQUAL_UINT32(0, link.configurationGeneration());
    Serial.println("configuration rollback restarting");
    Serial.flush();
    ESP.restart();
    delay(1000);
    TEST_FAIL_MESSAGE("expected reset");
  }
  TEST_ASSERT_EQUAL_UINT32(2, marker[0]);
  TEST_ASSERT_EQUAL_UINT32(1, link.configurationGeneration());
}

}  // namespace

void setUp() {}
void tearDown() {}

void setup() {
  Serial.begin(115200);
  delay(1200);
  UNITY_BEGIN();
  RUN_TEST(configurationFallsBackAfterReset);
  TEST_ASSERT_TRUE(sizeof(FlovaEsp8266WithLink) < sizeof(FlovaEsp8266));
  Serial.printf("managed device bytes: default=%u injected=%u\n",
                static_cast<unsigned>(sizeof(FlovaEsp8266)),
                static_cast<unsigned>(sizeof(FlovaEsp8266WithLink)));
  UNITY_END();
}

void loop() {}
