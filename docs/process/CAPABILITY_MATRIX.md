# Physics Capability Matrix

This matrix explains the current simulator capability envelope for Phase 1
trust reporting. It is descriptive only: it does not promote requirement
statuses or change simulation numerics.

Status vocabulary:
- `current support`: implemented, exercised by checked-in reduced-runtime
  evidence, and suitable for the stated study questions.
- `implemented-but-unvalidated`: implemented or partially emitted, but not yet
  backed by enough scenario evidence to support broad study claims.
- `planned`: in the roadmap or requirements backlog, but not active capability.
- `explicitly unsupported`: outside the active single-scull reduced-runtime
  scope and rejected or documented as unavailable.

| Area | Status | Current capability | Limits and trust notes |
|---|---|---|---|
| Hull physics | current support | Reduced single-scull hydrostatic restoring and longitudinal drag providers for passive float, tow, and calm-water baseline studies. | No wave radiation, slamming, off-axis calibrated resistance, SPH, or CFD fidelity is claimed. |
| Blade physics | current support | Reduced immersion-aware stroke propulsion provider with deterministic slip, work, and efficiency reporting. | Detailed blade-water flow, validated blade coefficients, ventilation, and turbulence are not claimed. |
| Rower coupling | implemented-but-unvalidated | Low-order rower center-of-mass coupling from configured seat motion can feed inertial load outputs. | This is not a full biomechanics or technique-optimization model. |
| Oar flexibility | planned | The architecture keeps flexible-oar expansion staged behind mechanics and output contracts. | Current runtime treats oars as rigid reduced-model bodies. |
| Aero | current support | Reduced steady apparent-wind aero is scenario-backed for headwind and crosswind runs; one calibrated reduced steady-wind aero provider can consume a validated calibration artifact. | Gust dynamics, full CFD, and broad aero validation are not claimed. Calibrated aero trust is artifact-declared. |
| Waves/current | planned | Environmental disturbance expansion is part of the full-simulation roadmap. | Current default runtime does not model waves or current. |
| Visualization | planned | Outputs carry frame, unit, provider, and trust metadata needed by future playback artifacts. | Interactive 3D playback and visualization-ready run bundles remain future work. |
| Validation | current support | Checked-in passive-float, tow, calm-water stroke, headwind, crosswind, gust-headwind, performance-budget, and comparison scenarios protect the reduced baseline. | Scenario evidence validates the reduced runtime envelope, not full real-world rowing fidelity. |
| Calibration | implemented-but-unvalidated | File-backed calibration, measurement-bundle, and measured-trial artifacts carry provenance and reference-contract metadata; calibrated steady-wind aero can use imported coefficients. | Calibration lifecycle, fit-quality reporting, hydro-side calibrated providers, and broad artifact scorecards remain open. |
| Truth-model workflows | implemented-but-unvalidated | Optional JSON truth-model handoff exports runtime inputs and provenance for offline studies. | Offline truth-model generation, re-import loops, and high-fidelity toolchain integration are not part of the default runtime. |

For current run artifacts, `metadata.trust_envelope` and the
`Physics Capability and Trust` report section are the authoritative run-level
summary. Provider-level metadata remains under `metadata.providers.<role>`.
