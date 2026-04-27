# DECISIONS Archive — pre 2026-04-27

Archived ADR blocks moved from `docs/ai/DECISIONS.md`.
## ADR-2026-04-02-002
- **Date**: 2026-04-02
- **Context**: The simulator needs stable top-level ownership boundaries early
  so that mechanics, loads, calibration, and validation do not accrete in one
  undifferentiated code path.
- **Decision**:
  - seed the project with the following primary simulator component themes:
    `Configuration and Validation`, `Simulation Orchestrator`, `Mechanics`,
    `Hydro Runtime Models`, `Aero Runtime Models`, `Control and Stroke Input`,
    `Output and Diagnostics`, `Scenario Harness and Validation`, and
    `External Calibration Integration`,
  - prefer adding `D-*` realizations inside these themes over creating new
    top-level `A-*` items.
- **Consequences**:
  - new requirements have an obvious architectural home,
  - dependency rules and tests can be tightened around known subsystem seams,
  - major changes are more likely to evolve existing components than create
    narrow feature containers.
