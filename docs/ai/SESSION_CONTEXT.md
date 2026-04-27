# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-27
- **Branch**: `new_roadmap`
- **Current Objective**: Continue additive visualization/trust surfaces without
  changing reduced-runtime physics.

## Current State
- The simulator remains a headless-first deterministic single-scull runtime
  with active `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`,
  `A-009`, and `A-010` contracts.
- Closed baseline packets remain the reference floor: reduced hydro/aero,
  preferred backend, calibration, time-varying wind, batch/truth-model
  guardrails, budgets, actuation, rower coupling, and propulsion metrics.
- `R-050..R-071` are now merged into the canonical requirements as the
  full-simulation extension backlog.
- The first `R-071` provider-capability metadata slice is in place:
  built-in provider catalog entries declare support status, fidelity level,
  validation status, and a plain-language capability summary, and JSON/HDF5 run
  outputs propagate those declarations additively under provider metadata.
- The Phase 1 trust-envelope/report slice is also in place: `RunMetadata`,
  JSON summaries, HDF5 metadata, and compact/full reports now expose a derived
  trust envelope, and `docs/process/CAPABILITY_MATRIX.md` documents the public
  capability matrix. `R-035`, `R-049`, and `R-071` remain open.
- The first `R-050` / `R-052` / `R-053` / `R-070` visualization path is in
  place: `output.visualization_path` emits `airow.visualization.v1`, malformed
  artifacts are rejected, examples emit the artifact, and `tools/run_analysis.py`
  can generate an offline interactive report with 2D top/side/end projections,
  projection and frame controls, vector toggles, linked plot cursors,
  plot-click seeking, broader selectable plot coverage, trust labels,
  unavailable-channel disabling, derived event markers for peaks, zero
  crossings, stroke boundaries, and trust warnings, report-visible moment
  vector provenance, mirrored yaw-moment evidence, and
  `metrics.json.interactive_controls` metadata. Full 3D playback, transformed
  local-frame vectors, and richer exports remain open.
- `R-019` and `R-023` review flags are cleared as of 2026-04-27 after
  `tools/tracecheck.py --json`, `QT-025`, and `QT-039` confirmed current trace
  and wind-contract evidence.
- `docs/process/ROADMAP_FULL_SIMULATION.md` is the active long-range roadmap.
- Active docs stay compact; use `python3 tools/tracecheck.py --json` for full
  trace data instead of hand-expanding generated `TRACEABILITY.md`.
- Superseded planning files are archived under `docs/archive/` and
  `docs/ai/archive/`.

## Guardrails
- Keep the current reduced built-in providers and scenario evidence framed as
  the validated baseline, not as calibrated full-simulation fidelity.
- Build observability, capability reporting, and artifact contracts before
  claiming deeper physics realism.
- Keep optional high-fidelity water workflows offline.
- Do not reintroduce the removed `environment.ambient_wind_world_mps` config
  field or `simulation.state_advancer` selector; represent constant wind as a
  single-sample series or equivalent constant profile.
- Do not use archived roadmaps or handoffs as active implementation guidance.
- Keep final responses for repository changes ending with a one-line
  `Commit message:` summary; do not hand-expand generated trace markdown.

## Next Actions
1. Continue `R-050` / `R-052` / `R-053` / `R-070` toward transformed
   local-frame channels, remaining interface/disturbance vector coverage, full
   3D playback linkage, and VTK/ParaView export.
2. Keep `R-052` and `R-053` `Needs-Review: yes` until true 3D playback and
   full linked timeline/channel coverage land.
3. Continue `R-071` Phase 1 with viewer entry pages and study/optimization
   links once those surfaces exist; keep `R-071` `Needs-Review: yes`.
4. Allocate each selected packet in `docs/process/ARCHITECTURE.md` before
   adding failing tests.
5. Preserve current numerical behavior unless the selected requirement changes runtime physics.
