---
name: tdd-loop
description: Execute the repository's failing-tests-first workflow for behavior-changing code work. Use for Lane 2 functional behavior slices and the behavior portions of Lane 3 public-contract work.
---

# TDD Loop

## Start

- Confirm the selected lane is Lane 2 or the behavior portion of Lane 3.
- For user-directed work, use the user's target as the work item.
- For backlog-driven work, select actionable requirement work from `docs/process/REQUIREMENTS.md`:
  - first items with `Needs-Review: yes`,
  - then the highest-priority `OPEN` item.
- Identify the target `R-*`, candidate owning `A-*`, and affected `D-*` items before changing tests when the behavior maps to existing traceable scope.
- For tiny local fixes that do not change observable behavior, return to Lane 1 instead of forcing TDD.
- Switch to `.agents/skills/major-change-loop/SKILL.md` when the change is cross-cutting, migratory, backend-related, semantic across multiple requirements, or architectural enough that ordinary TDD is too narrow.

## Execute

1. **Architecture ownership**
   - Evaluate existing subsystem owners first.
   - Reuse a stable subsystem when coherent.
   - Record an architecture delta only when ownership, boundaries, or allocation actually change.
   - Justify any new `A-*` with `Allocation Rationale` and reuse/cohesion intent.

2. **Red**
   - Use `.agents/skills/unit-test-design/SKILL.md` when `UT-*` is the narrowest correct lane.
   - Add or adjust the smallest targeted failing test first (`UT-*`, `IT-*`, or `QT-*` as appropriate).
   - Confirm the intended failure is reproducible.
   - Record compact `rgr:red` evidence.

3. **Green**
   - Implement the minimum code change required to pass the targeted failure.
   - Record compact `rgr:green` evidence.

4. **Refactor**
   - Perform behavior-preserving cleanup immediately after green.
   - If no cleanup edit is needed, record an explicit no-op rationale.
   - Record compact `rgr:refactor` evidence.

5. **Fast re-check**
   - Run `./scripts/test_tdd.sh` after refactor unless the user explicitly constrained the task to a narrower local check.

6. **Technology guardrail**
   - Recheck `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md` before landing changes that affect technology choice, solver direction, file-format policy, or external-tool integration.

## Completion Gates

Use the selected lane to decide final gates:

- Lane 2: `./scripts/test_tdd.sh` plus any targeted checks that prove the slice.
- Lane 3: full local completion gates from `AGENTS.md` before completion.
- Merge-ready or handoff work: `./scripts/verify.sh` when requested or appropriate.

## Finish

- Leave tests for the selected lane passing, or report blockers clearly.
- Update traceability only when trace links or evidence metadata changed.
- Update README, CHANGELOG, `docs/ai/*`, requirements, or architecture only when the documentation trigger table in `AGENTS.md` requires it.
- Keep RGR evidence markers (`rgr:red`, `rgr:green`, `rgr:refactor`) ordered in a commit message or evidence note.
