# HANDOFF.md

## Handoff Timestamp
- 2026-04-03

## What Changed In This Session
- Landed the first mechanics-backed `A-003` and `A-010` slice behind the
  shared orchestrator seam with public contracts in
  `include/project/mechanics/state.hpp` and
  `include/project/numerics/state_advancement.hpp`.
- Expanded `A-001` configuration loading to validate deterministic startup
  inputs for hull mass properties, initial hull state, port and starboard oar
  geometry, seat travel, and prescribed stroke timing.
- Reworked `run_simulation` so successful runs now perform startup assembly,
  deterministic state advancement, hydro or aero provider sampling against
  mechanics-backed step context, and in-memory state history capture.
- Added startup-validity and runtime-state failure mapping so invalid startup
  conditions and non-finite runtime states fail with stable diagnostics.
- Expanded simulator trace evidence through `D-018`, `UT-022`, `IT-006`, and
  `QT-006`.
- Hardened anti-regression workflow enforcement by removing the
  `trace: trivial` escape hatch, splitting oversized unit files, adding
  default unit-file size/test-count checks, and enforcing changed-file
  coverage ratchets in both `test_tdd.sh` and `test.sh`.

## Current Technical Posture
- The repository now has a real simulator architecture ownership surface in
  `A-001..A-010`.
- `A-001` is now in progress with a concrete public configuration contract in
  `include/project/configuration/simulator_config.hpp`.
- `A-002` is now in progress with shared file-backed, in-memory, and CLI
  single-run execution paths.
- `A-003` is now in progress with boundary-visible hull, oar, seat, and stroke
  state contracts.
- `A-010` is now in progress with a stable state-advancer interface and a
  deterministic internal baseline implementation.
- The architecture-workflow hardening plan is complete and no longer blocked on
  placeholder code.
- The near-term milestone remains the deterministic headless single-run
  baseline rather than the broader research backlog.
- Default local validation remains strict on process drift as well as code
  drift.

## Immediate Next Steps
1. Start `A-007` output shaping so the new in-memory mechanics state can be
   surfaced through stable machine-readable runtime results.
2. Start `A-008` passive-float and tow scenario evidence now that deterministic
   mechanics startup and stepping exist.
3. Revisit concrete Chrono and SUNDIALS wiring only after the current seam and
   baseline scenario evidence stabilize.
