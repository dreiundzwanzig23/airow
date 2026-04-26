---
name: release-doc-sync
description: Synchronize release-facing and AI context documentation with implementation and process changes. Use when public behavior, workflow policy, interfaces, requirement or architecture status, or milestone scope changes.
---

# Release Doc Sync

## Trigger Matrix
| Change | Required docs |
|---|---|
| User-visible CLI/config/output behavior | `README.md`, `CHANGELOG.md` |
| Process-visible workflow, gate, or evidence behavior | `AGENTS.md`, `.agents/skills/*`, `CHANGELOG.md` when externally relevant |
| Requirement status or milestone scope | `docs/ai/ROADMAP.md`, `docs/ai/SESSION_CONTEXT.md` |
| Architecture status or process-visible architecture direction | `docs/ai/SESSION_CONTEXT.md`, maybe `docs/ai/HANDOFF.md` |
| Durable technical decision | `docs/ai/DECISIONS.md` |
| Small internal implementation detail | Usually no release-doc update |

## Execute
1. Identify which trigger applies before editing docs. Avoid doc churn for purely
   internal changes that do not affect users, process, handoff, roadmap, or
   durable decisions.
2. Record behavior and process deltas with concise, concrete wording.
3. Keep `docs/ai/*` focused on current state, open risks, and next actions.
4. Update `CHANGELOG.md` when the change is user-visible or process-visible.
5. Update `README.md` for user-visible behavior, public workflow, or interface
   changes.
6. Update `docs/ai/ROADMAP.md` when backlog or milestone scope changes.
7. Update `docs/ai/DECISIONS.md` when recording a new durable decision.
8. Re-run `python3 tools/tracecheck.py --write` when trace-relevant docs or
   metadata changed.

## Required Output
Leave a doc-sync note when this skill is used:

```markdown
## Release Doc Sync

Trigger:
- user-visible | process-visible | roadmap | architecture | decision | none

Updated:
- README:
- CHANGELOG:
- docs/ai/SESSION_CONTEXT:
- docs/ai/HANDOFF:
- docs/ai/ROADMAP:
- docs/ai/DECISIONS:

Omitted docs:
- file:
- reason:
```

## Finish
- Keep release and handoff documentation aligned with code and policy state.
- Prefer no edit over noisy edit when no trigger applies.
