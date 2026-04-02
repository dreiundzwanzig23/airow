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

## R-001 — Configuration Loading and Validation
- **Title**: Load and validate simulation configuration deterministically
- **Acceptance Criteria**:
  - A simulation can be started from a machine-readable configuration file.
  - Missing required fields are rejected with a deterministic message that identifies the failing field path.
  - Invalid numeric values, including NaN, infinity, negative duration, and negative mass, are rejected before time stepping starts.
  - Accepted configuration values are echoed in normalized form in run metadata.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Initial scope uses SI units only.

## R-002 — Headless Simulation Execution
- **Title**: Execute one simulation without any GUI dependency
- **Acceptance Criteria**:
  - A simulation can be launched non-interactively from a CLI entry point.
  - The same run path is callable from automated tests without spawning a GUI.
  - Successful runs return a success status and failed runs return a non-zero failure status.
  - Run metadata includes simulator version, configuration identifier, and start and end timestamps.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-003 — In-Memory Simulation API
- **Title**: Provide a library API suitable for tests and harnesses
- **Acceptance Criteria**:
  - A test can execute a simulation in memory without shelling out to the CLI.
  - The API returns structured results including status, summary metrics, and diagnostics.
  - The API supports injection of deterministic stub or mock hydro and aero providers.
  - At least one automated test uses the in-memory API with stub providers.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement exists primarily to improve testability and agentic workflow ergonomics.

## R-004 — Deterministic Replay
- **Title**: Reproduce the same run deterministically on the same platform
- **Acceptance Criteria**:
  - Re-running the same configuration on the same executable and platform produces identical run metadata except for wall-clock timestamps.
  - Summary metrics, including final simulated time, traveled distance, and mean speed, match within a documented numerical tolerance.
  - Output record ordering is deterministic.
  - A replay test exists that runs the same case at least twice and compares the results.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-005 — Single-Scull 3D Hull Model
- **Title**: Represent the shell as a 3D rigid body with configurable mass properties
- **Acceptance Criteria**:
  - The simulator models one hull body with configurable mass, center of mass, and inertia.
  - Hull state outputs include position, orientation, linear velocity, and angular velocity in 3D.
  - A static initialization with valid parameters completes without non-finite state values.
  - Invalid hull mass properties are rejected during configuration validation.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Initial scope is single scull only.

## R-006 — Port and Starboard Oar Kinematics
- **Title**: Model two independent sculling oars with configurable geometry
- **Acceptance Criteria**:
  - The simulator includes separate port and starboard oars.
  - Oar geometry includes configurable inboard length, outboard length, and oarlock location.
  - Time-series outputs include each oar’s kinematic state.
  - Constraint or geometry residuals remain below a documented tolerance in a nominal run.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-007 — Seat Translation Model
- **Title**: Model seat motion along a configurable rail axis
- **Acceptance Criteria**:
  - Seat position is represented as a separate state variable constrained to a configured rail direction.
  - Seat travel limits can be configured and are enforced.
  - Time-series outputs include seat position and velocity.
  - An out-of-range initial seat position is rejected before simulation begins.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-008 — Prescribed Stroke Schedule
- **Title**: Drive the system with a deterministic prescribed stroke input
- **Acceptance Criteria**:
  - The simulator accepts a periodic stroke definition with configurable stroke rate and phase timing.
  - The prescribed stroke can be replayed for at least ten cycles without phase drift beyond a documented tolerance.
  - Invalid schedules, including non-positive cycle time or inconsistent phase durations, are rejected.
  - A nominal stroke test produces finite seat and oar trajectories for the full run.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Initial scope uses prescribed motion or control, not full musculoskeletal actuation.

## R-009 — Hydrostatic Float Equilibrium
- **Title**: Support calm-water floating equilibrium for the hull
- **Acceptance Criteria**:
  - With zero stroke and zero wind, the simulator can initialize and maintain a calm-water floating state for a valid hull setup.
  - At the reported equilibrium state, net vertical force and restoring moments are within documented tolerances.
  - A passive float test completes without non-finite values or runaway drift.
  - Output includes equilibrium draft or immersion-related diagnostics.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-010 — Reduced Hull-Water Resistance
- **Title**: Apply a reduced hydrodynamic resistance model to hull motion
- **Acceptance Criteria**:
  - For a prescribed forward towing speed in calm water, the model returns a drag force opposing the direction of motion.
  - Over a documented calibration range, drag magnitude increases monotonically with towing speed.
  - At zero forward speed, the forward drag component is zero or within a documented near-zero tolerance.
  - A tow-test scenario produces a reproducible drag-versus-speed curve.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-011 — Reduced Blade-Water Force Model
- **Title**: Compute blade hydrodynamic loads from blade state and relative water motion
- **Acceptance Criteria**:
  - Blade load computation depends on at least immersion state, blade orientation, and relative blade-water velocity.
  - A fully dry blade produces near-zero hydrodynamic load.
  - At fixed immersion and orientation, increasing relative blade-water speed increases force magnitude over a documented range.
  - The blade force provider can be exercised independently in automated tests.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-012 — Self-Propelled Stroke Response
- **Title**: Produce forward motion from a valid stroke in calm water
- **Acceptance Criteria**:
  - With a valid prescribed stroke and active blade forces, the boat achieves positive mean forward displacement over multiple cycles in calm water.
  - Time-series outputs for boat speed, blade loads, and power remain finite for the full run.
  - Disabling blade hydrodynamic forces while keeping the same stroke reduces mean boat speed relative to the baseline case.
  - A calm-water propulsion scenario is included in automated regression tests.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-013 — Apparent Wind Computation
- **Title**: Compute apparent wind from ambient wind and boat motion
- **Acceptance Criteria**:
  - The simulator accepts a 3D ambient wind input.
  - Apparent wind is computed from the relative velocity between ambient air and the boat state.
  - With zero ambient wind, apparent wind equals the negative of the boat air-relative velocity within tolerance.
  - Unit tests cover headwind, tailwind, and crosswind direction cases.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
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
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-015 — Machine-Readable Outputs
- **Title**: Emit structured outputs for analysis and regression testing
- **Acceptance Criteria**:
  - Each run writes a machine-readable summary file.
  - Each run can write machine-readable time-series data for at least hull state, oar state, seat state, boat speed, blade loads, and aero loads.
  - Output files include a configuration identifier and simulator version.
  - The simulator can be configured to enable or disable high-frequency time-series output.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-016 — Runtime Diagnostics and Safe Failure
- **Title**: Detect invalid runtime conditions and fail with actionable diagnostics
- **Acceptance Criteria**:
  - The simulator detects non-finite state values during execution.
  - The simulator detects invalid subsystem outputs, including non-finite forces from hydro or aero providers.
  - A failed run terminates with a stable diagnostic code and subsystem-specific message.
  - Automated tests induce at least one representative failure mode and verify the reported diagnostic.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-017 — Units and Numeric Safety
- **Title**: Enforce SI units and numeric safety constraints at the simulator boundary
- **Acceptance Criteria**:
  - External configuration and output quantities use documented SI units.
  - Unit-bearing fields in configuration and output documentation include their units explicitly.
  - Invalid or ambiguous unit-bearing inputs are rejected before time stepping starts.
  - Automated checks verify that core scenario configurations use only documented units.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is complemented by repo-level numerics policy but remains externally verifiable.

## R-018 — Reference Validation Scenarios
- **Title**: Provide a baseline set of repeatable validation scenarios
- **Acceptance Criteria**:
  - The repository includes at least these named scenarios: passive float, tow test, calm-water stroke, headwind stroke, and crosswind stroke.
  - Each reference scenario has a checked-in configuration and documented numeric acceptance envelope.
  - Reference scenarios are runnable headlessly in automated verification.
  - A failed acceptance check causes the verification job to fail.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is intentionally verification-heavy to support agentic development.

## R-019 — Verification Traceability for P0 Requirements
- **Title**: Trace each P0 requirement to at least one automated verification artifact
- **Acceptance Criteria**:
  - Every P0 requirement has at least one named automated test case or scenario associated with it.
  - Test names or metadata include the requirement identifier.
  - The trace from requirement identifier to verification artifact is machine-searchable in the repository.
  - Missing trace for any P0 requirement causes a verification check to fail.
- **Priority**: P0
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This is a workflow-support requirement rather than a physics requirement.

## R-020 — Runtime-Selectable Hydro and Aero Providers
- **Title**: Select reduced-model providers at runtime without recompilation
- **Acceptance Criteria**:
  - A run configuration can select the active hull resistance, blade-force, and aerodynamic load providers without recompilation.
  - The simulator reports the selected providers in run metadata.
  - Providers that implement the same contract satisfy a shared integration test suite.
  - Selecting an unknown provider causes deterministic configuration rejection.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-021 — External Calibration Dataset Ingestion
- **Title**: Load external calibration data for hydrodynamic or aerodynamic models
- **Acceptance Criteria**:
  - The simulator accepts an external machine-readable dataset for calibration or lookup use.
  - Malformed or incomplete calibration datasets are rejected deterministically with a schema-specific error.
  - Loaded datasets are queryable by the configured provider during a run.
  - At least one automated test loads a minimal valid dataset and verifies a successful query.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-022 — Calibration Provenance Metadata
- **Title**: Preserve provenance for imported calibration and fitted-model artifacts
- **Acceptance Criteria**:
  - Imported calibration artifacts include source identifier, artifact version, and a content hash or equivalent immutable identifier.
  - Run metadata records the identifiers of any external calibration artifacts used during execution.
  - Missing required provenance metadata causes deterministic rejection of the artifact.
  - A regression test verifies that provenance metadata is propagated into run outputs.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-023 — Time-Varying Wind Input
- **Title**: Support deterministic time-varying wind for gust and transition studies
- **Acceptance Criteria**:
  - The simulator accepts a time-varying wind definition or sampled wind time series.
  - For a constant wind time series, the computed aerodynamic loads match the constant-wind model within tolerance.
  - Replaying the same wind time series yields deterministic results on the same platform.
  - At least one regression scenario includes a non-constant wind input.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-024 — Runtime and Truth-Model Separation
- **Title**: Keep the default runtime path independent of optional high-fidelity toolchains
- **Acceptance Criteria**:
  - The default simulation runtime executes without requiring installation of optional SPH, CFD, or calibration-generation toolchains.
  - Any optional high-fidelity export or import path is configuration-controlled and disabled by default.
  - A documented workflow exists to export inputs for offline high-fidelity studies and re-import derived artifacts.
  - Automated verification includes at least one run that proves the default runtime path works without optional tooling present.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement protects the core runtime from accidental coupling to research-only tooling.

## R-025 — Batch Parameter Sweep Execution
- **Title**: Run multiple simulation cases in one headless batch job
- **Acceptance Criteria**:
  - A batch input can define multiple cases or parameter variations.
  - Each case produces a separate success or failure record without corrupting the results of other cases.
  - Batch summary output includes per-case identifiers and summary metrics.
  - Batch output ordering is deterministic for the same input ordering.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no

## R-026 — Scenario Performance Budget
- **Title**: Keep core verification scenarios within documented runtime budgets
- **Acceptance Criteria**:
  - At least one documented quick verification lane exists for local development.
  - Core reference scenarios specify approximate runtime budgets or upper bounds for the target development environment class.
  - A performance regression in a protected scenario is reported separately from functional test failures.
  - The quick lane remains usable without optional high-fidelity tooling.
- **Priority**: P1
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement is about protecting workflow usability as the simulator grows.

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
- **Notes**: This requirement intentionally stops short of full musculoskeletal modeling.

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

## R-030 — Multi-Rower and Non-Scull Expansion Readiness
- **Title**: Preserve a migration path toward future crew and sweep support without including it in the initial scope
- **Acceptance Criteria**:
  - Initial product documentation states that single scull is the supported baseline scope.
  - Configuration validation rejects unsupported boat classes in the initial release scope.
  - Public-facing documentation identifies crew and sweep support as future expansion rather than current capability.
  - Adding unsupported boat classes does not silently fall back to single-scull behavior.
- **Priority**: P2
- **Status**: OPEN
- **Created**: 2026-04-01
- **Updated**: 2026-04-01
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: This requirement protects scope clarity while keeping the architecture extensible.

## Bootstrap Appendix

The following requirement exists only to keep the temporary sample utility
traceable while the real simulator implementation is still being bootstrapped.
It is not part of the rowing-simulator product backlog and must not be used as
the architectural home for simulator work.

## R-900 — Bootstrap Placeholder Utility
- **Title**: Keep the temporary sample utility traceable during repository bootstrap
- **Acceptance Criteria**:
  - The placeholder sample utility remains buildable while the first simulator-facing implementation slice is pending.
  - The placeholder utility is isolated from the real simulator backlog through reserved bootstrap trace IDs.
  - Placeholder tests continue to prove trace and gate wiring without claiming simulator feature coverage.
  - Future simulator work does not allocate to this requirement.
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-04-02
- **Updated**: 2026-04-02
- **Change-Type**: none
- **Needs-Review**: no
- **Notes**: Reserved bootstrap IDs use the `900` series.
