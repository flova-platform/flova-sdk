// Never export this application or enable it in production firmware targets.
#if !defined(FLOVA_DEVELOPMENT_E2E) || FLOVA_DEVELOPMENT_E2E != 1
#error "Board E2E firmware is development-only"
#endif
#include "private_config.h"
#include <FlovaSDK.h>
#include TEST_BOARD_HEADER

namespace {
TestClient client;
auto temperature = FLOVA_DATASTREAM(client, double, "temperature");
auto humidity = FLOVA_DATASTREAM(client, double, "humidity");
bool reporting = false;
bool provisioned = false;
bool restored = false;
uint32_t reports = 0;
uint32_t rejected = 0;
uint32_t runCount = 0;
uint64_t runTotalUs = 0;
uint32_t runMaxUs = 0;
uint64_t sdkWorkTotalUs = 0;
uint32_t sdkWorkMaxUs = 0;

void event(const char* name) {
  Serial.printf("FLOVA_E2E {\"event\":\"%s\"}\n", name);
}

void handleTestCommand(char value) {
  switch (value) {
    case 's': reporting = true; event("started"); break;
    case 'p': reporting = false; event("paused"); break;
    case 'd': board_test::disconnect(); event("disconnected"); break;
    case 'c': board_test::connect(); event("connecting"); break;
    case 'f':
      Serial.printf("FLOVA_E2E {\"event\":\"flush\",\"ok\":%u,\"value_milli\":%ld}\n",
                    temperature.flush(), long(temperature.value() * 1000 + 0.5));
      break;
    case 'r': board_test::restart(); break;
    default: break;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  event("boot");
  board_test::begin();
  temperature.persist(flova::PersistencePolicy::Deferred, 5000);
  humidity.persist(flova::PersistencePolicy::None);
  if (!client.begin()) event("failed");
}

void loop() {
  if (!provisioned && client.lifecycle() == FlovaLifecycle::AwaitingProvisioning) {
    const flova::ProvisioningHandoff handoff(TEST_LINK_URL, TEST_PROVISION_TOKEN);
    if (client.provision(handoff) != FlovaProvisioningResponse::Accepted)
      event("failed");
    provisioned = true;
  }
  const uint32_t start = micros();
  client.run();
  const uint32_t elapsed = micros() - start;
  ++runCount;
  runTotalUs += elapsed;
  if (elapsed > runMaxUs) runMaxUs = elapsed;

  if (client.ready() && !restored) {
    Serial.printf("FLOVA_E2E {\"event\":\"ready\",\"temp_present\":%u,"
                  "\"temp_milli\":%ld,\"humidity_present\":%u}\n",
                  temperature.hasValue(), long(temperature.value() * 1000 + 0.5),
                  humidity.hasValue());
    restored = true;
  }
  static uint32_t lastReport = 0;
  const uint32_t reportStart = micros();
  if (reporting && client.ready() && millis() - lastReport >= 200) {
    lastReport = millis();
    if (!temperature.report(24.0 + (reports % 10) * 0.1).accepted()) ++rejected;
    if (!humidity.report(50.0 + reports % 5).accepted()) ++rejected;
    ++reports;
  }
  // Include immediate report/persistence work without including serial output.
  const uint32_t sdkWorkUs = elapsed + (micros() - reportStart);
  sdkWorkTotalUs += sdkWorkUs;
  if (sdkWorkUs > sdkWorkMaxUs) sdkWorkMaxUs = sdkWorkUs;
  static uint32_t lastSample = 0;
  if (millis() - lastSample >= 1000) {
    lastSample = millis();
    const bool dirty = client.datastream<double>("temperature").snapshot().dirty ||
                       client.datastream<double>("humidity").snapshot().dirty;
    Serial.printf("FLOVA_E2E {\"event\":\"sample\",\"ms\":%u,\"ready\":%u,"
                  "\"wifi\":%u,\"connected\":%u,\"heap\":%u,\"min_heap\":%u,"
                  "\"largest\":%u,\"stack\":%u,\"run_count\":%u,"
                  "\"run_total_us\":%llu,\"run_max_us\":%u,\"reports\":%u,"
                  "\"rejected\":%u,\"storage_failures\":%u,\"dirty\":%u,"
                  "\"sdk_work_total_us\":%llu,\"sdk_work_max_us\":%u}\n",
                  millis(), client.ready(), board_test::wifi(), client.connected(),
                  board_test::freeHeap(), board_test::minimumHeap(),
                  board_test::largestBlock(), board_test::stackHeadroom(), runCount,
                  static_cast<unsigned long long>(runTotalUs), runMaxUs, reports,
                  rejected, client.diagnostics().storageFailures, dirty,
                  static_cast<unsigned long long>(sdkWorkTotalUs), sdkWorkMaxUs);
    runCount = 0;
    runTotalUs = 0;
    runMaxUs = 0;
    sdkWorkTotalUs = 0;
    sdkWorkMaxUs = 0;
  }
  if (Serial.available()) handleTestCommand(Serial.read());
  delay(1);
}
