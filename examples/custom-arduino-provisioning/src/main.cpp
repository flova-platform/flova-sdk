#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FlovaSDK.h>

namespace {
// This demonstrates integration into an existing application that already
// owns Wi-Fi and an HTTP server. It is intentionally different from
// FlovaUniversalEsp32: the SDK will not create a SoftAP or replace this server.
const uint8_t RELAY_PIN = 2;
const char* WIFI_SSID = "your-wifi";
const char* WIFI_PASSWORD = "your-password";

// Advanced integration: each service has a stable application-owned lifetime.
FlovaEsp32Entropy entropy;
FlovaEsp32Platform platform;
ArduinoFlovaLink link(platform, entropy);
FlovaEsp32Storage storage;
ArduinoFlovaClock clock;
ArduinoFlovaLogger logger;
ArduinoFlovaManualHardware hardware;
FlovaProvisioningAdapter provisioning;
FlovaEsp32ObservedNetwork network;
ArduinoFlovaUtcBootstrap<WiFiUDP> tlsClock;
FlovaEsp32Identity identity("custom_arduino_esp32");
FlovaClient client(link, provisioning, network, tlsClock, identity, storage,
                   clock, logger, entropy, hardware);
WebServer server(80);
// Datastream keys are stable developer/API names. Engine supplies the compact
// runtime ID after the device connects and binds its declared keys.
auto relay = FLOVA_DATASTREAM(client, bool, "LED");
}

void setup() {
  Serial.begin(115200);

  // The application chooses how and where it joins Wi-Fi. A BLE, Ethernet,
  // cellular, or factory flow could provide the same ProvisioningHandoff
  // directly instead of using the helper routes below.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Dashboard, user, automation, and schedule writes all use this one safe
  // actuator path. The handler runs from client.run().
  FLOVA_ON_WRITE(relay, [](bool enabled) {
    digitalWrite(RELAY_PIN, enabled ? HIGH : LOW);
  });
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

  // A new device waits in AwaitingProvisioning until its selected channel
  // supplies a link URL/token handoff. Existing devices restore identity.
  if (!client.begin(false)) Serial.println("[flova] client startup failed");
}

void loop() {
  // The application remains responsible for servicing its own web server.
  server.handleClient();

  // Flova processes Device Link, applies remote commands, sends state, and
  // performs reconnect work from this cooperative loop.
  client.run();
  yield();
}
