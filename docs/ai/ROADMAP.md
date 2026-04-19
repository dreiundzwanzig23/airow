# ROADMAP.md

## Status
- The `v0.1` cut line is complete as of 2026-04-05.
- Slice 0 and Slice 1 are closed.
- Slice 2 is closed with the current reduced hydro and steady-wind built-in
  providers as the supported deterministic default-runtime baseline.
- Slice 3 is closed under `A-010` with
  `chrono_rigidbody + sundials_ida` as the preferred supported runtime and
  bounded fallback pairs.

## Post-`v0.1` Slices

### Slice 2 — Reduced-Model Fidelity Expansion
- Closed on 2026-04-18 without changing the stable built-in ids.
- Owners: `A-004` and `A-005`.
- Policy: `quadratic_drag_placeholder`, `stroke_propulsion_placeholder`, and
  `steady_wind_placeholder` remain the supported reduced baseline with stable
  config and metadata contracts.

### Slice 3 — External Backend Wiring and Backend Selection
- Closed on 2026-04-18.
- Owner: `A-010`, coordinated with `A-003` and `A-002`.
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
  - JSON and HDF5 outputs carry structured mechanics-backend and
    integration-backend policy metadata plus startup and runtime solver-status
    fields.

### Slice 4A — Calibration Ingestion and Provenance
- Closed on 2026-04-19 as the first `A-009` packet.
- Primary owner is `A-009`, coordinated with `A-001`, `A-005`, and `A-007`.
- Delivered scope:
  - deterministic file-backed external calibration artifact loading,
  - required provenance validation before runtime stepping,
  - one explicit `steady_wind_calibrated` aero provider path without changing
    the current baseline provider ids,
  - propagation of used-artifact provenance into JSON and HDF5 outputs.
- Guardrail retained:
  - keep the default runtime usable without any external artifact or optional
    truth-model toolchain.

### Slice 4B — Time-Varying Wind Input
- Deferred until Slice 4A stabilizes.
- Primary owners will be `A-005`, `A-002`, and `A-006`.
- Target scope:
  - add deterministic time-varying wind definitions and replay on the shared
    run path,
  - preserve steady-wind parity for constant time-series inputs,
  - keep runtime-versus-truth-model separation intact.

## Later Backlog
- Keep `R-024` and `R-026` as cross-cutting guardrails rather than the next
  headline delivery items.
- `R-025`, `R-027`, `R-028`, `R-029`, and `R-030` remain deferred until the
  near-term slices settle.
- Add real component-prefixed code paths and only add simulator-specific
  auxiliary or regression lanes when they improve verification materially.
