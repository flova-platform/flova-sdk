# Temporary ESP8266 moisture controller

This custom sensor application uses managed SoftAP provisioning and Flova Link.
Wi-Fi and device credentials arrive through setup; the same firmware can be
used with multiple devices. Calibration and wiring remain application-owned.

## Wiring

| Part | NodeMCU |
| --- | --- |
| ST7789 VCC | 3V3 |
| ST7789 BLK | 3V3 |
| ST7789 GND | GND |
| ST7789 SCL | D5 |
| ST7789 SDA | D7 |
| ST7789 RES | D2 |
| ST7789 DC | D1 |
| Moisture VCC | 3V3 |
| Moisture GND | GND |
| Moisture AO | A0 |

The display and sensor share the one 3V3 pin through a breadboard rail or a
splitter. The built-in LED is the dry-state indicator. It is not a pump output.
This seven-pin panel has no external CS connection; leave D0 unconnected.

## Build and upload

From this folder:

```sh
pio run
pio run --target upload
pio device monitor
```

The firmware declares these integer template settings:

| Key | Initial value | Meaning |
| --- | ---: | --- |
| `wet_adc` | 280 | Sensor raw value in water |
| `dry_adc` | 608 | Sensor raw value in dry air |

Create or edit the same settings in the template’s **تنظیمات فریم‌ور** CRUD
and set their initial values if the device should receive them from Flova.


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
