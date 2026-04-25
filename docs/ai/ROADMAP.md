# ROADMAP.md

## Status
- The `v0.1` cut line is complete and remains the deterministic reduced
  single-scull reference floor.
- `R-001..R-049` remain the implemented or staged baseline, measurement,
  calibration, actuation, comparison, and trust-reporting backlog.
- `R-050..R-071` are now merged into `docs/process/REQUIREMENTS.md` as the
  active full-simulation extension backlog.
- The active long-range plan is `docs/process/ROADMAP_FULL_SIMULATION.md`.
- Superseded planning snapshots live under `docs/archive/` and
  `docs/ai/archive/`; do not use them as current implementation guidance.

## Next Full-Simulation Program
- Start with capability and trust visibility before deeper physics:
  `R-071`, `R-035`, and `R-049`.
- Build visualization-ready outputs before custom viewers or richer models:
  `R-050`, `R-052`, `R-062`, and `R-070`.
- Add interactive inspection after stable artifact contracts exist:
  `R-051`, `R-053`, `R-054`, `R-055`, and `R-056`.
- Keep geometry and physics expansion staged behind explicit validity and
  provenance: `R-057..R-069`.

## Recommended Near-Term Packet
- Review and allocate `R-071` plus the artifact contract slice (`R-050`,
  `R-052`, `R-062`, `R-070`) against existing `A-*` owners before writing
  tests.
- Preserve current reduced-runtime numerical behavior unless a scoped physics
  requirement explicitly changes it.
- Keep archived roadmaps out of normal context retrieval unless historical
  rationale is explicitly needed.
