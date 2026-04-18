# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-18
- **Branch**: `External`
- **Current Objective**: Keep the newly closed Slice 3 composed-backend packet
  stable and choose the next unfinished roadmap slice.

## Current State
- The simulator remains a headless-first single-scull runtime with active
  `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, and `A-010`
  contracts under `include/project/**`.
- Slice 2 is closed: the current built-in reduced hydro and steady-wind aero
  providers are the supported deterministic default-runtime baseline.
- Slice 3 is closed as a composed backend packet:
  `chrono_rigidbody + sundials_ida` is the preferred supported runtime,
  `internal_baseline + sundials_ida` remains the supported fallback,
  `internal_baseline + deterministic_baseline` remains the explicit debug
  fallback, and `chrono_rigidbody + deterministic_baseline` is rejected.
- Run metadata now carries structured mechanics-backend and
  integration-backend policy metadata plus distinct startup and runtime
  solver-status fields in both JSON and HDF5 outputs.
- Standard non-libc++ builds now require Chrono through the repo-managed
  install prefix policy; libc++ sanitizer and coverage lanes remain the
  explicit no-Chrono exceptions.
- Chrono scenario evidence now covers passive-float, tow, calm-water stroke,
  headwind stroke, and crosswind stroke on the preferred runtime pair.
- The former WSL crash path is fixed: sub-epsilon advancement can no longer
  return a successful non-advancing state, and the ten-cycle replay system
  reproducer is green again.
- Validation wrappers under `A-008` now preserve nested child exit codes and
  clear stale summaries before a new run.
- Sanitized and GCC CTest presets now carry the leak-suppression environment
  required by runtime GoogleTest discovery, so test registration and test
  execution run under the same sanitizer contract.
- The SUNDIALS failure-injection seam is now an explicit test-only hook behind
  `PROJECT_TEST_HOOKS`; production runtime behavior no longer reads an ambient
  `AIROW_SUNDIALS_TEST_FAULT` environment variable.
- Workflow enforcement is strict again:
  - `check_rgr_evidence.sh` defaults to `strict`,
  - `verify.sh` runs `test_tdd.sh` before `test.sh`,
  - `test.sh` enforces repo-wide test-quality linting again,
  - legacy test-quality override tables are empty,
  - GoogleTest registration uses runtime discovery so valid tests cannot be
    skipped because of source formatting.
- Required gates are green locally:
  - verified on 2026-04-18,
  - `./scripts/format.sh`
  - `./scripts/lint.sh`
  - `./scripts/build.sh`
  - `./scripts/test_tdd.sh`
  - `./scripts/test.sh`
  - `./scripts/depcheck.sh`
  - `python3 tools/tracecheck.py --write`

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep workflow or gate policy changes synchronized across scripts, tooling
  contracts, README/process docs, and AI context in the same slice.
- Keep `rgr:red`, `rgr:green`, and `rgr:refactor` evidence present for
  functional loops.
- Keep `A-001` focused on configuration or validation and `A-002` focused on
  orchestration or lifecycle behavior.

## Next Actions
1. Decide whether Slice 4 or a smaller cross-cutting backlog packet should be
   the next active roadmap item after the closed Slice 3 composed-backend
   work.
2. Treat any future gate softening, skipped-test path, or context drift as a
   stop-the-line defect rather than local convenience.
