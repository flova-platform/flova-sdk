# Development board E2E and SDK benchmarks

Start from the SDK root. **Only the 4 MiB ESP32 DevKit (`esp32dev`) is supported
and hardware-validated initially.** ESP8266 and other boards are extension
targets, not claimed support. This is test tooling, not an SDK example or a
production provisioning service.

## Prerequisites

- Python 3.10+, PlatformIO (`pio`), OpenSSL, and pyserial. PlatformIO's Python
  environment normally already has pyserial; use that interpreter if your
  system Python does not. Linux serial permissions are required for hardware.
- A running **local development Engine**, listening on loopback port 4000
  (configurable). Fixture creation remains owned by Engine/Console, not SDK.
- A dedicated local device with a published template containing two **double,
  read-only datastreams**: `temperature` and `humidity`. Use the normal mobile/
  Console provisioning flow to generate a fresh **Device Link provisioning
  session token** for that device. The older Devices REST provisioning-token
  API is not interchangeable with this session token. Do not use a production
  device, token, database or network.
- A laptop LAN IPv4 address reachable from the board's Wi-Fi. AP/client
  isolation or a firewall can prevent this even when both devices join Wi-Fi.
  Permit the configured test TLS port only on your development LAN.

Copy `scripts/board_e2e/config.example.json` to a private ignored file such as
`.local/board-e2e-config.json`. Fill in the Wi-Fi credentials, session token and
the **laptop's** LAN address, then `chmod 600 .local/board-e2e-config.json`.
Do not put secrets in command-line arguments or tracked configuration.

`development: true` and an RFC1918 LAN IPv4 address are mandatory. The runner
creates a short-lived private CA and a temporary TLS ingress on
`lan_address:tls_port`, forwarding to `127.0.0.1:engine_port`. CA, hostname and
certificate-date verification stay enabled. It does not modify Caddy or the
Engine configuration. The test firmware seeds UTC from the host; **SNTP is
not tested**. The host clock must be correct.

## Run

Build only (default; no serial access, listeners, erase or upload):

```sh
python scripts/test_board.py --config .local/board-e2e-config.json
```

Full hardware run, with explicit authorization to modify the selected board:

```sh
python scripts/test_board.py --config .local/board-e2e-config.json \
  --board esp32 --flash --port /dev/ttyACM0 --seconds 20 --reconnects 3
```

The hardware pipeline:

1. Compile the existing embedded Unity tests and the separate E2E firmware.
2. Identify the chip and flash capacity; reject a nonmatching profile.
3. Read and size-check the **entire flash**, retaining its SHA-256 fingerprint.
4. Start temporary verified-TLS ingress; upload and execute the Unity suite.
5. Erase flash to isolate the fixture from old credentials/configuration;
   upload E2E firmware and provision the dedicated local device.
6. Report both streams every 200 ms. Check readiness, successful reporting,
   matching delivery ACKs, storage failures, heap floor and unexpected resets.
7. Force Wi-Fi disconnect/reconnect cycles and resume acknowledged telemetry.
8. Flush the deferred temperature value and reboot. Assert its exact restored
   value, and assert that nonpersistent humidity has no restored value.
9. Stop ingress and restore the original full-flash image, including its
   credentials/partitions, with esptool verification—even if the test failed.

Use a fresh provisioning token for each hardware run. Test-device server state
and telemetry remain in the local Engine; restoring flash does not roll those
back. No GPIO actuators are driven. Do not run a serial monitor or another
flashing tool concurrently. An advisory lock serializes hardware phases of this
runner within this checkout, not unrelated tools or other checkouts.

`--minimum-heap` may raise the default 4,096-byte safety floor, not lower it.
`--seconds` accepts 5–1,800; `--reconnects` accepts 0–100. These runs are smoke
and benchmark evidence, **not** the 10,000-cycle production stress gate.

## Reports and recovery

Each run creates a **new mode-0700 directory** under `.local/board-e2e/`.
Override with `--output /absolute/path/to/a-new-directory`. Existing output
directories are rejected, never overwritten. Treat the **whole directory as
sensitive**: compiled firmware, generated headers, private CA keys and flash
backups contain credentials. Do not attach it to issues or commit it.

- `report.json`: outcome, build/tool durations, revision and dirty-tree flag,
  checks, benchmark summary, backup fingerprint and restoration outcome.
- `events.jsonl`: bounded typed test events and numeric one-second windows.
  Unstructured SDK logs/payloads are not retained. Build/tool logs redact
  configured Wi-Fi credentials and provisioning tokens.
- `original-flash.bin`: full original board image. **Keep this until restoration
  has succeeded.** The report distinguishes compile-only from hardware success.

Benchmarks measure `client.run()` only: weighted mean, maximum, and **p95 of
one-second window maxima**, not p95 of individual calls. Serial output, Wi-Fi
setup and reporting callbacks are outside that timer. Heap low-water counters,
largest connected block and stack headroom are included. Compare the same
board, toolchain, workload and Engine; do not infer a hard latency bound, an
absence of leaks, or the universal firmware's memory headroom from this sketch.
Matching Link ACKs establish protocol delivery; this runner does not query the
Engine database or test dashboard rendering/history.

Ctrl+C/SIGTERM trigger restoration. A second signal is ignored during restore.
Power loss, unplugging the USB cable, SIGKILL, or host failure cannot be handled
in `finally`. If restoration fails or the process is killed, reconnect **the
same board** and restore manually (using the PlatformIO Python interpreter):

```sh
python ~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32 --port /dev/ttyACM0 --baud 460800 write_flash \
  0 /absolute/path/to/run/original-flash.bin
```

## Ownership and extension

- `runner.py`: tools, build/upload/backup/restore transaction and report files.
- `boards.py`: reviewed board profiles, flash sizes and development config.
- `monitor.py`: board-independent serial protocol, scenarios and summaries.
- `ingress.py`: temporary local TLS forwarding; never bypasses verification.
- `firmware/main.cpp`: bounded common workload and command/metric events.
- `firmware/platform_esp32.h`: client composition, networking, time/reset and
  hardware metrics. The common firmware is guarded by `FLOVA_DEVELOPMENT_E2E`.

To add ESP8266, supply a platform header and reviewed board profile, then check
stock/shared-IRAM builds, flash/partition backup coverage, metric units, stack
usage and real hardware before enabling it. Keep board APIs out of the portable
SDK and the common host scenarios. Do not add unsupported board names as aliases
to the ESP32 implementation. Future command/OTA/power-cut scenarios need their
own explicit fixtures and hardware authorization; they are not simulated here.

Host-side safety/parser/scenario tests need neither a board nor pyserial:

```sh
python -m unittest discover -s scripts/board_e2e -t scripts -v
```

## Initial ESP32 validation

On 2026-11-12, the runner passed build-only and the complete hardware pipeline
on an ESP32-D0WD DevKit with 4 MiB flash, PlatformIO espressif32 7.0.1 / Arduino
2.0.17, against the local development Engine. All 11 host runner tests and six
on-board Unity tests passed. The hardware run used 20 seconds of steady traffic,
three Wi-Fi reconnect cycles, acknowledged reporting, explicit flush and reboot
restore checks. Original-flash restoration succeeded; its backup fingerprint
matched the independently retained original image. Private run artifacts were
not added to the repository.

That smoke run contained 45 connected measurement windows: mean `run()` time
120.3 µs, maximum 9,654 µs, and p95 **window maximum** 8,358 µs. Minimum heap was
135,892 bytes, minimum connected largest block 77,812 bytes, and minimum loop
stack headroom 4,700 bytes. These are observations for this sketch/environment,
not portable pass thresholds or evidence for ESP8266 or long-duration stability.
