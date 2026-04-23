# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-23
- **Branch**: `External`
- **Current Objective**: Carry the next post-`R-041` fidelity packet without
  reopening the now-stable propulsion-metric or shared-baseline
  technique-comparison seams.

## Current State
- The simulator remains a headless-first single-scull runtime with active
  `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, `A-009`, and
  `A-010` contracts.
- Closed baseline packets remain the reference floor: reduced hydro and
  steady-wind providers, preferred `chrono_rigidbody + sundials_ida`,
  first calibration ingestion path, time-varying wind, batch execution,
  truth-model export guardrail, performance budgets, and explicit single-scull
  scope control.
- The first fidelity foundation slice is now implemented on the shared run
  path: `measurement_bundle.v1` and `measured_trial.v1` are separate
  file-backed contracts, imported boat or rigging or athlete identifiers flow
  into runtime metadata, and the truth-model handoff now carries the same
  reference-contract lineage.
- Low-order stroke actuation now supports `prescribed_kinematic`,
  `force_driven`, and `power_driven` modes, while bounded rower coupling adds
  center-of-mass and inertial-contribution channels without replacing the
  current reduced prescribed-kinematic baseline path.
- `R-041` is now closed: run analysis and JSON or HDF5 outputs report
  propulsion-metric support metadata, slip-speed channels, effective
  propulsive work, slip-loss work, and propulsion efficiency, while the
  checked-in `technique_comparison_actuation_modes.json` artifact proves one
  reusable actuation-mode comparison path through the scenario harness.
- The remaining `R-038` proof is now closed: checked-in integration and CLI
  evidence shows that changing valid measurement-bundle hull, rigging, and
  athlete parameter sets changes reported trim and/or performance metrics while
  preserving the active identifiers in machine-readable outputs.
- The mechanics-facing per-step runtime context no longer pulls in the full
  configuration schema; control now projects validated stroke actuation into a
  narrow runtime command packet, which keeps the depcheck boundary clean while
  still preserving `A-006` ownership.
- The current packet leaves calibrated reduced blade or hull providers,
  measured-vs-sim agreement metrics, and uncertainty reporting for later
  milestones; `R-042..R-049` remain open follow-on work, with `R-045` still
  open because only the first actuation-mode comparison surface has landed so
  far.

## Guardrails
- Keep the current reduced built-in providers and scenario evidence framed as
  the validated baseline, not as already calibrated research fidelity.
- Keep future calibrated reduced runtime work distinct from offline truth-model
  tooling and artifact-generation workflows.
- Keep `A-006` bounded to low-order actuation or control; do not let it become
  a full biomechanics container.
- Keep `A-010` responsible for constrained startup, stepping, and solver
  diagnostics rather than hydro or control semantics.

## Next Actions
1. Build the `R-042` / `R-043` blade-provider packet on top of the now-stable
   propulsion-metric contract rather than replacing the output names again.
2. Extend `R-045` only when the comparison surface can add acceleration and
   broader technique deltas without breaking the shared-baseline scenario API.
3. Keep calibrated reduced blade and hull providers deferred until the current
   artifact, actuation, and propulsion-metric contracts remain stable under
   routine gate runs.
