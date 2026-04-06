# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- Current work should focus on explicit post-`v0.1` slices rather than on a
  loose backlog or reopening the baseline milestone.

## Post-`v0.1` Slices

### Slice 1 — Runtime-Selectable Providers and Validity Metadata
- Close `R-020` and `R-033`.
- Add runtime-selectable hydro and aero provider variants, deterministic
  unknown-provider rejection, and machine-readable validity-metadata
  propagation for reduced runtime providers.
- Extend `A-001`, `A-002`, `A-004`, `A-005`, and `A-007` without reopening
  solver or backend selection.

### Slice 2 — Reduced-Model Fidelity Expansion
- Deepen reduced hydro and steady-wind aero behavior behind the existing
  provider seams while preserving the current deterministic headless run path.
- Keep `A-004` and `A-005` as the primary owners for richer reduced-model
  variants and stronger observable scenario fidelity.

### Slice 3 — External Backend Wiring and Backend Selection
- Revisit concrete Chrono and SUNDIALS adoption behind the existing mechanics
  and state-advancer seams.
- Anchor this work in `A-010`, with coordinated changes in `A-003` and
  `A-002` where backend wiring touches mechanics realization or run lifecycle.
- Keep this slice separate from hydro or aero provider selection.

### Slice 4 — Calibration and Time-Varying Environment
- Re-open `R-021`, `R-022`, and `R-023` after provider selection and backend
  seams are stable.
- Add external calibration ingestion, provenance propagation, and
  deterministic time-varying wind support without collapsing the runtime
  versus truth-model separation boundary.

## Later Backlog
- Keep `R-024` and `R-026` as cross-cutting guardrails across the post-`v0.1`
  slices rather than as the next headline delivery items.
- `R-025`, `R-027`, `R-028`, `R-029`, and `R-030` remain deferred until the
  four near-term slices above settle.
- Add real component-prefixed code paths so component-level depcheck rules
  become active in production code.
- Add simulator-specific auxiliary and regression lanes only when they improve
  local verification without polluting the baseline runtime path.
