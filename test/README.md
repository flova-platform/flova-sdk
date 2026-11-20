# SDK tests

- `host/` contains C++11 tests registered in `host/CMakeLists.txt`. These
  cover the SDK, Linux and Arduino services, and Link codecs without a board.
- `test_datastream/` contains PlatformIO Unity tests for datastream behavior
  and the provisioning parser on ESP32 and ESP8266.
- `test_configuration_rollback/` contains the PlatformIO Unity test for
  configuration recovery after a real ESP8266 reboot.

PlatformIO requires the `test_` prefix for embedded suite directories.
Keep portable behavior in `host/` and board-specific tests in `test_*` suites.

Run host tests with CMake and embedded compile contracts with PlatformIO:

```sh
cmake -S . -B /tmp/flova-core-build
cmake --build /tmp/flova-core-build
ctest --test-dir /tmp/flova-core-build --output-on-failure
pio test -e test-esp32 -e test-esp8266 -e test-config-rollback-esp8266 --without-uploading --without-testing
```

On an erased, disposable ESP8266, the `test-config-rollback-esp8266` target
seeds two configuration banks, injects a hardware apply failure, and verifies
the previous generation after a real reboot. The fixture uses on-board
storage and does not require Wi-Fi or a broker.

## Manual firmware smoke builds

`mqtt-smoke/` and `scheduling-smoke/` contain manual firmware fixtures, not
Unity suites. Their PlatformIO build environments are `mqtt-smoke-esp8266`
and `scheduling-smoke-esp8266`, respectively.
