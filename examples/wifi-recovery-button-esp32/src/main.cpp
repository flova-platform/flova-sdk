#include <Arduino.h>
#include <FlovaSDK.h>

// Create a writable Boolean stream named "led" in your Flova template.
// ESP32: connect an LED with a resistor to GPIO2. ESP8266: built-in active-low LED.
constexpr uint8_t LED_PIN = 2;
FlovaEsp32 device;
auto led = FLOVA_DATASTREAM(device, bool, "led");

// Wire a separate button from this GPIO to GND. Check your board pinout.
constexpr uint8_t WIFI_BUTTON_PIN = 4;
void setup() {
  pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
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
  static uint32_t pressedAt = 0;
  static bool pressed = false;
  static bool handled = false;
  if (digitalRead(WIFI_BUTTON_PIN) == LOW) {
    if (!pressed) { pressed = true; pressedAt = millis(); }
    if (!handled && static_cast<uint32_t>(millis() - pressedAt) >= 3000) {
      handled = true;
      Serial.println(device.startWifiSetup() ? "Join Flova-Setup and open http://192.168.4.1/setup" : "Device busy or not provisioned");
    }
  } else { pressed = false; handled = false; }

  static bool wasReady = false;
  if (device.ready() != wasReady) {
    wasReady = device.ready();
    Serial.println(wasReady ? "Flova ready" : "Flova offline; reconnecting");
  }
  yield();
}
