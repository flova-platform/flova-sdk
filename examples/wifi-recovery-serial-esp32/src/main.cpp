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
  // Type wifi followed by Enter in the 115200-baud serial monitor.
  // Input is bounded; oversized commands are discarded until the next newline.
  static char command[5] = {};
  static uint8_t length = 0;
  static bool overflow = false;
  for (uint8_t budget = 0; budget < 16 && Serial.available(); ++budget) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!overflow && !strcmp(command, "wifi"))
        Serial.println(device.startWifiSetup() ? "Join Flova-Setup and open http://192.168.4.1/setup" : "Device busy or not provisioned");
      length = 0; command[0] = 0; overflow = false;
    } else if (length < sizeof(command) - 1) {
      command[length++] = c; command[length] = 0;
    } else overflow = true;
  }

  static bool wasReady = false;
  if (device.ready() != wasReady) {
    wasReady = device.ready();
    Serial.println(wasReady ? "Flova ready" : "Flova offline; reconnecting");
  }
  yield();
}
