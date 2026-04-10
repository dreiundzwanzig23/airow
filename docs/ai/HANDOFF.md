# HANDOFF.md

## Handoff Timestamp
- 2026-04-10

## What Changed In This Session
- Started Slice 2A on `A-004` by deepening the existing built-in hydro ids in
  place instead of adding new provider ids or changing config or output
  schema.
- Landed low-speed-damped built-in hull resistance and phase-shaped
  drive-phase blade propulsion under new hydro design items (`D-036`,
  `D-037`) with new unit evidence (`UT-127`, `UT-128`).
- Re-characterized the checked-in calm-water, headwind, and crosswind scenario
  envelopes to the richer deterministic hydro baselines while preserving the
  current provider-selection and structured metadata contracts.
- Continued Slice 2B on `A-005` by deepening the existing built-in
  `steady_wind_placeholder` aero id in place instead of adding a new provider
  id or changing config or output schema.
- Landed stronger low-apparent-wind headwind drag sensitivity, explicit
  lateral crosswind force, and speed-shaped yaw response under new aero design
  items (`D-038`, `D-039`) with new unit evidence (`UT-129`, `UT-130`).
- Re-characterized the checked-in headwind and crosswind scenario envelopes to
  the richer deterministic steady-wind aero baseline while preserving the
  current provider-selection and structured metadata contracts.

## Current Technical Posture
- `v0.1`, the observability slice, and the provider-selection slice remain
  closed on the main roadmap, and Slice 2 is now underway through both hydro
  and steady-wind aero fidelity increments on the existing built-in ids.
- The shared run path still has two stable compatibility surfaces for later
  work: the human-readable run-analysis feature set and the structured
  provider-selection plus validity-metadata contract.
- External solver adoption remains deferred until the dedicated backend slice.

## Immediate Next Steps
1. Keep any further Slice 2 work on the existing built-in ids and preserve the
   landed `providers` config schema plus structured provider metadata.
2. Revisit concrete Chrono and SUNDIALS wiring only in the later backend slice
   under `A-010`.
3. Re-open calibration and time-varying environment work only after backend
   direction and the reduced-model baselines are stable.
