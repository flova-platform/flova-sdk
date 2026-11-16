#include <Arduino.h>
#include <FlovaSDK.h>

FlovaEsp32 device;
// Match this exact float telemetry key and °C unit in the device template.
auto temperature = FLOVA_DATASTREAM(device, float, "temperature");

void setup() {
  Serial.begin(115200);
  if (!device.begin()) {
    Serial.print("Startup failed: "); Serial.println(device.lastError());
  }
}

void loop() {
  device.run();
  static uint32_t lastReport = 0;
  if (static_cast<uint32_t>(millis() - lastReport) >= 1000) {
    lastReport = millis();
    // Simulated data: replace this constant with a real sensor reading.
    const auto result = FLOVA_REPORT(temperature, 22.5f);
    if (!result.accepted()) {
      Serial.print("Report rejected: "); Serial.println(result.reason);
    }
  }
  yield();
}
