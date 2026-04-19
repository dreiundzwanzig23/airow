# HANDOFF.md

## Handoff Timestamp
- 2026-04-19

## What Changed In This Session
- Added `docs/process/FIDELITY_GAP_MAP.md` as the focused planning document for
  the next major single-scull fidelity phase.
- Appended `R-035..R-049` to define the new backlog packet for trust-envelope
  reporting, measurement and calibration contracts, low-order actuation,
  calibrated reduced blade and hull models, study-facing comparison scenarios,
  offline-loop consistency, and uncertainty reporting.
- Updated `docs/process/ARCHITECTURE.md` to allocate that packet into the
  existing owners, keeping `A-004` vs `A-009` and physics vs numerics
  boundaries explicit.
- Replaced the roadmap’s generic later backlog with the eight-milestone
  fidelity program and synchronized README and changelog wording.

## Current Technical Posture
- The current runtime remains the validated reduced single-scull baseline with
  reduced hydro and aero providers, preferred `chrono_rigidbody + sundials_ida`,
  scenario evidence, and optional offline truth-model export.
- The next phase is now explicitly staged rather than implicit: first land the
  measurement or calibration foundation and the first low-order actuation slice
  before reopening blade or hull providers for calibrated reduced fidelity.
- Full biomechanics, flexible oars, broad disturbance modeling, non-single-
  scull support, and GUI-first work remain deferred.

## Immediate Next Steps
1. Implement Milestone 2 plus the first slice of Milestone 3 on `A-001`,
   `A-009`, `A-006`, and `A-003`.
2. Keep the current reduced built-in providers as the fallback baseline path
   while those new data and actuation seams settle.
3. Extend `A-009` through explicit artifact contracts and lineage rules rather
   than mixing offline-loop policy into runtime hydro ownership.
