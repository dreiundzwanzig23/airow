# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- Slice 0 closed on 2026-04-06 with `R-034`, additive summary-analysis
  output, optional CLI report modes, and an offline static report tool.
- Slice 1 closed on 2026-04-06 with top-level provider selection, built-in
  provider composition on the shared run path, and structured validity
  metadata in machine-readable outputs.
- Slice 2 closed on 2026-04-18 with the current reduced hydro and steady-wind
  aero built-in providers declared sufficient as the supported deterministic
  default-runtime baseline.
- Slice 3 closed on 2026-04-18 with built-in backend selection under `A-010`:
  `sundials_ida` is the required default runtime path, `deterministic_baseline`
  remains an explicit fallback, `chrono_rigidbody` remains optional and
  build-gated, and machine-readable outputs now stamp stable backend policy and
  solver-status metadata.

## Post-`v0.1` Slices

### Slice 0 — Human-Readable Run Analysis
- Closed `R-034` with CLI report modes, additive summary-analysis output, and
  an offline static report generator.

### Slice 1 — Runtime-Selectable Providers and Validity Metadata
- Closed `R-020` and `R-033` with top-level built-in provider selection,
  shared-run provider construction, and structured per-role validity
  metadata.

### Slice 2 — Reduced-Model Fidelity Expansion
- Closed on 2026-04-18 without changing the stable built-in ids.
- Primary owners remain `A-004` and `A-005`.
- Current policy: `quadratic_drag_placeholder`,
  `stroke_propulsion_placeholder`, and `steady_wind_placeholder` remain the
  supported reduced default-runtime baseline with stable config and metadata
  contracts.
- Future hydro or aero expansion should land only as a new explicit packet,
  not as an open-ended continuation of Slice 2.

### Slice 3 — External Backend Wiring and Backend Selection
- Closed on 2026-04-18.
- Primary owner remains `A-010`, coordinated with `A-003` and `A-002`.
- Current policy:
  - `simulation.state_advancer` stays the only user-facing selector.
  - `sundials_ida` is the required supported default runtime path with fixed
    built-in tolerances.
  - `deterministic_baseline` remains the explicit non-SUNDIALS fallback.
  - `chrono_rigidbody` remains optional, build-gated, and bounded to the
    current passive-float and tow evidence level.
  - JSON and HDF5 outputs now carry structured built-in state-advancer policy
    metadata plus startup and runtime solver-status fields.
- Future backend work should land as new explicit packets under `A-010`, not
  as an open-ended continuation of Slice 3.

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
