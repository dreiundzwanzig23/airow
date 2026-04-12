# MODEL_FIDELITY.md

This document defines fidelity expectations for the rowing simulator.

## Runtime vs truth models
- The default runtime uses reduced-order hydro and aero models around the 3D
  mechanics core.
- Optional high-fidelity SPH, CFD, or calibration-generation workflows remain
  offline by default.
- The runtime path must stay usable without optional high-fidelity toolchains
  installed.

## Closure model expectations
- Every reduced model should state its intended validity range.
- Every reduced runtime provider should expose validity metadata suitable for
  configuration validation and run reporting.
- Every calibrated model should identify its calibration source or artifact.
- Runtime provider selection must remain explicit and configuration-controlled.

## Guardrails
- Do not make optional truth-model tooling a hidden runtime dependency.
- Do not treat a calibration artifact as self-describing unless its provenance
  metadata is present.
- Do not ship a reduced provider without explicit validity-range semantics or
  equivalent machine-readable validity metadata.
