---
name: major-change-loop
description: "Execute the repository workflow for Lane 4 changes: cross-cutting, architectural, migratory, backend, semantic multi-requirement, or evidence-promotion work. Do not use for ordinary local TDD slices."
---

# Major-Change Loop

## Start

Use this skill only when the change is truly broader than ordinary TDD:

- architectural refactor or subsystem boundary change,
- migration or deprecation,
- backend replacement or solver-policy shift,
- semantic edits across multiple requirements,
- public artifact/schema transition that needs compatibility handling,
- evidence or trace promotion across lanes,
- invasive refactor that needs preserved-behavior characterization.

Do not use this skill merely because the change touches many documentation files or generated trace output.

Pair with `.agents/skills/simulation-evidence-design/SKILL.md` when the major change affects simulation physics, validation, calibration, visualization semantics, or optimization semantics.

## Execute

1. **Classify the change**
   - refactor,
   - migration,
   - architecture split,
   - semantic requirement change,
   - backend replacement,
   - public artifact transition,
   - evidence/trace promotion.

2. **Build an impact map**
   - affected `R-*`, `A-*`, `D-*`, tests, and files,
   - preserved behavior,
   - new behavior,
   - public contracts,
   - docs actually triggered.

3. **Characterize preserved behavior before invasive edits**
   - use `.agents/skills/unit-test-design/SKILL.md` for local `UT-*` characterization,
   - use `IT-*` for subsystem boundary contracts,
   - use `QT-*` for requirement/scenario-level behavior.

4. **Record architecture delta only when it exists**
   - before,
   - after,
   - why cohesion or reuse improves,
   - why existing owners were or were not sufficient.

5. **Implement seam-first**
   - split transitional steps when possible,
   - keep each observable behavior slice in its own red/green/refactor loop,
   - remove transitional code explicitly once the replacement path is stable.

6. **Run drift review**
   - requirements,
   - architecture,
   - design IDs,
   - tests,
   - README/CHANGELOG/docs/ai trigger state,
   - traceability.

7. **Finish with Lane 4 gates**
   - `./scripts/format.sh`
   - `./scripts/lint.sh`
   - `./scripts/build.sh`
   - `./scripts/test.sh`
   - `./scripts/depcheck.sh`
   - `python3 tools/tracecheck.py --write`
   - `./scripts/verify.sh` when merge-ready confidence is required

## Required Output

Leave a reviewable impact map:

```markdown
## Major Change Impact Map

Change class:
- refactor | migration | architecture split | semantic requirement change | backend replacement | public artifact transition | evidence promotion

Why Lane 4 is required:
-

Affected seams:
- R-###:
- A-###:
- D-###:
- tests:
- files:

Preserved behavior:
- existing tests:
- new characterization tests:

New or changed behavior:
-

Architecture delta:
- before:
- after:
- why this improves cohesion:

Transition plan:
- step 1:
- step 2:
- temporary code removal:

Documentation triggers:
- REQUIREMENTS:
- ARCHITECTURE:
- TRACEABILITY:
- README:
- CHANGELOG:
- docs/ai:

Drift review:
- requirements:
- architecture:
- design:
- tests:
- public docs:
```

## Finish

- Preserve or improve subsystem boundaries.
- Update traceability only when links or evidence metadata changed.
- Update release/context docs only where the documentation trigger table requires it.
- Keep the final patch reviewable by separating behavior, transition, and docs where practical.
