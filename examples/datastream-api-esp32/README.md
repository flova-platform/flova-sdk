# datastream-api-esp32

This example uses managed SoftAP setup; no Wi-Fi or device secrets go in source.
Build the matching named environment in the root `platformio.ini`. Moisture
projects instead use their own directory's `platformio.ini`.

Required template data: `temperature` — float telemetry (°C); `relay` — writable Boolean; `cook_mode` and `lamp_color` — writable text. Temperature is simulated; replace it with a real sensor reading.

Start the device provisioning flow in Flova after uploading, join `Flova-Setup-*`
when prompted, and use the provided handoff link. Return to the internet and
check the device is online. Toggle the writable LED stream, or observe sensor
reports for sensor-only examples. Reboot and verify automatic reconnection.
Open the serial monitor at 115200 baud for startup and connection errors.

ESP32 starter/recovery examples use an external LED with a resistor on GPIO2;
ESP8266 starters use the active-low built-in LED. Other examples specify their
pins at the top of the source. Verify the board pinout before wiring.
