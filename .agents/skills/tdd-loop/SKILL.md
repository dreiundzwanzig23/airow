---
name: tdd-loop
description: Execute the repository's failing-tests-first workflow for behavior-changing code work. Use when implementing features or fixing defects in library or app code, when selecting requirement-driven work, or when a task must finish with the standard local quality gates green.
---

# TDD Loop

## Start
- Select actionable requirement work from `docs/process/REQUIREMENTS.md`:
  - first any item with `Needs-Review: yes`,
  - then the highest-priority `OPEN` item.
- Identify the target requirement IDs (`R-*`), candidate owning architecture IDs
  (`A-*`), and affected design IDs (`D-*`) before changing tests.
- Switch to `.agents/skills/major-change-loop/SKILL.md` when the change is
  cross-cutting, migratory, semantic across multiple requirements, or
  architectural enough that ordinary TDD is too narrow.

## Execute
1. Select the highest-priority actionable requirement from `docs/process/REQUIREMENTS.md`.
2. Perform architecture allocation in `docs/process/ARCHITECTURE.md`:
   - evaluate existing subsystem owners first,
   - reuse a stable subsystem when coherent,
   - record any architecture delta before TDD,
   - justify any new `A-*` with `Allocation Rationale` and `Future Absorption`.
3. Red:
   - add or adjust targeted failing tests first (`UT-*`, `IT-*`, `QT-*` as
     needed),
   - confirm the intended failure is reproducible,
   - record `rgr:red` evidence.
4. Green:
   - implement the minimum code change required to pass the targeted failures,
   - record `rgr:green` evidence.
5. Refactor (mandatory):
   - perform behavior-preserving cleanup immediately after green,
   - if no cleanup edit is needed, record explicit no-op rationale,
   - record `rgr:refactor` evidence.
6. Run the fast iteration lane after refactor:
   - `./scripts/test_tdd.sh`
7. Recheck `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md`
   before landing any change that affects technology choice, solver direction,
   file-format policy, or external-tool integration.
8. Run required completion gates in order:
   - `./scripts/format.sh`
   - `./scripts/lint.sh`
   - `./scripts/build.sh`
   - `./scripts/test.sh`
   - `./scripts/depcheck.sh`
   - `python3 tools/tracecheck.py --write`

## Finish
- Leave all tests and gates passing.
- Update traceability and AI context artifacts when changes trigger
  documentation obligations.
- Update `README.md` and `CHANGELOG.md` when the change is user-visible or
  process-visible.
- Keep RGR evidence markers (`rgr:red`, `rgr:green`, `rgr:refactor`) in a
  commit message or evidence note. `./scripts/check_rgr_evidence.sh` warns by
  default and can be promoted to strict failures with
  `RGR_ENFORCEMENT_MODE=strict`.
- Do not create a thin 1:1 `R -> A` mapping unless the architecture policy
  explicitly justifies it.
