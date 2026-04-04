# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-04
- **Branch**: `main`
- **Current Objective**: Extend the first named scenario evidence slice from
  passive-float and tow toward remaining `v0.1` baseline scenarios.

## Current State
- The project direction is now defined around a single-scull rowing simulator
  with a headless-first runtime, reduced hydro and aero models, and later
  calibration/truth-model workflows.
- `A-001`, `A-002`, `A-003`, `A-007`, `A-008`, and `A-010` are now active with
  public contracts under `include/project/**`.
- The run path is mechanics-backed and includes deterministic startup and
  stepping, structured JSON/HDF5-capable output contracts, and stable runtime
  diagnostics.
- `A-008` is now active with public scenario-harness contracts in
  `include/project/orchestrator/scenario_harness.hpp` and first checked-in
  scenario artifacts under `scenarios/passive_float.json` and
  `scenarios/tow_test.json`.
- `R-009`, `R-010`, and `R-018` are now `IN_PROGRESS` with deterministic
  scenario-harness evidence and placeholder hydro behavior; fuller reduced
  hydro model depth remains open in `A-004`.
- Deferred `Needs-Review: yes` P2 requirements (`R-021`, `R-022`, `R-023`,
  `R-025`) remain intentionally deferred behind the `v0.1` cut line and
  should not block near-term scenario delivery work.
- Validation guardrails remain strict: full test gate includes sanitized/GCC
  lanes plus coverage gates, and depcheck enforces component and interface
  discipline.
- Workflow policy now treats functional changes as explicit red-green-refactor
  loops with a mandatory refactor phase and warning-mode marker checks in
  `test_tdd.sh` and `verify.sh`.
- `docs/process/ARCHITECTURE.md` now includes a compact arc42-lite overview
  section so readers can see system context, building blocks, runtime flows,
  and implemented-versus-planned status before dropping into the `A-*`
  ownership catalog.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep `A-001` focused on configuration and validation rather than letting
  runtime orchestration or mechanics details leak into it.
- Keep `A-002` focused on orchestration, provider seams, and lifecycle
  behavior rather than letting mechanics or output serialization accumulate
  there.
- Keep instruction coherence, depcheck, and traceability green.
- Keep `rgr:red`, `rgr:green`, and `rgr:refactor` evidence markers present for
  functional loops.

## Next Actions
1. Expand `A-008` beyond passive/tow by landing calm-water stroke, headwind
   stroke, and crosswind stroke checked-in scenario artifacts plus envelopes.
2. Increase `A-004` hydro realism from placeholder behavior to richer reduced
   runtime models while preserving the scenario-harness contract.
3. Revisit external backend wiring for Chrono and SUNDIALS only after the
   baseline scenario evidence set stabilizes.
