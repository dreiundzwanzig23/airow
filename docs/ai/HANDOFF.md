# HANDOFF.md

## Handoff Timestamp
- 2026-04-23

## What Changed In This Session
- Closed `R-041` by extending the existing `A-007` output contracts with
  propulsion-metric availability, definitions, run-level slip/work/efficiency
  metrics, per-record support flags, HDF5 parity, and a human-readable
  analysis section.
- Added the first `D-058` shared-baseline technique-comparison surface under
  `A-008`, including `comparison_window`, named variants, batch-style config
  overrides, explicit `varied_parameters`, deterministic baseline-versus-
  variant delta reporting, and the checked-in
  `scenarios/technique_comparison_actuation_modes.json` artifact.
- Added new evidence coverage for the packet with `UT-348..UT-355`,
  `IT-030..IT-031`, and `QT-046`.
- Recorded the `R-041` close-out status and the early `R-045` groundwork in
  requirements, architecture, README, changelog, and AI context docs.

## Current Technical Posture
- The current runtime remains the validated reduced single-scull baseline with
  reduced hydro and aero providers, preferred `chrono_rigidbody + sundials_ida`,
  scenario evidence, and optional offline truth-model export.
- The first fidelity-foundation slice is now live: file-backed measurement or
  trial artifacts, low-order actuation, rower coupling, and lineage-bearing
  outputs are all routed through the shared deterministic runtime path.
- Milestone 2 is closed and Milestone 3 now has its `R-041` metrics packet
  closed on the shared runtime path.
- `R-045` is still open: the new comparison surface is limited to one
  actuation-mode study and does not yet cover acceleration deltas or a fuller
  technique-scenario catalog.
- Full biomechanics, flexible oars, broad disturbance modeling, non-single-
  scull support, and GUI-first work remain deferred.

## Immediate Next Steps
1. Start the `R-042` / `R-043` blade-behavior packet on top of the new stable
   propulsion-metric names and availability contract.
2. Extend `R-045` only when the shared-baseline comparison surface can absorb
   acceleration deltas and broader technique metadata without fragmenting the
   existing scenario API.
3. Keep calibrated reduced blade and hull providers deferred until the
   propulsion-metric and comparison-scenario seams remain stable under routine
   gate runs.
