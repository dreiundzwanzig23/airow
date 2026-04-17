# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-17
- **Branch**: `External`
- **Current Objective**: Stop-the-line recovery on workflow and validation
  consistency: restore truthful local gates, keep traceability aligned with the
  landed SUNDIALS path, and isolate the heavy full-gate sequence that can still
  crash the current WSL session.

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
- The shared run path now also validates `simulation.state_advancer`,
  defaults it to required `sundials_ida`, preserves
  `deterministic_baseline` as an explicit fallback, and rejects
  `chrono_rigidbody` deterministically when Chrono support is unavailable in
  the build.
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
- The first two `A-010` backend-selection packets are now landed through the
  built-in advancer catalog, compile-time-guarded Chrono rigid-body stepping,
  and a required SUNDIALS IDA path that now backs the default built-in
  advancer and passes passive-float and tow scenario evidence.
- `A-008` now explicitly owns the workflow-facing validation wrappers under
  `scripts/`; the recovery slice tightened that seam so nested validation
  failures preserve their real exit codes and stale summary JSON is cleared
  before a new run starts.
- The repo now has an auxiliary regression
  (`tools/test_validation_output.py`) that exercises the validation summary
  contract directly, and `D-043` once again has direct unit trace coverage
  through `UT-142`.
- The former WSL crash reproducer is now isolated and fixed: a positive
  sub-epsilon tail step in shared state advancement used to return success
  without advancing `time_s`, which let the outer run loop grow history
  without bound; the targeted `QT-022` system reproducer now passes quickly
  again.
- The required full gate still is not fully revalidated in this environment:
  targeted sanitized regressions, tracecheck, tooling contracts, lint, the
  fast TDD unit or integration or system or aux lanes, and the targeted
  `QT-022` reproducer are green, but the fast lane still stops at the
  existing total branch-coverage threshold and the long end-to-end
  `./scripts/test.sh` sequence has not yet been rerun after the crash fix.
- Deferred `Needs-Review: yes` P2 requirements (`R-021`, `R-022`, `R-023`,
  `R-025`) remain intentionally behind the near-term slices.
- Validation guardrails remain strict: full test gate includes sanitized and
  GCC lanes plus coverage gates, but `test.sh` now scopes test-quality lint to
  changed test files while standalone `lint_tests.sh` remains the repo-wide
  audit lane.
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
1. Re-run the remaining required gates (`./scripts/test.sh`,
   `./scripts/depcheck.sh`, and `python3 tools/tracecheck.py --write`) when a
   heavier verification pass is acceptable.
2. Decide whether to raise current branch coverage to threshold now or record
   it explicitly as separate follow-up debt, because it is the present fast
   lane blocker after the crash fix.
3. Return to roadmap work only after the validation lane is both truthful and
   reliable under the current local platform.
