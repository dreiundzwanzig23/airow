---
name: major-change-loop
description: Execute the repository's workflow for cross-cutting, architectural, migratory, or semantic multi-requirement changes. Use when ordinary TDD is too narrow because the change spans multiple subsystems, needs transitional seams, or requires explicit impact and drift management.
---

# Major-Change Loop

## Start
- Use this skill for architectural refactors, migrations, deprecations,
  semantic multi-requirement edits, backend swaps, and other cross-cutting
  changes that touch multiple subsystem seams.
- Name the affected seams up front and confirm which `R-*`, `A-*`, `D-*`, and
  test lanes will move.
- Add characterization tests for preserved behavior before invasive edits.
- Pair with `.agents/skills/simulation-evidence-design/SKILL.md` when the major
  change affects simulation physics, validation, calibration, visualization, or
  optimization semantics.

## Execute
1. Classify the change:
   - refactor,
   - migration,
   - architecture split,
   - semantic requirement change,
   - backend replacement,
   - evidence/trace promotion.
2. Build an impact map across `R-*`, `A-*`, `D-*`, tests, and touched files.
3. Record the architecture delta before implementation begins.
4. Add characterization tests for preserved behavior before invasive edits:
   - use `.agents/skills/unit-test-design/SKILL.md` when preserved local
     behavior belongs in `UT-*`,
   - use `IT-*` when the preserved behavior is a subsystem boundary contract,
   - use `QT-*` when the preserved behavior is requirement/scenario-level.
5. Implement seam-first when the change can be split into transitional steps.
6. Remove transitional code explicitly once the replacement path is stable.
7. Run a drift review after the change to catch documentation or ownership gaps.
8. Finish with the standard completion gates:
   - `./scripts/format.sh`
   - `./scripts/lint.sh`
   - `./scripts/build.sh`
   - `./scripts/test.sh`
   - `./scripts/depcheck.sh`
   - `python3 tools/tracecheck.py --write`

## Required Output
Leave a reviewable impact map:

```markdown
## Major Change Impact Map

Change class:
- refactor | migration | architecture split | semantic requirement change | backend replacement | evidence promotion

Affected seams:
- R-###
- A-###
- D-###
- files:

Preserved behavior:
- existing tests:
- new characterization tests:

Architecture delta:
- before:
- after:
- why this improves cohesion:

Transition plan:
- step 1:
- step 2:
- temporary code removal:

Drift review:
- requirements:
- architecture:
- design:
- tests:
- README/CHANGELOG/docs/ai:
```

## Finish
- Leave a reviewable architecture delta, not just a green patch.
- Preserve or improve subsystem boundaries as part of the change.
- Update traceability, AI context, and process docs when the change affects
  them.
- Update `README.md` and `CHANGELOG.md` when the change is user-visible or
  process-visible.
