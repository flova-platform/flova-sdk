---
name: cpp-style
description: Apply Flova's modern C++11 style to hand-written SDK and firmware code. Use for readability reviews and local refactors of dense functions, callbacks, state transitions, or typed handles.
---

# Flova C++ style

Use this skill for the shape of the code. Follow [C++ code quality](../cpp-code-quality/SKILL.md)
for contracts and verification, and [C++ antipatterns](../cpp-antipatterns/SKILL.md)
for correctness hazards.

- Keep one-line bodies for simple accessors and forwarders. Give validation,
  I/O, state updates, and failure returns their own lines. Keep statements that
  form a single transition together so their execution order is visible.
- Prefer guard clauses to nested conditionals that select between errors and
  effects. Use braces for multi-statement branches and switch cases with locals.
  Preserve short-circuit evaluation and the precedence of failure results.
- Resolve a handle or state pointer once within an operation when its lifetime
  is stable. Give the local a role-based name such as `current` or `record`.
  Use `const` for read-only access and `auto` when the initializer makes the
  type clear. Keep sizes, numeric conversions, and ownership explicit.
- Prefer `nullptr`, `using` for new aliases, and `static_cast` for explicit
  standard conversions. Use C++11 initialization where it preserves the
  existing construction and representation. Leave published names and
  persisted record layouts intact.
- Prefer standard C++11 syntax to compiler extensions in ordinary application
  logic. For a nullable environment value, name the value and spell out the
  null fallback so an empty string retains its existing meaning.
- Keep the style diff local. Use existing public wrappers, bounded workspaces,
  and status types. Expand dense code where it clarifies an invariant. Keep
  generated and vendored sources outside hand-written cleanup.

In a typed datastream write, validate the value first, resolve the slot into
`current`, return the existing error for an invalid slot, then forward once to
the runtime. The sequence makes the error precedence and point of effect clear.
