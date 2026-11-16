#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FlovaSDK.h>

namespace {
// Existing-project integration: this sketch owns Wi-Fi, the HTTP server, the
// GPIO, and reboot policy. Flova adds bounded runtime services and optional
// provisioning routes without taking over the application.
const uint8_t RELAY_PIN = LED_BUILTIN;
const char* WIFI_SSID = "your-wifi";
const char* WIFI_PASSWORD = "your-password";
// Advanced integration: each service has a stable application-owned lifetime.
FlovaEsp8266Entropy entropy;
FlovaEsp8266Platform platform;
ArduinoFlovaLink link(platform, entropy);
FlovaEsp8266Storage storage;
ArduinoFlovaClock clock;
ArduinoFlovaLogger logger;
ArduinoFlovaManualHardware hardware;
FlovaProvisioningAdapter provisioning;
FlovaEsp8266ObservedNetwork network;
ArduinoFlovaUtcBootstrap<WiFiUDP> tlsClock;
FlovaEsp8266Identity identity("custom_arduino_esp8266");
FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                   clock, logger, entropy, hardware);
ESP8266WebServer server(80);
// The key is used for SDK declarations and Engine configuration. Device Link
// later uses the server-assigned numeric ID, not this string on every frame.
auto relay = FLOVA_DATASTREAM(client, bool, "LED");
}

void setup() {
  Serial.begin(115200);

  // This explicit composition never takes over networking. Other channels
  // can call provision() with the same channel-neutral Link URL/token handoff.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // LED_BUILTIN is active-low.

  // Dashboard, user, schedule, and automation writes all reach this callback.
  FLOVA_ON_WRITE(relay, [](bool enabled) {
    digitalWrite(RELAY_PIN, enabled ? LOW : HIGH);
  });
  // These routes are attached to the application's existing server. The SDK
  // does not start a SoftAP or silently stop this server after provisioning.
  // The application supplies only the Flova handoff; its Wi-Fi is already set up.
  // The bounded request workspace lives for the application's lifetime.
  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"status\":\"setup_mode\",\"protocol\":\"flova-link-v1\",\"can_retry\":true}");
  });
  server.on("/provision", HTTP_POST, []() {
    static flova::ProvisioningHandoff input;
    const String& body = server.arg("plain");
    if (body.length() >= 768 ||
        !flova::parseWifiProvisioningHandoff(body.c_str(), body.length(), input)) {
      server.send(400, "application/json", "{\"error\":\"invalid_handoff\"}");
      return;
    }
    const auto result = client.provision(input);
    if (result == FlovaProvisioningResponse::Accepted)
      server.send(202, "application/json", "{\"ok\":true,\"status\":\"accepted\"}");
    else if (result == FlovaProvisioningResponse::StorageFailed)
      server.send(500, "application/json", "{\"error\":\"storage_failed\"}");
    else
      server.send(409, "application/json", "{\"error\":\"invalid_state\"}");
  });
  server.begin();

  // A fresh device waits for a ProvisioningHandoff; a configured device
  // restores its private identity and continues toward runtime.
  if (!client.begin(false)) Serial.println("[flova] client startup failed");
}

void loop() {
  // Service the application-owned server, then process queued protocol work
  // and invoke hardware handlers from the loop.
  server.handleClient();
  client.run();
  yield();
}
