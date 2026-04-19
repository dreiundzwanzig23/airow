# MODEL_FIDELITY.md

This document defines fidelity expectations for the rowing simulator.

## Runtime vs truth models
- The default runtime uses reduced-order hydro and aero models around the 3D
  mechanics core.
- Optional high-fidelity SPH, CFD, or calibration-generation workflows remain
  offline by default.
- The runtime path must stay usable without optional high-fidelity toolchains
  installed.
- The first offline handoff contract is a one-way JSON export requested only by
  `output.truth_model_export_path`; runtime re-import stays on the existing
  validated calibration-artifact path.

## Fidelity tiers
- The current validated runtime is the reduced-model baseline used for
  deterministic single-scull execution and bounded scenario evidence.
- The next planned fidelity tier is a calibrated reduced runtime that still
  runs inside the default simulator loop while consuming richer measured-data
  and calibration artifacts.
- Offline truth models remain a separate optional tier used to generate,
  compare, or fit reduced artifacts rather than a replacement runtime.
- User-facing docs, scenario evidence, and run reports must distinguish these
  tiers explicitly; do not blur a calibrated reduced model into an online
  truth-model claim.

## Closure model expectations
- Every reduced model should state its intended validity range.
- Every reduced runtime provider should expose validity metadata suitable for
  configuration validation and run reporting.
- The current reduced default-runtime baseline is the built-in
  `quadratic_drag_placeholder`, `stroke_propulsion_placeholder`, and
  `steady_wind_placeholder` provider set; treat that baseline as sufficient
  unless a later scoped packet explicitly reopens hydro or aero behavior.
- Every calibrated model should identify its calibration source or artifact.
- Runtime provider selection must remain explicit and configuration-controlled.
- Future fidelity expansion should deepen the calibrated reduced runtime before
  broadening the offline truth-model consumer surface.

## Guardrails
- Do not make optional truth-model tooling a hidden runtime dependency.
- Do not treat truth-model export as a trigger for running optional offline
  tooling inside the default runtime.
- Do not treat a calibration artifact as self-describing unless its provenance
  metadata is present.
- Do not ship a reduced provider without explicit validity-range semantics or
  equivalent machine-readable validity metadata.
