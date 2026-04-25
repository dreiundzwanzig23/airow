# HANDOFF.md

## Handoff Timestamp
- 2026-04-24

## What Changed In This Session
- Merged the rebased full-simulation extension requirements `R-050..R-071`
  into `docs/process/REQUIREMENTS.md`.
- Made `docs/process/ROADMAP_FULL_SIMULATION.md` the active long-range roadmap
  for upcoming work.
- Archived superseded planning material under `docs/archive/` and preserved
  prior AI roadmap or handoff snapshots under `docs/ai/archive/`.
- Repointed active docs away from the old fidelity gap map so normal context
  exploration starts from the current full-simulation plan.
- Adjusted `R-019` and `QT-025` so P0 trace-coverage closure applies to
  reviewed requirements, not newly merged `Needs-Review: yes` backlog drafts.

## Current Technical Posture
- The current runtime remains the validated reduced single-scull baseline with
  reduced hydro and aero providers, preferred `chrono_rigidbody + sundials_ida`,
  scenario evidence, and optional offline truth-model export.
- `R-050..R-071` are `OPEN`, semantic, and review-flagged; implementation
  packets must perform architecture allocation before tests.
- `R-019` is also review-flagged after the traceability-scope clarification
  and should be cleared once that process wording is accepted.
- The old eight-milestone fidelity gap map is historical only.

## Immediate Next Steps
1. Clear or explicitly carry forward the `R-019` traceability-scope review.
2. Start from `docs/process/ROADMAP_FULL_SIMULATION.md` and select
   full-simulation `Needs-Review: yes` requirements next.
3. Allocate the first full-simulation slice to existing `A-*` owners before
   adding failing tests.
4. Keep archived documents excluded from routine context unless historical
   rationale is needed.
