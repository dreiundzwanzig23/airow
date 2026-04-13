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

## Current Technical Posture
- `v0.1`, the observability slice, and the provider-selection slice remain
  closed on the main roadmap, Slice 2 remains underway on the existing hydro
  and steady-wind aero ids, and Slice 3 has now started through built-in
  backend selection.
- The shared run path still has two stable compatibility surfaces for later
  work: the human-readable run-analysis feature set and the structured
  provider-selection plus validity-metadata contract, now joined by the new
  built-in state-advancer selection contract.
- Chrono support is optional and absent in the current environment, so the
  repository now proves deterministic rejection and guarded test skips on
  non-Chrono builds while leaving Chrono-enabled acceptance evidence pending.
- Evidence note: `rgr:red` came from new failing config, unit, integration,
  and guarded scenario tests around state-advancer selection; `rgr:green`
  added the built-in advancer catalog, config normalization, orchestrator
  selection, and Chrono guard path; `rgr:refactor` extracted the backend
  catalog and shared baseline-startup helper to keep backend wiring localized.

## Immediate Next Steps
1. Validate the landed `chrono_rigidbody` path on a Chrono-capable build
   against passive-float and tow scenarios.
2. Keep any further Slice 2 work on the existing built-in ids and preserve the
   landed `providers` config schema plus structured provider metadata.
3. Re-open calibration and time-varying environment work only after backend
   direction and the reduced-model baselines are stable.
