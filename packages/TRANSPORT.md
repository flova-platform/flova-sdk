# Device Link transport

Flova owns one Link socket per device. Protocol parsing, configuration, and
hardware callbacks run from `device.run()`. ESP32 performs socket operations in
one worker; ESP8266 uses public stock BearSSL from the application loop.

## Ordering and lifetime

All writes use one transport slot. Submission copies bytes; completion means the
client accepted them, not that Engine acknowledged them. WebSocket control
frames wait for the current frame. ESP32's receive ring stops reading when full,
letting TCP apply backpressure. Cancellation invalidates the current generation;
the worker closes it before allowing another connection. Destruction waits for
worker completion.

OTA enters maintenance, drains output with a five-second deadline, and waits for
Link closure before allocating its HTTPS session. Failed drain aborts the OTA.
During installation, local application processing may pause.

Runtime configuration activation retains the final CONFIG_ACK while the transmit
slot is busy (up to five seconds), then drains it and the WebSocket close before
requesting a restart. No further configuration or OTA work runs during this
handoff. A failed send or drain still permits activation of the verified durable
generation; after restart, CONFIG_REPORTED confirms the active checksum to Engine.
The transfer ACK alone does not prove that the new configuration is running.

## Board-owned power down

Deep sleep and other low-power states are board responsibilities. Flova exposes
`prepareForPowerDown()` on the Arduino client and ESP32/ESP8266 facades so an
application can quiesce the Link before it stops the board networking stack.
The method returns `Busy` while runtime work such as configuration, OTA, or
factory-reset handling is active, `Draining` while the existing bounded Link
maintenance drain runs, `Ready` after the socket is closed, and `Failed` when
the drain cannot complete. The application must keep calling `run()` while the
status is `Draining` and enter its board-specific power state only at `Ready`.

```cpp
const FlovaPowerDownStatus status = device.prepareForPowerDown();
if (status == FlovaPowerDownStatus::Ready) {
  // Stop Wi-Fi, configure the wake source, and call the board sleep API here.
} else {
  device.run();
}
```

The helper does not stop Wi-Fi, configure wake sources, or add a Link sleep
message. After a deep-sleep reset, the normal `begin()` and `run()` lifecycle
restores the persisted configuration and establishes a new Link session.

## ESP8266

Stock BearSSL can block inside connection and write operations, including
cryptographic work that cannot be interrupted by a Flova polling deadline.
Flova does not add a second per-frame TCP flush after BearSSL's own write;
this removes a redundant peer-ACK wait, not the synchronous work in BearSSL.
Applications requiring uninterrupted low-latency hardware handling should use
ESP32 or hardware peripherals that operate independently of the application loop.

ESP8266 uses the stock Arduino MMU and DRAM allocator, with 2,048-byte TLS RX
and 512-byte TX buffers. Flova endpoints must bound every server TLS record to
2,048 bytes; the official TLS edge provides this contract. The 512-byte Flova
frame limit does not constrain TLS record size. CA, hostname and certificate-date
verification remain enabled. For another endpoint without this guarantee, set
`FLOVA_ESP8266_BOUNDED_TLS_RECORDS=0` to use 16,384-byte RX; this requires more
contiguous DRAM and may fail preflight on a small device.

Use the stock MMU profile and the default DRAM allocator. Memory preflight
checks free DRAM and its largest block before
constructing the secure client; actual allocation can still fail. The client is
released on Link closure or OTA completion, including failure paths, releasing
BearSSL's shared 6,200-byte secondary stack when its last owner exits. Trust
anchors are parsed once from flash-resident PEM data. Executable [footprint checks](../scripts/check_flova_memory.py) enforce build
limits; they do not establish physical runtime acceptance.

## Build and diagnostics

The package compiles unchanged upstream zcbor through canonical C wrappers;
consumers need no encoding flags or core patch. `FLOVA_VERSION` is the single
Flova release version; `FLOVA_SDK_VERSION` and `FLOVA_FIRMWARE_VERSION` are
compatibility names for the same value in their respective reporting paths.
`FLOVA_NO_DEFAULT_BANNER` disables the startup banner. Call `Serial.begin()`
before the board facade's `begin()` to see it.

Temporary bootstrap failures retain the verified handoff and retry with jittered
backoff, capped at 60 seconds. Server-declared token rejection requires a new
handoff. Codec/configuration defects stop and preserve their error for diagnosis.
Retry counters remain in RAM. Factory reset remains the explicit way to erase
device configuration.

SoftAP provisioning starts station association in AP+STA mode and keeps the
setup portal responsive until the station has an address. The runtime retires
the setup channel before starting clock and Link work; failed credentials can
therefore be corrected without erasing the accepted handoff.
