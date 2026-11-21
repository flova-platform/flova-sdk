# Flova Link protocol assets

This directory contains the versioned Flova Link v1 wire contract and the
firmware artifacts derived from it. Keep the ownership order below clear:

1. [`flova-link-v1.cddl`](flova-link-v1.cddl) is the authoritative wire schema.
   It defines message types, field shapes, bounds, and the deterministic CBOR
   profile.
2. [`message-types.csv`](message-types.csv) is a human-readable message
   catalog. It is useful for review and tooling, but it does not override the
   CDDL.
3. [`conformance-vectors.json`](conformance-vectors.json) contains deterministic
   frame examples. The matching C header under `generated/` is derived from it.
4. [`generated/firmware/`](generated/firmware/) contains zcbor codecs, types,
   vector declarations, and PlatformIO library metadata. These files are build
   inputs, not hand-maintained protocol definitions.
5. [`zcbor-requirements.txt`](zcbor-requirements.txt) records the manually
   installed generator version used to refresh the checked-in codecs.

## Updating the protocol

Install the generator dependencies manually, then regenerate in this order:

```sh
python3 -m pip install --force-reinstall -r protocol/zcbor-requirements.txt
scripts/generate_flova_link_cbor.sh
python3 scripts/generate_flova_link_vectors.py
scripts/check_flova_link_contract.sh
cmake -S . -B /tmp/flova-core-build
cmake --build /tmp/flova-core-build
ctest --test-dir /tmp/flova-core-build --output-on-failure
```

Review both the schema and generated diff. Do not edit files below
`generated/` directly, and do not modify vendored zcbor under `third_party/` as
part of a protocol change. A wire-compatible change still requires explicit
review of the CDDL, vectors, generated codecs, and native tests.

## Runtime boundary

### Application events

`LOG_EVENT` (0x1a) sends `[boot ID, sequence, code, message]` after authenticated
binding. It shares the boot/sequence identity with `LIFECYCLE_REPORT`, but
uses a separate one-entry RAM slot. `INGESTION_ACK` confirms storage (or a
duplicate), `MESSAGE_REJECTED` frees the slot, and `FLOW_CONTROL` leaves it
pending for retry after five seconds. There is no capability probe for this
message. The maximum complete frame is 159 bytes. Sequence gaps in a view
filtered to one kind of log are not necessarily losses.

### Scheduling

Platform schedules are cloud-only and use the existing command message with
an absolute expiry. No schedule manifests are transferred, stored or restored.
The retired schedule opcodes 0x15, 0x16, 0x24, 0x2b and 0x2c, and configuration
record kinds 2 and 4, remain reserved and are rejected by new firmware.

The only scheduling-specific exchange reports firmware permission: PING with
reserved ID UINT64_MAX-1 negotiates SCHEDULING_READY (0x2e). The SDK reports
SCHEDULING_DECLARATIONS (0x2f) as `[generation, offset, total, stream rules]`,
with up to 16 `[runtime ID, mode]` pairs per chunk and 64 streams total. Modes
are disabled (0) and cloud-only (1). These declarations do not contain schedule
times/actions or storage policy. Legacy capability-map slots 4 and 5 remain
zero/reserved to preserve the existing bootstrap/configuration envelope.

### Lifecycle diagnostics

`LIFECYCLE_REPORT` (0x19) is a bounded seven-integer array: boot ID, sequence,
monotonic uptime, event code, code-specific detail, configuration generation,
and cumulative dropped count. It never contains free text, credentials or
payload dumps. Embedded clients retain at most 16 entries in RAM, replay them
after connecting, and remove an entry only after its matching INGESTION_ACK.
Reboot/power loss discards unuploaded RAM entries; sequence gaps and dropped
counts must not be presented as complete lossless history.

Support is negotiated without changing existing AUTH_OK frames: new clients
send PING with reserved message ID `UINT64_MAX` after binding. New Engines
reply with PONG and `LIFECYCLE_READY` (0x2d, empty array). Older Engines reply
only PONG, so new clients do not send unsupported reports. Older clients do
not probe and never receive the new frame.

Codes 0–10 represent boot (platform reset reason), lifecycle stage, network/TLS
readiness bits, Link connectivity, runtime/readiness bits, configuration
generation, error-active flag, maintenance flag, diagnostic-failure total,
connection-attempt total, and connection-failure total. This is a lifecycle
stream, not a remote Serial mirror or packet trace. Existing compile-time
Serial logging levels remain independent.

#### Memory evidence

Compared with SDK commit `a2c94c2`, using the same pinned toolchains and build
profiles on 2026-11-13:

| Profile | Static RAM before | Static RAM after | Increase |
| --- | ---: | ---: | ---: |
| universal-esp32 | 75,592 B | 76,432 B | 840 B |
| universal-esp8266 | 47,640 B | 48,488 B | 848 B |
| datastream-api-esp32 | 75,312 B | 76,152 B | 840 B |
| datastream-api-esp8266 | 40,956 B | 41,804 B | 848 B |

The increase is entirely BSS, not new RAM-resident strings or dynamically
growing log allocations. Lifecycle upload reuses the existing frame workspace.
Compiler-reported local stack frames are 32 B (ESP32) / 48 B (ESP8266) for
`drainLifecycleLog` and 128 B for `publishLifecycle`; these are individual
frames, not whole-call-chain maxima. The existing stack-budget guard passes.

The physical ESP32 board E2E workload (20 seconds, three reconnects and a
reboot) measured a 145,168 B minimum free heap, 77,812 B minimum connected
largest free block, and 4,828 B minimum stack headroom. This is evidence for
that custom test workload, not for the universal image, a long-run leak test,
or physical ESP8266 TLS/OTA heap acceptance. Static RAM figures alone cannot
establish runtime heap safety.

The generated codec is the only firmware protocol implementation. It is
published to PlatformIO as `flova-link`; generated zcbor headers remain an
implementation detail and are not part of the application-facing SDK API. The
portable SDK owns bounded message values and Link-facing contracts; Arduino
and board packages own WebSocket, TLS, Wi-Fi, storage, and callback scheduling.
The wire contract does not permit a second JSON, TLV, or generic CBOR tree
codec alongside the generated implementation.

The current profile uses complete frames no larger than 512 bytes, with a
fixed 12-byte frame header and bounded CBOR nesting. ESP8266 uses 16,384-byte
receive and 512-byte transmit TLS buffers for arbitrary endpoints. A separately
verified endpoint may select the guarded 2,048-byte receive profile for Link
and OTA. Application frames remain bounded to 512 bytes; they do not establish
a TLS-record bound.
