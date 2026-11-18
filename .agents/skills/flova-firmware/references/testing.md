# Firmware Testing Standard

## Goal

Flova firmware is not production-ready because normal operation succeeds.

It is production-ready when bounded memory, parser safety, lifecycle correctness, and failure recovery remain stable under repeated stress.

## Required failure scenarios

Test at minimum:

- repeated connect/disconnect cycles
- Wi-Fi loss during WSS operation
- router reboot
- DNS failure
- TLS handshake failure
- certificate rejection
- server unavailable
- authentication failure
- server-side socket closure
- malformed WebSocket frames
- truncated CBOR
- malformed CBOR
- maximum-sized valid frame
- oversized frame
- maximum supported nesting
- excessive nesting
- rapid telemetry
- queue saturation
- interrupted configuration update
- OTA interruption

## Reconnect stability

Run at least:

```text
10,000+ connect/reconnect cycles
```

Observe:

```text
free heap
minimum free heap
largest free block
fragmentation
stack headroom
```

After repeated reconnects, memory health must remain approximately stable.

Progressive degradation is a bug.

## Long-running tests

Exercise:

- sustained normal operation
- unstable Wi-Fi
- repeated server restarts
- telemetry bursts
- configuration changes
- idle periods followed by bursts

Track memory low-water marks over the entire run.

## Host-side tests

Portable core logic must compile and run outside the MCU.

Host-test at minimum:

- CBOR decoding
- CBOR encoding
- message validation
- state transitions
- retry/backoff logic
- queue-full policy
- datastream binding
- datastream dispatch
- configuration validation

Keep ESP8266-specific APIs out of the portable core.

## Fuzzing

The real firmware protocol decoder must be fuzzable on a host machine.

Feed:

- random bytes
- truncated CBOR
- corrupted CBOR
- invalid major types
- invalid lengths
- excessive nesting
- duplicate keys
- unexpected fields
- integer boundaries
- maximum payloads
- zero-length values
- incomplete maps
- incomplete arrays

Use host tooling such as:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
libFuzzer
```

Do not maintain a separate simplified parser only for tests.

## Acceptance

A change affecting protocol, networking, queues, persistence, ownership, TLS lifecycle, configuration, or OTA is accepted only when:

- relevant failure tests pass
- bounded-memory assumptions still hold
- malformed input is rejected safely
- reconnects do not progressively degrade memory
- host tests remain green
- parser fuzzing reveals no crash, overflow, use-after-free, or undefined behavior

## Memory redesign regression checks

Run the TLS-budget host boundary tests, 64-key multi-frame binding tests, and
all-text capacity/int64 safety tests alongside the usual suite. Build the
the universal ESP8266 default-MMU profile. Run the image budget checker both
normally and with `--stock-dram`; distinguish a reviewed image-size ceiling from
physical product acceptance. Keep failure evidence in private notes outside the repository
rather than weakening limits to make the gate green.

Do not flash or manipulate connected devices when the requested validation is
code/build only. Record provisioning, reconnect, OTA and fragmentation as
unverified when hardware acceptance was not performed.

## Phase ownership regression rules

Run `flova-compact-runtime-test` for storage-backed descriptors, 64 maximum
keys/text values, bounded delivery fairness, stale ACKs and failed policy writes.
Retest pause/checkpoint/restore failures, callback borrows during disconnect,
and the exported SDK whenever phase ownership changes. Report all four
`--stock-dram` phases; never treat linked-size savings as runtime acceptance.

## Storage traffic and latency regression checks

Use a counting storage test double through the existing storage interface;
exercise the real runtime, not a simplified implementation. Count reads,
writes, removals, and bytes, plus backend commits where observable. Reset
counters after initialization/restore so boot work cannot hide runtime I/O.

For affected paths, prove:

- the default RAM-only profile performs no storage initialization, mount, read,
  write, remove, commit, erase, format, or implicit platform credential save;
- storage-backed capabilities require explicit developer configuration, disabled
  modes exclude optional buffers/dependencies, and insufficient RAM never causes
  automatic spill to persistent storage;
- RAM-only reset behavior is documented and operations requiring unavailable
  durability fail before effects or durable acknowledgement;
- ordinary nonpersistent processing has zero storage reads/writes;
- ordinary operations stay within their storage budget both when alternating
  resolved stream handles with numeric safety limits and when repeatedly
  reporting to a key late in the registry through the public facade. Count
  backend reads for each case so a cache hit cannot mask a full key scan;
- unchanged settings and repeated registration do not cause redundant commits
  where verified persisted state already matches;
- deferred writes meet maximum dirty age under continuous updates, timer
  rollover, and fair servicing of multiple dirty streams;
- failures retain correct dirty/recovery state and have bounded retry frequency;
- rejected hardware writes leave authoritative and durable state unchanged;
- full storage and interrupted durable writes/promotion/checkpoints preserve
  the required recovery and acknowledgement guarantees.

The current ESP8266 descriptor cache is a known exception, not evidence that
zero-I/O acceptance passes. Record observed operation counts and the exception;
do not weaken assertions or claim an unimplemented RAM-first redesign. New
RAM-first paths must have regression tests enforcing their contract.

On hardware, record storage operation latency, maximum loop gap, deadline
misses, and watchdog behavior under normal/nearly-full storage, maintenance,
bursts, and relevant reconnect/OTA workloads. Distinguish logical traffic from
physical program/erase counts; host tests cannot establish timing or endurance.
Use `storage.md` for the required budgets and evidence. Missing hardware evidence
must remain explicitly unverified; do not manipulate devices for build-only work.

## Production image regression checks

For affected production profiles, compare before/after builds with the same
toolchain and feature settings. Report image, static DRAM, stack, and
phase peak-heap evidence separately; unavailable runtime evidence stays
unverified. Inspect linker maps for unexplained growth or new dependencies.
Verify disabled optional features remove their buffers and exclusive linked
dependencies. Run existing resource gates and actual partition/OTA-slot fit
checks; universal/probe compilation alone is insufficient. Never increase a
ceiling merely to make validation green.
