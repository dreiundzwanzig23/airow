# DEPENDENCY_RULES.md

This document defines allowed include-direction dependencies.

## Allowed dependencies
- `include -> include`
- `src/lib -> include, src/lib`
- `src/app -> include, src/lib`
- `tests/unit -> include, src/lib, tests/unit`
- `tests/integration -> include, src/lib, tests/integration`
- `tests/system -> include, src/lib, tests/system`

## Forbidden examples
- `src/lib -> src/app`
- `include -> src/*`
- `src/app -> tests/*`

## Allowed component dependencies

These rules apply only to files that live under component-prefixed paths such
as `include/project/mechanics/**` or `src/lib/mechanics/**`.

- `core -> core`
- `configuration -> core, configuration`
- `mechanics -> core, mechanics`
- `hydro -> core, mechanics, hydro`
- `aero -> core, mechanics, aero`
- `control -> core, mechanics, control`
- `output -> core, configuration, mechanics, hydro, aero, control, orchestrator, output`
- `orchestrator -> core, configuration, mechanics, hydro, aero, control, output, orchestrator`
- `calibration -> core, mechanics, hydro, aero, output, calibration`

## Forbidden component examples
- `hydro -> aero`
- `aero -> hydro`
- `mechanics -> orchestrator`
- `hydro -> orchestrator`
- `aero -> orchestrator`
- `control -> orchestrator`
- `calibration -> orchestrator`

## Architecture guardrails

- Cross-component access must go through `include/project/**` public headers.
  Direct includes into another component's `src/lib/**` implementation tree are
  forbidden.
- The actual realized component include graph must remain acyclic even when the
  broader allowed-dependency matrix would permit a bidirectional relationship.
- A realized component with files under `include/project/<component>` or
  `src/lib/<component>` must have:
  - an owning `A-*` architecture item,
  - at least one public header under `include/project/<component>`,
  - at least one non-aux test that `@verifies` the owning `A-*` item.
- An `IN_PROGRESS` or `DONE` component-backed `A-*` item must have matching
  component files in code.

## Validation
Run:
```bash
./scripts/depcheck.sh
```
