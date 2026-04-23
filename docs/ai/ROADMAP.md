# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- The closed post-`v0.1` baseline packets remain the reference floor:
  reduced hydro and steady-wind baseline providers, preferred
  `chrono_rigidbody + sundials_ida`, calibration-ingestion seed, time-varying
  wind, batch execution, truth-model export guardrail, performance budgets,
  and explicit single-scull scope control.
- The next major phase is a milestone-based fidelity program for
  single-scull hull-performance and stroke-dynamics studies.
- The remaining Milestone 2 proof is now closed: the shared
  `measurement_bundle` path has requirement-level evidence that parameter-set
  changes propagate into reported trim and/or performance metrics.
- `R-041` is now closed on the shared reduced-runtime path with propulsion
  metrics in outputs and one checked-in actuation-mode comparison artifact.
- The detailed gap map and milestone packet live in
  `docs/process/FIDELITY_GAP_MAP.md`.
## Next Fidelity Program
### Milestone 1 — Fidelity Target and Trust Envelope
- Owners: `A-007`, `A-008` with `A-001`, `A-009` support.
- Requirement packet: `R-035`, `R-049`.
- Outcome: explicit study-question scope, fidelity tiers, and trust reporting.
### Milestone 2 — Measurement and Calibration Foundation
- Owners: `A-001`, `A-009` with `A-007`, `A-008` support.
- Requirement packet: `R-036`, `R-037`, `R-038`, `R-048`.
- Outcome: versioned boat, rigging, athlete, and measured-trial contracts plus
  deterministic provenance and lineage handling.
### Milestone 3 — Low-Order Actuation and Rower Coupling
- Owners: `A-006`, `A-003` with `A-010`, `A-007` support.
- Requirement packet: `R-039`, `R-040`, `R-041`.
- Outcome: first non-kinematic actuation mode, low-order rower inertial
  coupling, and stroke-dynamics metrics.
### Milestone 4 — Calibrated Reduced Blade-Water Model
- Owners: `A-004`, `A-009` with `A-007`, `A-008`, `A-010` support.
- Requirement packet: `R-041`, `R-042`, `R-043`.
- Outcome: richer reduced blade behavior with calibrated artifacts,
  entry/extraction behavior, slip, and efficiency reporting.
### Milestone 5 — Calibrated Reduced Hull-Performance Model
- Owners: `A-004` with `A-003`, `A-009`, `A-007`, `A-008` support.
- Requirement packet: `R-038`, `R-044`, `R-046`.
- Outcome: calibrated reduced hull resistance, trim, and rigging or loading
  sensitivity path for hull-performance studies.
### Milestone 6 — Coupled Validation Scenarios for Real Rowing Questions
- Owners: `A-008`, `A-007` with `A-004`, `A-006`, `A-009` support.
- Requirement packet: `R-045`, `R-046`, `R-047`.
- Outcome: technique, hull or rigging, and measured-vs-sim comparison
  scenarios with deterministic delta metrics.
### Milestone 7 — Real Offline Truth-Model Loop
- Owners: `A-009` with `A-001`, `A-007`, `A-004` support.
- Requirement packet: `R-043`, `R-044`, `R-048`.
- Outcome: stable export, offline-study, fit, and re-import lineage for
  hydro-side reduced artifacts.
### Milestone 8 — Uncertainty and Trust Reporting
- Owners: `A-007`, `A-009` with `A-008`, `A-001` support.
- Requirement packet: `R-035`, `R-049`.
- Outcome: fit quality, validity-range, confidence, and out-of-envelope
  reporting in machine-readable and human-readable outputs.
## Recommended Near-Term Packet
- Start the `R-042` / `R-043` blade-behavior packet on top of the now-stable
  propulsion-metric and actuation seams.
- Extend `R-045` from the new shared-baseline comparison surface rather than
  creating a second technique-study harness.

## Explicit Deferrals
- `R-027`, `R-028`, and `R-029` remain deferred until the fidelity-foundation
  packets settle.
- Full OpenSim or musculoskeletal runtime work remains deferred.
- Flexible oars, broad disturbance expansion, non-single-scull support, and
  GUI-first work remain explicitly out of the near-term plan.
