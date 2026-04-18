# HANDOFF.md

## Handoff Timestamp
- 2026-04-18

## What Changed In This Session
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
- The former `QT-022` crash path is green under the enforced gates.
- Active docs now reflect strict-by-default enforcement and the restored gate
  truth again.

## Immediate Next Steps
1. Return to roadmap requirement work under the restored strict workflow.
2. Keep future workflow changes coupled to tooling-contract coverage so policy
   softening cannot drift silently again.
