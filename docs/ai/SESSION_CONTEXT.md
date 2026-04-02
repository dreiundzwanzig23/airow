# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-02
- **Branch**: `main`
- **Current Objective**: Start landing the first simulator-facing
  implementation slice inside the hardened architecture-first workflow with
  explicit solver and state-convention contracts.

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
- The repository still carries the bootstrap `string_utils` sample code while
  the first simulator implementation slice is pending.
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
- Do not treat the placeholder sample code as the simulator architecture.
- Keep instruction coherence, depcheck, and traceability green.

## Next Actions
1. Replace the placeholder sample code with the first coherent simulator slice.
2. Land the first real `D-*`, `UT-*`, `IT-*`, and `QT-*` evidence inside the
   seeded subsystem ownership boundaries, starting with configuration,
   mechanics, and numerical state-advancement seams.
