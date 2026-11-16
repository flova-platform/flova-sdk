#include <Arduino.h>
#include <FlovaSDK.h>

// Universal firmware owns cloud-configured GPIO mappings and OTA policy.
// FlovaEsp32 also manages setup and reconnection, but custom sketches keep
// ownership of their pins and write handlers.
// FLOVA_SETUP_PASSWORD is optional; omitting it leaves the setup AP open.
// For production, pass a device-unique 8-63 character WPA label password.
FlovaUniversalEsp32 device;

void setup() {
  Serial.begin(115200);
  delay(50);
  FLOVA_SERIAL_PRINTLN_INFO("");
  FLOVA_SERIAL_PRINTLN_INFO("[flova] esp32 boot");

  // A configured device restores private state. A fresh device enters the
  // universal provisioning lifecycle used by the mobile QR flow.
  device.begin();
}

void loop() {
  // This composition owns setup HTTP handling, Device Link, dynamic
  // datastream configuration, hardware mappings, OTA, and restart work.
  device.run();
}
