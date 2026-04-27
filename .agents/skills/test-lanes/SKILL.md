---
name: test-lanes
description: Select the correct repository test lane based on confidence, coverage, and iteration speed. Use when deciding which test script to run during development, fast TDD iteration, auxiliary contract checks, regression validation, or pre-merge verification.
---

# Test Lanes

## Lane Matrix
| Situation | Lane |
|---|---|
| Red/green/refactor iteration | `./scripts/test_tdd.sh` |
| Standard full local testing | `./scripts/test.sh` |
| Tooling, validation helpers, script contracts, skill structure | `./scripts/test_aux.sh` |
| Optional scenario regression protection | `./scripts/regression.sh` |
| Aggregate pre-merge confidence | `./scripts/verify.sh` |
| Trace regeneration/validation | `python3 tools/tracecheck.py --write` |

## Choose A Lane
- Use `./scripts/test_tdd.sh` for fast local TDD iteration.
- Use `./scripts/test.sh` for standard full local testing.
- Use `./scripts/test_aux.sh` for auxiliary script/tool/skill contract tests.
- Use `./scripts/regression.sh` for optional broader regression validation,
  especially when protecting named baseline scenarios.
- Use `./scripts/verify.sh` for aggregate pre-merge verification.

## Apply Constraints
- Do not treat `test_tdd.sh` as a replacement for required completion gates.
- Treat `test_tdd.sh` as the required loop re-check after each green->refactor
  transition.
- Keep explicit `rgr:red`, `rgr:green`, and `rgr:refactor` evidence markers.
- `./scripts/check_rgr_evidence.sh` is strict by default and is wired into
  `test_tdd.sh` and `verify.sh`; it rejects missing, incomplete, or out-of-order
  marker sequences. Use `RGR_ENFORCEMENT_MODE=warn` or `off` only as explicit
  local overrides.
- `TEST_LINT_SCOPE=changed` in the fast lane enforces the changed-unit-test
  authoring contract without forcing a legacy-wide rewrite.
- Treat `IT-*` as the main subsystem-contract lane for architecture boundary
  checks and characterization coverage around preserved seams.
- Treat scenario-oriented `QT-*` runs as the main requirement-level evidence lane
  for simulator behavior.
- Protect these named baseline scenarios as runtime evidence becomes available:
  passive float, tow test, calm-water stroke, headwind stroke, crosswind stroke.
- Add characterization coverage before invasive refactors or major-change work,
  preferably in the narrowest lane that protects the preserved behavior.
- Prefer `verify.sh` before handoff when broad confidence is required.
- Keep auxiliary tests informational unless they are deliberately promoted into
  evidence-bearing lanes.

## Required Output
When selecting lanes for non-trivial work, leave a short note:

```markdown
## Test Lane Selection

Iteration lane:
- command:
- why sufficient for current loop:

Completion gates:
- commands:
- status:

Optional broad confidence lane:
- command or omitted with reason:
```

## Finish
- Run the full required completion gates before considering work complete, even
  if a narrower lane was enough during iteration.
