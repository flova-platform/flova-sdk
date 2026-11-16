# Temporary ESP32 moisture controller

This custom sensor application uses managed SoftAP provisioning and Flova Link.
Wi-Fi and device credentials arrive through setup; the same firmware can be
used with multiple devices. Calibration and wiring remain application-owned.

## Wiring

| Moisture sensor pin | ESP32 DevKit |
| --- | --- |
| `VCC` | `3V3` |
| `GND` | `GND` |
| `AO` / analog output | `GPIO32` (`ADC1_CH4`) |
| `DO` / digital output | Leave unconnected |

Use `3V3` for the sensor supply. Do not feed a 5 V analog output into GPIO32.
The sensor and ESP32 must share ground. There are no display connections in
this version. The built-in LED does not need external wiring.

GPIO32 is an ADC1 input, so it remains available while the ESP32 uses Wi-Fi.
The firmware samples it at 12-bit resolution and averages eight readings.

## Calibration

The initial ESP32 raw calibration values are:

| Key | Initial value | Meaning |
| --- | ---: | --- |
| `wet_adc` | 1200 | Sensor raw value when wet |
| `dry_adc` | 3000 | Sensor raw value when dry |

These values depend on the sensor and soil. Adjust the same settings in the
template's **تنظیمات فریم‌ور** CRUD, or replace the defaults in `src/main.cpp`.
The `MOISTURE` and `led` keys must exist in the device's published template;
`MOISTURE` is numeric/read-only and `led` is boolean/read/write.

## Build and upload

From this folder:

```sh
pio run -e esp32dev
pio run -e esp32dev --target upload
pio device monitor -b 115200
```



## Managed setup (current SDK)

This example uses managed SoftAP setup; no Wi-Fi or device secrets go in source.
Build the matching named environment in the root `platformio.ini`. Moisture
projects instead use their own directory's `platformio.ini`.

Required template data: `MOISTURE` — double telemetry (%); calibration settings `dry_adc`, `wet_adc`, and `dry_threshold`. ESP32 also declares writable Boolean `led`. Use the pin constants and calibration values in the source; supply sensors with a safe 3.3 V signal.

Start the device provisioning flow in Flova after uploading, join `Flova-Setup-*`
when prompted, and use the provided handoff link. Return to the internet and
check the device is online. Toggle the writable LED stream, or observe sensor
reports for sensor-only examples. Reboot and verify automatic reconnection.
Open the serial monitor at 115200 baud for startup and connection errors.

ESP32 starter/recovery examples use an external LED with a resistor on GPIO2;
ESP8266 starters use the active-low built-in LED. Other examples specify their
pins at the top of the source. Verify the board pinout before wiring.
