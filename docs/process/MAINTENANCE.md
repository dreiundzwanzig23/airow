# MAINTENANCE.md

Weekly maintenance keeps the repository legible for long-running AI-assisted
development.

## Weekly checklist
1. Run `./scripts/maintenance.sh`.
2. Resolve stale `OPEN` requirements older than threshold or add notes.
3. Confirm `docs/process/QUALITY_SCORECARD.md` reflects current process state.
4. Confirm `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md`
   still match the approved project direction.
5. Review `docs/process/ARCHITECTURE_HEALTH.md` for stale risk notes and
   update follow-ups if architecture pressure points changed.
6. Confirm `docs/process/MODEL_FIDELITY.md`,
   `docs/process/NUMERICS_POLICY.md`, and
   `docs/process/CALIBRATION_PROVENANCE.md` still match project practice.
7. Remove or archive obsolete docs, tests, or scripts.
8. Regenerate traceability and confirm no drift.

Active AI docs should remain compact and non-duplicative:
- `docs/ai/SESSION_CONTEXT.md` <= 70 lines
- `docs/ai/HANDOFF.md` <= 60 lines
- `docs/ai/ROADMAP.md` <= 70 lines
- `docs/ai/DECISIONS.md` keeps only the newest active ADR window

## Staleness policy
- Default stale threshold: 45 days for `OPEN` requirements.
- Override with `STALE_DAYS=<N> ./scripts/maintenance.sh`.
