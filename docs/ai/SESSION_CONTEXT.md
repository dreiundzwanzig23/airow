# SESSION_CONTEXT.md

## Snapshot
- **Date**: 2026-04-19
- **Branch**: `External`
- **Current Objective**: Keep the recently closed post-`v0.1` packets stable
  while the next deferred backlog items settle.

## Current State
- The simulator remains a headless-first single-scull runtime with active
  `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`, `A-009`, and
  `A-010` contracts under `include/project/**`.
- Closed baseline packets remain stable:
  Slice 2 keeps the reduced hydro and steady-wind default-runtime providers,
  Slice 3 keeps `chrono_rigidbody + sundials_ida` as the preferred supported
  runtime, Slice 4A keeps the first calibrated-artifact ingestion path,
  Slice 4B keeps the one-of wind-input schema, and Slice 4C keeps ordered
  batch execution on the shared run path.
- `R-021`, `R-022`, `R-023`, and `R-025` remain `DONE`.
- `R-024` is now `DONE`: config accepts optional
  `output.truth_model_export_path`, the shared run path emits one
  deterministic JSON handoff bundle only when requested, and re-import stays
  on the existing calibrated-artifact runtime path.
- `R-026` is now `DONE`: the repo ships
  `scenarios/performance_budgets.json`, a dedicated
  `./scripts/test_performance.sh` lane, and a machine-readable budget report,
  while `./scripts/test_tdd.sh` stays free of protected-scenario budget
  checks.
- `A-009` remains `IN_PROGRESS` for richer schemas and future hydro-side
  consumers beyond the first calibrated aero path and first truth-model
  handoff boundary.
- Workflow enforcement remains strict with `rgr:red`, `rgr:green`, and
  `rgr:refactor` evidence, and the required repo gates are expected to stay
  green from this state.

## Guardrails
- Keep active AI docs compact and non-duplicative.
- Keep workflow or gate policy changes synchronized across scripts, README,
  process docs, and AI context in the same slice.
- Keep `A-001` focused on configuration or validation and `A-002` focused on
  orchestration or lifecycle behavior.

## Next Actions
1. Keep the closed guardrail packet stable while later backlog items settle.
2. Extend `A-009` only through new explicit packets so richer schemas or
   hydro-side consumers do not blur the closed Slice 4A boundary.
3. Treat `R-027`, `R-028`, `R-029`, and `R-030` as the next deferred backlog
   candidates unless a smaller stop-the-line defect preempts them.
