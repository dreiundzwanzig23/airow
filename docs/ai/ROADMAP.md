# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- Slice 0 closed on 2026-04-06 with `R-034`, additive summary-analysis
  output, optional CLI report modes, and an offline static report tool.
- Slice 1 closed on 2026-04-06 with top-level provider selection, built-in
  provider composition on the shared run path, and structured validity
  metadata in machine-readable outputs.
- Slice 2 remains underway through in-place hydro and steady-wind aero
  fidelity refinements on the existing built-in ids.
- Slice 3 now includes two landed backend packets: validated
  `simulation.state_advancer` selection plus the guarded `chrono_rigidbody`
  path from 2026-04-13, and the 2026-04-15 SUNDIALS packet that makes
  `sundials_ida` the required default built-in advancer and passes passive-
  float and tow scenario evidence.

## Post-`v0.1` Slices

### Slice 0 — Human-Readable Run Analysis
- Closed `R-034` with CLI report modes, additive summary-analysis output, and
  an offline static report generator.

### Slice 1 — Runtime-Selectable Providers and Validity Metadata
- Closed `R-020` and `R-033` with top-level built-in provider selection,
  shared-run provider construction, and structured per-role validity
  metadata.

### Slice 2 — Reduced-Model Fidelity Expansion
- Keep deepening reduced hydro and steady-wind aero behavior behind the
  existing seams while preserving the deterministic headless run path.
- Primary owners remain `A-004` and `A-005`.
- Current policy: keep `quadratic_drag_placeholder`,
  `stroke_propulsion_placeholder`, and `steady_wind_placeholder` stable in
  config and metadata while refining behavior in place.

### Slice 3 — External Backend Wiring and Backend Selection
- The first packet is landed: built-in state-advancer selection now supports
  optional `chrono_rigidbody`, which is rejected deterministically when
  Chrono support is absent and executes on Chrono-capable builds discovered
  through the local Chrono package config.
- The second packet is also landed: `simulation.state_advancer` now defaults
  to required `sundials_ida`, with `deterministic_baseline` preserved as an
  explicit fallback and SUNDIALS passive-float/tow evidence now checked in.
- Keep this slice anchored in `A-010`, coordinated with `A-003` and `A-002`,
  and separate from hydro or aero provider selection.
- Next step: decide whether to broaden Chrono-backed stroke evidence or deepen
  SUNDIALS-specific diagnostics and tolerance policy behind the same seam.

### Slice 4 — Calibration and Time-Varying Environment
- Re-open `R-021`, `R-022`, and `R-023` only after provider and backend seams
  are stable.
- Add calibration ingestion, provenance propagation, and deterministic
  time-varying wind without collapsing the runtime-versus-truth-model split.

## Later Backlog
- Keep `R-024` and `R-026` as cross-cutting guardrails rather than the next
  headline delivery items.
- `R-025`, `R-027`, `R-028`, `R-029`, and `R-030` remain deferred until the
  near-term slices settle.
- Add real component-prefixed code paths and only add simulator-specific
  auxiliary or regression lanes when they improve verification materially.
