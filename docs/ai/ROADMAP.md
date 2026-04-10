# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- Slice 0 for human-readable run analysis closed on 2026-04-06 with `R-034`,
  additive summary-analysis output, optional CLI report modes, and the first
  offline static report tool.
- Slice 1 for runtime-selectable providers and structured validity metadata
  closed on 2026-04-06 with top-level config-driven provider selection,
  built-in provider composition on the shared run path, and structured
  provider validity metadata in machine-readable outputs.
- Current work should now advance to reduced-model fidelity expansion rather
  than reopening the baseline milestone or the landed provider-selection slice.

## Post-`v0.1` Slices

### Slice 0 — Human-Readable Run Analysis
- Closed `R-034`.
- Added compact and full human-readable CLI report modes plus derived
  single-run analysis metrics in the JSON summary artifact.
- Added a standard-library-only offline report generator for static HTML and
  SVG inspection from emitted JSON artifacts.

### Slice 1 — Runtime-Selectable Providers and Validity Metadata
- Closed `R-020` and `R-033`.
- Added a top-level `providers` config block for built-in
  `hull_resistance`, `blade_force`, and `aero_load` selection with
  deterministic unknown-provider rejection.
- Added config-driven built-in provider construction on the shared run path,
  while keeping injected provider seams available for focused tests.
- Replaced flat provider-id output metadata with structured per-role provider
  metadata that includes validity identifiers and descriptions.

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
