# HANDOFF.md

## Handoff Timestamp
- 2026-04-27

## What Changed In This Session
- Added the next offline inspection slice under the existing visualization
  owners: top/side/end projection controls, frame selection labels, checkbox
  vector overlays derived from emitted artifact vectors, disabled unavailable
  channel controls, broader selectable plot coverage, plot-click seeking, and
  `metrics.json.interactive_controls` metadata.
- Added the follow-on event-marker and moment-vector slice: offline reports now
  expose peak, zero-crossing, stroke-boundary, and trust-warning markers,
  fill provenance for already-emitted force/moment vectors, and verify mirrored
  yaw-moment sign evidence.
- Added hull-body-frame visualization vector channels for already-emitted
  world-frame force, moment, wind, and rower-inertial vectors, with offline
  report toggles preserving unit, frame, and provenance labels.
- Added the first reduced ParaView/VTK export slice: validated
  `airow.visualization.v1` artifacts can produce deterministic geometry/vector
  VTK files plus a metadata sidecar, with optional `run_analysis.py` metadata.
- Added `QT-052`, `QT-053`, and `QT-054` coverage for the richer offline report
  controls and `QT-055` coverage for the ParaView bundle while keeping the
  `airow.visualization.v1` schema additive and unchanged.
- Cleared `R-019` and `R-023` review flags after `tools/tracecheck.py --json`,
  `QT-025`, and `QT-039` confirmed trace and wind-contract evidence.
- Updated `ARCHITECTURE.md`, `REQUIREMENTS.md`,
  `VISUALIZATION_ARTIFACT.md`, `README.md`, `CHANGELOG.md`, and active AI
  context to reflect the new controls and remaining limitations.
- Restored the existing `karpathy-guidelines` repository skill to the expected
  `SKILL.md` filename and load-on-demand indexes so auxiliary skill lint passes.
- Added a repository-wide final-response standard requiring a one-line
  `Commit message:` summary after repository changes.

## Current Technical Posture
- The reduced runtime remains numerically unchanged; this visualization-control
  slice is an additive output/reporting surface only.
- `R-035`, `R-049`, and `R-071` remain `Status: OPEN`; `R-071` remains
  `Needs-Review: yes`.
- `R-050`, `R-052`, `R-053`, `R-056`, and `R-070` are `IN_PROGRESS` for the
  first visualization artifact, interactive-report slice, and reduced
  ParaView/VTK export. `R-052`, `R-053`, and `R-056` remain
  `Needs-Review: yes` with explicit follow-up notes for remaining
  interface/disturbance vector coverage, full linked channel coverage, true 3D
  timeline linkage, and ParaView loading guidance.
- Required gates pass as of this handoff; tracecheck still reports only the
  existing numbering-gap warnings.

## Immediate Next Steps
1. Continue `R-050` / `R-052` / `R-053` / `R-056` / `R-070` with remaining
   interface/disturbance vector coverage, true 3D playback linkage, and
   ParaView loading guidance.
2. Keep `R-052` and `R-053` review-flagged until true 3D playback and full
   linked timeline/channel coverage are implemented.
3. Continue `R-071` with viewer entry pages and study-recommendation or
   optimization links once those surfaces exist.
4. Preserve current reduced-runtime behavior unless a scoped physics
   requirement changes it.
5. Use `python3 tools/tracecheck.py --json` when full trace mappings are
   needed.
