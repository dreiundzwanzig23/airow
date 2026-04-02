# ARCHITECTURE_HEALTH.md

This document records durable structural risks and pressure points that should
not be lost in short-lived handoff notes.

## Overlapping Responsibilities
- `A-002 Simulation Orchestrator` and `A-008 Scenario Harness and Validation`
  both touch run sequencing. Keep acceptance logic and scenario baselines in
  `A-008`; keep lifecycle and execution composition in `A-002`.
- `A-001 Configuration and Validation` and `A-009 External Calibration
  Integration` both touch external artifact handling. Keep schema acceptance in
  `A-001`; keep artifact ingestion and provenance semantics in `A-009`.

## Fragile Temporary Seams
- The bootstrap `900`-series sample still exists and should not accumulate new
  behavior.
- Component-level dependency rules are defined ahead of code layout; they will
  only become active as component-prefixed files are added.

## Tolerated Duplication
- Scenario naming appears in both requirements and test strategy for now
  because the scenario harness does not yet exist in code.

## Pressure Points
- `A-003 Mechanics Subsystem` is likely to attract cross-cutting growth early.
- `A-007 Output and Diagnostics` risks turning into a dumping ground for
  unrelated metadata concerns if contracts are not kept narrow.
- `A-009 External Calibration Integration` must stay optional from the default
  runtime path.

## Avoid Growing Complexity Here
- Do not add simulator behavior to the bootstrap `string_utils` sample.
- Do not let `A-002` absorb subsystem-specific algorithms.
- Do not mix hydro and aero internals just because both are force providers.
