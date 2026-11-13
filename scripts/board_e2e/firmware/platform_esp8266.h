#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <sys/time.h>
#include <FlovaEsp8266TlsProfile.h>

using TestClient = FlovaEsp8266;

namespace board_test {
// ESP8266 has no lifetime heap low-water API. Sample DRAM at application
// boundaries; allocations inside a blocking TLS call can be lower than this.
static uint32_t minimumDram = UINT32_MAX;
inline void sampleMemory() {
  const auto stats = flova::tlsHeapStats();
  if (stats.dramFree < minimumDram) minimumDram = stats.dramFree;
}
inline void connect() { WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD); }
inline void begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  timeval now = {TEST_EPOCH, 0};
  settimeofday(&now, nullptr);
  sampleMemory();
  connect();
}
inline bool wifi() { return WiFi.status() == WL_CONNECTED; }
inline void disconnect() { WiFi.disconnect(false); }
inline void restart() { ESP.restart(); }
inline uint32_t freeHeap() { return flova::tlsHeapStats().dramFree; }
inline uint32_t minimumHeap() { sampleMemory(); return minimumDram; }
inline uint32_t largestBlock() { return flova::tlsHeapStats().dramMaxBlock; }
inline uint32_t stackHeadroom() { return ESP.getFreeContStack(); }
inline void printMemoryFields() {
  const auto stats = flova::tlsHeapStats();
  Serial.printf(",\"fragmentation\":%u,\"tls_stack\":%u",
                stats.dramFragmentation, stats.tlsStackUsed);
}
}  // namespace board_test
