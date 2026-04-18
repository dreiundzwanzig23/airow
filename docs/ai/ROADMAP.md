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
- Slice 3 closed on 2026-04-18 as the composed backend packet under `A-010`:
  the preferred supported runtime is now `chrono_rigidbody + sundials_ida`,
  `internal_baseline + sundials_ida` remains the supported fallback,
  `internal_baseline + deterministic_baseline` remains the deterministic debug
  fallback, and machine-readable outputs now stamp split mechanics and
  integration backend policy plus solver-status metadata.

## Post-`v0.1` Slices

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
  - `simulation.mechanics_backend` and
    `simulation.integration_backend` are the public selectors.
  - the preferred supported runtime is `chrono_rigidbody + sundials_ida`,
    with fixed built-in `rtol = 1e-10` and `atol = 1e-10`.
  - `internal_baseline + sundials_ida` remains the supported fallback and
    cross-check mode.
  - `internal_baseline + deterministic_baseline` remains the explicit
    deterministic debug fallback.
  - `chrono_rigidbody + deterministic_baseline` is rejected deterministically.
  - standard non-libc++ builds now require Chrono, while libc++ sanitizer and
    coverage lanes remain explicit no-Chrono verification exceptions.
  - JSON and HDF5 outputs now carry structured mechanics-backend and
    integration-backend policy metadata plus startup and runtime solver-status
    fields.
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
