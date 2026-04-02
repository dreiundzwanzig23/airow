# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-02
- **Branch**: `main`
- **Current Objective**: Keep the rowing simulator bootstrap docs, process
  guardrails, and active AI context coherent and green.

## Current State
- The project direction is now defined around a single-scull rowing simulator
  with a headless-first runtime, reduced hydro and aero models, and later
  calibration/truth-model workflows.
- The real simulator architecture namespace now belongs to `A-001..A-009`,
  while bootstrap placeholder artifacts are reserved for the `900` series.
- `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md` record the
  approved technology and architectural direction.
- The repository still carries the bootstrap `string_utils` sample code while
  the first simulator implementation slice is pending.
- Validation scripts emit compact logs and JSON summaries through a shared
  wrapper.
- Traceability supports auxiliary tests and stronger evidence validation.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Do not treat the placeholder sample code as the simulator architecture.
- Keep instruction coherence, depcheck, and traceability green.

## Next Actions
1. Replace the placeholder sample code with the first coherent simulator slice.
2. Start landing simulator code under the seeded subsystem ownership boundaries.
