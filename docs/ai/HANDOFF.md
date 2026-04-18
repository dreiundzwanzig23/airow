# HANDOFF.md

## Handoff Timestamp
- 2026-04-18

## What Changed In This Session
- Closed Slice 2 on `A-004` and `A-005` by declaring the current reduced
  hydro and steady-wind aero built-in providers sufficient as the supported
  deterministic default-runtime baseline, strengthening the provider-validity
  contract, and removing stale roadmap or architecture wording that still
  implied another in-place fidelity follow-on under that slice.
- Closed Slice 3 on `A-010`: built-in state-advancer selection is now fully
  documented as a closed backend slice with required `sundials_ida` default
  runtime, explicit `deterministic_baseline` fallback, optional guarded
  `chrono_rigidbody` path, and structured backend-policy plus runtime
  solver-status metadata in JSON/HDF5 outputs.
- Completed the workflow-enforcement recovery slice on `External`.
- Validation summaries now preserve nested child exit codes, stale summary
  files are cleared before each run, and `test_aux.sh` includes a direct
  regression for that contract.
- The WSL crash reproducer is fixed: sub-epsilon SUNDIALS tail steps now still
  advance time, and the orchestration loop rejects non-advancing advancer
  results deterministically.
- RGR evidence enforcement is strict by default again, `verify.sh` now runs
  `test_tdd.sh` before the full gate, repo-wide test-quality linting is back
  in `test.sh`, legacy lint overrides were removed, and GoogleTest discovery
  now uses runtime registration so multiline tests cannot be skipped silently.
- Sanitized and GCC CTest presets now carry the sanitizer environment required
  by runtime GoogleTest discovery, so discovery and execution share the same
  contract.
- The former ambient `AIROW_SUNDIALS_TEST_FAULT` seam was replaced with an
  explicit compile-time-gated test hook used only by tests.
- Full required verification is green locally as of 2026-04-18, including
  `format.sh`, `lint.sh`, `build.sh`, `test_tdd.sh`, `test.sh`,
  `depcheck.sh`, and `python3 tools/tracecheck.py --write`.

## Current Technical Posture
- Workflow and validation policy are back in sync with automation.
- Slice 2 is no longer an active roadmap slice; future hydro or aero
  expansion must be scoped as a new packet instead of continuing the closed
  reduced-model baseline work implicitly.
- Slice 3 is no longer an active roadmap slice; future backend work should
  land as a new explicit `A-010` packet instead of reopening the closed
  SUNDIALS-first selection baseline implicitly.
- The former `QT-022` crash path is green under the enforced gates.
- Active docs now reflect strict-by-default enforcement, restored gate truth,
  and the closed Slice 3 backend-policy contract.

## Immediate Next Steps
1. Choose the next unfinished roadmap slice after the closed Slice 2 and Slice
   3 packets.
2. Keep future workflow changes coupled to tooling-contract coverage so policy
   softening cannot drift silently again.
