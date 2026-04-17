# HANDOFF.md

## Handoff Timestamp
- 2026-04-15

## What Changed In This Session
- Continued Slice 3 on `A-010` by wiring required SUNDIALS IDA support into
  the repo build and making `sundials_ida` the default built-in
  state-advancer selection under `A-001`.
- Added a new backend design item (`D-043`) for the SUNDIALS-backed
  rigid-body advancer while preserving the existing built-in advancer
  catalog/factory seam (`D-040`, `D-041`) and the optional Chrono path
  (`D-042`).
- Refactored the segmented state-advancement loop so deterministic baseline,
  SUNDIALS, and Chrono all reuse the same phase-boundary splitting, prescribed
  seat/oar updates, and post-step finite-state checks.
- Added SUNDIALS-specific unit and scenario evidence
  (`UT-140`, `UT-141`, `QT-033`, `QT-034`) and updated the default-path tests
  to expect `sundials_ida` metadata and startup status.
- Updated `./scripts/setup.sh`, README, changelog, and AI context docs so the
  repo now treats `libsundials-dev` as a required dependency while keeping
  `deterministic_baseline` as an explicit fallback and Chrono optional.

## Current Technical Posture
- `v0.1`, the observability slice, and the provider-selection slice remain
  closed on the main roadmap, Slice 2 remains underway on the existing hydro
  and steady-wind aero ids, and Slice 3 now has both Chrono and SUNDIALS
  backend packets landed behind built-in backend selection.
- The shared run path still has two stable compatibility surfaces for later
  work: the human-readable run-analysis feature set and the structured
  provider-selection plus validity-metadata contract, now joined by the new
  built-in state-advancer selection contract.
- SUNDIALS is now the required default build/runtime path for built-in state
  advancement, while Chrono remains optional and validated on local
  Chrono-capable builds.
- Evidence note: `rgr:red` came from the new default-path unit/system tests
  and the initial missing `sundials_ida` catalog/build wiring; `rgr:green`
  added required SUNDIALS build wiring, the new IDA advancer, updated default
  metadata/config behavior, and passing targeted unit/integration/system
  evidence; `rgr:refactor` extracted the shared segmented advancement loop so
  backend-specific stepping stayed localized.

## Immediate Next Steps
1. Keep any further Slice 2 work on the existing built-in ids and preserve the
   landed `providers` config schema plus structured provider metadata.
2. Decide whether the next backend step is broader Chrono-backed scenario
   evidence or deeper SUNDIALS-specific diagnostics/tolerance policy under the
   same `A-010` seam.
3. Re-open calibration and time-varying environment work only after backend
   direction and the reduced-model baselines are stable.
