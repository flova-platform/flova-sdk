# Custom Arduino LED starter

Start here for custom ESP32 or ESP8266 firmware. `FlovaEsp32` and
`FlovaEsp8266` manage setup and reconnection; the sketch owns its pins
and write handlers. Declare streams before `begin()` and keep calling `run()`.

`FLOVA_ON_WRITE` applies commands, `FLOVA_WRITE` submits a local command,
`FLOVA_REPORT` supplies an observed value, and `FLOVA_READ` reads the local cache.
Use `FLOVA_HAS_VALUE` before reading a stream that may have no value yet.

Build from the repository root:

```sh
pio run -e custom-arduino-client-esp32
pio run -e custom-arduino-client-esp8266
```

## Managed setup (current SDK)

This example uses managed SoftAP setup; no Wi-Fi or device secrets go in source.

Required template data: `led` — writable Boolean.

Start the device provisioning flow in Flova after uploading, join `Flova-Setup-*`
when prompted, and use the provided handoff link. Return to the internet and
check the device is online. Toggle the writable LED stream, or observe sensor
reports for sensor-only examples. Reboot and verify automatic reconnection.
Open the serial monitor at 115200 baud for startup and connection errors.

ESP32 starter/recovery examples use an external LED with a resistor on GPIO2;
ESP8266 starters use the active-low built-in LED. Other examples specify their
pins at the top of the source. Verify the board pinout before wiring.
