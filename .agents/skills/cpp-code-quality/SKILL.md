---
name: cpp-code-quality
description: Implement and review maintainable C++11 Flova firmware and SDK code with explicit contracts, small changes, bounded costs, and regression evidence. Use for C++ feature work, cleanup, refactoring, API design, testing, or code-quality reviews.
---

# C++ Code Quality

Follow repository `AGENTS.md`, `../flova-firmware/SKILL.md`, and
`../cpp-antipatterns/SKILL.md`. Keep rules centralized there rather than copying
resource budgets into this skill. This skill adds a workflow, not a new framework,
formatter, dependency, or mandatory repository-wide cleanup.

## Before editing

1. Read the owning component, its callers, tests, and relevant feature contracts.
   Inspect `git status`; preserve unrelated work.
2. State the behavior being changed, the invariants being preserved, and the
   smallest runnable check that will demonstrate success.
3. Reuse existing services/helpers. Fix the responsible layer, not each caller's
   symptom. Do not add speculative abstractions or future configuration knobs.
4. For behavior-preserving refactors, establish a passing baseline first and
   keep feature changes separate. Report an unavailable baseline explicitly.

## Implementation contracts

- Portable core remains C++11, exception/RTTI-free, and platform-independent.
  Do not introduce `std::span`, `std::string_view`, concepts, or other newer
  facilities there. Use existing pointer/length and bounded value conventions.
- Follow neighboring naming, layout, includes, and error conventions. Avoid
  unrelated renames, mass formatting, or arbitrary function/line-count quotas.
- Give each operation a clear purpose. Extract a helper when it clarifies a
  real repeated invariant or ownership boundary, not merely to shorten a file.
- Prefer explicit constructors, scoped enums, named units/capacities, `override`,
  and `const` where they clarify contracts without breaking established APIs.
  Avoid implicit conversions and boolean arguments that obscure meaning.
- Make headers self-contained and guard them consistently. Prevent ODR violations
  in header-defined functions; do not rely on incidental include order or
  writable namespace-scope header globals. Preserve existing intentional macros.
- Document ownership, valid ranges, units, buffer capacity/termination, callback
  borrow duration, allowed execution phase, and failure effects at boundaries.
  A pointer/reference alone does not establish ownership or lifetime.
- Model resource ownership explicitly. Use stack/member RAII for cleanup where
  appropriate; RAII does not require heap allocation or shared ownership.
  Destructors must not conceal unbounded I/O or required fallible commits.
- Return failures using existing explicit status/result conventions. Check
  hardware, storage, encoding, enqueue, and transport results. Distinguish retry,
  rejection, and durable success; never disguise failure as an empty valid value.
- Stage/validate changes before publication. Specify what remains unchanged on
  failure and what cleanup/retry is legal. Preserve last-valid configuration,
  fail-closed safety, and revision-aware acknowledgement behavior.
- Keep loops and retries explicitly bounded. Declare queue-full policies and
  maximum work per invocation; consider fairness as well as capacity.
- Comment reasons and invariants, not obvious syntax. Explain any measured
  exception with its target profile and revalidation trigger.

## Compatibility and scope

Treat public headers, compile-time flags, capacity profiles, wire schemas,
persisted records, and exported SDK builds as contracts. Before changing one,
identify affected callers and compatibility/recovery requirements. Never edit
generated codecs directly. A cleanup must not silently change defaults, capacity,
wire representation, durability, timing, or failure ordering.

Do not add a dependency, template hierarchy, factory, global registry, generic
event bus, or asynchronous framework when a concrete component or existing seam
is enough. Do not remove safety checks for aesthetics or guessed performance.
Optimize measured costs; report image, RAM, stack, storage, and latency effects
when the change touches those resources.

## Verification and review

Use the smallest relevant existing test target for iteration, then the affected
host and target gates listed in `AGENTS.md`. For broad host validation:

```sh
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
git diff --check
```

- Add a focused regression for nontrivial changed behavior using the existing
  test harness. Exercise the real implementation, including relevant failure
  paths; do not create a parallel simplified implementation for tests.
- Include boundary cases: empty, exact maximum, one over maximum, invalid input,
  arithmetic boundaries, and repeated lifecycle transitions as applicable.
- Keep tests deterministic: use existing fake clocks, counting storage, and
  controlled callbacks rather than sleeps or physical dependencies for host tests.
- Fix new warnings in touched code; do not globally enable noisy flags or rewrite
  third-party code to satisfy an unrelated cleanup. Use available targeted static
  analysis and ASan/UBSan when appropriate; do not claim tools ran unless they did.
- Verify exported headers/SDK and affected ESP32/ESP8266 profiles for API/layout
  changes. Host success is not proof of MCU resources or hardware acceptance.
- Review the final diff for unnecessary scope, stale comments, duplicated rules,
  hidden allocations/I/O, and altered failure semantics. Report remaining risks
  and exact unavailable checks instead of overstating completion.

For a review-only request, do not edit files. Lead with concrete findings ordered
by impact, each with `path:line`, trigger, consequence, and a minimal correction.
If no defects are found, say so and identify material validation gaps. Style
preferences without a maintenance or correctness consequence are not defects.
