# ROADMAP.md

## Near-term
- Harden architecture allocation policy, trace guardrails, and simulator
  process docs around the seeded subsystem ownership model.
- Replace the bootstrap sample code with the first headless simulator slice.
- Preserve strict TDD, traceability, and deterministic local gates.

## Backlog
- Add deterministic baseline scenarios for passive float, tow, calm-water
  stroke, headwind, and crosswind runs.
- Add real component-prefixed code paths so component-level depcheck rules
  become active in production code.
- Add simulator-specific auxiliary and regression lanes only when they improve
  local verification without polluting the baseline runtime path.
