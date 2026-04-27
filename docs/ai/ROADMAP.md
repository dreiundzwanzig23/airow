# ROADMAP.md

## Status
- The `v0.1` cut line is complete and remains the deterministic reduced
  single-scull reference floor.
- `R-001..R-049` remain the implemented or staged baseline, measurement,
  calibration, actuation, comparison, and trust-reporting backlog.
- `R-050..R-071` are now merged into `docs/process/REQUIREMENTS.md` as the
  active full-simulation extension backlog.
- `R-050` and `R-070` are `IN_PROGRESS` for the first visualization-ready
  artifact and analysis-first CLI slice.
- The active long-range plan is `docs/process/ROADMAP_FULL_SIMULATION.md`.
- Superseded planning snapshots live under `docs/archive/` and
  `docs/ai/archive/`; do not use them as current implementation guidance.

## Next Full-Simulation Program
- Start with capability and trust visibility before deeper physics:
  `R-071`, `R-035`, and `R-049`.
- Build out the visualization-ready path from the current
  `airow.visualization.v1` artifact toward richer channels and export tooling:
  `R-050`, `R-052`, `R-062`, and `R-070`.
- Extend interactive inspection beyond the first offline 2D report:
  `R-051`, `R-053`, `R-054`, `R-055`, and `R-056`.
- Keep geometry and physics expansion staged behind explicit validity and
  provenance: `R-057..R-069`.

## Recommended Near-Term Packet
- Continue `R-050` / `R-052` / `R-053` / `R-070` from the current artifact and
  offline report slice into configurable frames, full vector toggles, broader
  linked plot coverage, and VTK/ParaView export.
- Preserve current reduced-runtime numerical behavior unless a scoped physics
  requirement explicitly changes it.
- Keep archived roadmaps out of normal context retrieval unless historical
  rationale is explicitly needed.
