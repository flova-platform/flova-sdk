# Storage, Durability, and Runtime Latency

## Default contract

All persistent-storage integrations are optional and disabled by default.
Developers must explicitly select and enable the backend and each required
storage-backed capability using documented configuration. Backend availability,
board type, reconnect, or queue saturation must not implicitly enable storage.
Do not invent a new flag scheme when existing explicit configuration can express
the contract; document unsupported combinations and fail configuration clearly.

The default runtime holds all mutable state in bounded RAM: live values, active
configuration, credentials supplied for the session, routing, safety metadata,
schedules, and volatile queues. Reset loses that state; document how the developer
supplies it again. Never spill to flash when RAM fills. Use bounded rejection,
coalescing, or dropping according to the feature contract.

With persistent storage disabled, do not initialize/mount a backend or perform
reads, writes, commits, erases, formatting, or implicit platform credential saves.
Compile out optional storage buffers and exclusive dependencies. Persistent
provisioning/settings, recovery, history, checkpoints, persistent logs, and OTA
staging must each require explicit opt-in. If a requested operation cannot meet
its durability contract in RAM-only mode, report unsupported/configuration error
before effects or durable ACK; never silently weaken that contract.

RAM-only refers to mutable runtime state and persistence, not firmware placement:
code and immutable constants can remain in flash/ROM. Firmware installation/OTA
necessarily writes an image and is a separate explicitly enabled operation, not
an exception permitting ordinary runtime persistence.

Normal control, safety validation, sampling, schedule evaluation, routing, and
telemetry encoding use bounded resident runtime state. After initialization,
ordinary nonpersistent hot paths must perform zero storage reads or writes.
Boot restore, binding/configuration, explicit persistence, required durable
checkpoints, and OTA are separate lifecycle operations, not implicit exceptions
for every loop iteration. Identify their call paths and execution phases.

Do not offload hot state to LittleFS, EEPROM/emulated EEPROM, or NVS merely to
reduce static RAM. Keep only the hot subset resident; cold descriptive metadata
may remain in storage, and immutable compiled data may remain in flash/ROM using
target-correct access. Filesystem access and mapped constant reads are not the
same operation. Include storage-library heap and workspaces in resource budgets.

## Choose durability explicitly

Classify each new or changed state item:

| Class | Default behavior |
| --- | --- |
| Replaceable samples, telemetry, caches, transient diagnostics | Volatile; coalesce/drop under a bounded policy |
| Session credentials, active configuration, user settings | RAM-only by default; durable only with developer-enabled persistence and the required acceptance/recovery contract |
| Selected values, schedule progress, command results, offline history | Persistent storage only with explicit developer opt-in and a durability requirement; reject operations that require unavailable durability |

Explain why a record must survive reset before adding it. Full RAM queues do not
justify a new persistent backlog. Keep explicitly durable queues bounded with
defined retention, expiration, and full-storage behavior.

For every persistent record or queue, record in the relevant feature documentation
or change evidence:

- owner, key/layout version, maximum record size and retained count;
- write trigger and unchanged-value deduplication policy;
- maximum logical writes/commits and bytes per specified interval;
- coalescing window, maximum dirty age, and behavior under continuous changes;
- acknowledgement point and maximum permitted loss after reset;
- allowed execution phases, synchronous latency budget, and deadline impact;
- full-storage behavior, bounded retry frequency/backoff, and failure reporting;
- recovery, migration, and rollback behavior for incompatible layouts.

No universal numeric write/latency limit is safe for every chip. Use explicit
numbers from the product requirement and target evidence; if absent, report the
missing budget rather than inventing one or claiming acceptance.

## Preserve correctness while reducing writes

Skip unchanged durable values when their existing verified state permits it.
Coalesce only replaceable intermediate values. Deferred saving must have a fixed
maximum dirty-age contract; continuous updates must not postpone saving forever.
Use rollover-safe timing and bounded failure retries; no retry-every-loop storms.

Do not weaken persist-and-verify-before-ACK configuration semantics, A/B generation
promotion, checkpoint-before-action, or synchronous persistence policies. A RAM
update is not durable success. On failure, preserve the last valid state and the
existing rejection/retry semantics. Never update authoritative state after a
rejected hardware write. Define how an unsaved dirty value is reported.

Power-loss recovery must cover interrupted writes, verification, promotion,
deletion, and metadata updates. Preserve last-valid records; never auto-format
on mount failure or erase unrelated application storage without an explicit
authorized recovery/reset policy. Version changed persisted representations.

## Count actual costs

EEPROM emulation may commit an erase sector; NVS may compact pages; LittleFS may
write metadata and perform maintenance. Small payloads and logical chunking do
not prove low physical cost, bounded latency, or low wear.

Measure logical reads/writes/removes/commits and bytes. Where backend tooling
permits, also measure physical program/erase activity and write amplification.
State unknown physical costs explicitly. Endurance estimates need the actual
chip/backend geometry, documented endurance assumptions, write workload, and
safety margin; do not infer lifetime from logical bytes alone.

Audit implicit writes too: platform Wi-Fi credential persistence, reconnect or
boot counters, descriptor registration, diagnostics, logs, and filesystem
housekeeping. Disable unnecessary automatic persistence through supported
platform controls without breaking provisioning. Production logs must not gain
a recurring persistent sink by default.

## Execution and measurement

Never access storage in an ISR or transport callback. Callback work is limited
to the existing bounded enqueue/copy contract. Schedule storage at explicit
board-loop/lifecycle boundaries. One operation per loop or calls to `yield()`
are not evidence that an erase/commit meets control deadlines.

Measure worst-observed operation latency, maximum loop gap, deadline misses, and
watchdog behavior on hardware. Include normal and nearly-full storage,
maintenance/compaction, bursts, retries, reconnect, and OTA interactions. Label
sample counts, target/core/backend versions, workload, and observed maxima;
observed maxima are not a proven worst-case execution bound. Host tests prove
operation counts and semantics, not flash timing or physical endurance.

## Existing exception and future changes

Current ESP8266 storage-backed descriptors have a single-entry cache. Alternating
streams can force reads during safety validation. Preserve fail-closed safety,
registration, handle, and ACK semantics until a separate redesign is verified;
do not claim zero-I/O acceptance or copy this design into new hot paths merely
because it already exists. See `memory.md` and `packages/MEMORY.md` for context.
Existing storage-dependent boot/configuration paths also do not establish a
working RAM-only default. These rules define the desired contract, not an
implemented feature flag or permission to disable durability checks in place.

Any exception to the default contract must name the target/feature profile,
affected paths, reason, measured RAM/image/I/O/latency tradeoff, acceptance
status, and revalidation triggers. Missing evidence means unverified, not
accepted. Revalidate affected assumptions after core, storage backend, compiler,
TLS, partition, capacity, or persistence-policy changes. Keep current numbers
and evidence in `packages/MEMORY.md` or the relevant feature evidence; keep
executable ceilings in existing budget checks. Do not duplicate moving numeric
budgets throughout the skills or raise them merely to pass a gate.
