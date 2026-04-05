# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- Current work should focus on post-`v0.1` fidelity, provider selection,
  validity metadata, and deferred backlog items rather than reopening the
  baseline milestone.

## v0.1 Cut Line

### In
- Replace the bootstrap sample code with the first headless simulator slice.
- Land the first real subsystem-owned `D-*`, `UT-*`, `IT-*`, and `QT-*`
  evidence inside `A-001..A-010`.
- Implement validated configuration, in-memory API and CLI execution, explicit
  frame conventions, and numerical startup-validity diagnostics.
- Implement the first stable 3D single-scull mechanics assembly for hull, oars,
  seat, and prescribed stroke input.
- Implement reduced baseline runtime models for hull flotation or drag,
  blade-water loading, and steady-wind aero.
- Add named runtime-backed baseline scenarios for passive float, tow,
  calm-water stroke, headwind stroke, and crosswind stroke.
- Preserve strict TDD, traceability, deterministic replay expectations on the
  same executable or platform, and machine-readable outputs.

### Out
- External calibration dataset ingestion and calibration provenance workflows.
- Time-varying wind and gust studies.
- Batch parameter sweeps.
- Low-order balance control.
- Flexible oars.
- Disturbance inputs beyond the steady baseline environment.
- Optional truth-model export or import work beyond protecting the separation
  boundary.

## Sequencing
1. Configuration, units, frame conventions, and startup validity.
2. Library API plus CLI orchestration for a single deterministic run.
3. Mechanics assembly and constrained initialization.
4. Passive float and tow scenarios with structured outputs.
5. Self-propelled calm-water stroke.
6. Steady headwind and crosswind scenarios.

## Backlog
- Add real component-prefixed code paths so component-level depcheck rules
  become active in production code.
- Land runtime-selectable provider variants only after the first baseline
  provider for each force family is stable.
- Re-open external calibration ingestion, provenance propagation, and
  validity-metadata expansion now that `v0.1` is complete.
- Add simulator-specific auxiliary and regression lanes only when they improve
  local verification without polluting the baseline runtime path.
