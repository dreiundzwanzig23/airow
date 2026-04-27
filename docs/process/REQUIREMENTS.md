<!-- TRACE-SYNTAX: REQUIREMENTS
Each requirement must use this block format:

## R-xyz — <Title>
- **Title**: <short summary>
- **Acceptance Criteria**:
  - <bullet, observable>
- **Priority**: P0|P1|P2
- **Status**: OPEN|IN_PROGRESS|DONE
- **Created**: YYYY-MM-DD
- **Updated**: YYYY-MM-DD
- **Change-Type**: none|editorial|semantic
- **Needs-Review**: yes|no
- **Change-Note**: <optional text>
- **Notes**: <optional text>

Validation policy:
- DONE requirements must be satisfied by at least one architecture item.
- IN_PROGRESS requirements should be satisfied by at least one architecture item.
- OPEN requirements may exist without architecture mapping during backlog refinement.
- `Change-Type: semantic` requires `Needs-Review: yes`.
-->

# REQUIREMENTS_ROWING_SIMULATOR.md — Rowing Simulator Requirements Draft

## Scope and backlog intent

This document is a requirement set with an initial,
traceable backlog for an open-source rowing simulator.

Initial product scope:
- single scull only,
- headless execution first,
- full 3D mechanics,
- reduced hydro and aero in the runtime loop,
- deterministic outputs and strong automated verification,
- optional offline calibration and high-fidelity workflows kept separate from
  the default runtime path.

Requirement-writing intent:
- requirements are externally observable and verifiable,
- requirements must not be shaped as thin mirrors of architecture items,
- multiple related requirements are expected to allocate into shared `A-*`
  subsystem themes.

Milestone framing:
- `v0.1` should deliver a deterministic headless baseline with validated
  configuration, in-memory and CLI execution, stable startup, 3D single-scull
  mechanics, reduced hull and blade runtime models, steady-wind aero, named
  baseline scenarios, and structured machine-readable outputs.
- `v0.1` should not depend on external calibration ingestion, time-varying
  wind, batch sweeps, low-order balance control, flexible oars, or disturbance
  inputs beyond the steady baseline cases.
- The next fidelity phase should preserve that closed baseline while extending
  the simulator toward research-oriented hull-performance and stroke-dynamics
  studies through milestone-scoped reduced-model, calibration, and reporting
  work.
- The next fidelity phase must keep the validated reduced baseline,
  future calibrated reduced runtime, and optional offline truth-model
  workflows explicitly distinct.

## R-001 — Configuration Loading and Validation
- **Title**: Load and validate simulation configuration deterministically
- **Acceptance Criteria**:
  - A simulation can be started from a machine-readable configuration file.
  - Missing required fields are rejected with a deterministic message that identifies the failing field path.
  - Invalid numeric values, including NaN, infinity, negative duration, and negative mass, are rejected before time stepping starts.
  - Accepted configuration values are echoed in normalized form in run metadata.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-03
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Initial scope uses SI units only. The first implementation slice covers deterministic JSON loading, validation, and normalized metadata; full simulation execution remains with `R-002`.

## R-002 — Headless Simulation Execution
- **Title**: Execute one simulation without any GUI dependency
- **Acceptance Criteria**:
  - A simulation can be launched non-interactively from a CLI entry point.
  - The same run path is callable from automated tests without spawning a GUI.
  - Successful runs return a success status and failed runs return a non-zero failure status.
  - Run metadata includes simulator version, configuration identifier, and start and end timestamps.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-03
- **Change-Type**: editorial
- **Needs-Review**: no
- **Notes**: The current implementation slice introduces the first shared
  single-run path for CLI and in-memory execution with deterministic metadata
  and exit-code behavior before real mechanics or machine-readable artifact
  emission land. In the current repository state, "simulation" here means the
  headless execution of the validated runtime path, not yet a mechanics-backed
  rowing-physics model.

## R-003 — In-Memory Simulation API
- **Title**: Provide a library API suitable for tests and harnesses
- **Acceptance Criteria**:
  - A test can execute a simulation in memory without shelling out to the CLI.
  - The API returns structured results including status, summary metrics, and diagnostics.
  - The API supports injection of deterministic stub or mock hydro and aero providers.
  - At least one automated test uses the in-memory API with stub providers.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-03
- **Change-Type**: editorial
- **Needs-Review**: no
- **Notes**: This requirement exists primarily to improve testability and
  agentic workflow ergonomics. The first implementation slice uses injected
  deterministic hydro and aero stub interfaces around a bounded single-run
  loop before mechanics-state contracts exist. In the current repository
  state, the API guarantees structured execution and test seams, not yet a
  mechanics-backed physics result.

## R-004 — Deterministic Replay
- **Title**: Reproduce the same run deterministically on the same platform
- **Acceptance Criteria**:
  - Re-running the same configuration on the same executable and platform produces identical run metadata except for wall-clock timestamps.
  - Summary metrics, including final simulated time, traveled distance, and mean speed, match within a documented numerical tolerance.
  - Output record ordering is deterministic.
  - A replay test exists that runs the same case at least twice and compares the results.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-04
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline runtime now accepts explicit world-frame ambient wind
  input, computes apparent wind deterministically through the shared aero seam,
  and includes unit coverage for headwind, tailwind, and crosswind direction
  cases.

## R-005 — Single-Scull 3D Hull Model
- **Title**: Represent the shell as a 3D rigid body with configurable mass properties
- **Acceptance Criteria**:
  - The simulator models one hull body with configurable mass, center of mass, and inertia.
  - Hull state outputs include position, orientation, linear velocity, and angular velocity in 3D.
  - A static initialization with valid parameters completes without non-finite state values.
  - Invalid hull mass properties are rejected during configuration validation.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Initial scope is single scull only. The `v0.1` closure now
  includes requirement-level evidence for finite hull startup, emitted 3D hull
  state channels, and deterministic rejection of invalid hull mass properties.

## R-006 — Port and Starboard Oar Kinematics
- **Title**: Model two independent sculling oars with configurable geometry
- **Acceptance Criteria**:
  - The simulator includes separate port and starboard oars.
  - Oar geometry includes configurable inboard length, outboard length, and oarlock location.
  - Time-series outputs include each oar’s kinematic state.
  - Constraint or geometry residuals remain below a documented tolerance in a nominal run.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The `v0.1` closure now includes requirement-level evidence that
  port and starboard oar state is emitted separately through the public output
  contract and that nominal constraint residuals remain bounded in the
  deterministic baseline run path.

## R-007 — Seat Translation Model
- **Title**: Model seat motion along a configurable rail axis
- **Acceptance Criteria**:
  - Seat position is represented as a separate state variable constrained to a configured rail direction.
  - Seat travel limits can be configured and are enforced.
  - Time-series outputs include seat position and velocity.
  - An out-of-range initial seat position is rejected before simulation begins.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The `v0.1` closure now includes requirement-level evidence for
  emitted seat position or velocity channels, bounded seat travel during
  nominal runs, and deterministic rejection of out-of-range initial seat
  positions.

## R-008 — Prescribed Stroke Schedule
- **Title**: Drive the system with a deterministic prescribed stroke input
- **Acceptance Criteria**:
  - The simulator accepts a periodic stroke definition with configurable stroke rate and phase timing.
  - The prescribed stroke can be replayed for at least ten cycles without phase drift beyond a documented tolerance.
  - Invalid schedules, including non-positive cycle time or inconsistent phase durations, are rejected.
  - A nominal stroke test produces finite seat and oar trajectories for the full run.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Initial scope uses prescribed motion or control, not full musculoskeletal actuation. The current slice introduces deterministic periodic stroke timing and baseline oar or seat kinematic replay behind the mechanics seam before propulsion scenarios land.
  The `v0.1` closure now includes ten-cycle requirement-level replay evidence
  with deterministic phase wrapping and schedule-rejection coverage.

## R-009 — Hydrostatic Float Equilibrium
- **Title**: Support calm-water floating equilibrium for the hull
- **Acceptance Criteria**:
  - With zero stroke and zero wind, the simulator can initialize and maintain a calm-water floating state for a valid hull setup.
  - At the reported equilibrium state, net vertical force and restoring moments are within documented tolerances.
  - A passive float test completes without non-finite values or runaway drift.
  - Output includes equilibrium draft or immersion-related diagnostics.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-04
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline `A-004` slice now reports deterministic reduced
  hydrostatic heave and restoring-moment behavior around the nominal waterline,
  with passive-float acceptance checks on final vertical force, restoring
  moments, and final hull `z` diagnostics.

## R-010 — Reduced Hull-Water Resistance
- **Title**: Apply a reduced hydrodynamic resistance model to hull motion
- **Acceptance Criteria**:
  - For a prescribed forward towing speed in calm water, the model returns a drag force opposing the direction of motion.
  - Over a documented calibration range, drag magnitude increases monotonically with towing speed.
  - At zero forward speed, the forward drag component is zero or within a documented near-zero tolerance.
  - A tow-test scenario produces a reproducible drag-versus-speed curve.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-10
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline `A-004` slice now applies deterministic reduced
  hull drag through the shared hydro seam with monotonic speed-squared
  resistance plus a low-speed damping term on the built-in runtime provider,
  while preserving the checked-in tow scenario envelope and monotonic
  drag-speed evidence.

## R-011 — Reduced Blade-Water Force Model
- **Title**: Compute blade hydrodynamic loads from blade state and relative water motion
- **Acceptance Criteria**:
  - Blade load computation depends on at least immersion state, blade orientation, and relative blade-water velocity.
  - A fully dry blade produces near-zero hydrodynamic load.
  - At fixed immersion and orientation, increasing relative blade-water speed increases force magnitude over a documented range.
  - The blade force provider can be exercised independently in automated tests.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-10
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline `A-004` slice now computes deterministic reduced
  blade-water loads from explicit blade immersion, drive-phase shaping,
  handle-angle orientation surrogate, and relative blade-water speed, with
  dry-blade and monotonic speed evidence plus calm-water scenario coverage.

## R-012 — Self-Propelled Stroke Response
- **Title**: Produce forward motion from a valid stroke in calm water
- **Acceptance Criteria**:
  - With a valid prescribed stroke and active blade forces, the boat achieves positive mean forward displacement over multiple cycles in calm water.
  - Time-series outputs for boat speed, blade loads, and power remain finite for the full run.
  - Disabling blade hydrodynamic forces while keeping the same stroke reduces mean boat speed relative to the baseline case.
  - A calm-water propulsion scenario is included in automated regression tests.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-10
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline runtime now includes deterministic structured blade
  load propagation, a calm-water stroke scenario artifact, and regression
  coverage proving positive mean propulsion with phase-shaped placeholder
  blade loads plus reduced mean speed when those blade loads are disabled.

## R-013 — Apparent Wind Computation
- **Title**: Compute apparent wind from ambient wind and boat motion
- **Acceptance Criteria**:
  - The simulator accepts a 3D ambient wind input.
  - Apparent wind is computed from the relative velocity between ambient air and the boat state.
  - With zero ambient wind, apparent wind equals the negative of the boat air-relative velocity within tolerance.
  - Unit tests cover headwind, tailwind, and crosswind direction cases.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-04
- **Change-Type**: none
- **Needs-Review**: no

## R-014 — Aerodynamic Load Model
- **Title**: Apply aerodynamic drag and wind-induced moments to the system
- **Acceptance Criteria**:
  - The simulator computes aerodynamic loads from apparent wind on the boat-plus-rower aggregate.
  - Zero apparent wind produces zero or near-zero aerodynamic load.
  - Increasing headwind at fixed stroke conditions reduces mean boat speed relative to a calm-air baseline.
  - Mirroring a steady crosswind direction changes the sign of the yawing moment.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-10
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline `A-005` slice now applies deterministic reduced
  steady-wind loads through the shared aero seam with explicit apparent-wind
  reporting, stronger low-apparent-wind headwind drag sensitivity, explicit
  lateral crosswind force, and mirrored yaw-sign behavior on the built-in
  runtime provider while preserving the existing provider id and structured
  output metadata.

## R-015 — Machine-Readable Outputs
- **Title**: Emit structured outputs for analysis and regression testing
- **Acceptance Criteria**:
  - Each run writes a machine-readable summary file.
  - Each run can write machine-readable time-series data for at least hull state, oar state, seat state, boat speed, hull-water loads, blade loads, aerodynamic loads, and rower-input or stroke-power accounting channels.
  - Output files include a configuration identifier and simulator version.
  - Vector quantities in machine-readable outputs identify their units and reference frame, and derived power or accounting channels identify their units explicitly.
  - The simulator can be configured to enable or disable high-frequency time-series output.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-03
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The baseline contract now supports deterministic structured summary
  and time-series artifacts with explicit unit or frame annotations,
  configuration-controlled high-frequency sampling, and output-format
  selection (`json`, `hdf5`, or both). HDF5 output is optional and rejected
  deterministically at configuration parse time when unavailable in the build.

## R-016 — Runtime Diagnostics and Safe Failure
- **Title**: Detect invalid runtime conditions and fail with actionable diagnostics
- **Acceptance Criteria**:
  - The simulator detects non-finite state values during execution.
  - The simulator detects invalid subsystem outputs, including non-finite forces from hydro or aero providers.
  - A failed run terminates with a stable diagnostic code and subsystem-specific message.
  - Automated tests induce at least one representative failure mode and verify the reported diagnostic.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The `v0.1` closure now includes requirement-level evidence for
  deterministic provider-failure diagnostics, detection of non-finite
  boundary-visible runtime state, and stable subsystem-specific error mapping.

## R-017 — Units and Numeric Safety
- **Title**: Enforce SI units and numeric safety constraints at the simulator boundary
- **Acceptance Criteria**:
  - External configuration and output quantities use documented SI units.
  - Unit-bearing fields in configuration and output documentation include their units explicitly.
  - Invalid or ambiguous unit-bearing inputs are rejected before time stepping starts.
  - Automated checks verify that core scenario configurations use only documented units.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is complemented by repo-level numerics policy and
  is now closed for `v0.1` with requirement-level evidence that checked-in
  baseline scenarios normalize to the documented unit set, output artifacts
  carry explicit units or frames, and ambiguous typed inputs are rejected
  deterministically.

## R-018 — Reference Validation Scenarios
- **Title**: Provide a baseline set of repeatable validation scenarios
- **Acceptance Criteria**:
  - The repository includes at least these named scenarios: passive float, tow test, calm-water stroke, headwind stroke, and crosswind stroke.
  - Each reference scenario has a checked-in configuration and documented numeric acceptance envelope.
  - Reference scenarios are runnable headlessly in automated verification.
  - A failed acceptance check causes the verification job to fail.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-04
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Passive float, tow test, calm-water stroke, headwind stroke, and
  crosswind stroke are now checked in with documented envelopes and
  runtime-backed verification coverage.

## R-019 — Verification Traceability for P0 Requirements
- **Title**: Trace each reviewed P0 requirement to at least one automated verification artifact
- **Acceptance Criteria**:
  - Every P0 requirement that is not marked `Needs-Review: yes` has at least one named automated test case or scenario associated with it.
  - Test names or metadata include the requirement identifier.
  - The trace from requirement identifier to verification artifact is machine-searchable in the repository.
  - Missing trace for any reviewed P0 requirement causes a verification check to fail.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-24
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This is a workflow-support requirement rather than a physics
  requirement. The `v0.1` closure now includes requirement-level evidence that
  `tools/tracecheck.py --json` exposes machine-searchable trace data and that
  every reviewed `P0` requirement has requirement-level `QT-*` coverage. The
  2026-04-27 review confirmed `Needs-Review: yes` roadmap items remain
  intentionally excluded from reviewed-P0 closure until individually allocated
  and implemented.

## R-020 — Runtime-Selectable Hydro and Aero Providers
- **Title**: Select reduced-model providers at runtime without recompilation
- **Acceptance Criteria**:
  - A run configuration can select the active hull resistance, blade-force, and aerodynamic load providers without recompilation.
  - The simulator reports the selected providers in run metadata.
  - Providers that implement the same contract satisfy a shared integration test suite.
  - Selecting an unknown provider causes deterministic configuration rejection.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-06
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The runtime now validates a top-level `providers` block for
  `hull_resistance`, `blade_force`, and `aero_load`, constructs the selected
  built-in reduced providers without recompilation on the shared run path, and
  records the selected role ids in structured run metadata.

## R-021 — External Calibration Dataset Ingestion
- **Title**: Load external calibration data for hydrodynamic or aerodynamic models
- **Acceptance Criteria**:
  - The simulator accepts an external machine-readable dataset for calibration or lookup use.
  - Malformed or incomplete calibration datasets are rejected deterministically with a schema-specific error.
  - Loaded datasets are queryable by the configured provider during a run.
  - At least one automated test loads a minimal valid dataset and verifies a successful query.
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Not part of the `v0.1` cut line. Slice 4A now delivers one
  deterministic file-backed calibration artifact path on the shared runtime:
  the simulator validates a versioned machine-readable artifact before
  stepping, exposes the loaded coefficients through one explicit calibrated
  aero provider, and keeps the existing default-runtime baseline provider ids
  unchanged.

## R-022 — Calibration Provenance Metadata
- **Title**: Preserve provenance for imported calibration and fitted-model artifacts
- **Acceptance Criteria**:
  - Imported calibration artifacts include source identifier, artifact version, and a content hash or equivalent immutable identifier.
  - Run metadata records the identifiers of any external calibration artifacts used during execution.
  - Missing required provenance metadata causes deterministic rejection of the artifact.
  - A regression test verifies that provenance metadata is propagated into run outputs.
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Not part of the `v0.1` cut line. Slice 4A now requires imported
  calibration artifacts to declare `source_id`, `artifact_version`,
  `content_hash`, and `schema_id`, rejects partially specified provenance
  deterministically, and propagates used-artifact identifiers into JSON and
  HDF5 run metadata on the calibrated runtime path.

## R-023 — Time-Varying Wind Input
- **Title**: Support deterministic time-varying wind for gust and transition studies
- **Acceptance Criteria**:
  - The simulator accepts a time-varying wind definition or sampled wind time series.
  - For a constant wind time series, the computed aerodynamic loads match the constant-wind model within tolerance.
  - Replaying the same wind time series yields deterministic results on the same platform.
  - At least one regression scenario includes a non-constant wind input.
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-27
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Delivered post-`v0.1` through an exclusive `environment`
  wind-input contract: replay-oriented `wind_time_series` with zero-order hold
  or authored `wind_profile` with deterministic linear interpolation. Removed
  legacy constant `ambient_wind_world_mps`; constant wind is represented by a
  single-sample series or equivalent constant profile. Constant series or
  profile inputs preserve steady-wind parity, and
  `scenarios/gust_headwind_stroke.json` provides the first checked-in
  non-constant regression artifact. The 2026-04-27 review confirmed current
  wind evidence remains intact under the narrowed contract.

## R-024 — Runtime and Truth-Model Separation
- **Title**: Keep the default runtime path independent of optional high-fidelity toolchains
- **Acceptance Criteria**:
  - The default simulation runtime executes without requiring installation of optional SPH, CFD, or calibration-generation toolchains.
  - Any optional high-fidelity export or import path is configuration-controlled and disabled by default.
  - A documented workflow exists to export inputs for offline high-fidelity studies and re-import derived artifacts.
  - Automated verification includes at least one run that proves the default runtime path works without optional tooling present.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The shared runtime now keeps optional truth-model handoff export
  behind `output.truth_model_export_path`, which stays disabled by default and
  emits one deterministic JSON input bundle only when requested. The default
  runtime continues to execute without optional truth-model tooling installed,
  and the documented re-import path remains the existing validated
  `steady_wind_calibrated` plus `artifacts.calibration.path` contract rather
  than a new runtime consumer. Requirement-level closure now rests on
  `QT-041` for the export path and the existing calibrated-artifact runtime
  evidence in `QT-038`.

## R-025 — Batch Parameter Sweep Execution
- **Title**: Run multiple simulation cases in one headless batch job
- **Acceptance Criteria**:
  - A batch input can define multiple cases or parameter variations.
  - Each case produces a separate success or failure record without corrupting the results of other cases.
  - Batch summary output includes per-case identifiers and summary metrics.
  - Batch output ordering is deterministic for the same input ordering.
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Change-Note**: Repositioned batch execution behind the first single-run deterministic baseline.
- **Notes**: Delivered after the `v0.1` cut line as the closed Slice 4C batch-orchestration packet.

## R-026 — Scenario Performance Budget
- **Title**: Keep core verification scenarios within documented runtime budgets
- **Acceptance Criteria**:
  - At least one documented quick verification lane exists for local development.
  - Core reference scenarios specify approximate runtime budgets or upper bounds for the target development environment class.
  - A performance regression in a protected scenario is reported separately from functional test failures.
  - The quick lane remains usable without optional high-fidelity tooling.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The repository now carries a checked-in
  `scenarios/performance_budgets.json` manifest for the five protected core
  scenarios, a dedicated `./scripts/test_performance.sh` validation lane that
  reports budget status separately from ordinary functional failures, and a
  machine-readable budget report generated from the lane summary. The quick
  `./scripts/test_tdd.sh` lane remains free of these protected-scenario budget
  checks, while `test.sh` and `verify.sh` now run the dedicated performance
  lane explicitly. Requirement-level closure rests on `QT-042`.

## R-027 — Low-Order Balance Control
- **Title**: Support an optional low-order balance or stabilization controller for the rower model
- **Acceptance Criteria**:
  - The simulator can enable or disable a low-order balance controller by configuration.
  - With the controller disabled, the system still executes valid prescribed-motion scenarios intended for the uncontrolled mode.
  - With the controller enabled in a documented test scenario, reported control outputs remain finite and deterministic.
  - The selected controller mode is recorded in run metadata.
- **Priority**: P2
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement intentionally stops short of full musculoskeletal modeling and is not part of the `v0.1` cut line.

## R-028 — Optional Flexible Oar Model
- **Title**: Support an optional flexible oar representation without changing the default rigid-oar runtime path
- **Acceptance Criteria**:
  - The simulator can select rigid or flexible oar representation by configuration.
  - The default rigid-oar path remains the baseline configuration and requires no additional artifacts.
  - The flexible-oar mode emits additional state or diagnostic outputs relevant to oar deformation.
  - Unknown oar model selections are rejected deterministically during configuration validation.
- **Priority**: P2
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Not part of the `v0.1` cut line.

## R-029 — Optional Wave or Disturbance Input
- **Title**: Support optional environmental disturbance inputs beyond steady calm water
- **Acceptance Criteria**:
  - The simulator can enable or disable an environmental disturbance input by configuration.
  - With the disturbance input disabled, calm-water baseline scenarios remain unchanged within documented tolerance.
  - Disturbance-enabled runs record the disturbance source and parameters in run metadata.
  - At least one regression scenario exercises a non-calm environmental disturbance input.
- **Priority**: P2
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Not part of the `v0.1` cut line.

## R-030 — Multi-Rower and Non-Scull Expansion Readiness
- **Title**: Preserve a migration path toward future crew and sweep support without including it in the initial scope
- **Acceptance Criteria**:
  - Initial product documentation states that single scull is the supported baseline scope.
  - Configuration validation rejects unsupported boat classes in the initial release scope.
  - Public-facing documentation identifies crew and sweep support as future expansion rather than current capability.
  - Adding unsupported boat classes does not silently fall back to single-scull behavior.
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-04-01
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement protects scope clarity while keeping the architecture extensible. The current packet now formalizes one optional top-level `boat_class` selector at the configuration boundary, defaults omitted configs to `single_scull`, rejects any explicit unsupported boat class deterministically before runtime stepping, and keeps README scope wording aligned with future crew and sweep expansion remaining out of scope today.

## R-031 — State and Frame Conventions
- **Title**: Define explicit state, frame, and sign conventions at the simulator boundary
- **Acceptance Criteria**:
  - The simulator documentation defines the world frame, body frame, gravity direction, port and starboard sign convention, and orientation representation used at runtime boundaries.
  - Machine-readable outputs and configuration fields that carry vector quantities identify the reference frame expected for those quantities.
  - Wind, load, and moment direction cases are interpreted consistently with the documented conventions.
  - Automated verification covers at least one mirrored port and starboard direction case and one headwind, tailwind, or crosswind interpretation case.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-02
- **Updated**: 2026-04-04
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: `docs/process/STATE_CONVENTIONS.md` remains the source of truth,
  and the runtime now carries frame-annotated apparent-wind, aerodynamic-load,
  and aerodynamic-moment channels with mirrored crosswind verification.

## R-032 — Consistent Initialization and Startup Validity
- **Title**: Start each run from a numerically and mechanically valid initial state
- **Acceptance Criteria**:
  - A valid simulation run produces a finite, constraint-consistent initial state before time stepping begins.
  - Inconsistent initial states, invalid constraint setup, or non-converged startup conditions are rejected deterministically with startup-specific diagnostics.
  - Startup diagnostics report whether initialization succeeded and include solver or convergence status when applicable.
  - Automated verification includes at least one valid startup case and one rejected startup case.
- **Priority**: P0
- **Status**: DONE
- **Created**: 2026-04-02
- **Updated**: 2026-04-05
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: The `v0.1` closure now includes requirement-level evidence for
  both valid startup success metadata and deterministic startup-specific
  failure before time stepping begins.

## R-033 — Runtime Model Validity Metadata
- **Title**: Preserve validity-range metadata for reduced runtime providers
- **Acceptance Criteria**:
  - Each selectable reduced hull, blade, and aerodynamic provider exposes documented validity metadata or calibration-range metadata suitable for run reporting.
  - Run metadata records the selected provider identifiers together with their validity metadata identifiers or descriptors.
  - Provider definitions that omit required validity metadata are rejected deterministically before execution.
  - Automated verification checks that validity metadata is propagated into machine-readable outputs for at least one run.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-02
- **Updated**: 2026-04-06
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Built-in reduced runtime providers now expose catalog-backed
  validity metadata, configuration rejects unknown provider ids before
  execution, and machine-readable summary output propagates structured
  per-role provider validity descriptors.

## R-034 — Human-Readable Run Analysis
- **Title**: Surface simulator state and result envelopes in human-readable reports
- **Acceptance Criteria**:
  - A successful run can emit a human-readable compact report that highlights startup health, final state, and key state or load envelopes without requiring manual JSON inspection.
  - Machine-readable summary output includes derived analysis metrics computed from the full in-memory run result, including sample coverage, state envelopes, load envelopes, and peak timestamps.
  - An offline repository tool can turn emitted JSON artifacts into a static analysis report bundle with summary tables and plots for single-run inspection.
  - Low-frequency time-series output still preserves useful derived analysis metrics in the summary report.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-06
- **Updated**: 2026-04-06
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This slice is intentionally limited to single-run understanding.
  Run-to-run comparison and batch sweep analysis remain separate follow-on work.

## R-035 — Fidelity Trust Envelope for Study Claims
- **Title**: Make study fidelity scope and trust envelope explicit in docs and outputs
- **Acceptance Criteria**:
  - User-facing documentation defines the supported single-scull hull-performance and stroke-dynamics study questions for the active fidelity phase.
  - Machine-readable outputs and human-readable reports identify whether a run is operating as the validated reduced baseline, a calibrated reduced study path, or an out-of-validity use.
  - Project documentation distinguishes the current validated reduced runtime, future calibrated reduced runtime, and optional offline truth-model workflows without conflating them.
  - At least one checked-in validation artifact or report fixture ties a study conclusion to an explicit trust envelope.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement anchors trust and validity reporting for the full-simulation roadmap in `docs/process/ROADMAP_FULL_SIMULATION.md`.

## R-036 — Measurement Dataset Contract
- **Title**: Accept versioned measured boat, rigging, and athlete datasets for calibration-facing workflows
- **Acceptance Criteria**:
  - The simulator accepts a versioned machine-readable dataset bundle describing at least boat, rigging, and athlete parameters with explicit units and identifiers.
  - Missing required measurement fields, invalid units, or inconsistent identifiers are rejected deterministically with field-specific diagnostics.
  - Accepted datasets expose normalized identifiers and provenance metadata to runtime consumers and outputs.
  - At least one automated test loads a minimal valid measurement bundle and verifies normalized metadata.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This is the first calibration-foundation boundary for Milestone 2.

## R-037 — Measured Trial Time-Series Import
- **Title**: Import versioned measured rowing-trial time series for comparison and calibration
- **Acceptance Criteria**:
  - The simulator accepts a versioned measured-trial artifact with monotonic sample times and machine-readable channel descriptors.
  - The imported trial can carry boat-motion channels plus stroke-kinematic or measured load/power channels needed for later comparison workflows.
  - Duplicate timestamps, non-monotonic time bases, or unsupported channel definitions are rejected deterministically before execution.
  - Run metadata records the imported trial identifier and the aligned sample window when the artifact is used.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is separate from generic calibration artifacts because later measured-vs-sim comparison needs time-aligned trial semantics.

## R-038 — Boat, Rigging, and Mass-Distribution Parameterization
- **Title**: Represent hull, rigging, and mass-distribution parameter sets explicitly
- **Acceptance Criteria**:
  - A run configuration or imported artifact can specify hull mass properties, athlete mass-distribution parameters, and rigging geometry without editing source code.
  - Invalid geometry references, inconsistent frame definitions, or physically impossible parameter values are rejected deterministically before runtime stepping.
  - Machine-readable outputs record the active hull, rigging, and mass-distribution identifiers used for the run.
  - At least one validation artifact demonstrates that changing a parameter set changes reported trim, resistance, or performance metrics.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-19
- **Updated**: 2026-04-21
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement supports both hull-performance studies and later low-order rower coupling work.

## R-039 — Force- or Power-Driven Stroke Actuation
- **Title**: Support non-kinematic stroke actuation modes suitable for stroke-dynamics studies
- **Acceptance Criteria**:
  - A run can select a prescribed-kinematic, force-driven, or power-driven reduced stroke-actuation mode by configuration.
  - Invalid or overconstrained actuation definitions are rejected deterministically before stepping begins.
  - The selected actuation mode and its key bounded inputs are recorded in run metadata and outputs.
  - At least one automated validation scenario exercises a non-kinematic actuation mode without requiring a full biomechanics stack.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement begins Milestone 3 and intentionally remains lower order than a musculoskeletal rower model.

## R-040 — Low-Order Rower Inertial Coupling
- **Title**: Couple rower mass or center-of-mass motion into reduced stroke studies
- **Acceptance Criteria**:
  - The simulator can enable or disable a low-order rower inertial coupling model that changes hull load distribution or motion during the stroke.
  - With the coupling disabled, existing reduced-baseline prescribed-motion scenarios remain valid within documented tolerance.
  - With the coupling enabled under documented bounds, startup diagnostics and runtime outputs remain finite and deterministic.
  - Outputs report the rower inertial or center-of-mass contribution needed for later stroke-dynamics analysis.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement keeps rower coupling low-order and bounded; it is not a request for full-body biomechanics.

## R-041 — Blade Slip and Propulsion-Efficiency Metrics
- **Title**: Report blade slip, effective propulsive work, and loss metrics
- **Acceptance Criteria**:
  - Machine-readable outputs include per-run or intra-stroke blade-slip metrics and at least one effective propulsive-work or propulsion-efficiency metric.
  - Metric definitions identify the relevant frames, signs, or validity conditions.
  - Outputs mark the metric unavailable or out-of-validity when the active provider or data path cannot support it.
  - At least one analysis artifact compares these metrics across two stroke variants or provider modes.
- **Priority**: P1
- **Status**: DONE
- **Created**: 2026-04-19
- **Updated**: 2026-04-23
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Closed on 2026-04-23 with deterministic propulsion-metric
  support metadata across JSON, HDF5, and human-readable analysis plus one
  checked-in actuation-mode comparison artifact; fuller technique-comparison
  delta coverage remains open under `R-045`.

## R-042 — Blade Entry and Extraction Behavior
- **Title**: Represent catch, immersed drive, extraction, and dry-transition behavior in the reduced blade model
- **Acceptance Criteria**:
  - The active reduced blade-water model distinguishes catch, immersed drive, extraction, and recovery or dry transitions with finite deterministic loads.
  - Partial immersion, loss of immersion, and slip-direction changes are handled without singularities or silent sign flips.
  - Run outputs expose phase markers or transition timing needed for later technique analysis.
  - At least one validation artifact exercises sensitivity to catch or release timing.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement deepens the reduced blade model without requiring online free-surface truth-model execution.

## R-043 — Calibrated Reduced Blade-Water Provider
- **Title**: Support a calibrated reduced blade provider derived from explicit external artifacts
- **Acceptance Criteria**:
  - A run can select a calibrated reduced blade-water provider by configuration without recompilation.
  - The imported artifact exposes provenance, validity range, and calibration-error metadata that propagate into outputs.
  - Out-of-range use or missing calibration metadata is flagged deterministically before or during reporting.
  - At least one validation scenario compares calibrated reduced blade outputs against reference blade or propulsion data using documented agreement metrics.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This is the blade-side runtime consumer that turns the existing offline boundary into a real fidelity loop.

## R-044 — Calibrated Reduced Hull-Performance Provider
- **Title**: Support a calibrated reduced hull provider for resistance, trim, and loading studies
- **Acceptance Criteria**:
  - A run can select a reduced hull-performance provider that responds to speed, attitude, loading, or rigging parameters without recompilation.
  - The provider exposes provenance, validity range, and calibration metadata suitable for run reporting.
  - Machine-readable outputs report the hull-resistance or trim-related terms needed for hull-performance comparison.
  - At least one validation scenario demonstrates meaningful differences across two hull, loading, or rigging parameterizations.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement targets reduced calibrated runtime behavior, not online CFD in the default simulator loop.

## R-045 — Technique Comparison Scenarios
- **Title**: Provide named validation scenarios for technique and timing comparison questions
- **Acceptance Criteria**:
  - The scenario harness can compare at least two technique or timing variants derived from a shared baseline run definition.
  - Scenario outputs report deterministic delta metrics for speed, acceleration, blade slip, and efficiency or loss metrics when supported by the active providers.
  - Scenario metadata records the varied technique parameters and comparison window.
  - At least one checked-in scenario studies a technique question instead of only reproducing a baseline pass or fail run.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is the scenario-level bridge from richer outputs to real stroke-dynamics studies.

## R-046 — Hull and Rigging Sensitivity Scenarios
- **Title**: Provide named validation scenarios for hull, loading, and rigging sensitivity questions
- **Acceptance Criteria**:
  - The scenario harness can run a named set of hull, loading, or rigging variations from a shared baseline definition.
  - Scenario outputs report deterministic delta metrics for resistance, trim, and mean-speed behavior across the set.
  - Scenario metadata identifies the varied geometry, rigging, or mass-distribution parameters.
  - At least one checked-in scenario studies a hull or rigging sensitivity question instead of only baseline motion existence.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is the scenario-level bridge from calibrated reduced hull providers to hull-performance questions.

## R-047 — Measured-vs-Sim Agreement Metrics
- **Title**: Compare simulated runs against measured rowing trials with explicit agreement metrics
- **Acceptance Criteria**:
  - The simulator can compare a run against an imported measured trial over a declared time window.
  - Comparison output reports agreement metrics for at least speed, stroke timing or phase, and one measured load or power channel when present.
  - Missing channels or partial trial coverage are handled deterministically and recorded in the comparison output.
  - At least one checked-in validation artifact encodes a measured-vs-sim comparison workflow.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement closes the loop between measurement ingestion and scenario-backed study trust.

## R-048 — Truth-Model Export and Re-Import Consistency
- **Title**: Keep offline study export and reduced-artifact re-import contracts mutually consistent
- **Acceptance Criteria**:
  - Exported offline-study artifacts and re-imported reduced-fit artifacts share stable identifiers for geometry, rigging, boundary conditions, and reference frames.
  - Imported artifacts whose identifiers, units, or frames do not match the declared export contract are rejected deterministically.
  - Outputs record lineage from runtime run definition to offline-study artifact and back to the reduced provider or artifact used in the run.
  - At least one regression test verifies round-trip contract consistency without invoking external truth-model tooling.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This keeps `A-009` grounded as a boundary-management subsystem instead of an ad hoc file bucket.

## R-049 — Uncertainty and Validity Reporting
- **Title**: Expose uncertainty, fit quality, and out-of-validity reporting for study-facing outputs
- **Acceptance Criteria**:
  - Machine-readable outputs and human-readable reports expose validity-range status plus fit-quality, uncertainty, or confidence annotations for the active reduced providers and imported artifacts.
  - Runs that exceed declared validity ranges surface deterministic warnings or degraded-trust metadata instead of silently reporting full-confidence conclusions.
  - Scenario and comparison reports indicate whether their reported conclusions fall inside or outside the documented trust envelope.
  - At least one validation artifact exercises an out-of-validity or low-confidence reporting path.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-19
- **Updated**: 2026-04-19
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This is the reporting half of the fidelity-trust packet and depends on explicit model provenance and validity metadata.

## R-050 — Visualization-Ready Result Artifact Contract

- **Title**: Emit result artifacts suitable for 3D playback and analysis tooling
- **Acceptance Criteria**:
  - A run can emit a visualization artifact or artifact bundle without changing the numerical result of the run.
  - The artifact contains time-indexed transforms for hull, seat, oars, blades, rower reference masses, waterline, and configurable reference frames.
  - The artifact contains time-indexed vectors for selected forces, moments, velocities, accelerations, apparent wind, water/current direction, and environmental disturbances when available.
  - The artifact declares units, frames, coordinate axes, time base, scenario id, simulator version, provider ids, backend ids, calibration ids, and fidelity/trust-envelope metadata.
  - The artifact schema is versioned, documented, and rejected deterministically by visualization tools when unsupported or malformed.
- **Priority**: P0
- **Status**: IN_PROGRESS
- **Created**: 2026-04-24
- **Updated**: 2026-04-27
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Current slices emit and validate `airow.visualization.v1` JSON with frame, channel, transform, vector, provider, backend, and trust metadata. The artifact now includes additive hull-body-frame variants for available world-frame vector channels, and the offline report tool rejects malformed visualization artifacts before generating an interactive report with projection, frame, vector, and plot-control metadata. Full 3D playback and richer disturbance channels remain future work.

## R-051 — Interactive 3D Run Playback

- **Title**: Inspect a completed simulation through interactive 3D playback
- **Acceptance Criteria**:
  - A user can open a completed run artifact and play, pause, scrub, step frame-by-frame, and change playback speed.
  - The 3D view displays hull, oars, blades, seat position, relevant rower mass/reference markers, waterline, world axes, body axes, and port/starboard labels.
  - The viewer shows current simulation time, stroke phase, stroke number, boat speed, selected state values, active providers, and current fidelity/trust status during playback.
  - The viewer can load at least one checked-in reference scenario artifact generated by automated verification or an explicitly documented fixture-generation workflow.
  - The viewer works as an offline post-processing tool and is not required for headless runtime execution.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This directly addresses the current difficulty of understanding the simulated state and direction.

## R-052 — Frame, Direction, and Vector Overlays

- **Title**: Visualize coordinate frames, directions, forces, moments, and velocities
- **Acceptance Criteria**:
  - Visualization tools can display world-frame axes, hull body-frame axes, port/starboard labels, forward direction, gravity direction, waterline orientation, and selected local component frames.
  - Users can toggle force vectors for hull hydro, blade hydro, aero, rower/seat interaction, oarlock/gate loads, foot-stretcher loads, total net force, and external disturbance loads when available.
  - Users can toggle moment vectors or equivalent rotation indicators for roll, pitch, yaw, and total applied moment when available.
  - Vector overlays include scale, units, frame label, sign convention, channel provenance, and availability state.
  - A mirrored port/starboard scenario visibly changes the expected sign or direction of mirrored channels.
- **Priority**: P0
- **Status**: IN_PROGRESS
- **Created**: 2026-04-24
- **Updated**: 2026-04-27
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: The offline inspection report now adds dependency-free 2D top/side/end projections, projection and frame controls, checkbox vector overlays derived from emitted world-frame and hull-body-frame artifact channels, unit/frame/provenance labels, trust labels, disabled unavailable-channel controls, report-visible force/moment vector provenance, and mirrored yaw-moment sign evidence. `Needs-Review: yes` remains because true 3D playback, interface-load vectors, and broader disturbance coverage are still follow-up work.

## R-053 — Synchronized Time-Series Inspection

- **Title**: Provide synchronized plots linked to the 3D playback timeline
- **Acceptance Criteria**:
  - A user can inspect plots for boat speed, displacement, acceleration, roll, pitch, yaw, seat motion, oar angles, blade immersion, blade loads, hull loads, aero loads, interface loads, environment channels, and energy/power channels.
  - Scrubbing the 3D timeline updates a cursor on all visible plots.
  - Selecting a point on a plot updates the 3D view to the corresponding simulation time.
  - The tool identifies peak values, zero crossings, catch/release timing, stroke boundaries, validity-envelope crossings, and configured warning events.
  - Plot channels preserve units, frames, sign conventions, and availability/provenance labels from the machine-readable outputs.
- **Priority**: P1
- **Status**: IN_PROGRESS
- **Created**: 2026-04-24
- **Updated**: 2026-04-27
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: The offline inspection report now adds playback controls, scrubbing, step controls, plot cursors, plot-click seek hooks, selectable linked plot channels, and machine-readable control metadata for available plot coverage. It also emits deterministic peak, zero-crossing, stroke-boundary, and trust-warning event markers in report metadata and HTML seek controls. `Needs-Review: yes` remains because full channel coverage and true 3D timeline linkage are still follow-up work.

## R-054 — Run Comparison Visualization

- **Title**: Compare two or more simulation runs visually and numerically
- **Acceptance Criteria**:
  - A user can load at least two compatible runs and compare their headline metrics, time-series channels, selected 3D states, and selected force/moment overlays.
  - Comparison reports identify configuration differences, provider differences, calibration differences, backend differences, geometry differences, and fidelity-level differences.
  - Time-series comparisons can be aligned by absolute simulation time, stroke phase, and declared comparison windows.
  - The tool reports whether compared runs are physically comparable, calibration-comparable, or only software-comparable based on model validity metadata.
  - At least one checked-in comparison example shows a baseline scenario against a changed rigging, environment, technique, or provider setting.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This extends `R-045`, `R-046`, `R-047`, and `R-049` toward human-inspectable comparisons.

## R-055 — Failure and Instability Replay Bundle

- **Title**: Preserve enough state to understand failed or unstable runs
- **Acceptance Criteria**:
  - When a run fails after startup, the simulator can emit a bounded debug artifact containing the last valid states, failure time, diagnostic code, solver status, subsystem outputs, and relevant provider outputs.
  - The debug artifact can be opened by the same visualization tooling used for successful runs.
  - The visualization marks the failure time and highlights the channel, validity envelope, solver status, or subsystem associated with the failure diagnostic.
  - Debug bundle generation is configurable so normal fast runs can disable it.
  - Automated verification includes one intentionally failing run that produces an inspectable debug bundle or a documented fixture equivalent.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: Physical simulation debugging without replay tends to become guesswork in nicer clothes.

## R-056 — ParaView and VTK-Compatible Export

- **Title**: Export simulation results to an established scientific visualization workflow
- **Acceptance Criteria**:
  - A completed run can produce a ParaView/VTK-compatible artifact containing at least hull pose, oar pose, blade pose, waterline representation, and selected vector fields.
  - Exported vector quantities include units and frame metadata in an accessible sidecar or embedded metadata structure.
  - The repository includes a minimal ParaView loading guide, state file, or scripted loading example for at least one reference scenario.
  - Export generation is deterministic for the same run artifact and export settings.
  - The export path is optional and does not become a dependency of the headless runtime path.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: ParaView support gives airow a serious scientific visualization route before every custom-viewer feature exists.

## R-057 — Geometry and Surface Asset Contract

- **Title**: Represent boat, blade, oar, rigging, and analysis surfaces as first-class assets
- **Acceptance Criteria**:
  - The simulator accepts or references geometry assets for hull, blades, oars, riggers, gates/oarlocks, seat/rail references, and relevant rower mass/reference locations.
  - Geometry assets include coordinate frame, scale, units, source/provenance metadata, version identifiers, and validation status.
  - Hull geometry exposes enough information to compute or import displacement, waterline, wetted surface, hydrostatic coefficients, visualization surfaces, and reference points for load application.
  - Blade geometry exposes enough information to compute or import blade area, orientation, immersion region, center of pressure assumptions, and visualization surfaces.
  - Invalid geometry references, inconsistent units, malformed surfaces, or incompatible coordinate frames are rejected deterministically before runtime stepping.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This deepens current `R-038` from parameter sets into geometry-backed physical and visual assets.

## R-058 — Geometry-Aware 6-DOF Hull Hydrostatics and Hydrodynamics

- **Title**: Model hull-water behavior in six degrees of freedom with geometry and validity metadata
- **Acceptance Criteria**:
  - The hull-water model can produce forces and moments for surge, sway, heave, roll, pitch, and yaw when the selected provider supports them.
  - Hydrostatic restoring behavior depends on hull geometry, loading, displacement, trim, heel, and declared waterline assumptions, or else reports a documented reduced equivalent.
  - Hydrodynamic resistance and damping terms identify whether they are empirical, calibrated, surrogate-based, analytical, or placeholder/reduced.
  - Outputs expose per-component hull-water force and moment channels with units, frames, and validity status.
  - At least one scenario or artifact demonstrates sensitivity to trim, heel, yaw/sway, or loading beyond simple forward drag.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: Hull optimization is not credible while hull-water behavior is effectively forward-drag-only.

## R-059 — 3D Blade-Water Load Model

- **Title**: Compute blade-water loads as 3D forces and moments with immersion and relative-flow effects
- **Acceptance Criteria**:
  - The blade-water model can compute 3D force vectors from blade pose, blade geometry, immersion, local water velocity, boat motion, oar motion, and selected environmental inputs when the active provider supports them.
  - The model distinguishes dry recovery, catch, partial immersion, immersed drive, extraction, slip direction changes, and loss of immersion without singularities or silent sign flips.
  - The model exposes whether lift, drag, added mass, free-surface correction, ventilation, wake memory, or empirical coefficients are active, reduced, calibrated, or unavailable.
  - Outputs include blade force, center-of-pressure or effective application point, torque/moment contribution, slip metrics, and phase/transition diagnostics when supported.
  - At least one scenario demonstrates that changing catch timing, blade depth, or blade orientation changes the force history in a physically explainable way.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This deepens current `R-011`, `R-041`, `R-042`, and `R-043` toward realistic technique analysis.

## R-060 — Coupled Rower-Oar-Hull Dynamics

- **Title**: Couple rower actuation, oar mechanics, gate loads, seat motion, and hull motion through forces
- **Acceptance Criteria**:
  - The mechanics model can represent at least one force- or power-driven stroke mode in which hull acceleration feeds back into rower/oar/blade dynamics instead of only replaying prescribed kinematics.
  - The model can report whether rower motion is prescribed, reduced inertial, force-driven, power-driven, calibrated, or unavailable.
  - Oar inertia, gate constraints, seat/rail constraints, foot-stretcher interaction, and handle-side inputs are represented explicitly or declared as reduced/unavailable.
  - The coupled model preserves finite states and deterministic diagnostics over a bounded reference scenario.
  - Disabling coupling or switching to prescribed kinematics changes relevant load, energy, or acceleration channels in a documented way.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This extends current `R-039` and `R-040` from reduced study hooks toward full coupled rowing mechanics.

## R-061 — Rowing Interface Load Channels

- **Title**: Emit physically meaningful oarlock, gate, handle, foot-stretcher, and seat load channels
- **Acceptance Criteria**:
  - Time-series outputs include oarlock/gate force and moment channels for port and starboard when the active mechanics model supports them.
  - Outputs include handle force, foot-stretcher force, seat reaction force, rail reaction, or documented reduced equivalents when available.
  - Output channels declare whether they are directly simulated, reconstructed, prescribed, calibrated, reduced, or unavailable.
  - Visualization can display these loads as vectors and linked plot channels.
  - At least one scenario verifies that disabling blade loads or changing actuation mode changes interface load channels in a physically explainable way.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: These are the channels a rower, coach, or engineer expects when discussing technique rather than only boat speed.

## R-062 — Energy and Power Accounting

- **Title**: Track where mechanical work and energy go during a run
- **Acceptance Criteria**:
  - Outputs include rower input work/power, blade work, hull kinetic-energy change, oar kinetic-energy change, rower/seat kinetic-energy change, aerodynamic loss, hull-water loss, and unavailable/reduced terms when supported.
  - Energy and power channels declare units, sign convention, integration method, reference frame, and support status.
  - Human-readable reports include an energy-flow summary that explains dominant sources, sinks, and unavailable terms.
  - Numerical energy residuals are bounded or explicitly reported for supported scenarios.
  - At least one scenario compares energy accounting across a baseline run and a changed technique, provider, or environment setting.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: Optimization without energy accounting is especially good at finding fake improvements.

## R-063 — Environmental Disturbance Model

- **Title**: Represent waves, current, water density, and environmental disturbances with explicit validity limits
- **Acceptance Criteria**:
  - A run can represent at least calm water, steady current, and deterministic disturbance inputs without changing existing calm-water baseline behavior when disabled.
  - Wave or disturbance inputs declare frame, units, phase/time base, boundary assumptions, and whether the model is reduced, calibrated, or truth-model-derived.
  - Hull and blade providers receive enough environmental information to compute environment-dependent loads or to report that the effect is unsupported.
  - Outputs expose environmental state channels used by the active providers.
  - At least one regression scenario demonstrates a deterministic non-calm-water environment without relying on optional CFD/SPH runtime tooling.
- **Priority**: P2
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This extends the existing wind work toward water-side environment realism.

## R-064 — Flexible Oar and Structural Response Readiness

- **Title**: Support future flexible-oar and structural-response studies without contaminating the baseline
- **Acceptance Criteria**:
  - The model boundary can distinguish rigid-oar, reduced-flexible-oar, calibrated-flexible-oar, and unavailable modes.
  - Flexible-oar outputs, when supported, expose deflection, effective blade angle, elastic energy, and relevant load channels.
  - Rigid-oar baseline scenarios remain deterministic and unchanged when flexible-oar behavior is disabled.
  - Geometry and material inputs required for flexible-oar studies include units, provenance, validity limits, and validation status.
  - Visualization can show rigid versus deformed oar state when the channel is supported.
- **Priority**: P2
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: Flexible oars should be allowed later without quietly changing current rigid-oar assumptions.

## R-065 — High-Fidelity Truth-Model Study Definition

- **Title**: Export hull, blade, and environment study definitions suitable for optional full-3D water reference workflows
- **Acceptance Criteria**:
  - The simulator can export versioned study definitions for selected hull-water, blade-water, or coupled short-window cases without running optional high-fidelity tooling inside the default runtime.
  - Study definitions include geometry identifiers, boundary conditions, reference frames, units, time windows, provider/source run metadata, and intended comparison channels.
  - Exported studies distinguish hull-resistance, blade-flow, free-surface, wake, and coupled short-window purposes.
  - The export contract is stable enough that OpenFOAM, SPH, or other truth-model tooling can consume it through adapters rather than ad hoc manual reconstruction.
  - At least one dry-run export test validates schema completeness without requiring high-fidelity tooling to be installed.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This expands current `R-024` and `R-048` from one-way handoff/consistency into explicit study definitions for the dream-goal workflow.

## R-066 — Truth-Model Result Import and Visualization

- **Title**: Import and compare high-fidelity reference results against reduced runtime outputs
- **Acceptance Criteria**:
  - The project can import versioned truth-model result artifacts containing at least force, moment, pressure/surface, or flow-summary channels relevant to a declared study definition.
  - Imported results carry provenance, solver/tool identifier, mesh/particle/resolution metadata, boundary-condition identifiers, and comparison validity notes.
  - Comparison reports align truth-model outputs with reduced runtime outputs by time, stroke phase, geometry id, frame, and selected channels.
  - Visualization can show truth-model-derived pressure/surface/force diagnostics alongside reduced runtime state when available.
  - Incompatible geometry, units, frames, or boundary identifiers are rejected deterministically.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: The truth model should feed understanding and calibration, not silently replace the normal runtime.

## R-067 — Validation Scorecards and Realism Gates

- **Title**: Summarize measured-vs-sim and reference-vs-sim agreement in human-readable scorecards
- **Acceptance Criteria**:
  - Validation scorecards report agreement metrics for speed, acceleration, stroke phase, blade/gate loads, trim/heel/yaw behavior, and energy/power channels when supported by the available data.
  - Scorecards identify unavailable channels, missing measured data, out-of-range conditions, and unsupported model effects.
  - A scorecard can state whether a run or provider remains inside a declared validity envelope for a named study question.
  - Human-readable reports and visualization entry pages expose the scorecard summary before optimization or comparison conclusions.
  - At least one fixture demonstrates a passing, failing, or insufficient-evidence scorecard.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This turns current `R-047` and `R-049` into a visible validation product rather than buried metadata.

## R-068 — Calibration and Surrogate Model Lifecycle

- **Title**: Manage calibrated reduced models and surrogate models as versioned, inspectable artifacts
- **Acceptance Criteria**:
  - Calibrated providers and surrogate models declare training data, truth-model data, measured data, fitting method, validity range, uncertainty, version, and content hash.
  - The simulator records which calibration or surrogate artifact was used by a run and exposes it in machine-readable output, human-readable reports, and visualization metadata.
  - Out-of-range use is reported deterministically and can degrade trust status instead of silently presenting full-confidence results.
  - The repository includes at least one minimal calibration/surrogate artifact fixture and schema validation test.
  - Replacement of a calibration artifact changes reproducibility metadata even when input configuration is otherwise unchanged.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This complements existing calibration provenance work and the calibrated blade/hull provider requirements.

## R-069 — Optimization-Safe Experiment Outputs

- **Title**: Support parameter studies and optimization without hiding validity or comparability limits
- **Acceptance Criteria**:
  - Batch, sweep, or optimization outputs include per-case configuration deltas, result metrics, validity status, fidelity level, calibration ids, and comparability flags.
  - Ranking or recommendation outputs mark cases that are out-of-validity, not physically comparable, or missing required channels.
  - Sensitivity results distinguish numerical variation, provider variation, calibration variation, and geometry/technique variation.
  - The output format supports later replay of selected cases through visualization artifacts.
  - At least one optimization-style fixture demonstrates that an out-of-validity fast result is not presented as a trustworthy physical optimum.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: This protects hull and technique optimization from optimizing the model's blind spots.

## R-070 — Analysis-First CLI Workflow

- **Title**: Provide one clear command path for producing understandable run artifacts
- **Acceptance Criteria**:
  - A documented command or script can run a scenario and emit summary JSON, time-series data, human-readable report, visualization artifact, optional VTK/ParaView export, and optional debug bundle.
  - The command prints artifact paths, report paths, viewer/export instructions, and any validity warnings.
  - The command supports a minimal checked-in reference scenario without optional truth-model tooling.
  - The workflow is deterministic for the same inputs and build configuration.
  - The workflow documents which artifacts are for physics, which are for visualization, and which are for debugging.
- **Priority**: P0
- **Status**: IN_PROGRESS
- **Created**: 2026-04-24
- **Updated**: 2026-04-27
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Current slices have direct CLI/example configs emitting summary, time-series, and visualization JSON paths, plus documented offline HTML report generation with playback, vector, projection, frame, linked-plot, and event-marker controls. Optional VTK/ParaView export and debug bundles remain open.

## R-071 — Human-Facing Physics Explanation and Capability Matrix

- **Title**: Explain active physics, missing physics, and supported study questions in plain language
- **Acceptance Criteria**:
  - Human-readable reports and visualization entry pages explain which physics are active, reduced, calibrated, validated, placeholder, disabled, or unavailable for the run.
  - The project maintains a public capability matrix covering hull physics, blade physics, rower coupling, oar flexibility, aero, waves/current, visualization, validation, calibration, and truth-model workflows.
  - The capability matrix distinguishes current support, implemented-but-unvalidated support, planned support, and explicitly unsupported scope.
  - Study recommendations or optimization outputs link back to the capability/trust status that justifies or limits the conclusion.
  - At least one checked-in report fixture demonstrates the explanation layer for a placeholder/reduced run.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-24
- **Updated**: 2026-04-24
- **Change-Type**: semantic
- **Needs-Review**: yes
- **Notes**: A realistic simulator should be physically accurate, but it also needs to be legible to humans. The first Phase 1 slice starts the provider metadata foundation by adding catalog-backed capability declarations to run outputs; report prose and the public capability matrix remain follow-up slices while this requirement stays review-flagged.
