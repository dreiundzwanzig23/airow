# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-06
- **Branch**: `First_implementations`
- **Current Objective**: Resume the post-`v0.1` roadmap after the landed
  observability slice: provider selection first, then fidelity expansion, then
  external backend wiring, then calibration and time-varying environment work.

## Current State
- The project is now a single-scull headless-first simulator with a
  mechanics-backed run path, reduced hydro and aero models, and later
  calibration or truth-model workflows.
- `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, and `A-010`
  are active with public contracts under `include/project/**`.
- The baseline runtime includes deterministic startup and stepping, structured
  JSON/HDF5-capable outputs, and checked-in passive, tow, calm-water,
  headwind, and crosswind scenario artifacts.
- The baseline output surface now also includes additive human-readable run
  analysis: derived summary metrics in JSON, optional compact or full CLI
  report modes, and an offline static HTML or SVG report tool for emitted JSON
  artifacts.
- `v0.1` is closed at the requirement level, including `QT-019..QT-026` for
  the remaining cut-line closure items.
- The roadmap is now staged around a completed observability slice followed by
  four post-`v0.1` slices: provider selection plus validity metadata,
  reduced-model fidelity expansion, external backend wiring, and only then
  calibration plus time-varying environment work.
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
1. Land runtime-selectable provider variants and validity-metadata propagation
   behind the existing hydro or aero seams without regressing the baseline
   deterministic path or the new run-analysis surface.
2. Extend `A-004` and `A-005` beyond the first reduced hydro and steady-wind
   baselines while preserving the scenario-harness contract and current
   frame-aware or analysis-facing output surface.
3. Revisit external backend wiring for Chrono and SUNDIALS under `A-010` only
   after the provider-selection and fidelity slices stabilize.
