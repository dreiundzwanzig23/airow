# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-19
- **Branch**: `External`
- **Current Objective**: Keep the newly closed Slice 4A, Slice 4B, and
  Slice 4C packets stable while preparing the next post-`v0.1`
  cross-cutting guardrails.

## Current State
- The simulator remains a headless-first single-scull runtime with active
  `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, `A-009`, and
  `A-010` contracts under `include/project/**`.
- Closed baseline packets remain stable:
  Slice 2 keeps the reduced hydro and steady-wind default-runtime providers,
  and Slice 3 keeps `chrono_rigidbody + sundials_ida` as the preferred
  supported runtime with bounded fallback pairs.
- Slice 4A is now closed as the first `A-009` packet:
  the shared run path can load one file-backed machine-readable calibration
  artifact, validate required provenance fields before stepping, construct the
  explicit `steady_wind_calibrated` aero provider from imported coefficients,
  and propagate used-artifact provenance into JSON and HDF5 outputs.
- Slice 4B is now closed on the shared runtime path:
  config accepts exactly one ambient-wind mode per run
  (`ambient_wind_world_mps`, `wind_time_series`, or `wind_profile`), sampled
  series use zero-order hold, keyframe profiles use deterministic linear
  interpolation, and outputs now record the effective ambient-wind vector per
  emitted time-series record.
- Slice 4C is now closed across `A-001`, `A-002`, and `A-007`:
  config can carry a top-level `batch` container with ordered cases and
  per-case override objects, the shared run path executes each case
  sequentially with isolated per-case result records, and one deterministic
  batch-summary artifact now captures ordered per-case ids, statuses,
  metrics, diagnostics, and artifact locations.
- `R-021` and `R-022` are now `DONE`; `A-009` remains `IN_PROGRESS` for richer
  schemas and future hydro-side consumers beyond the first calibrated aero
  path.
- `R-023` is now `DONE` with new `UT-245..UT-254`, `IT-024`, and `QT-039`
  evidence, while `A-009` remains `IN_PROGRESS` for richer future calibration
  packets.
- `R-025` is now `DONE` with `D-049`, `D-050`, `D-051`,
  `UT-261..UT-289`, `IT-025`, and `QT-040` evidence.
- Run metadata now carries structured mechanics-backend and
  integration-backend policy metadata plus distinct startup and runtime
  solver-status fields in both JSON and HDF5 outputs.
- Standard non-libc++ builds now require Chrono through the repo-managed
  install prefix policy; libc++ sanitizer and coverage lanes remain the
  explicit no-Chrono exceptions.
- Chrono scenario evidence now covers passive-float, tow, calm-water stroke,
  headwind stroke, and crosswind stroke on the preferred runtime pair.
- Workflow enforcement remains strict and the required gates are green locally
  on 2026-04-19, including `test_tdd.sh`, `test.sh`, `depcheck.sh`, and
  `tracecheck.py --write`.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep workflow or gate policy changes synchronized across scripts, README,
  process docs, and AI context in the same slice.
- Keep `rgr:red`, `rgr:green`, and `rgr:refactor` evidence present for
  functional loops.
- Keep `A-001` focused on configuration or validation and `A-002` focused on
  orchestration or lifecycle behavior.

## Next Actions
1. Keep the closed batch and time-varying-wind contracts stable while broader
   backlog items settle.
2. Extend `A-009` only through new explicit packets so richer schemas or
   hydro-side consumers do not blur the closed Slice 4A boundary.
3. Treat `R-024` and `R-026` as the next standing cross-cutting guardrails
   unless a smaller stop-the-line defect preempts them.
