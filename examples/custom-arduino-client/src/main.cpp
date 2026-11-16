#include <Arduino.h>
#include <FlovaSDK.h>

// Create a writable Boolean stream named "led" in your Flova template.
// ESP32: connect an LED with a resistor to GPIO2. ESP8266: built-in active-low LED.
constexpr uint8_t LED_PIN = 2;
FlovaEsp32 device;
auto led = FLOVA_DATASTREAM(device, bool, "led");

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  FLOVA_ON_WRITE(led, [](bool enabled) {
    digitalWrite(LED_PIN, enabled ? HIGH : LOW);
  });
  device.setStatusListener([](void*, const FlovaStatusEvent& event) {
    if (event.current.errorCode[0]) {
      Serial.print("Flova error: "); Serial.println(event.current.errorCode);
    } else if (event.current.lifecycle == FlovaLifecycle::Setup) {
      Serial.println("Wi-Fi setup: continue provisioning in Flova.");
    } else if (!event.current.networkConnected) {
      Serial.println("Connecting to Wi-Fi...");
    } else if (!event.current.ready) {
      Serial.println("Wi-Fi connected; connecting to Flova...");
    }
  });
  if (!device.begin()) {
    Serial.print("Startup failed: ");
    Serial.println(device.lastError());
  }
}

void loop() {
  device.run();
  static bool wasReady = false;
  if (device.ready() != wasReady) {
    wasReady = device.ready();
    Serial.println(wasReady ? "Flova ready" : "Flova offline; reconnecting");
  }
  yield();
}
