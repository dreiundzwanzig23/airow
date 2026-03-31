# AGENTS.md — docs/process

## Scope
Owns process contracts, requirement/architecture definitions, and
traceability-facing schemas.

## Required updates when changing process
- Keep `REQUIREMENTS.md` and `ARCHITECTURE.md` syntax valid for
  `tools/tracecheck.py`.
- Keep `WORKFLOW.md`, `TEST_STRATEGY.md`, `MAINTENANCE.md`, and
  `LLM_DRIFT_REVIEW.md` aligned with actual scripts and folder layout.
- Regenerate `TRACEABILITY.md` via tool, never hand-edit.
- Preserve `DONE` evidence gates (`R->QT`, `A->IT`, `D->UT`).
- Keep skill references and script references coherent with actual repo files.

## Guardrails
- Do not introduce undocumented ID formats.
- Do not mark an item `DONE` without actual evidence.
- Maintain independent per-level numbering and 1:n verification compatibility.
- Keep requirement drift metadata lightweight: `Change-Type`, `Needs-Review`,
  `Change-Note`.
