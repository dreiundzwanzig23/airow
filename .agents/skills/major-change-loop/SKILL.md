---
name: major-change-loop
description: Execute the repository's workflow for cross-cutting, architectural, migratory, or semantic multi-requirement changes. Use when ordinary TDD is too narrow because the change spans multiple subsystems, needs transitional seams, or requires explicit impact and drift management.
---

# Major-Change Loop

## Start
- Use this skill for architectural refactors, migrations, deprecations,
  semantic multi-requirement edits, and other cross-cutting changes that touch
  multiple subsystem seams.
- Name the affected seams up front and confirm which `R-*`, `A-*`, `D-*`, and
  test lanes will move.

## Execute
1. Classify the change and name the affected subsystem seams.
2. Build an impact map across `R-*`, `A-*`, `D-*`, tests, and touched files.
3. Record the architecture delta before implementation begins.
4. Add characterization tests for preserved behavior before invasive edits.
   - use `.agents/skills/unit-test-design/SKILL.md` when preserved local
     behavior belongs in `UT-*`.
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

## Finish
- Leave a reviewable architecture delta, not just a green patch.
- Preserve or improve subsystem boundaries as part of the change.
- Update traceability, AI context, and process docs when the change affects them.
- Update `README.md` and `CHANGELOG.md` when the change is user-visible or
  process-visible.
