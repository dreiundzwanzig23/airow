# HANDOFF.md

## Handoff Timestamp
- 2026-04-27

## What Changed In This Session
- Tightened TDD/unit-test standards: functional work repeats RGR per behavior
  slice, RGR markers are order-checked, and changed `tests/unit/**` blocks must
  declare one `@case` and one `@oracle`.
- Added offline inspection controls under existing visualization owners:
  projections, frame labels, vector toggles, linked plots, event markers,
  mirrored moment-vector evidence, and hull-body-frame vector variants.
- Added reduced ParaView/VTK export from validated `airow.visualization.v1`
  artifacts, including deterministic geometry/vector files and a metadata sidecar.
- Added the offline report capability/trust entry page and export usability
  slice: reports now expose run-level physics capability metadata before
  inspection controls, and reduced ParaView bundles include a deterministic
  loading guide linked from report/export metadata.
- Added `QT-052..QT-057` coverage for richer report controls, event/moment
  evidence, ParaView export, report-entry capability/trust metadata, and
  loading-guide linkage.
- Cleared `R-019` and `R-023` review flags after `tools/tracecheck.py --json`,
  `QT-025`, and `QT-039` confirmed trace and wind-contract evidence.
- Updated `ARCHITECTURE.md`, `REQUIREMENTS.md`,
  `VISUALIZATION_ARTIFACT.md`, `README.md`, `CHANGELOG.md`, and active AI
  context to reflect the new controls and remaining limitations.
- Restored the `karpathy-guidelines` skill filename/index and added the
  repository-wide final-response `Commit message:` standard.

## Current Technical Posture
- The reduced runtime remains numerically unchanged; this visualization-control
  slice is an additive output/reporting surface only.
- The process/tooling change affects changed-test lint and RGR evidence only;
  simulator runtime behavior and public C++ APIs are unchanged.
- `R-035`, `R-049`, and `R-071` remain `Status: OPEN`; `R-071` remains
  `Needs-Review: yes`.
- `R-050`, `R-052`, `R-053`, `R-056`, and `R-070` are `IN_PROGRESS` for the
  first visualization artifact, interactive-report slice, and reduced
  ParaView/VTK export. `R-052`, `R-053`, and `R-056` remain
  `Needs-Review: yes` with explicit follow-up notes for remaining
  interface/disturbance vector coverage, full linked channel coverage, true 3D
  timeline linkage, and reference-scenario ParaView loading review.
- Required gates pass as of this handoff; tracecheck still reports only the
  existing numbering-gap warnings.

## Immediate Next Steps
1. Continue `R-050` / `R-052` / `R-053` / `R-056` / `R-070` with remaining
   vector coverage, true 3D playback linkage, and reference-scenario ParaView
   loading review.
2. Keep `R-052` and `R-053` review-flagged until true 3D playback and full
   linked timeline/channel coverage are implemented.
3. Continue `R-071` with study-recommendation or optimization links and
   broader viewer explanation coverage once those surfaces exist.
4. Preserve current reduced-runtime behavior unless a scoped physics requirement changes it.
