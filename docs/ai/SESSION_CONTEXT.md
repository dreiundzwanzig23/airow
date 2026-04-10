# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-06
- **Branch**: `First_implementations`
- **Current Objective**: Resume the post-`v0.1` roadmap after the landed
  observability and provider-selection slices: reduced-model fidelity
  expansion first, then external backend wiring, then calibration and
  time-varying environment work.

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
- The shared run path now validates a top-level `providers` block, constructs
  built-in reduced hull-resistance, blade-force, and aero providers from
  configuration when injected seams are absent, and emits structured
  per-role provider metadata with validity descriptors in machine-readable
  outputs.
- `v0.1` is closed at the requirement level, including `QT-019..QT-026` for
  the remaining cut-line closure items.
- `R-020` and `R-033` are now closed, so the roadmap moves from the landed
  observability and provider-selection slices into reduced-model fidelity
  expansion before external backend wiring and later calibration or
  time-varying environment work.
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
1. Extend `A-004` and `A-005` beyond the landed provider-selection slice with
   richer reduced hydro and steady-wind behavior while preserving the current
   config-driven provider catalog and structured output metadata.
2. Protect the new structured provider metadata and example-config provider
   selections as compatibility constraints while reduced-model fidelity grows.
3. Revisit external backend wiring for Chrono and SUNDIALS under `A-010` only
   after the provider-selection and fidelity slices stabilize.
