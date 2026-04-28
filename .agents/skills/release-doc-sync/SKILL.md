---
name: release-doc-sync
description: Synchronize release-facing and AI-context documentation only when their documented triggers are met. Use for Lane 3 public-contract docs, Lane 5 release/handoff work, or explicit documentation-sync tasks.
---

# Release Doc Sync

## Start

Do not update release or AI-context docs by default. First name the trigger.

## Trigger Table

| Artifact | Update when |
| --- | --- |
| `README.md` | Setup, CLI usage, examples, public behavior, user-visible output, or public workflow entry points change |
| `CHANGELOG.md` | User-visible behavior, process-visible workflow, public artifact, or milestone state changes |
| `docs/ai/SESSION_CONTEXT.md` | Persistent current-state guardrails, active objective, or next actions change |
| `docs/ai/HANDOFF.md` | A durable session, milestone, or merge handoff is being prepared |
| `docs/ai/ROADMAP.md` | Backlog, milestone direction, or prioritization changes |
| `docs/ai/DECISIONS.md` | A durable technical or process decision is accepted |
| `docs/process/REQUIREMENTS.md` | Requirement wording, status, acceptance, semantics, or review flags change |
| `docs/process/ARCHITECTURE.md` | Ownership, boundaries, allocation, or architecture status changes |
| `docs/process/TRACEABILITY.md` | Regenerated after trace-relevant metadata changed |

## Execute

1. Record only concrete deltas.
2. Keep `docs/ai/*` short and current:
   - `SESSION_CONTEXT.md` = active state and guardrails,
   - `HANDOFF.md` = latest durable handoff only,
   - `ROADMAP.md` = future direction,
   - `DECISIONS.md` = durable constraints.
3. Avoid duplicating requirements, architecture, trace, and README detail in AI-context docs.
4. Re-run `python3 tools/tracecheck.py --write` only when trace-relevant docs or metadata changed.
5. Do not hand-expand generated trace markdown.

## Required Output

```markdown
## Release Doc Sync
Triggers:
- README: yes | no, reason
- CHANGELOG: yes | no, reason
- SESSION_CONTEXT: yes | no, reason
- HANDOFF: yes | no, reason
- ROADMAP: yes | no, reason
- DECISIONS: yes | no, reason
- REQUIREMENTS/ARCHITECTURE/TRACE: yes | no, reason

Updated artifacts:
-
Skipped artifacts:
-
```

## Finish

Leave docs aligned with the actual change, not with the amount of work performed.
