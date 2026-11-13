# Firmware memory acceptance

## Stock ESP8266 profile (2026-11-13)

All current ESP8266 examples use the default MMU and DRAM heap. Optional IRAM
heap builds are rejected. TLS receives 2,048-byte records through the official
record-bounded edge with certificate verification enabled. Private board-test
samples measure free DRAM, largest block, fragmentation and continuation-stack
headroom; they are never published as sensor datastreams. Physical stock-profile
acceptance requires sampled DRAM of at least 8,192 bytes and continuation-stack
headroom of at least 1,024 bytes through traffic, reconnects and reboot checks.
Historical measurements below describe earlier builds and do not certify this
profile. Instruction RAM used by ordinary compiled code is not an optional heap.

The clean current-HEAD reference build measured 48,544 / 76,432 static DRAM
bytes for universal ESP8266 / ESP32; these already exceeded the older ceilings.
Production log throttling adds 16 bytes per device object (48,560 / 76,448).
Reviewed static ceilings are now 48,640 / 76,800; the ESP32 image ceiling is
1,056,768 bytes. Binding codecs borrow phase-exclusive scratch slots, with
compile-time size checks, rather than adding resident memory. These linked
bounds do not relax the physical heap or continuation-stack gates.

## SDK safety hardening (2026-11-12)

The command-outcome ledger now keeps its bounded payloads in storage, including
a verified intent before hardware execution and the original result after it.
It replaces four resident command-ID strings with small fingerprint/sequence
indexes. State aggregation reuses the four delivery slots and reads values on
demand; it adds no resident batch payload or per-message allocation. Arduino
rollover extension, batch deadlines and restore retry state remain fixed-size.
Persisted result layouts remain compatible in this change.

Compared with the storage-policy baseline recorded below, using the same pinned
toolchains and universal profiles:

| Environment | Previous static DRAM | Hardened static DRAM | Previous image | Hardened image |
| --- | ---: | ---: | ---: | ---: |
| `universal-esp8266` | 47,712 | 47,696 (-16) | 567,616 | 570,512 (+2,896) |
| `universal-esp32` | 75,896 | 75,592 (-304) | 1,048,192 | 1,050,848 (+2,656) |

IRAM remains 29,015 / 84,714 bytes respectively. The flash increase pays for
durable intent/result verification and replay, finite-value and schedule-shape
validation, rollover-safe clock/deadlines, bounded restore retries and grouped
state ACK handling. Exact result read-back uses the existing workspace instead
of a second record/stack buffer. The image ceilings move to the next 4 KiB
boundaries (573,440 / 1,052,672); static DRAM, IRAM, phase allocations and all
TLS/heap safety reserves are unchanged. This is an explicit flash/safety
tradeoff, not evidence of additional runtime heap headroom.

Both ESP targets and their datastream examples compile. Host regressions cover
the command, restore, flow-control, batching and corruption failure paths.
The stack checker additionally bounds command receive/result storage and batch
construction. Full SDK-work timing is available in the development runner.
Physical power-cut behavior, worst-case TLS/OTA loop pauses, fragmentation and
10,000-cycle reconnect stability remain unverified; stock ESP8266 TLS is still
synchronous. No board was flashed for this change.

Validation: 18 host suites pass normally and under Clang ASan/UBSan; 11 board
runner host tests pass; universal/datastream firmware and all three embedded
test targets compile. The exported package compiles for ESP32 and shared/stock
ESP8266 MMU. The updated runner builds with the public example fixture without
hardware access. Protocol/public-surface, stack and both footprint checks pass.
`scripts/check_flova_layers.sh` remains blocked by its existing exception-scan
false positive on JavaScript `try`/`catch` in unchanged
`FlovaSoftApPortal.h:17`. GCC sanitizers could not link on this host because its
ASan/UBSan shared libraries are missing; the Clang sanitizer build passed.

The released universal ESP8266 image uses the verified 64-stream, stock-DRAM
profile. The linked-image checks below remain build evidence; runtime TLS peaks
and fragmentation still require hardware acceptance. ESP32 keeps its existing
capacities.

## Changes and compatibility

- Values share numeric/text storage; safety bounds contain numeric data only.
  Integer safety comparisons retain all 64 bits. Keys have their own 49-byte
  storage, supporting the wire contract's 48 bytes plus terminator.
- Binding uses at most nine keys per request, with correlation, generation,
  cross-batch uniqueness and timeout checks. The existing Engine supports
  independent binding requests; no CDDL or endpoint change is required.
- Configuration records retain encoded bytes only. The installer takes its
  own copy before a shared workspace is reused for typed semantic validation.
  Validation still precedes durable acceptance and ACK.
- Universal setup services exist only during setup. Teardown releases request
  buffers and registered handlers before TLS; application-owned servers are
  untouched. Setup can be recreated after failure.
- ESP8266 CA PEM data lives in flash. Link/OTA own a single TLS client at a time;
  closing it releases the 6,200-byte stock BearSSL secondary stack when its last
  owner exits. Preflight precedes client construction. OTA borrows the stopped
  Link write buffer and destroys HTTP borrowers before the TLS owner.
- Arbitrary endpoints remain 16,384-byte RX / 512-byte TX. The guarded
  bounded-endpoint candidate uses 2,048-byte RX / 512-byte TX while retaining
  CA, hostname and certificate-date validation. Preflight includes contexts,
  record overhead, the secondary stack, a 4 KiB
  safety reserve plus 3 KiB TCP/allocator allowance, and a 4 KiB updater page.
  Current builds allocate these resources only in DRAM.

This changes low-level `Value` layout and the advanced
`FlovaClientLink::decodeStoredConfigurationUnit` interface. Public typed
registration/write/report APIs remain intact. Persisted values, command
results, history metadata and schedule manifests have new magic values;
incompatible old records are not restored. Verified configuration and device
credentials retain their formats. Schedules can be reconstructed from verified
configuration. Previously persisted output values/history/results are not
migrated: installations depending on offline output restoration must account
for this breaking change before adopting the build. No release is published by
these changes.

## Reproducible build evidence

Run:

```sh
pio run -e universal-esp32 -e universal-esp8266
python3 scripts/check_flova_memory.py universal-esp32 universal-esp8266
python3 scripts/check_flova_memory.py --stock-dram universal-esp8266
```

The final command checks an optimistic necessary bound, not physical product
acceptance. Do not weaken that gate to promote the candidate. Normal invocation
enforces reviewed linked-image ceilings from `scripts/flova_memory_budgets.json`.
Changing those ceilings requires reviewing the actual RAM/flash delta. ESP8266
figures include `.noinit`, unlike PlatformIO's RAM summary.

Measured with PlatformIO espressif8266 4.2.1 / Arduino core 3.1.2 and
espressif32 7.0.1. Revalidate the context/secondary-stack costs on core updates.
The original PlatformIO static-RAM summaries were 57,896 bytes for ESP8266
(four streams, shared IRAM) and 123,664 bytes for ESP32 (64 streams).

Current linked images (bytes):

| Environment | Static DRAM including `.noinit` | IRAM | Firmware image |
| --- | ---: | ---: | ---: |
| `universal-esp8266` | 47,712 | 29,015 | 567,616 |
| `universal-esp32` | 75,896 | 84,714 | 1,048,192 |

Compared with the first reduction pass, the 64-stream universal image sheds
18,860 static DRAM bytes and the SDK probe sheds 18,732. Some storage moved to
phase-owned heap objects; these static savings are not equivalent to heap
headroom. Flash grew by roughly 3–4 KiB for lifecycle/storage handling. Reviewed
image ceilings were rounded up to the next 4 KiB boundary for that tradeoff.

## Resident state and phase ledger

ESP8266 descriptors (keys, numeric bounds, retention policy) use verified
`stream:<slot>` storage records and one bounded cache. Register streams from
`setup()`, after board services exist; registration mounts storage and can fail.
Callbacks and all live values remain resident. Slot handles do not point into
replaceable metadata. Other portable integrations retain resident descriptors
unless `FLOVA_STREAM_DESCRIPTORS_IN_STORAGE=1` is selected; storage-backed ports
must implement `Link::bindDatastreamKeys` with the bounded key-reader callback.
The old pointer-array binding seam remains available for resident descriptors.

Four delivery slots replace per-stream message timers. ACKs retire only the
revision actually transmitted. History and command results retain their durable
payloads in storage, with bounded RAM indexes/workspaces. Storage adapters must
actually support read-after-write; an adapter which discards successful writes
cannot provide these services.

`FlovaPhaseStorage<T>` owns compile-time-sized Arduino phase objects. Measured
ESP8266 allocation payloads are 2,248 bytes for configuration, 1,656 for schedules,
2,528 for transport framing, and 984 for pending control records. These are
additional heap allocations, not hidden static savings. Control/frame buffers
are allocated after TLS handshake. Borrowed callback records remain alive until
dispatch returns. Configuration buffers are released before opening TLS.

The stock core 3.1.2 source frees its 3,064-byte X509 context after handshake,
but retains the 6,200-byte TLS secondary stack. For the guarded 2,048-byte
bounded-endpoint candidate, the checker therefore reports:

| Phase | Additional minimum heap, including safety/TCP reserves |
| --- | ---: |
| Link handshake | 22,810 |
| Connected runtime | 24,914 |
| Configuration transfer | 27,162 |
| OTA | 26,906 |

These optimistic bounds exclude pre-existing Wi-Fi, filesystem, trust-anchor
and allocator heap. They use the actual 80 KiB allocator region, ending at
`0x3fffc000`, not the whole address range up to `0x40000000`.
The universal 64-stream candidate passes this theoretical linked-image bound by
7,046 bytes at the configuration phase. That estimate excludes existing Wi-Fi,
filesystem, trust-anchor and fragmentation costs, so the product target remains
unaccepted until endpoint captures and physical runtime measurements pass. The
custom SDK probe retains the full-record profile and its 8 KiB application
reservation.

Handshake and OTA maintenance pause local stream processing and schedules,
without driving outputs to a different value. Schedule progress is checkpointed
and verified before releasing its workspace. Restoration requires the expected
manifest revision and valid progress; it must not silently reset progress and
replay actions. Status snapshots expose `maintenance` and emit
`MaintenanceChanged`. Writes/reports during a pause fail with `maintenance`.
OTA currently keeps live stream values resident; it releases schedules and Link
workspaces. No claim is made that all runtime RAM is reclaimed during OTA.

A 48-byte text capacity means at most 47 text bytes plus a terminator, not 48
text streams. Current resident payloads still reserve the maximum value size for
every stream. A typed payload pool could reduce predominantly numeric workloads,
but that is not implemented here and would require revisiting the all-64-text
worst-case requirement before claiming a smaller guaranteed budget.

The custom-SDK probe contains a retained 8 KiB application reservation and
registers 64 maximum-length text values with 48-byte keys. The universal probe
reserves 64 runtime entries; a matching 64-stream server configuration is still
required to exercise all of them. Probe builds are not production firmware.

## Validation boundary

Host tests cover storage-backed 64-stream values, binding, maintenance rejection,
schedule checkpoint restoration, stale ACKs, delivery fairness, exact integer safety rejection,
configuration workspace reuse, TLS budget boundaries, optional IRAM fallback,
and existing parser/reconnect/persistence behavior. Target builds establish
compilation, static layout and stack frames only.

All eleven host tests pass, including a Clang AddressSanitizer/UndefinedBehaviorSanitizer
build. Universal ESP32, ESP32 BLE and ESP8266, both datastream examples, all three
memory probes, embedded test compile targets, and exported SDK builds pass.
The exported SDK was compiled for ESP8266 with both shared IRAM and stock MMU.
Protocol, public-surface, version, stack-usage and linked-image ceiling checks
must pass together; the stock-DRAM gate is only a theoretical prerequisite.

Per the requested code/build-only validation boundary, provisioning, TLS
handshake peaks, sustained traffic, long reconnect stress, OTA interruption,
and power-loss/fragmentation acceptance remain unverified. No additional device
operations are part of these checks. Once hardware testing is requested,
measure free heap, largest block, fragmentation and stack headroom throughout
all those workloads with the custom application reservation retained. At least
4 KiB must remain at workload peaks. Increase the reservation in 4 KiB increments
only after the 8 KiB target passes; publish only a measured allowance.

The universal ESP8266 example emits periodic loop samples.
These samples do not observe allocations inside a blocking TLS call and must
not be described as the absolute handshake low-water mark.

## Datastream storage-policy validation (2026-11-12)

`Persistent`/`Immediate` keep synchronous durability. Optional `OnChange` and
`Deferred` policies add two 32-bit timer fields per runtime entry (512 bytes
at 64 entries), plus bounded dirty/stored flags, a persistence cursor and
history-head metadata. Deferred writes run one due stream per loop, with a
fixed first-update deadline and delayed failure retries. Configuration key
fingerprints reuse the phase-owned validation/image union; the ESP8266
configuration allocation remains 2,248 bytes. The fair schedule cursor adds
8 bytes including alignment to the schedule allocation. No TLS reserve or
runtime phase minimum was reduced.

Clean baseline commit `d3a91d4` and the working tree built with the same
toolchains give:

| Environment | Baseline static DRAM | New static DRAM | Baseline image | New image |
| --- | ---: | ---: | ---: | ---: |
| `universal-esp8266` | 47,176 | 47,712 (+536) | 566,464 | 567,616 (+1,152) |
| `universal-esp32` | 75,352 | 75,896 (+544) | 1,046,864 | 1,048,192 (+1,328) |

IRAM is unchanged. The ESP8266 baseline already exceeded its previous image
ceiling by 192 bytes. Reviewed ceilings now allow 569,344 image bytes there
(the next 4 KiB boundary) and 76,032 static DRAM bytes on ESP32 (the next
256-byte boundary). Other image/static ceilings and all safety reserves are
unchanged. The four theoretical stock-DRAM phase checks still do not establish
ESP8266 physical acceptance.

Validation passed: 18 host tests; universal and datastream example builds on
both chips; all three embedded test compile targets; protocol/public-surface
and stack-usage guards; normal and stock-DRAM footprint gates; exported SDK
builds on ESP32 and ESP8266 (shared IRAM and stock MMU). Six datastream tests
also ran successfully on a physical ESP32. Host regressions cover failed
persistence writes, rollover, deferred
deadline/coalescing, history metadata failures, fingerprint collisions and
bounded schedule catch-up.

A private ESP32 application used two streams on a dedicated development device
and the local Engine through a temporary certificate-verified TLS ingress.
The application seeded UTC from the host for this test; SNTP was not tested.
It reported both streams every 200 ms, with temperature deferred by 5 seconds
and humidity nonpersistent. Engine confirmed applied configuration generation 1
and both latest readings as `synced`. Reboot restored temperature but not
humidity; explicit flush succeeded. Three application-triggered Wi-Fi
disconnect/reconnect cycles recovered without reprovisioning. Connected free
heap returned to about 148–152 kB (148,380–151,920 bytes); the
minimum sampled ESP heap low-water counter was 142,756 bytes and the connected
largest free block was 77,812 bytes. The smallest observed loop-task stack
headroom was 4,700 bytes. Most five-second-window `run()` maxima were 6–11 ms;
one window reached 47.95 ms. These are sampled application-loop durations,
not a hard latency bound or a long-duration leak test.

This ESP32 smoke test does not replace the still-required ESP8266 hardware
measurements, 10,000-cycle reconnect stress, OTA interruption, router reboot,
or interrupted configuration/power-loss acceptance. Credentials, private test
firmware and the original full-flash backup remain outside the repository.
After testing, the original 4 MiB flash image was restored with esptool hash
verification and the temporary TLS ingress was stopped. The dedicated local
development fixture remains available for future tests.


## Cloud-only platform schedules (2026-11-13)

This supersedes the downloaded-schedule runtime/allocation evidence above.
Platform schedules run in Engine and send existing expiring commands. SDK
`FlovaScheduleRuntime`, manifest compilation, schedule persistence, renewal and
schedule-transfer opcodes/records are removed. `FlovaScheduler.h` remains an
application-owned, eight-entry RAM helper; it never accepts platform schedules.
The generic resource allocator is gone; fixed capacities and explicit history
retention still bound their respective features.

With the same pinned toolchains and universal profiles as the interrupted
scheduling implementation, the final linked results are:

| Profile | Static DRAM | IRAM | Image | Image delta |
| --- | ---: | ---: | ---: | ---: |
| universal-esp32 | 76,288 B | 84,714 B | 1,045,888 B | -7,360 B |
| universal-esp8266 | 48,248 B | 29,015 B | 566,080 B | -7,328 B |

The static DRAM deltas are -64 B and -188 B respectively. The ESP8266
stock-DRAM check passes all four phase bounds without a schedule allocation.
The stack guard passes; `stepConfigurationSemantic` and
`finishConfigurationApply` are now inlined into the checked
`stepConfigurationWork` frame (208 B), confirmed by the linked symbol table.
The command receive frame is 96 B and the Arduino transport receive frame is
32 B. Rare full-message temporaries use separate call frames; the inbound
message uses the existing connection-owned workspace instead of overlapping
the durable command/ACK stack. Its measured total is 1,752 B (including the
256 B message); the phase budget records the actual target type size.

Verification includes all 18 host CTest targets, the four universal/datastream
ESP32/ESP8266 profiles, embedded test-image builds (without upload/execution),
and exported SDK builds for both boards. The Engine migration/full database
suite passes 392 tests in an isolated database; clients pass type checks.

The physical 4 MiB ESP8266 smoke workload passed 34 checks across two boots:
explicit cloud permission, local timers without network, invalid-clock blocking,
hardware-write rejection/state preservation, missed-minute skipping, fresh RAM
runtime behavior, retired opcodes and zero timer storage operations. Observed
free heap/largest block were 49,168 B; stack headroom was 3,088 B. No GPIO was
driven. The original complete flash was restored and verified. The timer workload contains no TLS.

A separate non-GPIO physical run used an isolated Engine database and local
CA/hostname/date-verified, record-bounded TLS ingress. The device reported its
firmware permission, Engine created and ran a one-time cloud schedule, and the
ESP8266 handler accepted the RAM write. Engine recorded reported value `true`
with desired/acknowledged version 1. The full original flash was restored and
verified again. The first delivery run exposed insufficient stack headroom;
the command/transport lifetime fixes above were verified in a repeated run.
Minimum observed connected heap was 23,728 B,
largest block 23,472 B and continuation stack headroom 1,456 B, passing the
8,192 B heap and 1,024 B stack floors. Final report:
`/tmp/opencode/esp8266-cloud-20261113T172215Z/report.json`.
Restoration was retried at 115200 baud after a serial failure; both the write
hash and independent full-flash digest verification matched the original image.
This proves physical cloud delivery on the custom fixture, not production-profile
TLS/OTA peak heap, SNTP, universal provisioning, or long-run reconnect acceptance.
