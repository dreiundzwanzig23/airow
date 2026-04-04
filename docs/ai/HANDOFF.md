# HANDOFF.md

## Handoff Timestamp
- 2026-04-04

## What Changed In This Session
- Landed the broader `A-004` reduced-hydro slice with widened hydro force or
  moment samples, reduced passive-float restoring behavior, explicit
  blade-tip velocity or immersion state, and immersion-aware blade-water loads
  through the shared run path and machine-readable outputs.
- Added `D-028`, `D-029`, `UT-105..UT-112`, and `IT-011`; updated the passive
  float or stroke scenario artifacts and re-baselined the headwind envelope to
  the new reduced hydro runtime.
- Updated requirement state: `R-009`, `R-010`, and `R-011` -> `DONE`, and
  corrected stale `REQUIREMENTS.md` entries so `R-013` and `R-014` now match
  the already-landed wind slice.
- Landed the first `A-005` runtime slice with a new public aero baseline
  provider header in `include/project/aero/baseline_providers.hpp`, explicit
  ambient-wind configuration, deterministic apparent-wind computation, and
  structured aerodynamic force or moment propagation through the shared run
  path and machine-readable outputs.
- Added `scenarios/headwind_stroke.json` and `scenarios/crosswind_stroke.json`
  plus new evidence `D-026`, `D-027`, `UT-075..UT-104`, `IT-010`, and
  `QT-016..QT-018`.
- Updated requirement and architecture state: `R-013`, `R-014`, `R-018`, and
  `R-031` -> `DONE`; `A-005` -> `IN_PROGRESS`.
- Existing `A-004` hydro, `A-008` scenario-harness, workflow, and examples
  slices remain in place; this session built on them rather than changing
  their scope.

## Current Technical Posture
- `A-001`, `A-002`, `A-003`, `A-005`, `A-007`, `A-008`, and `A-010` are now
  active with public contracts in `include/project/**`.
- Scenario loading and acceptance evaluation are now centralized under the
  scenario harness seam, while scenario execution continues through the shared
  in-memory run path in `A-002`.
- Passive-float, tow, calm-water, headwind, and crosswind scenarios are now
  checked in and runtime-backed; hydro and aero behavior now include reduced
  hydrostatic restoring, structured blade immersion, hydro moments,
  apparent-wind, and aerodynamic-moment propagation.
- Full required local gates, depcheck, and tracecheck are green with the
  updated scenario slice.

## Immediate Next Steps
1. Tighten remaining `P0` requirement closure work around replay,
   diagnostics, units, startup validity, and traceability coverage.
2. Revisit concrete Chrono and SUNDIALS wiring only after the expanded
   baseline scenario evidence set stabilizes.
