# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-26
- **Branch**: `new_roadmap`
- **Current Objective**: Use the merged full-simulation roadmap and
  requirements as the active guide for upcoming work.

## Current State
- The simulator remains a headless-first deterministic single-scull runtime
  with active `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`,
  `A-009`, and `A-010` contracts.
- Closed baseline packets remain the reference floor: reduced hydro and
  steady-wind providers, preferred `chrono_rigidbody + sundials_ida`,
  calibration ingestion, time-varying wind, batch execution, truth-model
  export guardrail, performance budgets, actuation modes, rower coupling, and
  propulsion metrics.
- `R-050..R-071` are now merged into the canonical requirements as the
  full-simulation extension backlog.
- `R-019` is temporarily review-flagged because P0 trace closure now excludes
  `Needs-Review: yes` draft backlog items until they are reviewed and
  allocated.
- `R-023` is review-flagged after removing the legacy constant-wind config
  field; current configs must use `wind_time_series` or `wind_profile`.
- `docs/process/ROADMAP_FULL_SIMULATION.md` is the active long-range roadmap.
- Superseded planning files are archived under `docs/archive/` and
  `docs/ai/archive/`.

## Guardrails
- Keep the current reduced built-in providers and scenario evidence framed as
  the validated baseline, not as calibrated full-simulation fidelity.
- Build observability, capability reporting, and artifact contracts before
  claiming deeper physics realism.
- Keep optional high-fidelity water workflows offline and separate from the
  default runtime.
- Do not reintroduce the removed `environment.ambient_wind_world_mps` config
  field or `simulation.state_advancer` selector; represent constant wind as a
  single-sample series or equivalent constant profile.
- Do not use archived roadmaps or handoffs as active implementation guidance.

## Next Actions
1. Clear or carry forward the `R-019` and `R-023` reviews.
2. Select the first `Needs-Review: yes` full-simulation packet from
   `R-050..R-071`, starting with trust/capability and visualization artifact
   foundations.
3. Allocate the selected packet in `docs/process/ARCHITECTURE.md` before
   adding failing tests.
4. Preserve current numerical behavior unless the selected requirement
   explicitly changes runtime physics.
