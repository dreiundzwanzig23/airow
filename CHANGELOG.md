# Changelog

## [Unreleased]
### Added
- Added optional `output.visualization_path` support for versioned
  `airow.visualization.v1` JSON artifacts, plus a deterministic validator and
  checked-in example configs that emit the artifact.
- Added optional interactive inspection reports in `tools/run_analysis.py` when
  a visualization artifact is provided, with offline playback controls, 2D
  projection canvases, linked plot cursors, trust labels, and deterministic
  malformed-artifact rejection.
- Added richer offline visualization report controls: top/side/end projection
  selection, frame labels, vector toggles with disabled unavailable channels,
  selectable linked plot channels, plot-click seeking, and
  `metrics.json.interactive_controls` metadata.
- Added offline inspection event markers for peak values, zero crossings,
  stroke boundaries, and trust warnings, plus report-visible force and moment
  vector provenance for mirrored direction checks.

### Changed
- Standardized final Codex responses after repository changes to include a
  one-line `Commit message:` summary.
- Restored the `karpathy-guidelines` repository skill index entry and expected
  `SKILL.md` filename so auxiliary skill lint can validate it.
- Cleared the review flags for `R-019` and `R-023` after confirming trace and
  wind-contract evidence remained intact.
- Refocused `README.md` as a concise simulator-user entry point, moving
  process, roadmap, gate, and implementation-inventory detail behind links to
  the canonical docs.
- Compacted active documentation for agentic work: generated traceability now
  writes a compact summary, historical changelog detail is archived, and
  roadmap/workflow/test/scenario docs avoid duplicating canonical sources.

## Archived Detail
- Pre-cleanup unreleased detail is archived at
  `docs/archive/CHANGELOG_pre_context_cleanup_2026-04-26.md`.
