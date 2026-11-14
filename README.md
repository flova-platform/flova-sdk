<div align="center">
  <h1>FlovaSDK</h1>
  <p><strong>Build connected Linux, ESP32, and ESP8266 devices with Flova.</strong></p>
  <p>
    <a href="README_FA.md">فارسی</a> ·
    <a href="https://docs.flova.ir">Documentation</a> ·
    <a href="examples">Examples</a> ·
    <a href="https://github.com/flova-platform/flova-sdk/tags">SDK versions</a>
  </p>
  <p>
    <a href="https://github.com/flova-platform/flova-sdk/tags"><img alt="SDK version" src="https://img.shields.io/github/v/tag/flova-platform/flova-sdk?filter=v*&amp;sort=semver&amp;style=flat-square&amp;label=SDK"></a>
    <a href="https://registry.platformio.org/libraries/flova-platform/FlovaSDK"><img alt="PlatformIO Registry" src="https://badges.registry.platformio.org/packages/flova-platform/library/FlovaSDK.svg"></a>
    <a href="https://github.com/arduino/library-registry/pull/9043"><img alt="Arduino Library Manager" src="https://img.shields.io/badge/Arduino%20Library%20Manager-FlovaSDK-00878F?style=flat-square&amp;logo=arduino&amp;logoColor=white"></a>
    <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/github/license/flova-platform/flova-sdk?style=flat-square"></a>
  </p>
</div>

FlovaSDK is the official embedded C++ SDK for connecting devices to the Flova
platform. It provides ready-to-use integrations for Linux, ESP32, and ESP8266,
with a portable C++11 core for custom hardware.

> For provisioning, datastreams, device configuration, OTA, and complete API
> guides, visit **[docs.flova.ir](https://docs.flova.ir)**.

`FlovaEsp32` and `FlovaEsp8266` use Flova Link by default. If you prefer MQTT,
pass your `PubSubClient` to the same device constructor. Your datastream code
stays the same. See [`examples/custom-mqtt`](examples/custom-mqtt) for the
complete example.

## Install

### PlatformIO

For an existing ESP32 PlatformIO project, add:

```ini
  lib_deps = flova-platform/FlovaSDK@^0.3.12
```

For ESP8266, use the packaged `extras/platformio/esp8266/platformio.ini`.
It provides the tested ESP8266 board and TLS profile for the SDK.

### Arduino IDE

FlovaSDK 0.3.12 supports ESP32 and ESP8266 in Arduino IDE:

1. Open **Tools → Manage Libraries**.
2. Search for **FlovaSDK**.
3. Select **Install**.

Then include the single SDK entry point:

```cpp
#include <FlovaSDK.h>
```

`FlovaSDK.h` selects the ESP32 or ESP8266 facade for the board you selected.
It also exposes the custom datastream helpers and the built-in TLS roots used
by the MQTT adapter.

### Linux and Raspberry Pi

Build and install the native Linux package with CMake. OpenSSL development
headers are required for OTA digest verification:

```sh
sudo apt install cmake libssl-dev
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Linux applications use `find_package(FlovaLinux CONFIG REQUIRED)` and link
`Flova::Linux`. The package provides the ready `FlovaLinux` facade, POSIX WSS,
durable file storage, logging, and supervisor-owned OTA staging. The
application calls `run()` from its nonblocking loop.

## Quick start

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <FlovaSDK.h>

FlovaEsp32 device;
auto relay = FLOVA_DATASTREAM(device, bool, "relay");

void setRelay(bool enabled) {
  digitalWrite(2, enabled ? HIGH : LOW);
}

void setup() {
  WiFi.begin("your-wifi", "your-password");
  pinMode(2, OUTPUT);
  FLOVA_ON_WRITE(relay, setRelay);
  device.begin();
}

void loop() { device.run(); }
```

The same `FLOVA_*` helpers work with both transports. Datastream keys must
match the device's template.

## Cloud schedules and local timers

Platform schedules run in Engine. When an occurrence is due, Engine sends an
ordinary command over the existing connection. The device must be online.
Commands expire 60 seconds after the scheduled occurrence; missed occurrences
are skipped. Firmware safety validation still has final authority.

Custom firmware opts each writable stream in before `begin()`:

```cpp
relay.scheduling(flova::Scheduling::CloudOnly);
// The default is Scheduling::Disabled.
```

Console and AI tools cannot enable this permission. Universal firmware explicitly
allows cloud scheduling for configured digital/PWM outputs; input mappings remain
disabled. Custom firmware may opt in virtual writable streams with the same helper.
MQTT does not currently report these permissions and cannot opt in implicitly.

The shared Arduino/ESP/Linux Link adapter reports only these firmware permissions:
PING with reserved ID `UINT64_MAX-1` negotiates support, followed by bounded
16-stream declaration chunks. Partial or stale reports never grant support.
There are no schedule downloads, installed manifests, checkpoints or renewal jobs.

Developers may independently use the existing RAM-only `flova::Scheduler`.
Given an application-owned `flova::WallClock` named `wallClock`:

```cpp
#include <FlovaScheduler.h>
flova::Scheduler timers(wallClock);
void turnOff() { relay.write(false); }

// In setup(), with a timezone string that remains valid:
timers.daily(20, 0, "IRST-3:30", turnOff);
// In loop(), alongside device.run():
timers.run();
```

The application supplies clock synchronization/timezone conversion. Timers run
only during their due minute, require a valid clock, and do not depend on the
network. Eight entries and their cursors stay in RAM. Reset loses them; setup
registers them again. Reboot during a due minute can run the callback again.
Use datastream `write()` in callbacks to retain hardware safety checks. Platform
schedules and developer timers are independent; avoid assigning both to the same
action/time unless that is intentional.

Storage allocation budgets, resource priorities and `Device::resourcePlan()`
have been removed. Use fixed board capacities and explicit history retention;
backend failures remain errors. `storageInfo()` is opt-in diagnostics, not
remote storage policy. Existing provisioning, command-ledger and configuration
recovery still retain their durability contracts; this change does not turn
storage-backed board startup into a RAM-only profile.

### Configuration bank compatibility

Configuration transfers select the physical bank opposite the active bank,
independently of generation parity. This preserves the last active configuration
when an offline device skips cloud generations. Version-2 bank pointers record
this physical association; legacy version-1 pointers and record bodies remain
readable and are migrated during a transfer. Missing generations cannot resolve
to another generation's manifest.

Before installing firmware with version-2 pointers, retain a full-flash backup.
Downgrading to firmware that only understands version-1 pointers requires
restoring that backup or explicitly reprovisioning the device. Firmware rollback
without restoring compatible storage is not supported after pointer migration.
No automatic formatting or erasure is performed by this migration.

## Datastream storage policies

For Flova Link on **ESP32, ESP8266, and Linux**, current values stay in RAM.
Value persistence and offline history are independently opt-in per stream.
Configure them in `setup()` before `begin()`; declaring a global stream helper
does not access storage.

```cpp
// No value writes to flash (the default).
temperature.persist(flova::PersistencePolicy::None);

// Save accepted changes immediately; identical sensor reports do not rewrite.
relay.persist(flova::PersistencePolicy::OnChange);

// Alternatively, coalesce the latest value for up to 30 seconds.
temperature.persist(flova::PersistencePolicy::Deferred, 30000);

// Optional offline history, independent of value persistence.
flova::HistoryRetentionPolicy history;
history.minimumIntervalMs = 60000;
history.maximumRecords = 4;
history.maximumAgeSeconds = 3600;
temperature.offline(flova::OfflinePolicy::StoreHistory).retention(history);
```

`Persistent` retains its original immediate-write behavior, including every
sensor report. `Immediate` is an alias. `OnChange` only changes storage writes;
reports still update timestamps/revisions and participate in normal delivery.

`Deferred` starts a fixed deadline on the first unsaved update, so continuous
sampling cannot postpone persistence forever. Call `run()` frequently, even
offline; it saves at most one due stream per iteration and retries failed
writes after a delay. The interval must be 1–2,147,483,647 milliseconds.
**A reset or power loss can lose unflushed changes.** Call `stream.flush()` and
check its result before an application-controlled reset, or use immediate
persistence for critical state. The portable `Device::flushPersistence()`
flushes all pending streams; Arduino OTA/power-down preparation also flushes
before proceeding.

The default offline policy is `KeepLatest`, not history. History's default
minimum interval is zero; set it explicitly for frequent samples. Retention
 is also bounded by the board's fixed history capacity.

Configuration transactions, durable command results, and schedule progress
are separate safety records and are not disabled by a stream's policy.
MQTT continues to use application-owned state; these local storage policies
are Link-only.

### Command delivery and runtime safety

Remote Link commands require writable, read-back-verifiable storage. The SDK
persists an intent **before** invoking hardware and retains the original result,
including rejections, across retries/reboots. Delivery ACKs do not erase that
result. The ledger holds `FLOVA_COMMAND_DEDUP_CAPACITY` records; only the oldest
delivered record can be replaced. A full/unavailable ledger returns
`command_storage_unavailable` without invoking hardware. Budget for two record
writes per new command plus one when its result is acknowledged.

A reset between hardware execution and durable completion returns
`command_outcome_unknown` on retry, without executing the command again. This is
not an exactly-once guarantee. After ledger eviction, ID-based deduplication is
no longer guaranteed: use command expiry and idempotent handlers for older
retries. Retained accepted results also restore desired-version fences.

NaN/infinity are rejected before numeric writes/reports update state. Restored
actuator values must satisfy current safety limits; failed restoration retries
no faster than once per second. Arduino monotonic time extends 32-bit rollover;
keep clock calls on the board loop and sample at least once every 49.7 days.

Arduino Link coalesces up to four readings per state frame, honors system
`batch_flush_ms` (50 ms by default), and retires only the revisions covered by
the batch ACK. Flow control also gates direct reports/writes. ESP8266's stock
BearSSL connect/write calls remain synchronous. OTA additionally enforces
`FLOVA_OTA_TOTAL_TIMEOUT_MS` (300,000 ms by default), checked between bounded
network calls; treat ESP8266 OTA as a maintenance pause. Physical latency,
power-loss recovery and reconnect endurance still need hardware acceptance.

Stream helpers cache resolved runtime handles, and history caches only its head
metadata. These bounded internal optimizations do not require user settings.
ESP8266 filesystem capacity is cached at mount; advanced integrations can
 request fresh accounting with `Device::storageInfo(true)` rather than
paying for a filesystem traversal on every write/removal.

## MQTT transport

MQTT is optional. Create your MQTT client, then pass it to the device:

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <FlovaSDK.h>

WiFiClientSecure socket;
PubSubClient mqtt(socket);
FlovaEsp32 device(mqtt);
```

Use `mqtt.flova.ir:8883` with the device UUID and secret. The MQTT adapter
uses the same JSON datastream format and helpers as Flova Link.

See the [custom MQTT example](examples/custom-mqtt) for Wi-Fi, TLS, reporting,
and ESP8266 setup. See the [documentation](https://docs.flova.ir) for
provisioning and production setup.

## Firmware logging

Flova diagnostics are compile-time filtered. Production builds default to
warnings and errors:

```ini
build_flags =
  -DFLOVA_LOGGING_ENABLED=1
  -DFLOVA_LOG_LEVEL=FLOVA_LOG_LEVEL_DEBUG
```

Available levels are `FLOVA_LOG_LEVEL_ERROR`, `WARN`, `INFO`, `DEBUG`, and
`TRACE`. Set `FLOVA_LOGGING_ENABLED=0` to remove Flova logging calls entirely.
Do not log credentials, provisioning tokens, device secrets, or complete
sensitive payloads.

## Universal firmware from the SDK

Build and upload the universal composition from your own project. The selected
toolchain creates the complete board image, including the bootloader and
partition table; no prebuilt firmware download is required.

For ESP32, install `FlovaSDK` from Arduino Library Manager or the PlatformIO
Registry and include the universal composition:

```cpp
#include <Arduino.h>
#include <FlovaSDK.h>

FlovaUniversalEsp32 device;

void setup() {
  Serial.begin(115200);
  device.begin();
}

void loop() { device.run(); }
```

Use an ESP32 board. Arduino IDE or PlatformIO will upload the complete image.

`FLOVA_SETUP_PASSWORD` is optional and defaults to `nullptr`, leaving the
setup SoftAP open. To protect setup, supply a device-unique WPA password
(8–63 characters) to the universal constructor or define `FLOVA_SETUP_PASSWORD`
before including the SDK. A supplied password must meet that length limit.
For production, print the unique password on the device label; do not
ship a shared password. BLE compositions require a device-unique
proof of possession via their constructor or `FLOVA_SETUP_PROOF_OF_POSSESSION`
(1–64 characters).

For local development only, defining
`FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING=1` explicitly allows BLE setup
without proof of possession. Never enable it in production. This flag is not
needed for an open SoftAP.

For ESP8266, use `<FlovaUniversalEsp8266.h>` with Arduino IDE or PlatformIO.
See [transport behavior](packages/TRANSPORT.md) for ESP8266 timing and memory requirements.

## Links

- [Documentation](https://docs.flova.ir)
- [PlatformIO Registry](https://registry.platformio.org/libraries/flova-platform/FlovaSDK)
- [Examples](examples)
- [SDK versions](https://github.com/flova-platform/flova-sdk/tags)
- [Report an issue](https://github.com/flova-platform/flova-sdk/issues)

## License

FlovaSDK is available under the [MIT License](LICENSE).
