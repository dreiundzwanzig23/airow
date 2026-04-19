# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-19
- **Branch**: `External`
- **Current Objective**: Move the repo from an implicit post-`v0.1` fidelity
  backlog to an explicit, milestone-based plan for hull-performance and
  stroke-dynamics studies without destabilizing the validated reduced baseline.

## Current State
- The simulator remains a headless-first single-scull runtime with active
  `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, `A-009`, and
  `A-010` contracts.
- Closed baseline packets remain the reference floor: reduced hydro and
  steady-wind providers, preferred `chrono_rigidbody + sundials_ida`,
  first calibration ingestion path, time-varying wind, batch execution,
  truth-model export guardrail, performance budgets, and explicit single-scull
  scope control.
- `docs/process/FIDELITY_GAP_MAP.md` now defines the next major phase as eight
  milestones covering trust-envelope definition, measurement or calibration
  foundations, low-order actuation, calibrated reduced blade and hull models,
  study-facing validation scenarios, offline-loop concretization, and
  uncertainty reporting.
- `docs/process/REQUIREMENTS.md` now carries the new `R-035..R-049` packet,
  all still `OPEN`, so the planning state is explicit without claiming runtime
  closure prematurely.
- `docs/process/ARCHITECTURE.md` now allocates those milestones onto the
  existing subsystem owners, preserving `A-004` runtime hydro ownership,
  `A-009` external artifact ownership, and `A-010` numerics ownership as
  separate concerns.

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
1. Implement the Milestone 2 measurement or calibration foundation.
2. Add the first Milestone 3 low-order actuation mode and bounded rower
   coupling packet.
3. Defer calibrated blade and hull provider work until those boundary contracts
   and outputs are stable.
