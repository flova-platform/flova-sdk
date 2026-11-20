#include <Arduino.h>
#include <FlovaSDK.h>

// Create a writable Boolean stream named "led" in your Flova template.
// ESP32: connect an LED with a resistor to GPIO2. ESP8266: built-in active-low LED.
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t LED_PIN = 2;
constexpr uint8_t LED_ON = HIGH, LED_OFF = LOW;
FlovaEsp32 device;
#else
constexpr uint8_t LED_PIN = LED_BUILTIN;
constexpr uint8_t LED_ON = LOW, LED_OFF = HIGH;
FlovaEsp8266 device;
#endif
auto led = FLOVA_DATASTREAM(device, bool, "led");

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
  FLOVA_ON_WRITE(led, [](bool enabled) {
    digitalWrite(LED_PIN, enabled ? LED_ON : LED_OFF);
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
