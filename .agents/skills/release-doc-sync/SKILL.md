---
name: release-doc-sync
description: Synchronize release-facing and AI context documentation with implementation and process changes. Use when public behavior, workflow policy, interfaces, requirement or architecture status, or milestone scope changes.
---

# Release Doc Sync

## Update Required Artifacts
- Update `CHANGELOG.md`.
- Update `README.md` for user-visible or process-visible changes.
- Update `docs/ai/SESSION_CONTEXT.md`.
- Update `docs/ai/HANDOFF.md`.
- Update `docs/ai/ROADMAP.md` when backlog or milestone scope changes.
- Update `docs/ai/DECISIONS.md` when recording a new durable decision.

## Execute
1. Record behavior and process deltas with concise, concrete wording.
2. Keep `docs/ai/*` focused on current state, open risks, and next actions.
3. Re-run `python3 tools/tracecheck.py --write` when trace-relevant docs or
   metadata changed.

## Finish
- Keep release and handoff documentation aligned with code and policy state.
