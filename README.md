<div align="center">
  <h1>FlovaSDK</h1>
  <p><strong>Build connected Linux, ESP32, and ESP8266 devices with Flova.</strong></p>
  <p>
    <a href="README_FA.md">فارسی</a> ·
    <a href="https://docs.flova.ir/en/docs/sdk/overview">Documentation</a> ·
    <a href="https://docs.flova.ir/en/docs/sdk/first-sketch/">Tutorial</a> ·
    <a href="https://github.com/flova-platform/flova-sdk/tags">SDK versions</a>
  </p>
  <p>
    <a href="https://github.com/flova-platform/flova-sdk/tags"><img alt="SDK version" src="https://img.shields.io/github/v/tag/flova-platform/flova-sdk?filter=v*&amp;sort=semver&amp;style=flat-square&amp;label=SDK"></a>
    <a href="https://registry.platformio.org/libraries/flova-platform/FlovaSDK"><img alt="PlatformIO Registry" src="https://badges.registry.platformio.org/packages/flova-platform/library/FlovaSDK.svg"></a>
    <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/github/license/flova-platform/flova-sdk?style=flat-square"></a>
  </p>
</div>

FlovaSDK is the official embedded C++ SDK for connecting ESP32, ESP8266, Linux,
and custom C++ devices to Flova. It provides typed datastreams, provisioning,
configuration, device lifecycle handling, and Flova Link transport adapters.

## Log an event from Arduino firmware

After `device.begin()`, use `logEvent` from the application loop to send a
custom event to **Monitor → Custom events** in Flova Console. There is no
Device Type registration step.

```cpp
device.logEvent("temperature_high");
FLOVA_LOG_EVENT(device, "temperature_high", "Temperature exceeded 80 C");
```

Codes are 1–32 ASCII bytes: start with a lowercase letter, then use lowercase
letters, digits, or underscores. The optional message is at most 96 UTF-8 bytes.
Never include credentials, secrets, or sensitive data. Log transitions, not every
sample. The SDK holds one pending event in RAM even when offline; `false` means
the input was invalid, the client has not started, or the slot is occupied.
`true` means queued, not stored. The SDK retries until Engine acknowledges or
rejects the event; restarting the device discards any unacknowledged event.
Deploy the matching Engine release before using this API in firmware.


### Offline actuator ownership

Custom builds can enable `FLOVA_ACTUATOR_CONTROL=1` and attach an
`ActuatorControl` to a writable datastream with `setActuatorControl()` before
startup. Keep the control object and its callback context alive for the Device
lifetime. The callback runs at startup and when cloud ownership is lost. It
should apply the firmware’s safe state or start the application’s bounded
local control. Return success only after the application has taken control.
Disable stream-value persistence for the actuator and configure safe hardware
startup before networking.

When ready, a trusted application action can call `authorizeCloudControl()`.
Authorization is held in RAM and expires on disconnect or authenticated
rebind. While local control owns the actuator, the SDK rejects remote writes.
Remote writes also require a valid command deadline. The application can
publish `cloudControlsActuator()` alongside actuator feedback.

Console-authored offline guardrails use the shared datastream writer for
relay, PWM, and custom outputs. Writable state streams need a result-returning
local writer, and the device must report any required input locally.

The standard `universal-esp32` and `universal-esp8266` profiles compile out the
shared guardrail evaluator and its storage. Choose `universal-esp32-guardrails`
or `universal-esp8266-guardrails` to enable two slots. These builds advertise
their slots after persistent storage initializes. Numeric and Boolean values
are stored inline.
Text policies share a bounded pool of twice the profile's SDK text capacity.
Custom builds can set `FLOVA_OFFLINE_GUARDRAIL_CAPACITY` and validate firmware
memory and control-loop timing on their hardware.
Deploy the matching Engine before enabling a firmware profile that advertises
guardrail support.

## Choose a starting point

| Target                                          | Use                                                   | Start here                                                        |
| ----------------------------------------------- | ----------------------------------------------------- | ----------------------------------------------------------------- |
| ESP32/ESP8266, no application code              | Flova owns setup, networking, mappings, and lifecycle | [Quick start](https://docs.flova.ir/en/docs/sdk/quickstart)       |
| Browser-based editing, building, and USB upload | Flova IDE in Console                                  | [Flova IDE guide](https://docs.flova.ir/en/docs/sdk/ide)          |
| Existing Arduino application                    | Your code owns hardware and application behavior      | [Custom applications](https://docs.flova.ir/en/docs/sdk/advanced) |
| Linux or Raspberry Pi                           | Your process owns the event loop and hardware         | [Installation](https://docs.flova.ir/en/docs/sdk/install)         |
| Other C++11 hardware                            | Your board supplies the SDK service interfaces        | [Custom boards](https://docs.flova.ir/en/docs/sdk/advanced)       |

## Install

These examples target the `0.5.2` source tree. If your library manager does
not offer that version, follow the [source export instructions](https://docs.flova.ir/en/docs/sdk/install#install-from-source).

### PlatformIO

Add the SDK to `platformio.ini`:

```ini
lib_deps = flova-platform/FlovaSDK@^0.5.2
```

Use the packaged ESP8266 profile in
`packages/flova-sdk-release/extras/platformio/esp8266/platformio.ini` when you
need the tested memory and TLS settings.

### Arduino IDE

1. Install the ESP32 or ESP8266 board package in **Tools → Board → Boards Manager**.
2. Install **FlovaSDK** from **Tools → Manage Libraries**.
3. Select your board and serial port.
4. Include the SDK:

```cpp
#include <FlovaSDK.h>
```

The selected board determines whether the ESP32 or ESP8266 facade is used. The
full IDE setup, board profiles, and troubleshooting steps are in the
[installation guide](https://docs.flova.ir/en/docs/sdk/install).

### Linux and Raspberry Pi

```sh
sudo apt install cmake libssl-dev
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Applications link `Flova::Linux` with `find_package(FlovaLinux CONFIG REQUIRED)`.
See [Linux installation](https://docs.flova.ir/en/docs/sdk/install).

## Quick start

This managed ESP32 sketch controls an LED wired to GPIO2. Flova handles Wi-Fi
setup and reconnection.

```cpp
#include <Arduino.h>
#include <FlovaSDK.h>

FlovaEsp32 device;
auto led = FLOVA_DATASTREAM(device, bool, "led");

void setup() {
  pinMode(2, OUTPUT);
  FLOVA_ON_WRITE(led, [](bool enabled) {
    digitalWrite(2, enabled ? HIGH : LOW);
  });
  device.begin();
}

void loop() {
  device.run();
}
```

Create the matching published datastream in your Flova device template before
using the application. Call `device.run()` frequently; remote writes are
delivered from that loop. Check `begin()`, registration, and write results in
production code.

## Learn the SDK

- [SDK overview](https://docs.flova.ir/en/docs/sdk/overview)
- [Datastreams](https://docs.flova.ir/en/docs/sdk/datastreams)
- [Firmware timers](https://docs.flova.ir/en/docs/sdk/timers)
- [Provisioning](https://docs.flova.ir/en/docs/sdk/provisioning)
- [Configuration](https://docs.flova.ir/en/docs/sdk/configuration)
- [Transports](https://docs.flova.ir/en/docs/sdk/transports)
- [Lifecycle, OTA, and logging](https://docs.flova.ir/en/docs/sdk/lifecycle)
- [API reference](https://docs.flova.ir/en/docs/sdk/reference)

The documentation is the source for detailed behavior, including persistence,
offline history, schedules, Wi-Fi recovery, command delivery, and target
limitations. Do not put credentials, provisioning tokens, or device secrets in
source code or logs.

## License

FlovaSDK is available under the [MIT License](LICENSE).
