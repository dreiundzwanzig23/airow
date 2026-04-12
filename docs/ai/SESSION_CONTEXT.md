# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-11
- **Branch**: `First_implementations`
- **Current Objective**: Continue the post-`v0.1` roadmap: reduced-model
  fidelity first, external backend wiring second, calibration or
  time-varying environment later.

## Current State
- The project is now a single-scull headless-first simulator with a
  mechanics-backed run path, reduced hydro or aero models, and later
  calibration or truth-model workflows.
- `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, and `A-010`
  are active with public contracts under `include/project/**`.
- The baseline runtime includes deterministic startup and stepping, structured
  JSON/HDF5-capable outputs, checked-in passive or tow or calm-water or
  headwind or crosswind scenarios, and additive human-readable run analysis.
- The shared run path validates the top-level `providers` block, constructs
  built-in reduced hull-resistance or blade-force or aero providers when
  injected seams are absent, and emits structured per-role validity metadata.
- The landed `A-004` follow-on deepens the existing hydro ids in place with
  low-speed-damped hull drag, phase-shaped blade propulsion, backward-slip
  gating, exact catch or release zero-load tapering, and refreshed calm-water
  or headwind or crosswind scenario envelopes.
- The landed `A-005` follow-on deepens the existing
  `steady_wind_placeholder` id in place with stronger low-apparent-wind
  headwind drag, explicit lateral crosswind force, speed-shaped yaw, and
  refreshed headwind or crosswind scenario envelopes.
- `v0.1` is closed at the requirement level, including `QT-019..QT-026` for
  the remaining cut-line closure items.
- `R-020` and `R-033` are closed, so the roadmap now moves from landed
  observability or provider-selection work into reduced-model fidelity before
  external backend wiring and later calibration or time-varying environment.
- Chrono and SUNDIALS are explicitly part of the later external backend
  wiring slice under `A-010`, not part of hydro or aero provider selection.
- Deferred `Needs-Review: yes` P2 requirements (`R-021`, `R-022`, `R-023`,
  `R-025`) remain intentionally behind the near-term slices.
- Validation guardrails remain strict: full test gate includes sanitized and
  GCC lanes plus coverage gates, and depcheck enforces component boundaries.
- Workflow policy remains explicit red-green-refactor with a mandatory
  refactor phase and warning-mode marker checks in `test_tdd.sh` and
  `verify.sh`.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep `A-001` focused on configuration and validation.
- Keep `A-002` focused on orchestration, provider seams, and lifecycle
  behavior.
- Keep instruction coherence, depcheck, and traceability green.
- Keep `rgr:red`, `rgr:green`, and `rgr:refactor` evidence markers present for
  functional loops.

## Next Actions
1. Continue Slice 2 only with further reduced-model fidelity work that keeps
   the current built-in hydro and aero provider ids stable and preserves the
   structured provider metadata contract.
2. Revisit external backend wiring for Chrono and SUNDIALS under `A-010` now
   that the provider-selection slice and the first hydro or aero fidelity
   increments are both landed.
3. Re-open deferred calibration or time-varying environment work only after
   backend direction and reduced-model stability are clearer.
