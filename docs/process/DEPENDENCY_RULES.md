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

## Validation
Run:
```bash
./scripts/depcheck.sh
```
