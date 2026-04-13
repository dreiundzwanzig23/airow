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
- Slice 3 started on 2026-04-13 with validated `simulation.state_advancer`
  selection, built-in advancer construction, and a guarded
  `chrono_rigidbody` path; Chrono-enabled acceptance evidence is still
  pending a Chrono-capable build.

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
- The first packet is landed: `simulation.state_advancer` defaults to
  `deterministic_baseline` and can request `chrono_rigidbody`, which is
  rejected deterministically when Chrono support is absent.
- Keep this slice anchored in `A-010`, coordinated with `A-003` and `A-002`,
  and separate from hydro or aero provider selection.
- Next step: validate the Chrono path on Chrono-enabled builds against
  passive-float and tow evidence before broadening mechanics scope.

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
