# ARCHITECTURE_HEALTH.md

This document records durable structural risks and pressure points that should
not be lost in short-lived handoff notes.

## Overlapping Responsibilities
- `A-002 Simulation Orchestrator` and `A-008 Scenario Harness and Validation`
  both touch run sequencing. Keep acceptance logic and scenario baselines in
  `A-008`; keep lifecycle and execution composition in `A-002`.
- `A-003 Mechanics Subsystem` and `A-010 Numerical Integration and State
  Advancement` both touch startup behavior. Keep physical state ownership and
  constraint formulation in `A-003`; keep consistent initialization, solver
  policy, and termination diagnostics in `A-010`.
- `A-001 Configuration and Validation` and `A-009 External Calibration
  Integration` both touch external artifact handling. Keep schema acceptance in
  `A-001`; keep artifact ingestion and provenance semantics in `A-009`.

## Fragile Temporary Seams
- Component-level dependency rules are now active for the first
  component-prefixed production code paths and must stay aligned with the real
  include graph.
- Cross-component access should stay on public headers only; direct reaches into
  another component's `src/lib/**` tree now count as architecture drift.

## Tolerated Duplication
- Scenario naming appears in both requirements and test strategy for now
  because the scenario harness does not yet exist in code.

## Pressure Points
- `A-003 Mechanics Subsystem` is likely to attract cross-cutting growth early.
- `A-002 Simulation Orchestrator` now has a real execution seam and must not
  become the fallback home for mechanics state, provider algorithms, or output
  serialization details just because it touches all of them.
- As `A-007 Output and Diagnostics` comes online, keep the realized component
  include graph acyclic even if orchestration lifecycle events and output
  contracts need to meet at a shared seam.
- `A-007 Output and Diagnostics` risks turning into a dumping ground for
  unrelated metadata concerns if contracts are not kept narrow.
- `A-010 Numerical Integration and State Advancement` must stay solver-focused
  and avoid absorbing mechanics ownership or provider-specific load logic.
- `A-009 External Calibration Integration` must stay optional from the default
  runtime path.

## Avoid Growing Complexity Here
- Do not reintroduce bootstrap-only sample behavior under reserved `900`-series
  trace IDs now that the first simulator slice has landed.
- Do not let `A-002` absorb subsystem-specific algorithms.
- Do not duplicate configuration normalization logic outside `A-001`; reuse the
  public normalization contract when orchestration needs echoed metadata.
- Do not mix hydro and aero internals just because both are force providers.
