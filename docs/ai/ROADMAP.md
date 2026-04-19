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
- Owners: `A-004`, `A-005`.
- Guardrail: keep the stable built-in ids and metadata contracts intact.
### Slice 3 — External Backend Wiring and Backend Selection
- Closed on 2026-04-18.
- Owners: `A-010`, coordinated with `A-003`, `A-002`.
- Policy: `simulation.mechanics_backend` and
  `simulation.integration_backend` stay public; preferred runtime remains
  `chrono_rigidbody + sundials_ida`; supported fallbacks remain
  `internal_baseline + sundials_ida` and
  `internal_baseline + deterministic_baseline`; unsupported
  `chrono_rigidbody + deterministic_baseline` stays rejected.

### Slice 4A — Calibration Ingestion and Provenance
- Closed on 2026-04-19 as the first `A-009` packet.
- Owners: `A-009`, coordinated with `A-001`, `A-005`, `A-007`.
- Scope: deterministic file-backed calibration loading, required provenance
  validation, one `steady_wind_calibrated` provider path, and output
  provenance propagation.
- Guardrail: keep the default runtime usable without imported artifacts.

### Slice 4B — Time-Varying Wind Input
- Closed on 2026-04-19.
- Owners: `A-005`, `A-002`, with `A-001` on schema and `A-006` retaining
  future wind-aware scheduling hooks.
- Scope: exclusive steady/series/profile wind modes, per-step sampling,
  steady-wind parity for constant inputs, emitted ambient-wind channels, and
  the checked-in `gust_headwind_stroke` replay scenario.
- Guardrail: keep runtime-versus-truth-model separation intact.

### Slice 4C — Batch Parameter Sweep Execution
- Closed on 2026-04-19.
- Primary owners are `A-001`, `A-002`, and `A-007`.
- Scope: top-level ordered `batch.cases`, per-case overrides, sequential reuse
  of the shared single-run path, deterministic batch-summary artifacts, and
  CLI batch auto-detection without batch `--report` support.
- Guardrail: keep batch orchestration sequential on the existing run path.

### Guardrail Packet — Runtime Separation and Scenario Budgets
- Closed on 2026-04-19.
- Primary owners are `A-001`, `A-002`, `A-007`, `A-008`, and `A-009`.
- Scope: optional `output.truth_model_export_path`, one deterministic JSON
  truth-model handoff artifact, documented offline re-import boundary on the
  existing calibrated-artifact path, checked-in
  `scenarios/performance_budgets.json`, and a dedicated
  `./scripts/test_performance.sh` lane with a machine-readable budget report.
- Guardrail: keep optional truth-model tooling out of the default runtime and
  keep protected-scenario budget checks out of the quick TDD lane.

## Later Backlog
- `R-027`, `R-028`, `R-029`, and `R-030` remain deferred until the near-term
  slices settle.
- Add real component-prefixed code paths and only add simulator-specific
  auxiliary or regression lanes when they improve verification materially.
