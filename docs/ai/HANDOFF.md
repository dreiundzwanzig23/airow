# HANDOFF.md

## Handoff Timestamp
- 2026-04-19

## What Changed In This Session
- Closed Slice 4C as the deterministic batch-orchestration packet.
- Public config can now carry a top-level `batch` object with ordered `cases`,
  optional batch-summary output selection, and per-case override objects
  layered onto one shared base run definition.
- The shared run path now executes each batch case sequentially through the
  existing single-run machinery, derives stable per-case config ids, isolates
  per-case output artifact paths, and preserves independent per-case success
  or failure records.
- Machine-readable output now includes one deterministic batch-summary
  artifact with ordered per-case ids, statuses, metrics, diagnostics, and
  artifact locations; the CLI auto-detects batch configs and rejects
  unsupported human-readable `--report` modes for batch jobs.
- Added direct batch evidence across unit, integration, and system lanes;
  `R-025` is now `DONE`.
- Closed Slice 4B as the time-varying ambient-wind packet.
- Public config now accepts exactly one ambient-wind mode per run:
  legacy `ambient_wind_world_mps`, replay-oriented `wind_time_series`, or
  authored `wind_profile`.
- The shared run path now samples time-varying ambient wind once per outer
  run step, using zero-order hold for sampled series and deterministic linear
  interpolation for keyframe profiles, then passes the sampled vector through
  the existing aero seam without changing the built-in provider ids.
- Constant series and constant profile inputs now match the legacy steady-wind
  path within the same deterministic load history.
- JSON and HDF5 time-series outputs now record the effective
  `ambient_wind_world_mps` channel alongside the existing apparent-wind and
  aerodynamic channels.
- Added the checked-in `scenarios/gust_headwind_stroke.json` regression
  artifact plus direct evidence for the new path across unit, integration, and
  system lanes; `R-023` is now `DONE`.

## Current Technical Posture
- Slice 4A, Slice 4B, and Slice 4C are no longer active roadmap slices;
  future calibration work should land only as a new explicit `A-009` packet,
  any further wind work should preserve the closed one-of wind-input schema
  plus the current aero-provider ids unless a later packet explicitly changes
  them, and future batch work should extend the sequential shared-run path
  rather than introducing parallel scheduling by accident.
- Closed Slice 2 and Slice 3 packets remain the stable baseline underneath the
  new calibrated-artifact and time-varying-wind paths.

## Immediate Next Steps
1. Keep the closed Slice 4B wind-input schema and Slice 4C batch contract
   stable while later backlog items settle.
2. Keep future calibration growth under explicit `A-009` packets so schema or
   consumer expansion does not erode the closed Slice 4A contract.
3. Treat `R-024` and `R-026` as the next standing backlog guardrails unless a
   smaller stop-the-line defect preempts them.
