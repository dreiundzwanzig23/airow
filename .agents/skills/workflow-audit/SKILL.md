---
name: workflow-audit
description: Audit whether a branch, task, or handoff followed the repository's lane selection, TDD, traceability, evidence, documentation, and gate discipline. Use before handoff, before merge, after broad changes, or whenever process cleanliness is uncertain.
---

# Workflow Audit

## Start

- Audit the work that actually changed.
- Do not invent process evidence that is not present.
- Prefer concrete commands, diffs, trace links, and documentation triggers over generic statements.
- Use the selected work lane as the audit baseline.

## Audit Checklist

1. **Lane selection**
   - selected lane is named,
   - lane was light enough for the task,
   - any escalation was triggered by actual change scope,
   - major-change workflow was used only when Lane 4 triggers existed.

2. **Target alignment**
   - user-directed tasks stayed on the requested target,
   - backlog-driven tasks followed requirement selection policy,
   - relevant `R-*`, `A-*`, `D-*`, and test IDs are named where applicable.

3. **TDD evidence**
   - behavior-changing work has ordered `rgr:red`, `rgr:green`, and `rgr:refactor`,
   - red evidence shows the intended failure,
   - green evidence shows the minimal passing change,
   - refactor evidence or no-op rationale exists.

4. **Traceability**
   - `UT-* -> D-*`, `IT-* -> A-*`, and `QT-* -> R-*` links are lane-consistent,
   - `python3 tools/tracecheck.py --write` was run when trace-relevant files changed,
   - generated trace diffs were reviewed instead of hand-edited.

5. **Quality gates**
   - lane-required gates were run or explicitly blocked,
   - auxiliary/script/skill checks were run when tooling changed,
   - full gates were used for public-interface, major, merge, or release work.

6. **Documentation triggers**
   - README, CHANGELOG, requirements, architecture, traceability, and `docs/ai/*` changed only when triggered,
   - process-visible changes are documented where future agents will see them,
   - handoff docs were not used as a running log.

7. **Drift and risk**
   - major changes have an impact map,
   - physics, visualization, calibration, validation, or optimization claims have simulation evidence design,
   - stale requirements, architecture, design, tests, or docs are identified.

## Required Output

Produce an audit note in this shape:

```markdown
## Workflow Audit

Status:
- READY | BLOCKED

Work lane:
- selected:
- appropriate: yes | no, reason

Target:
- Requirement:
- Architecture:
- Design:
- Tests:

RGR:
- required: yes | no, reason
- red evidence:
- green evidence:
- refactor evidence:

Gates:
- targeted:
- format:
- lint:
- build:
- test_tdd:
- test:
- depcheck:
- tracecheck:
- verify:
- auxiliary/tooling:

Trace:
- result:
- generated diff reviewed:

Docs:
- README:
- CHANGELOG:
- REQUIREMENTS:
- ARCHITECTURE:
- docs/ai:
- DECISIONS:

Blocking issues:
1.

Follow-ups:
1.
```

## Finish

- Mark `READY` only when lane choice, required evidence, gates, trace links, and triggered docs are clean.
- Mark `BLOCKED` when required evidence is missing, a gate failed, or a trace/documentation obligation is unresolved.
- Be explicit about uncertainty. A process audit is useful only if it names the missing evidence.
