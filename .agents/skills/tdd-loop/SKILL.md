---
name: tdd-loop
description: Execute the repository's failing-tests-first workflow for behavior-changing code work. Use when implementing features or fixing defects in library or app code, when selecting requirement-driven work, or when a task must finish with the standard local quality gates green.
---

# TDD Loop

## Start
- When the user gives a concrete task, map that task to the relevant `R-*`,
  owning `A-*`, affected `D-*`, and evidence lane before selecting unrelated
  backlog work.
- When no concrete task is given, select actionable requirement work from
  `docs/process/REQUIREMENTS.md`:
  - first any item with `Needs-Review: yes`,
  - then the highest-priority `OPEN` item.
- Keep each red/green/refactor loop to one behavior claim whenever practical.
  Split broad work into multiple micro-slices instead of hiding several
  behaviors behind one failing test.
- Identify the target requirement IDs (`R-*`), candidate owning architecture IDs
  (`A-*`), and affected design IDs (`D-*`) before changing tests.
- Switch to `.agents/skills/major-change-loop/SKILL.md` when the change is
  cross-cutting, migratory, semantic across multiple requirements, or
  architectural enough that ordinary TDD is too narrow.
- Pair with `.agents/skills/simulation-evidence-design/SKILL.md` before
  implementing hydro, aero, mechanics, visualization, calibration, validation,
  or optimizer behavior that makes a physical or interpretive claim.

## Execute
1. Resolve the target behavior and evidence lane:
   - `UT-*` for design-level behavior,
   - `IT-*` for architecture/subsystem contracts,
   - `QT-*` for requirement/scenario evidence.
2. Perform architecture allocation in `docs/process/ARCHITECTURE.md`:
   - evaluate existing subsystem owners first,
   - reuse a stable subsystem when coherent,
   - record any architecture delta before TDD,
   - justify any new `A-*` with `Allocation Rationale` and `Future Absorption`.
3. Red:
   - use `.agents/skills/unit-test-design/SKILL.md` when the narrowest correct
     lane is `UT-*`,
   - add or adjust targeted failing tests first (`UT-*`, `IT-*`, `QT-*` as
     needed),
   - confirm the intended failure is reproducible,
   - explain why the failure proves the intended missing behavior instead of an
     incidental setup problem,
   - record `rgr:red` evidence.
4. Green:
   - implement the minimum code change required to pass the targeted failures,
   - avoid broad cleanup or unrelated behavior during green,
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

## Required Evidence Output
Leave an evidence note or commit-message section in this shape:

```markdown
## RGR Evidence

Target:
- Requirement: R-###
- Architecture: A-###
- Design: D-###
- Test: UT/IT/QT-###

rgr:red:
- Command:
- Expected failing test:
- Observed failure:
- Why this proves the intended missing behavior:

rgr:green:
- Command:
- Minimal implementation change:
- Observed pass:

rgr:refactor:
- Cleanup performed:
- Or explicit no-op rationale:

Gates:
- test_tdd:
- format:
- lint:
- build:
- test:
- depcheck:
- tracecheck:
```

## Finish
- Leave all tests and gates passing.
- Update traceability and AI context artifacts when changes trigger
  documentation obligations.
- Update `README.md` and `CHANGELOG.md` when the change is user-visible or
  process-visible.
- Keep RGR evidence markers (`rgr:red`, `rgr:green`, `rgr:refactor`) in a
  commit message or evidence note. `./scripts/check_rgr_evidence.sh` is strict
  by default; use `RGR_ENFORCEMENT_MODE=warn` or `off` only as explicit local
  overrides.
- Do not create a thin 1:1 `R -> A` mapping unless the architecture policy
  explicitly justifies it.
