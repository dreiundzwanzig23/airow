# HANDOFF.md

## Handoff Timestamp
- 2026-04-26

## What Changed In This Session
- Removed stale public config compatibility for
  `environment.ambient_wind_world_mps`; configs now use `wind_time_series` or
  `wind_profile` for ambient wind, including constant wind.
- Removed the legacy-specific `simulation.state_advancer` compatibility branch;
  the old selector is rejected as an unsupported field.
- Updated checked-in headwind and crosswind scenarios, tests, README,
  CHANGELOG, architecture notes, and scenario docs to the sampled wind
  contract.
- Centralized propulsion-metric provider support in the provider catalog so
  output and analysis code no longer duplicate the blade-provider id check.

## Current Technical Posture
- The reduced runtime remains numerically unchanged for equivalent sampled wind
  inputs.
- `R-023` is now `Needs-Review: yes` because its accepted config contract was
  semantically narrowed.
- `R-019` remains review-flagged after the traceability-scope clarification.
- `R-050..R-071` remain the next full-simulation extension backlog.

## Immediate Next Steps
1. Clear or explicitly carry forward the `R-019` and `R-023` reviews.
2. Start the first full-simulation packet from trust/capability and artifact
   foundations after architecture allocation.
3. Preserve current reduced-runtime behavior for sampled-wind equivalent
   configs unless a scoped physics requirement changes it.
