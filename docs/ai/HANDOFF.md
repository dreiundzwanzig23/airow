# HANDOFF.md

## Handoff Timestamp
- 2026-04-19

## What Changed In This Session
- Closed Slice 4A as the first `A-009` packet.
- Public config now accepts an optional `artifacts.calibration.path` reference
  for file-backed imported calibration artifacts on the shared runtime path.
- Added a new explicit built-in aero provider id,
  `steady_wind_calibrated`, which loads imported steady-wind drag and yaw
  coefficients from a validated artifact without changing the existing
  `steady_wind_placeholder` default-runtime baseline id.
- Imported calibration artifacts now require
  `source_id`, `artifact_version`, `content_hash`, and `schema_id`, and the
  shared run path rejects malformed or partially specified artifacts before
  runtime stepping.
- JSON and HDF5 summaries now record `metadata.external_artifacts` provenance
  for imported calibration artifacts used during a run.
- Added direct evidence for the new path across unit, integration, and system
  lanes; `R-021` and `R-022` are now `DONE`, while `A-009` remains
  `IN_PROGRESS` for richer future schemas and consumers.
- Full required verification is green locally as of 2026-04-19, including
  `format.sh`, `lint.sh`, `build.sh`, `test_tdd.sh`, `test.sh`,
  `depcheck.sh`, and `python3 tools/tracecheck.py --write`.

## Current Technical Posture
- Slice 4A is no longer an active roadmap slice; future calibration work
  should land only as a new explicit `A-009` packet instead of broadening the
  first calibrated aero import path implicitly.
- Closed Slice 2 and Slice 3 packets remain the stable baseline underneath the
  new calibrated-artifact path.

## Immediate Next Steps
1. Start Slice 4B only through a clean time-varying wind packet that preserves
   parity with the closed steady-wind and calibrated-artifact paths.
2. Keep future calibration growth under explicit `A-009` packets so schema or
   consumer expansion does not erode the closed Slice 4A contract.
