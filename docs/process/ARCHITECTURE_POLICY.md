# ARCHITECTURE_POLICY.md

This document defines the allocation policy for `A-*` architecture items.

## Core rules
- `A-*` items represent cohesive subsystem responsibilities, not requirement
  containers.
- New requirement work must first be evaluated against existing `A-*`
  ownership.
- A new `A-*` requires explicit cohesion justification in
  `Allocation Rationale`.
- One `A-*` may satisfy many `R-*`; this is expected.
- One `R-*` may allocate to multiple `A-*`; this is expected.
- Most implementation granularity belongs in `D-*`, not `A-*`.

## Allocation workflow
1. Identify candidate owning `A-*` items.
2. Prefer extending an existing subsystem when the concepts, interfaces, and
   invariants already align.
3. Create a new `A-*` only when the behavior would otherwise weaken cohesion or
   overload an existing boundary.
4. Record the architecture delta before TDD starts.

## Quality intent
- Optimize for cohesion, low coupling, stable interfaces, and test seams.
- Avoid accidental 1:1 `R -> A` mappings unless the architecture genuinely
  needs a new ownership boundary.
- Keep temporary bootstrap placeholders in reserved `900`-series IDs so they do
  not become the home for simulator work.
