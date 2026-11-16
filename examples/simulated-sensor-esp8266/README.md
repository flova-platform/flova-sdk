# simulated-sensor · ESP8266

Build the root environment `simulated-sensor-esp8266`. No external wiring is required:
temperature is a simulated 22.5 °C reading, reported once per second. Replace
the constant with your sensor reading when ready. The template needs float
telemetry `temperature` with unit °C.

Upload, start provisioning in Flova, join `Flova-Setup-*` when prompted, and
use the supplied setup link. Return to the internet and observe temperature
in the device page. Reboot and verify it reconnects without changing code.
See the root README for setup-password and storage behavior.

Sensor-value persistence and offline history are disabled in this example.
