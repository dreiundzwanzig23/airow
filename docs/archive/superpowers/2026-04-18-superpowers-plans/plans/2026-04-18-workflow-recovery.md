# Workflow Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore truthful strict-process workflow enforcement by fixing the sanitized full gate, isolating the SUNDIALS test seam from production behavior, and syncing docs with fresh verification evidence.

**Architecture:** Keep runtime GoogleTest discovery, but move sanitizer discovery configuration into the sanitized CTest preset so discovery and execution share the same environment. Replace the env-driven SUNDIALS fault seam with a compile-time-gated test hook available only to unit tests, then update process/docs from verified gate results.

**Tech Stack:** CMake presets, CTest, GoogleTest, C++20, existing repo workflow scripts and trace tooling

---

### Task 1: Fix The Sanitized Discovery Lane

**Files:**
- Modify: `CMakePresets.json`
- Modify: `tools/test_tooling_contracts.py`
- Verify: `scripts/test_sanitized.sh`

- [ ] Add sanitized test-preset environment for discovery.
- [ ] Verify `ctest --preset sanitized-clang-libcxx -L unit -LE aux` no longer fails during discovery.
- [ ] Update tooling contracts so the sanitized preset must keep the discovery-safe sanitizer environment.

### Task 2: Replace The Production Env Fault Seam

**Files:**
- Modify: `include/project/numerics/state_advancement.hpp`
- Modify: `src/lib/numerics/state_advancement.cpp`
- Modify: `tests/unit/test_state_advancement_sundials.cpp`
- Modify: `CMakeLists.txt`

- [ ] Introduce a compile-time-gated test-only SUNDIALS fault mode API.
- [ ] Remove `getenv("AIROW_SUNDIALS_TEST_FAULT")` from production behavior.
- [ ] Update the SUNDIALS unit tests to use the explicit test hook instead of environment variables.
- [ ] Verify the focused SUNDIALS unit coverage still passes.

### Task 3: Sync Workflow Messaging

**Files:**
- Modify: `scripts/test_tdd.sh`
- Modify: `scripts/verify.sh`

- [ ] Replace stale “warning-only by default” comments with strict-default wording.

### Task 4: Re-verify And Restate Status

**Files:**
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/ai/SESSION_CONTEXT.md`
- Modify: `docs/ai/HANDOFF.md`

- [ ] Run the required gates with fresh evidence.
- [ ] Update release/AI docs to reflect the actual verified state.
- [ ] Regenerate traceability with `python3 tools/tracecheck.py --write`.
