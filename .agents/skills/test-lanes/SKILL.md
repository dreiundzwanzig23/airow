---
name: test-lanes
description: Select the correct repository test lane based on confidence, coverage, and iteration speed. Use when deciding which test script to run during development, fast TDD iteration, auxiliary contract checks, regression validation, or pre-merge verification.
---

# Test Lanes

## Choose A Lane
- Use `./scripts/test_tdd.sh` for fast local TDD iteration.
- Use `./scripts/test.sh` for standard full local testing.
- Use `./scripts/test_aux.sh` for auxiliary script/tool contract tests.
- Use `./scripts/regression.sh` for optional broader regression validation,
  especially when protecting named baseline scenarios.
- Use `./scripts/verify.sh` for aggregate pre-merge verification.

## Apply Constraints
- Do not treat `test_tdd.sh` as a replacement for required completion gates.
- Treat `test_tdd.sh` as the required loop re-check after each green->refactor
  transition.
- Keep explicit `rgr:red`, `rgr:green`, and `rgr:refactor` evidence markers.
- `./scripts/check_rgr_evidence.sh` runs in warning mode from both
  `test_tdd.sh` and `verify.sh`; use `RGR_ENFORCEMENT_MODE=strict` when you
  need marker absence to fail the lane.
- Treat `IT-*` as the main subsystem-contract lane for architecture boundary
  checks and characterization coverage around preserved seams.
- Treat scenario-oriented `QT-*` runs as the main requirement-level evidence
  lane for simulator behavior.
- Protect these named baseline scenarios as runtime evidence becomes available:
  passive float, tow test, calm-water stroke, headwind stroke, crosswind
  stroke.
- Add characterization coverage before invasive refactors or major-change work,
  preferably in the narrowest lane that protects the preserved behavior.
- Prefer `verify.sh` before handoff when broad confidence is required.
- Keep auxiliary tests informational unless they are deliberately promoted into
  evidence-bearing lanes.

## Finish
- Run the full required completion gates before considering work complete,
  even if a narrower lane was enough during iteration.
