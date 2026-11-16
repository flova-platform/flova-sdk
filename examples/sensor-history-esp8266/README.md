# sensor-history · ESP8266

Build the root environment `sensor-history-esp8266`. No external wiring is required:
temperature is a simulated 22.5 °C reading, reported once per second. Replace
the constant with your sensor reading when ready. The template needs float
telemetry `temperature` with unit °C.

Upload, start provisioning in Flova, join `Flova-Setup-*` when prompted, and
use the supplied setup link. Return to the internet and observe temperature
in the device page. Reboot and verify it reconnects without changing code.
See the root README for setup-password and storage behavior.

This example explicitly enables deferred latest-value persistence (30 seconds)
and offline history (four records, at least 60 seconds apart, at most one hour
old). Power loss can lose the unflushed latest value. These are separate from
provisioning credentials, which are always durable in this managed composition.
