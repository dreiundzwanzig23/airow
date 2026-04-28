# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-28
- **Branch**: `new_roadmap`
- **Current Objective**: Continue additive artifact/report surfaces without changing reduced-runtime physics.

## Current State
- The simulator remains a headless-first deterministic single-scull runtime
  with active `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`,
  `A-009`, and `A-010`; closed baseline packets remain the reference floor.
- `R-050..R-071` are now merged into the canonical requirements as the
  full-simulation extension backlog.
- `R-071` provider-capability metadata, trust-envelope reports, and
  `docs/process/CAPABILITY_MATRIX.md` are in place; `R-035`, `R-049`, and
  `R-071` remain open.
- The first `R-050` / `R-052` / `R-053` / `R-054` / `R-070` visualization path is in
  place: `output.visualization_path` emits `airow.visualization.v1`, malformed
  artifacts are rejected, examples emit the artifact, and `tools/run_analysis.py`
  can generate an offline interactive report with 2D top/side/end projections,
  projection and frame controls, vector toggles, linked plot cursors,
  plot-click seeking, broader selectable plot coverage, trust labels,
  unavailable-channel disabling, derived event markers, report-visible moment
  vector provenance, mirrored yaw-moment evidence, hull-body-frame variants, and
  `metrics.json.interactive_controls` metadata. Offline reports also include
  `metrics.json.physics_capability_and_trust` metadata. The first `R-054`
  comparison slice consumes emitted artifacts through `tools/compare_runs.py`
  and writes reduced comparison metrics, SVGs, comparability flags, and HTML.
  The first `R-056` export slice writes deterministic reduced ParaView/VTK
  files plus metadata and a loading guide. Full 3D playback, unavailable
  interface/disturbance vectors, checked-in comparison examples,
  reference-scenario loading review, and richer exports remain open.
- `R-062` is DONE as of 2026-04-27. Successful runs now expose reduced energy
  accounting in summary/time-series JSON, optional HDF5, terminal reports,
  offline analysis bundles, and the checked-in technique-comparison scenario.
  Oar kinetic energy remains explicitly unavailable until oar mass/inertia
  modeling lands, and residuals are labeled unbounded reduced-model residuals.
- `R-019` and `R-023` review flags are cleared as of 2026-04-27.
- `docs/process/ROADMAP_FULL_SIMULATION.md` is the active long-range roadmap.
- Active docs stay compact; use `python3 tools/tracecheck.py --json` for full
  trace data.
- Process/tooling guardrail: classify each Codex task into Lane 0..5, default to
  the lightest sufficient lane, and update release/context docs only when
  explicitly triggered.
- Process/tooling guardrail: functional work now repeats red/green/refactor per
  observable behavior slice; changed `tests/unit/**` blocks must declare one
  `@case` and one `@oracle`; RGR evidence markers must be ordered.
- Superseded planning files are archived under `docs/archive/` and `docs/ai/archive/`.

## Guardrails
- Keep the current reduced built-in providers and scenario evidence framed as
  the validated baseline, not as calibrated full-simulation fidelity.
- Build observability, capability reporting, and artifact contracts before
  claiming deeper physics realism.
- Keep optional high-fidelity water workflows offline.
- Do not reintroduce removed wind/state-advancer config fields; represent
  constant wind as a single-sample series or equivalent constant profile.
- Do not use archived roadmaps or handoffs as active implementation guidance.
- Keep final responses for repository changes ending with a one-line
  `Commit message:` summary; do not hand-expand generated trace markdown.
- Keep changed unit tests focused on one observable behavior and let the
  changed-test lint lane enforce `@case` / `@oracle` metadata instead of
  mass-rewriting untouched legacy tests.

## Next Actions
1. Continue visualization/comparison/export work toward interface vectors, true
   3D playback linkage, checked-in examples, and ParaView loading review.
2. Continue `R-071` Phase 1 with study/optimization links and broader viewer
   explanation coverage; keep `R-071` `Needs-Review: yes`.
3. Preserve current numerical behavior unless selected work changes runtime physics.
