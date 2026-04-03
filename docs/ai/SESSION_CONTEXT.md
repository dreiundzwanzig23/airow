# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-03
- **Branch**: `main`
- **Current Objective**: Move from completed `R-015` dual-format output
  contracts into first named scenario evidence work.

## Current State
- The project direction is now defined around a single-scull rowing simulator
  with a headless-first runtime, reduced hydro and aero models, and later
  calibration/truth-model workflows.
- The real simulator architecture namespace now belongs to `A-001..A-010`,
  while bootstrap placeholder artifacts are reserved for the `900` series.
- `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md` record the
  approved technology and architectural direction.
- `docs/process/STATE_CONVENTIONS.md` now defines the baseline world-frame,
  sign, and orientation conventions for simulator boundary contracts.
- `docs/ai/ROADMAP.md` now defines `v0.1` as a deterministic single-run
  baseline and moves calibration ingestion, time-varying wind, batch sweeps,
  low-order balance control, flexible oars, and disturbance inputs beyond the
  steady baseline out of that cut line.
- The current run path now includes the first mechanics-backed slice:
  validated hull, oar, seat, and stroke startup inputs; deterministic startup
  assembly; a stable internal state-advancer seam; and in-memory mechanics
  state history for hull, oars, seat, and stroke phase.
- The current run path now includes the completed `R-015` output contract in
  `A-007`: deterministic JSON summary and time-series artifacts, optional HDF5
  artifact emission selected by configuration, explicit unit/frame channel
  annotations, per-step load-history capture, and configuration-controlled
  high-frequency sampling.
- `A-002` is now active with a concrete public contract in `include/project/orchestrator/simulation_run.hpp` and `include/project/orchestrator/cli.hpp`.
- `A-003` and `A-010` are now active with concrete public contracts in
  `include/project/mechanics/state.hpp` and
  `include/project/numerics/state_advancement.hpp`.
- `A-007` is now active with public contracts in
  `include/project/output/run_result.hpp` and
  `include/project/output/run_output.hpp`.
- The validation baseline is now stricter at the tool level: stronger
  compiler-warning flags, enabled debug hardening, explicit CTest timeouts,
  an auxiliary tooling-contract check, public-header self-containment
  compilation, LLVM-native include-cleaner coverage in `clang-tidy`, and
  dedicated sanitized plus GCC lanes inside the full test gate.
- Architecture guardrails are now tightening beyond include allowlists toward
  public-header-only cross-component access, realized component cycle checks,
  and orphan detection tied to `A-*` ownership and non-aux evidence.
- Traceability supports auxiliary tests and stronger evidence validation.
- Trace and test enforcement now explicitly blocks `trace: trivial` bypasses,
  enforces helper-refinement shape (`@refines` exactly one parent for helper
  blocks), caps default unit test file size/case count, and runs changed-file
  coverage ratchets in both fast and full test lanes.
- Repo-local operational skills now live in `.agents/skills` with real names and `agents/openai.yaml` metadata instead of `template-*` placeholders.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep `A-001` focused on configuration and validation rather than letting
  runtime orchestration or mechanics details leak into it.
- Keep `A-002` focused on orchestration, provider seams, and lifecycle
  behavior rather than letting mechanics or output serialization accumulate
  there.
- Keep instruction coherence, depcheck, and traceability green.

## Next Actions
1. Start the first `A-008` scenario and validation evidence work for passive
   float and tow cases now that mechanics startup exists.
2. Revisit external backend wiring for Chrono and SUNDIALS only after the
   current seam and baseline scenario evidence stabilize.
3. Expand HDF5 channel-level schema depth after initial scenario harness
   evidence is stable.
