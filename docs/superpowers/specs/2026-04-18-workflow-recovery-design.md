# Workflow Recovery Design

## Context

The `External` branch tightened workflow enforcement and test registration, but
the repository drifted into an inconsistent state:

- `./scripts/test_tdd.sh` and supporting tooling contracts are green.
- `./scripts/test.sh` fails in the sanitized lane during GoogleTest discovery.
- production numerics code reads `AIROW_SUNDIALS_TEST_FAULT`, which is a
  test-only seam leaking into normal runtime behavior.
- AI context files currently overstate verification status.

This slice restores truthful strict-process behavior without backing away from
runtime GoogleTest discovery or the SUNDIALS-backed advancer work.

## Affected Seams

- `A-008` validation/workflow wrappers and machine-readable gate behavior
- `A-010` state-advancer implementation and SUNDIALS failure mapping tests
- CMake/CTest registration and sanitized test-lane configuration
- release-facing/process-facing docs and AI continuity docs

## Goals

1. Make `./scripts/test.sh` genuinely pass again in the current environment.
2. Keep runtime GoogleTest discovery so multiline tests cannot be skipped.
3. Remove production dependence on `AIROW_SUNDIALS_TEST_FAULT`.
4. Add regression protection around the sanitized lane and workflow claims.
5. Update docs only from fresh verification evidence.

## Design

### Sanitized Lane

The sanitized failure is caused by `gtest_discover_tests(... DISCOVERY_MODE
PRE_TEST)` running inside the `ctest` process without the leak-suppression
environment currently attached only to discovered test properties. The fix is
to apply the same sanitizer environment to the sanitized `ctest` preset so
discovery and execution share the same runtime assumptions.

This preserves runtime GoogleTest discovery while restoring the sanitized lane.

### SUNDIALS Failure Mapping Tests

The SUNDIALS failure mapping coverage remains valuable, but the current
env-variable seam is not acceptable in normal production behavior. Replace it
with a compile-time-gated test hook:

- normal builds always behave as if no injected fault exists
- unit-test builds define `PROJECT_TEST_HOOKS`
- test-only declarations in `state_advancement.hpp` expose a small setter/reset
  API for fault mode control
- `state_advancement.cpp` reads the fault mode only when `PROJECT_TEST_HOOKS`
  is enabled

This keeps the tests explicit and local to the SUNDIALS advancer boundary.

### Documentation Truth

Workflow comments, README text, and AI continuity docs must describe the actual
verified state. “Full required verification is green” may only be restored
after the full required gates rerun successfully in this slice.

## Verification Plan

1. Targeted sanitized reproduction
2. Targeted unit test for the new SUNDIALS test hook path
3. `./scripts/test_tdd.sh`
4. `./scripts/test.sh`
5. `./scripts/depcheck.sh`
6. `python3 tools/tracecheck.py --write`

## Acceptance Criteria

- `./scripts/test.sh` passes locally.
- production code no longer reads `AIROW_SUNDIALS_TEST_FAULT`.
- stale “warning-only” comments are removed.
- tooling/contracts cover the sanitized preset expectation.
- docs and AI context reflect fresh verification results only.
