# HANDOFF.md

## Handoff Timestamp
- 2026-04-13

## What Changed In This Session
- Started Slice 3A on `A-010` by adding a built-in state-advancer catalog and
  validated `simulation.state_advancer` config handling under `A-001`.
- Extended the shared run path so built-in state-advancer construction now
  mirrors built-in provider construction: injected seams still take
  precedence, otherwise the orchestrator selects the configured built-in
  advancer.
- Landed new backend design items (`D-040`, `D-041`, `D-042`) covering the
  built-in advancer catalog, built-in advancer factory binding, and the first
  Chrono-backed rigid-body advancer path.
- Added a compile-time-guarded `chrono_rigidbody` state advancer that reuses
  the current public startup and snapshot contract while stepping hull motion
  through Chrono only when the build exposes Chrono support.
- Added new unit or integration evidence (`UT-132..UT-137`,
  `UT-138..UT-139`, `IT-016..IT-018`) for default advancer selection,
  unknown-id rejection, guarded Chrono rejection on non-Chrono builds,
  built-in catalog determinism, and config-driven built-in advancer
  selection on the shared run path.
- Added Chrono-specific passive-float and tow scenario tests
  (`QT-031`, `QT-032`) that stay skipped unless the build is Chrono-capable.
- Wired the repository CMake logic to Chrono 10's exported
  `Chrono::Chrono_core` target, updated the guarded Chrono advancer for the
  Chrono 10 vector and body APIs, and validated the Chrono-enabled build
  against `QT-031` and `QT-032` on a local `~/.local` Chrono install.

## Current Technical Posture
- `v0.1`, the observability slice, and the provider-selection slice remain
  closed on the main roadmap, Slice 2 remains underway on the existing hydro
  and steady-wind aero ids, and Slice 3 has now started through built-in
  backend selection.
- The shared run path still has two stable compatibility surfaces for later
  work: the human-readable run-analysis feature set and the structured
  provider-selection plus validity-metadata contract, now joined by the new
  built-in state-advancer selection contract.
- Chrono support remains optional, but the current environment now has a
  working Chrono-capable build path that auto-detects a local `~/.local`
  install and passes the guarded Chrono scenario evidence.
- Evidence note: `rgr:red` came from new failing config, unit, integration,
  and guarded scenario tests around state-advancer selection plus the initial
  Chrono-disabled configure path; `rgr:green` added Chrono 10 target
  detection, Chrono 10 API compatibility in the guarded advancer, and passing
  Chrono scenario evidence; `rgr:refactor` was a no-op after the focused
  compatibility patch because the Chrono-specific logic remained localized to
  the existing backend-selection seam.

## Immediate Next Steps
1. Keep any further Slice 2 work on the existing built-in ids and preserve the
   landed `providers` config schema plus structured provider metadata.
2. Decide whether the next backend step is broader Chrono evidence or
   SUNDIALS-first wiring under the same `A-010` seam.
3. Re-open calibration and time-varying environment work only after backend
   direction and the reduced-model baselines are stable.
