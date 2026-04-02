---
name: template-tdd-loop
description: Execute the repository's required failing-tests-first loop for behavior-changing code work. Use when implementing features or fixing defects in library or app code that affect runtime behavior and must pass all project quality gates.
---

# Template TDD Loop

## Capture Inputs
- Identify target requirement IDs: `R-*`.
- Identify impacted architecture and design IDs: `A-*`, `D-*`.
- Identify candidate owning `A-*` items before any test edits.

## Run Workflow
1. Select the highest-priority actionable requirement from `docs/process/REQUIREMENTS.md`.
2. Perform architecture allocation in `docs/process/ARCHITECTURE.md`:
   - evaluate existing subsystem owners first,
   - reuse a stable subsystem when coherent,
   - record any architecture delta before TDD,
   - justify any new `A-*` with `Allocation Rationale` and `Future Absorption`.
3. Add or adjust failing tests first (`UT-*`, `IT-*`, `QT-*` as needed).
4. Implement the minimum code change required to pass tests.
5. Refactor only when behavior remains unchanged.
6. Run the fast iteration lane until stable:
   - `./scripts/test_tdd.sh`
7. Run required completion gates in order:
   - `./scripts/format.sh`
   - `./scripts/lint.sh`
   - `./scripts/build.sh`
   - `./scripts/test.sh`
   - `./scripts/depcheck.sh`
   - `python3 tools/tracecheck.py --write`

## Produce Outputs
- Leave all tests and gates passing.
- Update traceability and AI context artifacts when changes trigger
  documentation obligations.
- Do not create a thin 1:1 `R -> A` mapping unless the architecture policy
  explicitly justifies it.
