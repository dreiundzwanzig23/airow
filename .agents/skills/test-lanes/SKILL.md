---
name: test-lanes
description: Select the correct repository test lane based on work lane, confidence need, coverage, and iteration speed. Use when deciding which test script to run during development, regression validation, or pre-merge verification.
---

# Test Lanes

## Choose By Work Lane

| Work lane | Typical checks |
| --- | --- |
| Lane 0 Explore | None |
| Lane 1 Tiny local fix | Narrow targeted command, formatter, or relevant script |
| Lane 2 Functional behavior slice | Targeted failing/passing test, then `./scripts/test_tdd.sh` after refactor |
| Lane 3 Public interface or artifact | Full local gates from `AGENTS.md` before completion |
| Lane 4 Major architecture or migration | Full local gates plus drift review; `./scripts/verify.sh` when merge-ready |
| Lane 5 Release or handoff | `./scripts/verify.sh` |

## Available Commands

- `./scripts/test_tdd.sh` — fast local TDD iteration; includes RGR check, unit/integration/system lanes excluding slow/regression/aux, changed-scope aux lint, and coverage checks.
- `./scripts/test.sh` — standard full local functional test lane.
- `./scripts/test_aux.sh` — auxiliary script/tool contract checks.
- `./scripts/test_performance.sh` — protected scenario performance guardrail.
- `./scripts/regression.sh` — optional broader regression validation, especially for named baseline scenarios.
- `./scripts/verify.sh` — aggregate pre-merge verification.
- `./scripts/check_rgr_evidence.sh` — ordered RGR evidence check.

## Apply Constraints

- Do not use full gates as the default for exploration or tiny local fixes.
- Do not treat `test_tdd.sh` as a replacement for full gates when the selected lane requires full gates.
- `check_rgr_evidence.sh`, `test_tdd.sh`, and `verify.sh` are strict by default for RGR evidence.
- Use `RGR_ENFORCEMENT_MODE=warn` or `off` only as an explicit local override for lanes that do not change functional behavior.
- Treat `IT-*` as the main subsystem-contract lane for architecture boundary checks and characterization around preserved seams.
- Treat scenario-oriented `QT-*` runs as the main requirement-level evidence lane for simulator behavior.
- Keep auxiliary tests informational unless deliberately promoted into evidence-bearing lanes.

## Protected Baseline Scenarios

Protect named baseline scenarios as runtime evidence becomes available:

- passive float,
- tow test,
- calm-water stroke,
- headwind stroke,
- crosswind stroke.

## Finish

Report exactly which commands ran and whether any required lane gate was skipped, narrowed, or blocked.
