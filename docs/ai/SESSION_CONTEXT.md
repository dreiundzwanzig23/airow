# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-27
- **Branch**: `new_roadmap`
- **Current Objective**: Continue additive visualization/trust surfaces without changing reduced-runtime physics.

## Current State
- The simulator remains a headless-first deterministic single-scull runtime
  with active `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`,
  `A-009`, and `A-010`; closed baseline packets remain the reference floor.
- `R-050..R-071` are now merged into the canonical requirements as the
  full-simulation extension backlog.
- The first `R-071` provider-capability metadata slice is in place and
  propagates support, fidelity, validation, and capability summaries into run
  outputs.
- The Phase 1 trust-envelope/report slice is in place, and
  `docs/process/CAPABILITY_MATRIX.md` documents the public capability matrix.
  `R-035`, `R-049`, and `R-071` remain open.
- The first `R-050` / `R-052` / `R-053` / `R-070` visualization path is in
  place: `output.visualization_path` emits `airow.visualization.v1`, malformed
  artifacts are rejected, examples emit the artifact, and `tools/run_analysis.py`
  can generate an offline interactive report with 2D top/side/end projections,
  projection and frame controls, vector toggles, linked plot cursors,
  plot-click seeking, broader selectable plot coverage, trust labels,
  unavailable-channel disabling, derived event markers for peaks, zero
  crossings, stroke boundaries, and trust warnings, report-visible moment
  vector provenance, mirrored yaw-moment evidence, hull-body-frame vector
  variants derived from emitted world-frame vectors, and
  `metrics.json.interactive_controls` metadata. The first `R-056` export slice
  converts validated visualization artifacts into deterministic reduced
  ParaView/VTK geometry/vector files plus a metadata sidecar. Full 3D playback,
  unavailable interface/disturbance vectors, ParaView loading guides, and richer
  exports remain open.
- `R-019` and `R-023` review flags are cleared as of 2026-04-27.
- `docs/process/ROADMAP_FULL_SIMULATION.md` is the active long-range roadmap.
- Active docs stay compact; use `python3 tools/tracecheck.py --json` for full
  trace data instead of hand-expanding generated `TRACEABILITY.md`.
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
1. Continue `R-050` / `R-052` / `R-053` / `R-056` / `R-070` toward remaining
   interface/disturbance vector coverage, full 3D playback linkage, and
   ParaView loading guidance.
2. Keep `R-052` and `R-053` `Needs-Review: yes` until true 3D playback and
   full linked timeline/channel coverage land.
3. Continue `R-071` Phase 1 with viewer entry pages and study/optimization
   links once those surfaces exist; keep `R-071` `Needs-Review: yes`.
4. Allocate each selected packet in `docs/process/ARCHITECTURE.md` before
   adding failing tests.
5. Preserve current numerical behavior unless selected work changes runtime physics.
