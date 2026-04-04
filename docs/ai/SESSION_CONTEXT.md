# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-04
- **Branch**: `main`
- **Current Objective**: Extend the baseline scenario evidence slice from the
  new calm-water propulsion scenario toward the remaining wind-backed `v0.1`
  scenarios.

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
- `A-004` is now active with a public hydro baseline-provider header in
  `include/project/hydro/baseline_providers.hpp`, structured hull/blade load
  sampling through the shared run path, and first deterministic calm-water
  propulsion placeholder behavior.
- `R-009`, `R-010`, and `R-018` are now `IN_PROGRESS` with deterministic
  scenario-harness evidence and placeholder hydro behavior; fuller reduced
  hydro model depth remains open in `A-004`.
- `R-012` is now `DONE` with checked-in `scenarios/calm_water_stroke.json`,
  runtime-backed `QT-012`/`QT-013` evidence, and real blade-load plus stroke
  power propagation in machine-readable outputs.
- The repo now has a tracked `examples/` surface with direct CLI configs for
  passive float, tow test, and calm-water stroke plus a helper script that
  keeps output paths stable under `examples/output/`.
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
1. Expand `A-008` beyond calm-water by landing headwind stroke and crosswind
   stroke checked-in scenario artifacts plus envelopes.
2. Increase `A-004` and `A-005` realism from placeholder behavior to richer
   reduced hydro and aero runtime models while preserving the scenario-harness
   contract.
3. Revisit external backend wiring for Chrono and SUNDIALS only after the
   expanded baseline scenario evidence set stabilizes.
