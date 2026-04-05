# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-05
- **Branch**: `First_implementations`
- **Current Objective**: Advance post-`v0.1` reduced-model fidelity and
  provider-selection readiness now that the milestone cut line is complete.

## Current State
- The project direction is now defined around a single-scull rowing simulator
  with a headless-first runtime, reduced hydro and aero models, and later
  calibration/truth-model workflows.
- `A-001`, `A-002`, `A-003`, `A-005`, `A-007`, `A-008`, and `A-010` are now
  active with public contracts under `include/project/**`.
- The run path is mechanics-backed and includes deterministic startup and
  stepping, structured JSON/HDF5-capable output contracts, and stable runtime
  diagnostics.
- `A-004`, `A-005`, and `A-008` now expose public hydro, aero, and
  scenario-harness seams with checked-in passive, tow, calm-water, headwind,
  and crosswind artifacts plus structured blade-load, apparent-wind,
  aerodynamic-moment, hydro-moment, and blade-immersion propagation.
- `R-009`, `R-010`, and `R-011` are now `DONE` with deterministic reduced
  hydrostatic restoring, hull-drag, and immersion-aware blade-water behavior
  under `A-004`, while richer multi-axis hydro fidelity remains open there.
- `R-013`, `R-014`, `R-018`, and `R-031` are now `DONE` with runtime-backed
  steady-wind evidence, checked-in headwind/crosswind scenario artifacts, and
  frame-aware output coverage.
- `R-005`, `R-006`, `R-007`, `R-008`, `R-016`, `R-017`, `R-019`, and `R-032`
  are now `DONE` with new requirement-level `QT-019..QT-026` evidence, so the
  `v0.1` cut line is fully closed at the requirement level.
- The repo now has a tracked `examples/` surface with direct CLI configs for
  passive float, tow test, and calm-water stroke plus a helper script that
  keeps output paths stable under `examples/output/`.
- `tools/tracecheck.py --json` is now machine-consumable without trailing text,
  which lets system-level traceability checks consume repo state directly.
- The shared runtime path now rejects non-finite boundary-visible oar
  blade-tip velocity or immersion state instead of allowing those invalid
  samples to pass through `state_advancement`.
- Deferred `Needs-Review: yes` P2 requirements (`R-021`, `R-022`, `R-023`,
  `R-025`) remain intentionally deferred behind the `v0.1` cut line and
  should not block near-term scenario delivery work.
- Validation guardrails remain strict: full test gate includes sanitized/GCC
  lanes plus coverage gates, and depcheck enforces component boundaries.
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
1. Extend `A-005` beyond the first steady-wind baseline while preserving the
   scenario-harness contract and the current frame-aware output surface.
2. Land runtime-selectable provider variants and validity-metadata propagation
   behind the existing hydro or aero seams without regressing the baseline
   deterministic path.
3. Revisit external backend wiring for Chrono and SUNDIALS only after the
   post-`v0.1` baseline scenario evidence set stabilizes.
