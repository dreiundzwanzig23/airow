# ROADMAP.md

## Near-term
- Replace the bootstrap sample code with the first headless simulator slice.
- Land the first real subsystem-owned `D-*`, `UT-*`, `IT-*`, and `QT-*`
  evidence inside `A-001..A-010`.
- Implement the first runtime contracts around configuration validation,
  mechanics assembly, and numerical state advancement with explicit startup
  diagnostics and frame conventions.
- Add the first named runtime-backed scenario baselines for passive float, tow,
  calm-water stroke, headwind stroke, and crosswind stroke.
- Preserve strict TDD, traceability, and deterministic local gates.

## Backlog
- Add real component-prefixed code paths so component-level depcheck rules
  become active in production code.
- Add simulator-specific auxiliary and regression lanes only when they improve
  local verification without polluting the baseline runtime path.
