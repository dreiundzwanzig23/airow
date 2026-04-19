# Slice 2 Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close Slice 2 by freezing the current reduced hydro and steady-wind aero baseline as sufficient, removing stale "still underway" roadmap or architecture promises, and re-verifying the repository under the strict gates.

**Architecture:** This is a stabilization slice, not a new fidelity push. Keep `A-004` and `A-005` behavior stable, tighten one small contract around built-in provider validity metadata so the default reduced baseline is explicitly pinned, then update release-facing and AI-context docs to reflect that Slice 2 is closed while later hydro or aero expansion remains deferred to future slices.

**Tech Stack:** C++20, CMake/CTest, GoogleTest, Markdown process docs, repo traceability tooling.

---

### Task 1: Freeze the built-in reduced-provider baseline contract

**Files:**
- Modify: `src/lib/configuration/provider_catalog.cpp`
- Modify: `tests/unit/test_simulator_config_runtime.cpp`

- [ ] **Step 1: Tighten the built-in provider validity wording**

Update the validity descriptions for `quadratic_drag_placeholder`,
`stroke_propulsion_placeholder`, and `steady_wind_placeholder` so they describe
the current reduced baseline as the supported deterministic default runtime
surface rather than an implied temporary follow-on target.

- [ ] **Step 2: Strengthen the provider-catalog unit test**

Extend `SimulatorConfigRuntime.BuiltInProviderCatalogExposesValidityMetadata`
to verify the exact validity ids and key closure wording for the three built-in
reduced providers instead of only checking for non-empty strings.

- [ ] **Step 3: Run the focused unit lane**

Run:

```bash
ctest --preset release --output-on-failure -R '^SimulatorConfigRuntime\.'
```

Expected: PASS with the provider-catalog contract still green.

### Task 2: Close Slice 2 in roadmap and architecture artifacts

**Files:**
- Modify: `docs/ai/ROADMAP.md`
- Modify: `docs/process/ARCHITECTURE.md`
- Modify: `docs/process/MODEL_FIDELITY.md`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/ai/SESSION_CONTEXT.md`
- Modify: `docs/ai/HANDOFF.md`

- [ ] **Step 1: Remove stale "underway" fidelity-follow-on wording**

Replace Slice 2 language that still says reduced hydro or aero fidelity is
"underway", "next fidelity pass", or "follow-on step" with wording that marks
the current reduced baseline as sufficient and closed.

- [ ] **Step 2: Keep future work separated**

Clarify that future hydro or aero expansion belongs to later scoped packets
such as calibration ingestion, provenance, time-varying wind, or other
deliberate follow-on work, not to an open-ended Slice 2 continuation.

- [ ] **Step 3: Sync release-facing and AI context docs**

Record the closure consistently across roadmap, architecture snapshot, README,
changelog, session context, and handoff so the repo no longer claims Slice 2
is active.

### Task 3: Regenerate trace artifacts and verify the closed slice

**Files:**
- Modify: `docs/process/TRACEABILITY.md` (generated)

- [ ] **Step 1: Regenerate traceability**

Run:

```bash
python3 tools/tracecheck.py --write
```

Expected: `Trace OK` with only any pre-existing numbering-gap warnings.

- [ ] **Step 2: Run required gates**

Run:

```bash
./scripts/format.sh
./scripts/lint.sh
./scripts/build.sh
./scripts/test.sh
./scripts/depcheck.sh
python3 tools/tracecheck.py --write
```

Expected: all required gates pass.

- [ ] **Step 3: Confirm closure state**

Run:

```bash
git diff --stat -- src/lib/configuration/provider_catalog.cpp tests/unit/test_simulator_config_runtime.cpp docs/ai/ROADMAP.md docs/process/ARCHITECTURE.md docs/process/MODEL_FIDELITY.md README.md CHANGELOG.md docs/ai/SESSION_CONTEXT.md docs/ai/HANDOFF.md docs/process/TRACEABILITY.md
```

Expected: the final diff is limited to the baseline-contract freeze plus Slice
2 closure documentation and generated trace output.
