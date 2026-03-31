# AI Archive Policy

This folder stores historical AI session artifacts that are no longer needed in
active handoff files.

## Retention Model
- Active docs (`docs/ai/*.md`) keep current operational state.
- Historical detail is moved here for long-term auditability.
- Active decisions retention is capped to top-N ADRs
  (`DEFAULT_MAX_ACTIVE_ADRS = 8`) in `docs/ai/DECISIONS.md`.
- Active docs must stay compact and non-duplicative.

## Naming
- Decisions archive: `DECISIONS_pre_YYYY-MM-DD.md`
- Decisions archive index: `DECISIONS_INDEX.md`

## Automation
- `./scripts/depcheck.sh` runs `python3 tools/archive_ai_decisions.py`.
- The archiver keeps the newest ADR blocks active and regenerates
  `DECISIONS_INDEX.md`.
