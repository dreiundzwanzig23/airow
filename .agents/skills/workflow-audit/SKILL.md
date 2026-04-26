---
name: workflow-audit
description: Audit whether a branch, task, or handoff followed the repository's TDD, traceability, evidence, documentation, and gate discipline. Use before handoff, before merge, after broad changes, or whenever process cleanliness is uncertain.
---

# Workflow Audit

## Start
- Use this skill before handoff, before merge, after major changes, or when the
  user asks whether a branch is process-clean.
- Audit the work that actually changed. Do not invent process evidence that is
  not present.
- Prefer concrete commands, diffs, and trace links over generic statements.

## Audit Checklist
1. Target alignment:
   - the task maps to relevant `R-*`, `A-*`, `D-*`, and test IDs,
   - requirement selection follows the repo policy or the user-provided task.
2. TDD evidence:
   - `rgr:red` exists and shows an intended failure,
   - `rgr:green` exists and shows the minimal passing change,
   - `rgr:refactor` exists or has an explicit no-op rationale.
3. Traceability:
   - `UT-* -> D-*`, `IT-* -> A-*`, and `QT-* -> R-*` links are lane-consistent,
   - `python3 tools/tracecheck.py --write` was run when trace-relevant files
     changed,
   - generated trace diffs were reviewed instead of hand-edited.
4. Quality gates:
   - required gates were run or explicitly blocked,
   - auxiliary/script/skill checks were run when tooling changed.
5. Documentation:
   - README, CHANGELOG, and `docs/ai/*` changed only when triggered,
   - process-visible changes are documented where future agents will see them.
6. Drift and risk:
   - major changes have an impact map,
   - physics or visualization claims have simulation evidence design,
   - stale requirements, architecture, design, tests, or docs are identified.

## Required Output
Produce an audit note in this shape:

```markdown
## Workflow Audit

Status:
- READY | BLOCKED

Target:
- Requirement:
- Architecture:
- Design:
- Tests:

RGR:
- red evidence:
- green evidence:
- refactor evidence:

Gates:
- format:
- lint:
- build:
- test:
- depcheck:
- tracecheck:
- auxiliary/tooling:

Trace:
- result:
- generated diff reviewed:

Docs:
- README:
- CHANGELOG:
- docs/ai:
- decisions:

Blocking issues:
1.
2.

Follow-ups:
1.
2.
```

## Finish
- Mark `READY` only when evidence, gates, trace links, and triggered docs are
  clean.
- Mark `BLOCKED` when required evidence is missing, a gate failed, or a trace or
  documentation obligation is unresolved.
- Be explicit about uncertainty. A process audit is useful only if it names the
  missing evidence.
