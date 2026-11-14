---
name: flova-firmware
description: Engineering rules for Flova embedded firmware and SDK. Use for implementation, refactoring, debugging, protocol, networking, memory, performance, and testing work on constrained embedded targets including ESP8266.
---

# Flova Firmware Engineering

Build production-grade embedded code with deterministic memory use, bounded resource consumption, low overhead, explicit ownership, and predictable failure behavior.

## Mandatory rules

- Do not introduce unbounded memory usage.
- Flova-owned runtime hot paths must not dynamically allocate.
- Do not place large buffers on the stack.
- Do not use recursion.
- Use fixed-capacity queues and buffers.
- Validate every network-controlled length before processing.
- Bound frame sizes, collection sizes, strings, nesting depth, and queues.
- Stream or chunk large payloads.
- Reuse TLS and networking resources.
- Use explicit state machines for lifecycle management.
- Keep ownership and lifetimes obvious.
- Keep normal control and telemetry processing RAM-first; no filesystem or EEPROM/NVS access in ordinary hot paths without a documented, measured exception.
- Runtime state is volatile by default. Require an explicit recovery or delivery contract before adding persistence or durable backlog.
- All persistent-storage integration is optional and disabled by default. Developers must explicitly enable and configure the required backend/features. The default runtime keeps state in bounded RAM and must not initialize, mount, read, write, or format persistent storage.
- Keep explicitly durable backlog bounded in persistent storage, with bounded RAM indexes and workspaces.
- Avoid unnecessary copies and intermediate representations.
- Keep portable protocol logic independent from ESP8266 APIs.
- Measure memory instead of assuming it is safe.
- Verify both the board default profile and explicitly requested capacity/MMU profiles; a 64-entry ceiling is not proof that 64 entries fit with TLS.
- Reject unexplained RAM and flash regressions.
- Stress reconnect and failure paths.
- Host-test and fuzz protocol parsing.

## RAM-first execution and minimal images

Keep the hot subset of live values, routing, safety limits, and scheduling state
in explicitly owned bounded RAM. Do not move hot metadata to a filesystem just
to reduce BSS: account for I/O latency, cache misses, filesystem heap, and peak
RAM across all phases. A fixed-capacity implementation is not necessarily fast.

RAM-first does not mean copying firmware or all constants into RAM. Keep
immutable tables, keys, and literals in flash/ROM where target access rules
permit it. Preserve portable-core isolation from platform storage APIs.

Optional features must compile out their code, buffers, and exclusive
dependencies when disabled. Report linked image, static DRAM, IRAM, stack, and
peak heap deltas for affected production profiles; do not infer peak savings
from BSS reductions. Inspect linker maps for unexplained growth. A new
dependency needs measured cost and a reason existing code cannot suffice.
Validate actual partition/OTA-slot fit. Never raise a budget just to pass a gate.

## Persistence and latency

Keep runtime state in bounded RAM by default, including active configuration and
metadata needed for processing. Credentials/settings supplied to a RAM-only
runtime do not survive reset. Do not automatically enable persistence because a
backend is available or a board normally provides LittleFS, EEPROM, or NVS.
Storage-backed provisioning, configuration recovery, history, checkpoints, logs,
and OTA staging are opt-in capabilities. Disabled storage must remove optional
storage buffers and exclusive dependencies from the image. Firmware code and
immutable constants may remain in flash/ROM; RAM-only describes runtime state
and optional persistence, not executing the whole image from RAM.

If an operation requires durability that the developer has not enabled, reject
it explicitly before effects or durable acknowledgement; never pretend a RAM
write is persistent. Preserve existing durability guarantees in enabled modes.
Existing storage-dependent paths require separate implementation work before
the current firmware can be claimed to satisfy this default.

No storage I/O in ISRs or transport callbacks. Each synchronous storage operation
must have an allowed lifecycle phase, latency budget, and failure policy.
Chunking or limiting writes per loop does not prove nonblocking behavior.
Preserve persist-and-verify-before-ACK and checkpoint-before-action guarantees;
do not silently debounce an operation promising synchronous durability.

Read `references/storage.md` before changing persistence, filesystem access,
EEPROM/NVS, storage-backed metadata, logging sinks, or write scheduling.

## Logging

Use the compile-time logging controls `FLOVA_LOGGING_ENABLED` and
`FLOVA_LOG_LEVEL`. Production defaults to warnings and errors; use
`FLOVA_LOG_LEVEL_DEBUG` for memory/resource diagnostics and
`FLOVA_LOG_LEVEL_TRACE` only for targeted transport timing or frame tracing.
Disabled levels must compile out formatting buffers and serial/logger calls.
Never log credentials, provisioning tokens, device secrets, or complete
sensitive payloads.

## CBOR and protocol

Use bounded typed decoding.

Do not build a dynamic generic CBOR DOM.

Encode directly from typed application state into bounded transport buffers.

Use compact numeric protocol fields and opcodes in hot paths.

Read `references/protocol.md` before changing protocol framing, CBOR schemas, WSS behavior, message limits, or large-payload handling.

## Datastreams

Datastream keys are human and API identifiers.

Runtime protocol identity is a stable server-assigned `uint16_t` `DatastreamId`.

ESP8266 runtime traffic uses IDs, never string names.

Do not use hashes, runtime string maps, CBOR string-reference tables, or connection-scoped aliases for datastream identity.

ESP8266 supports a maximum of 64 active datastream runtime entries.

Read `references/datastreams.md` before modifying datastream identity, binding, configuration, SDK declarations, or wire representation.

## Memory

Treat runtime memory behavior as part of correctness.

Track:

- current free heap
- minimum free heap
- largest free block
- heap fragmentation
- stack headroom

Large reusable buffers must use explicitly owned bounded storage.

Follow the repository memory rules and executable resource limits before modifying memory ownership, buffers, queues, TLS resources, persistent state, or ESP8266 runtime layout.

## Testing

Changes affecting networking, protocol, ownership, persistence, or memory must include appropriate failure-path tests.

Protocol parsers must remain host-testable and fuzzable.

Reconnect behavior must not progressively degrade heap state.

Read `references/testing.md` before changing parsers, reconnect logic, queues, persistence, TLS lifecycle, configuration handling, or OTA behavior.

## Architecture

Prefer small concrete components with narrow responsibilities.

Do not introduce abstraction layers without a concrete portability, testability, or ownership benefit.

Avoid desktop and enterprise architecture patterns that hide allocation, ownership, or runtime cost.

Optimize architecture before micro-optimizing instructions.

For C++ implementation and review, also follow
`../cpp-code-quality/SKILL.md` and `../cpp-antipatterns/SKILL.md` for language
correctness, API/ownership contracts, and focused regression evidence.

## Definition of done

A firmware or SDK change is complete only when:

- resource usage remains bounded
- failure behavior is deterministic
- ownership is clear
- protocol input is validated
- memory budgets remain acceptable
- ordinary hot paths meet their storage-I/O contract, with exceptions explicitly evidenced
- persistence has bounded traffic, retries, and documented recovery semantics
- affected production images fit reviewed resource and OTA budgets
- latency claims distinguish measured hardware evidence from host/build checks
- relevant tests pass
- no progressive heap degradation is introduced
- target-specific code remains isolated from portable core logic
