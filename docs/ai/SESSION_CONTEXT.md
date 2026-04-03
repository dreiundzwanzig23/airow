# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-03
- **Branch**: `main`
- **Current Objective**: Extend the first landed simulator-facing
  configuration slice into executable single-run orchestration while preserving
  the hardened architecture-first workflow and explicit solver or
  state-convention contracts.

## Current State
- The project direction is now defined around a single-scull rowing simulator
  with a headless-first runtime, reduced hydro and aero models, and later
  calibration/truth-model workflows.
- The real simulator architecture namespace now belongs to `A-001..A-010`,
  while bootstrap placeholder artifacts are reserved for the `900` series.
- The architecture-workflow hardening effort is complete and the repository is
  ready to move on to the first simulator-facing implementation slice.
- `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md` record the
  approved technology and architectural direction.
- `docs/process/STATE_CONVENTIONS.md` now defines the baseline world-frame,
  sign, and orientation conventions for simulator boundary contracts.
- `A-010 Numerical Integration and State Advancement` now owns consistent
  initialization, solver abstraction, and solver-facing diagnostics.
- `docs/ai/ROADMAP.md` now defines `v0.1` as a deterministic single-run
  baseline and moves calibration ingestion, time-varying wind, batch sweeps,
  low-order balance control, flexible oars, and disturbance inputs beyond the
  steady baseline out of that cut line.
- The repository now includes the first simulator-facing implementation slice:
  deterministic JSON loading and validation for `R-001` in the `configuration`
  component.
- Bootstrap placeholder code and `900`-series evidence have been removed from
  the compiled code path.
- Validation scripts emit compact logs and JSON summaries through a shared
  wrapper.
- Traceability supports auxiliary tests and stronger evidence validation.
- Test strategy guidance now explicitly distinguishes subsystem-contract
  `IT-*`, scenario-oriented `QT-*`, and characterization coverage for major
  changes.
- Repo-local operational skills now live in `.agents/skills` with real skill
  names and `agents/openai.yaml` metadata instead of `template-*`
  placeholders.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep `A-001` focused on configuration and validation rather than letting
  runtime orchestration or mechanics details leak into it.
- Keep instruction coherence, depcheck, and traceability green.

## Next Actions
1. Implement `R-002` and `R-003` around the existing validated configuration
   contract.
2. Start expanding mechanics-facing configuration fields only when `A-003` and
   `A-010` implementation work needs them.
3. Clear or confirm the remaining `Needs-Review: yes` backlog items once the
   post-`v0.1` milestone ordering is accepted.
