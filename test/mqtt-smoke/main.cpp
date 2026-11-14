#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <sys/time.h>

// Generated privately by the hardware runner; never commit Wi-Fi credentials,
// broker credentials or a test CA's private key.
#include <MqttSmokeConfig.h>
#include <FlovaEsp8266BuildProfile.h>
#include <FlovaEsp8266Platform.h>
#include <FlovaMqtt.h>

static FlovaEsp8266Platform platform;
static FlovaMqtt mqtt(platform);
static uint8_t payload[512];
static uint32_t attempts, connections, echoes, sequence, sentAt, reconnectAt;
static uint32_t minHeap = UINT32_MAX, minLargest = UINT32_MAX, minStack = UINT32_MAX, maxPollUs;
static bool starting, online, complete;

static bool received(void*, const uint8_t* bytes, size_t length) {
  if (length != sizeof(payload) || memcmp(bytes, payload, length)) return false;
  ++echoes;
  return true;
}

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(MQTT_SMOKE_WIFI_SSID, MQTT_SMOKE_WIFI_PASSWORD);
  struct timeval now = {MQTT_SMOKE_UTC, 0};
  settimeofday(&now, nullptr);
  Serial.printf("{\"mqtt_smoke\":\"boot\",\"reset_reason\":%u}\n", ESP.getResetInfoPtr()->reason);
}

void loop() {
  if (complete) { delay(10); return; }
  minHeap = min(minHeap, ESP.getFreeHeap());
  minLargest = min(minLargest, ESP.getMaxFreeBlockSize());
  minStack = min(minStack, ESP.getFreeContStack());
  if (WiFi.status() != WL_CONNECTED || millis() < reconnectAt) { delay(1); return; }
  if (!online && !starting) {
    ++attempts;
    const bool roots = platform.beginLink();
    const bool opened = roots && platform.startLink(MQTT_SMOKE_HOST, MQTT_SMOKE_PORT);
    if (attempts > 12 || !roots || !opened) {
      Serial.printf("{\"mqtt_smoke\":\"failed_open\",\"attempts\":%lu,\"roots\":%u,\"opened\":%u,\"heap\":%u}\n",
        static_cast<unsigned long>(attempts), roots ? 1U : 0U, opened ? 1U : 0U, ESP.getFreeHeap());
      complete = true; return;
    }
    starting = true;
  }
  if (starting) {
    const FlovaLinkOpenStatus status = platform.pollLink();
    if (status == FlovaLinkOpenStatus::InProgress) return;
    if (status == FlovaLinkOpenStatus::Failed) {
      Serial.printf("{\"mqtt_smoke\":\"tls_failed\",\"attempts\":%lu,\"error\":\"%s\"}\n",
        static_cast<unsigned long>(attempts), platform.linkError());
      starting = false; reconnectAt = millis() + 1000; return;
    }
    char epoch[17];
    snprintf(epoch, sizeof(epoch), "%08lx%08lx", static_cast<unsigned long>(ESP.random()),
             static_cast<unsigned long>(ESP.random()));
    if (!mqtt.connect(MQTT_SMOKE_PRINCIPAL, MQTT_SMOKE_PASSWORD, epoch, received, nullptr)) {
      Serial.printf("{\"mqtt_smoke\":\"connect_failed\",\"status\":%u}\n", static_cast<unsigned>(mqtt.lastStatus()));
      platform.closeLink(); starting = false; reconnectAt = millis() + 1000; return;
    }
    starting = false; online = true; ++connections;
  }
  const uint32_t start = micros();
  const bool ok = mqtt.poll();
  maxPollUs = max(maxPollUs, static_cast<uint32_t>(micros() - start));
  if (!ok) {
    Serial.println("{\"mqtt_smoke\":\"poll_failed\"}");
    platform.closeLink(); mqtt.reset(); online = false; reconnectAt = millis() + 1000; return;
  }
  // One outstanding echo keeps the comparison workspace immutable until ACKed.
  if (mqtt.ready() && echoes == sequence && millis() - sentAt >= 200) {
    if (echoes && echoes % 20 == 0 && connections <= echoes / 20) {
      Serial.printf("{\"mqtt_smoke\":\"cycle\",\"connections\":%lu,\"echoes\":%lu,\"heap\":%u,\"largest\":%u,\"stack\":%u,\"max_poll_us\":%lu}\n",
        static_cast<unsigned long>(connections), static_cast<unsigned long>(echoes),
        minHeap, minLargest, minStack, static_cast<unsigned long>(maxPollUs));
      platform.closeLink(); mqtt.reset(); online = false;
      if (connections >= 6) { complete = true; Serial.println("{\"mqtt_smoke\":\"passed\"}"); }
      else reconnectAt = millis() + 500;
      return;
    }
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<uint8_t>(sequence + i);
    if (mqtt.publish(payload, sizeof(payload))) { ++sequence; sentAt = millis(); }
  }
  if (sequence > echoes && millis() - sentAt > 10000) {
    complete = true; Serial.println("{\"mqtt_smoke\":\"echo_timeout\"}");
  }
  delay(1);
}
