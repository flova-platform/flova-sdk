# Flova packages

These directories are reusable libraries, not application entry points.

| Package | Owns | Must not own |
| --- | --- | --- |
| `flova-embedded-sdk` | Portable `flova::Device`, datastream semantics, bounded configuration and scheduling | Arduino, ESP, GPIO, Wi-Fi, WebSocket, TLS, or filesystem APIs |
| `flova-arduino` | Generic Arduino clocks, storage, logging, bounded Device Link framing, and the board-platform seam | ESP/TLS/updater APIs, product-specific GPIO mappings, or a second domain runtime |
| `flova-linux` | POSIX clock, file storage, logging, and supervisor-owned OTA staging for Linux devices | Arduino, ESP board policy, or a Linux service manager's process lifecycle |
| `flova-esp32` | ESP32 socket/TLS/OTA, setup provisioning, runtime network, boot/storage policy, identity, pin policy, and board composition | Portable SDK semantics or provisioning-owned runtime connectivity |
| `flova-esp8266` | ESP8266 BearSSL/socket/OTA, resource policy, setup provisioning, runtime network, boot/storage, identity, pin policy, and board composition | Portable SDK semantics or provisioning-owned runtime connectivity |

Arduino users who want to keep their own application code should install the
matching board package and include `<FlovaEsp32.h>` or `<FlovaEsp8266.h>`.
Both concrete classes expose the same typed application API; the application
retains ownership of GPIO, sensors, and application work. `begin()` restores
saved networking or starts SoftAP provisioning on a fresh device. These standard
classes own Wi-Fi and managed restart policy starting with 0.5.0.
`FlovaUniversalEsp32` and `FlovaUniversalEsp8266` are the full-device, no-code
compositions. Advanced Arduino ports may include `<FlovaArduino.h>` and compose
`FlovaClient` with their own Link, temporary provisioning, runtime network, TLS
clock bootstrap, identity, and storage services.
Custom Ethernet applications may include `<FlovaEthernetAdapter.h>` and inject
an Ethernet/TLS socket implementation. The adapter owns only the bounded Link
platform seam; the application owns the Ethernet chip library, pins, DHCP, and
certificate setup.
ESP32 LAN8720 applications can use `<FlovaEsp32Ethernet.h>` for the complete
direct-device composition and automatic factory-token bootstrap.
For an ESP32 LAN8720 composition reusing the native TCP/IP and TLS stack, see
[`examples/custom-ethernet-esp32`](../examples/custom-ethernet-esp32).
Managed custom applications use `FlovaEsp32` or
`FlovaEsp8266`: the existing SoftAP and stored-network adapters with
manual application-owned hardware handlers. These opt into durable setup and
support Wi-Fi-only replacement without deleting Flova identity. Applications
that own networking use explicit `FlovaClient` composition. Link uses CBOR over
MQTT/TLS internally; a PubSubClient is not a supported facade constructor.
The public protocol package is named `flova-link`; generated zcbor headers
remain an internal implementation detail of that package.

`FlovaDevice.h` is the portable runtime. New board ports implement the four
`flova::` service interfaces and compose an explicit board class as shown in
`examples/custom-board-basic`.

Linux applications install `Flova::Linux` with CMake and include
`FlovaLinux.h`. `FlovaLinux` owns the POSIX services and nonblocking WSS
transport while applications retain their event loop and hardware ownership.
Raspberry Pi service startup and executable activation remain supervisor
responsibilities.
