# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-26
- **Branch**: `new_roadmap`
- **Current Objective**: Build the first Phase 1 trust/capability visibility
  slices from the merged full-simulation roadmap.

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
  can generate an offline interactive report with 2D projections, playback
  controls, vector labels, linked plot cursors, and trust/unavailable-channel
  labels. Full 3D playback and richer exports remain open.
- `R-019` is temporarily review-flagged because P0 trace closure now excludes
  `Needs-Review: yes` draft backlog items until they are reviewed and
  allocated.
- `R-023` is review-flagged after removing the legacy constant-wind config
  field; current configs must use `wind_time_series` or `wind_profile`.
- `docs/process/ROADMAP_FULL_SIMULATION.md` is the active long-range roadmap.
- Active documentation has been compacted for agent context: `README.md`,
  `CHANGELOG.md`, the full-simulation roadmap, workflow/test/scenario docs,
  and generated `TRACEABILITY.md` now link to canonical detail instead of
  duplicating it. Use `python3 tools/tracecheck.py --json` for full trace data.
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
- Do not hand-expand generated trace markdown; keep full trace inspection on
  the JSON output path.

## Next Actions
1. Clear or carry forward the `R-019` and `R-023` reviews.
2. Continue `R-050` / `R-052` / `R-053` / `R-070` toward richer channel
   coverage, configurable reference-frame selection, and VTK/ParaView export.
3. Continue `R-071` Phase 1 with viewer entry pages and study-recommendation or
   optimization links once those surfaces exist; keep `R-071`
   `Needs-Review: yes`.
4. Allocate each selected packet in `docs/process/ARCHITECTURE.md` before
   adding failing tests.
5. Preserve current numerical behavior unless the selected requirement
   explicitly changes runtime physics.
