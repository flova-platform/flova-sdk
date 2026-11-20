# Flova SDK repository guide

This repository contains the portable Flova device SDK, Arduino services,
ESP32/ESP8266 board packages, firmware examples, and the bounded Flova Link
protocol implementation.

Read `README.md`, `packages/README.md`, and `protocol/README.md` before changing
an architectural or protocol area.

## Repository boundaries

- `packages/flova-embedded-sdk`: canonical C++11 `flova::Device` runtime. Its
  portable include closure must not depend on Arduino, ESP, GPIO, Wi-Fi,
  WebSocket, TLS, filesystem, exceptions, RTTI, or unbounded containers.
- `packages/flova-arduino`: generic Arduino clock, storage, logging, bounded
  Device Link services, and the board-platform seam. It must not contain ESP
  transport, TLS, updater, or pin-policy code.
- `packages/flova-esp32` and `packages/flova-esp8266`: board-specific
  provisioning, boot/storage policy, TLS/resource setup, OTA, pin policy, and
  hardware composition.
- `examples/`: selected PlatformIO applications and SDK compile contracts.
  The root `src/` is only a required PlatformIO source anchor; it contains no
  firmware implementation.
- `protocol/`: CDDL authority, generated zcbor codecs, message catalog, and
  deterministic conformance vectors. Do not hand-edit generated files.
- `test/`: host and embedded tests. `scripts/`: generators and static guards.

`FlovaDevice.h` is the only portable device runtime. `FlovaLinkMessages.h` contains the
bounded Arduino codec records but no transport base class. Add SDK behavior to
the `flova::` service interfaces and compose board services explicitly.

## Required skills

For firmware/SDK work, follow `.agents/skills/flova-firmware/SKILL.md`.
For C++ implementation and review, also follow:

- `.agents/skills/cpp-code-quality/SKILL.md`: contracts, scope, and verification.
- `.agents/skills/cpp-antipatterns/SKILL.md`: language and embedded hazards.
- `.agents/skills/cpp-style/SKILL.md`: local C++11 readability and naming.

Read the firmware references for the area being changed: `storage.md`,
`protocol.md`, `datastreams.md`, and `testing.md` under
`.agents/skills/flova-firmware/references/`. Keep detailed rules there rather
than duplicating them. The `flova-sdk` skill is an alias, not a second authority.

## RAM-only default and optional storage

These are the required design defaults, not a claim that every existing path
already implements them:

- Keep mutable runtime state in explicitly owned, bounded RAM: live values,
  active configuration, session credentials, routing, safety metadata,
  schedules, and volatile queues. Reset loses volatile state; document how it
  is supplied again. Never spill to persistent storage when RAM fills.
- All persistent-storage backends and storage-backed features are optional and
  disabled by default. Developers must explicitly enable and configure each
  required capability/backend. Board type or backend availability is not opt-in.
- Storage-disabled operation must not initialize/mount storage or perform
  reads, writes, commits, erases, formatting, or implicit Wi-Fi credential saves.
  Recovery, durable settings, history, checkpoints, persistent logs, and OTA
  staging must not become hidden storage dependencies.
- Operations requiring unavailable durability must fail explicitly before
  effects or durable acknowledgement. Do not silently downgrade an existing
  durable protocol to volatile success. A RAM-only configuration protocol needs
  explicitly defined volatile semantics before it can acknowledge acceptance.
- In enabled durable modes, persist and verify configuration records before
  acknowledging them, retain the last active A/B generation until promotion
  succeeds, and preserve checkpoint-before-action guarantees.
- Every persistence feature needs an explicit recovery/delivery requirement,
  retention/full-storage policy, write/commit budget, maximum dirty age, bounded
  retries, and latency budget. Skip unchanged writes and coalesce only where
  semantics permit. Never auto-format on mount failure without an authorized
  recovery policy.
- Ordinary control, safety, sampling, schedule evaluation, and telemetry paths
  use resident state with zero persistent-storage I/O. Any exception needs an
  explicit profile, measured tradeoff, acceptance status, and revalidation trigger.

Code and immutable constants may remain in target-appropriate flash/ROM;
RAM-only does not mean loading the whole firmware image into RAM. Firmware
installation/OTA writes are separate explicitly enabled lifecycle operations.

**Known gap:** current ESP8266 storage-backed descriptors use a two-entry
cache. Safety validation can still read storage when the working set exceeds
the cache or key lookups evict its entries. Other boot/configuration paths
also depend on storage. Preserve their safety and
durability semantics until separately redesigned and tested; do not claim that
editing these instructions implements a RAM-only profile.

## Runtime and C++ quality

- Route local writes, remote writes, scheduled actions, and mapped outputs
  through one bounded datastream write path. A rejected hardware write must not
  update authoritative cached state, persistence, revisions, or outbound state.
- Transport callbacks may only copy/enqueue bounded work. Apply hardware work
  from the board-owned `flova::Device::run()` loop. No storage I/O in ISRs or
  transport callbacks; document callback borrows and teardown ordering.
- Flova-owned hot paths must not dynamically allocate. No recursion, unbounded
  containers, or large stack buffers. Explicitly bound frames, strings, queues,
  registries, command history, configuration records, nesting, and retry work.
- Make ownership and lifetimes explicit. No pointers into expired callbacks or
  replaceable caches, implicit copying of resource owners, or shared-workspace
  reuse while borrowed. Use existing lifecycle states and cleanup mechanisms.
- Check lengths and arithmetic before access/conversion. Avoid signed overflow,
  unsafe narrowing, alignment/aliasing violations, and treating struct padding
  as data. Keep exact integer safety comparisons exact.
- Check hardware, storage, codec, enqueue, and transport results. Keep failure
  effects and retry behavior explicit; never turn an error into apparent success.
- Keep headers self-contained and the portable core C++11-compatible. Reuse
  existing services and conventions; no speculative abstraction, dependency,
  newer-language requirement, or repository-wide formatting for a local change.
- Comment ownership, lifecycle, memory, hardware, and protocol invariants—not
  obvious syntax. Keep required work out of assertions that release builds omit.

## Minimal images and resource evidence

- Disabled optional features must compile out their code, buffers, and exclusive
  dependencies. New dependencies need a concrete reason and measured linked cost.
- Report affected production-profile image, static DRAM, stack, and phase
  peak-heap deltas separately. Moving BSS to heap or metadata to a filesystem is
  not proof of a resource improvement. Inspect linker maps for unexplained growth.
- Verify real partition/OTA-slot fit and relevant default/capacity/MMU profiles.
  Do not infer physical acceptance from a universal example or probe build.
- Measure storage latency, loop gaps, memory low-water marks, and largest free
  blocks where relevant. Chunking, `yield()`, and one write per loop do not prove
  deadlines are met. Account for TLS, filesystem, and maintenance peaks.
- Never raise ceilings merely to pass a gate. Keep run-specific measurements in
  private notes outside the repository and executable limits in existing checks. Revalidate
  affected assumptions on core, compiler, backend, TLS, partition, or capacity
  changes; keep missing hardware evidence explicitly unverified.

## Protocol, security, and compatibility

- Keep Device Link schema-directed and deterministic: CDDL is authoritative,
  complete frames are at most 512 bytes, and ESP8266 Link uses the verified
  16,384-byte RX / 512-byte TX profile. OTA remains a separate 16,384-byte RX /
  512-byte TX workload. Smaller TLS records require the documented endpoint proof,
  not just small application frames.
- Validate network-controlled bounds before processing. Keep real codecs
  host-testable and fuzzable; do not add a generic dynamic protocol DOM.
- Do not log credentials, provisioning tokens, device secrets, or complete
  sensitive payloads. Preserve certificate/hostname/date validation.
- Public headers, feature flags/defaults, exported SDKs, wire schemas, and
  persisted layouts are compatibility contracts. Version storage changes and
  preserve recovery/rollback; do not hide behavior changes inside cleanup.

## Working discipline

- Inspect `git status` and preserve unrelated staged/uncommitted work. Never
  reset or discard the worktree to isolate a task.
- Trace the owning implementation and callers before editing. Keep changes at
  the narrowest responsible layer. Establish a test baseline before refactoring.
- Review-only requests do not authorize edits. Report concrete findings with
  location, trigger, impact, and a minimal fix; avoid speculative style findings.
- Add focused regressions in the existing harness for changed behavior. Exercise
  the real implementation and relevant boundaries/failure paths, not a duplicate
  simplified implementation. Stop once the requested scope is verified.

## Validation

Run the relevant checks before handoff. For a broad firmware change, run:

```sh
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
scripts/check_flova_link_contract.sh
scripts/check_flova_public_surface.sh
scripts/check_esp8266_stack_usage.py
pio run -e universal-esp32 -e universal-esp8266
pio run -e datastream-api-esp32 -e datastream-api-esp8266
python3 scripts/check_flova_memory.py universal-esp32 universal-esp8266
python3 scripts/check_flova_memory.py --stock-dram universal-esp8266
pio test -e test-esp32 -e test-esp8266 -e test-config-rollback-esp8266 --without-uploading --without-testing
git diff --check
```

For storage/default changes, test disabled-backend operation counts, explicit
opt-in, unavailable-durability rejection, full queues, failed writes, dirty-age
deadlines, and interrupted recovery as applicable. Test alternating streams to
expose descriptor-cache I/O. Validate optional-feature exclusion and exported
SDK builds when their contracts change. Documentation-only changes need link,
consistency, and whitespace checks, not firmware builds.

Document commands run, results, and any unavailable target with the exact command
and failure. Do not flash or manipulate hardware for code/build-only work. Build
success is not physical acceptance: provisioning, WSS/TLS, reconnect, OTA,
power-loss configuration recovery, and ESP8266 heap/fragmentation checks
require hardware or the documented external test setup.
