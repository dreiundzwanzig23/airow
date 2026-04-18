# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-18
- **Branch**: `External`
- **Current Objective**: Resume requirement-led product work under the
  restored strict workflow and freshly verified local gates.

## Current State
- The simulator remains a headless-first single-scull runtime with active
  `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, and `A-010`
  contracts under `include/project/**`.
- The default built-in state advancer is `sundials_ida`; `deterministic_baseline`
  remains an explicit fallback and `chrono_rigidbody` remains build-gated.
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
1. Return to requirement-led roadmap work under the restored strict workflow.
2. Treat any future gate softening, skipped-test path, or context drift as a
   stop-the-line defect rather than local convenience.
