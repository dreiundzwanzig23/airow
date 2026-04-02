---
name: template-test-lanes
description: Select the correct repository test lane based on confidence, coverage, and iteration speed. Use when deciding which script to run during development, verification, regression checks, or pre-merge validation.
---

# Template Test Lanes

## Select Lane
- Use `./scripts/test_tdd.sh` for fast local TDD iteration.
- Use `./scripts/test.sh` for standard full local testing.
- Use `./scripts/test_aux.sh` for auxiliary script/tool contract tests.
- Use `./scripts/regression.sh` for optional regression validation.
- Use `./scripts/verify.sh` for aggregate pre-merge verification.

## Apply Constraints
- Do not treat `test_tdd.sh` as a replacement for required completion gates.
- Treat scenario-oriented `QT-*` runs as the main requirement-level evidence
  lane for simulator behavior.
- Add characterization coverage before invasive refactors or major-change work.
- Prefer `verify.sh` before handoff when broad confidence is required.
- Keep auxiliary tests informational unless they are deliberately promoted into
  evidence-bearing lanes.
