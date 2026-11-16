# ESP32 touch/LED offline test

This is the ESP32 custom touch/LED example. The sketch owns its fixed GPIOs,
GPIO behavior; the managed SDK owns Wi-Fi setup and reconnection.

Wiring:

```text
external LED + resistor   -> D2 / GPIO2
touch module digital OUT  -> D4 / GPIO4
```

The example uses these exact template keys:

```text
LED          = boolean read/write
TOUCH_SENSOR = boolean read-only
```

If your template uses different keys, change the two string literals in the
selected example source. The keys must match Engine exactly.

Build and flash:

```sh
pio run -e esp32-touch-test -t upload
# BLE provisioning variant:
pio run -e esp32-touch-test-ble -t upload
pio device monitor -b 115200
```

Touching the sensor reports `TOUCH_SENSOR` and toggles `LED` through
`led.write(...)`. With Wi-Fi disconnected after provisioning, local toggles
continue because `LED` uses `OfflinePolicy::KeepLatest`; after reconnect, the
latest LED and touch states synchronize to Engine. The LED value is also
persisted and reapplied to GPIO2 after a power cycle.

Disable any automation that also toggles `LED` from `TOUCH_SENSOR` while testing
the local behavior, or one physical touch may be toggled twice.

This custom example intentionally does not use template pin mappings. The
sketch owns GPIO2/GPIO4 directly; advanced applications can change those
constants or replace the hardware behavior entirely.

The BLE target uses the repository-owned 4 MiB A/B partition and supports
authenticated OTA. Existing devices flashed with the previous `no_ota.csv`
layout need one wired PlatformIO upload before they can receive OTA updates;
the NVS partition remains at the same offset, so this migration does not erase
Flova configuration. It also uses the bounded BLE profile, so its runtime
datastream capacity is smaller than the SoftAP target. BLE provisioning
currently uses Security 1 with a null proof of possession for MVP testing.


## Managed setup (current SDK)

This example uses managed SoftAP setup; no Wi-Fi or device secrets go in source.
Build the matching named environment in the root `platformio.ini`. Moisture
projects instead use their own directory's `platformio.ini`.

Required template data: `LED` — writable Boolean; `TOUCH_SENSOR` — Boolean telemetry. Match the GPIO constants to your wiring.

Start the device provisioning flow in Flova after uploading, join `Flova-Setup-*`
when prompted, and use the provided handoff link. Return to the internet and
check the device is online. Toggle the writable LED stream, or observe sensor
reports for sensor-only examples. Reboot and verify automatic reconnection.
Open the serial monitor at 115200 baud for startup and connection errors.

ESP32 starter/recovery examples use an external LED with a resistor on GPIO2;
ESP8266 starters use the active-low built-in LED. Other examples specify their
pins at the top of the source. Verify the board pinout before wiring.
