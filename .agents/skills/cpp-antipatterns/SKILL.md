---
name: cpp-antipatterns
description: Find and prevent C++ correctness and resource antipatterns in Flova firmware and SDK. Use when reviewing, debugging, writing, or refactoring C++ involving lifetimes, buffers, arithmetic, callbacks, concurrency, serialization, or embedded resources.
---

# C++ Antipatterns

Read the repository guide at `../../../AGENTS.md`. Follow
`../flova-firmware/SKILL.md` for resource and storage contracts and
`../cpp-code-quality/SKILL.md` for the implementation/verification workflow.
The portable core is C++11 with exceptions and RTTI disabled. Do not suggest
newer language facilities or heap-backed replacements without checking targets.

## Correctness hazards

| Antipattern | Concrete replacement or proof |
| --- | --- |
| Returning a pointer/reference to a local, temporary, or expired callback frame | Copy into bounded owned storage or prove the owner's lifetime covers every use |
| Capturing `this` or a reference in a retained callback without teardown | Unregister/drain callbacks before owner destruction; prove no pending borrower survives |
| Keeping pointers into an evictable cache, union workspace, or relocated object | Use stable slot/generation handles and re-resolve; bound borrow lifetimes |
| Implicit copying of a resource owner | Delete copy operations; implement moves only if handle transfer and borrower validity are proven |
| Deleting a derived object through a base without a virtual destructor | Use a correct polymorphic destruction contract or prohibit base deletion; do not add vtables indiscriminately |
| Uninitialized fields or reading an inactive union member | Initialize every active field and track the discriminant through transitions |
| `memset`/`memcpy` on non-trivial C++ objects | Use constructors/assignment; byte-copy only representations whose type and lifetime rules permit it |
| Casting packet bytes to a struct or unrelated scalar pointer | Decode checked fields with explicit byte order; avoid alignment, aliasing, and object-lifetime violations |
| Persisting native struct bytes as a portable wire/storage schema | Use versioned explicit encoding; existing ABI-local layouts require compatibility proof, not silent rewrites |
| Using `memcmp` as semantic equality on padded structs | Compare fields or an explicit canonical representation; padding is not state |
| Narrowing network lengths before checking them | Validate in the source-width type, then convert |
| `offset + length <= capacity` for untrusted sizes | Check `offset <= capacity && length <= capacity - offset` before access |
| Signed overflow, invalid shifts, unchecked count multiplication | Check operand ranges before arithmetic; do not assume signed wrapping |
| Mixing signed/unsigned sentinel values and indexes | Validate the sentinel before conversion/indexing; use explicit status plus bounded index |
| Converting exact integer safety thresholds to floating point | Preserve exact integer comparisons; validate non-finite floats before acceptance |
| Ignoring string termination or formatting truncation | Check source bounds and return values, including negative formatting results; reserve terminator space |
| Using wall-clock time for retry/deadline timing | Use monotonic ticks and a documented rollover-safe comparison with a bounded interval |

Example: check a range without addition overflow (C++11):

```cpp
inline bool rangeFits(size_t offset, size_t length, size_t capacity) {
    return offset <= capacity && length <= capacity - offset;
}
```

Before using the range, also validate buffer presence/lifetime and the enclosing
message's semantics. An arithmetic check alone is not full input validation.

## State, callbacks, and concurrency

- Do not mutate authoritative state, revisions, persistence, or outbound state
  before the hardware write succeeds. Preserve existing durability/ACK ordering.
- Do not use a boolean cluster to conceal invalid lifecycle combinations. Use
  existing explicit states and define failure/retry transitions.
- Do not retain a shared-workspace borrow across a reentrant call or phase change.
  Document which calls can dispatch callbacks or invalidate references.
- `volatile` is not synchronization. Use the platform's proven ISR/task handoff
  or critical-section mechanism. Verify atomics are supported and suitable on
  the target; do not assume a type is lock-free or ISR-safe.
- Do not hold locks/critical sections across storage, networking, logging, or
  user callbacks. Bound interrupt-disabled work and avoid callback reentrancy.
- No I/O, dynamic allocation, unbounded work, or hardware application in transport
  callbacks; use the existing bounded board-loop handoff. ISRs have stricter
  target-specific rules, including instruction/data residency where required.

## Cost and architecture traps

- No runtime-growing containers, repeated `String` concatenation, per-message
  allocation, recursive parsing, or large automatic buffers in firmware hot paths.
- A fixed array can still be too large; a bounded scan can still miss a deadline.
  Account for capacity, algorithmic work, stack, and the all-maximum workload.
- Moving BSS to heap or metadata to LittleFS is not a resource improvement by
  itself. Count phase peaks, storage traffic, fragmentation, and loop latency.
- Do not blanket-replace virtual interfaces with templates or vice versa.
  Preserve service seams; measure dispatch cost and template/code-size growth.
- Avoid global constructors with storage, network, hardware, or cross-translation
  unit initialization dependencies. Use explicit board-owned setup/teardown.
- Avoid side-effecting macros and required work inside `assert`: assertions may
  disappear in release builds. Use small typed functions and explicit checks.
- Do not suppress warnings or add casts merely to silence a diagnostic. Explain
  the valid range, lifetime, or representation and fix the responsible boundary.

## Review evidence

Report actual reachable problems, not a checklist of speculative smells. For each
finding give `path:line`, trigger, impact, and smallest responsible fix. Distinguish
confirmed bugs from missing proof. Search callers before changing shared code.
Add a regression at the real boundary (empty, exact-capacity, over-capacity,
overflow, stale callback/ACK, failed write, or teardown as applicable).
Run relevant existing host/target checks; sanitizers help with host UB but cannot
prove MCU alignment behavior, interrupt safety, storage timing, or RAM acceptance.
