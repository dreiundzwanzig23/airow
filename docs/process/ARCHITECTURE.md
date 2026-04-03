<!-- TRACE-SYNTAX: ARCHITECTURE
Each architecture item must use:

<any markdown heading level> A-xyz — <Component or Capability Name>
- **Title**: <concise name>
- **Satisfies**: [R-001, R-002]
- **Status**: OPEN|IN_PROGRESS|DONE
- **Evidence Profile**: CODE|ASSET (optional, default CODE)
- **Responsibility**: <required for CODE>
- **Owned Concepts**: <required for CODE>
- **Inputs**: <required for CODE>
- **Outputs**: <required for CODE>
- **Depends On**: <required for CODE>
- **Must Not Depend On**: <required for CODE>
- **Invariants**: <required for CODE>
- **Allocation Rationale**: <required for CODE items with satisfied requirements>
- **Future Absorption**: <required for CODE>
- **Description**: <optional>
- **Interfaces**: <required for CODE DONE items>

Validation policy:
- DONE architecture items with `Evidence Profile: CODE` must map to at least
  one D-### implementation and one IT-### test.
- IN_PROGRESS architecture items with `Evidence Profile: CODE` should map to at
  least one D-### implementation.
- DONE architecture items with `Evidence Profile: ASSET` must be covered by at
  least one QT-### through satisfied requirements.
-->

# ARCHITECTURE.md — Rowing Simulator Bootstrap Architecture

This file defines the stable architecture ownership surface for the rowing
simulator.

Real simulator architecture items use `A-001..A-010`.
The `A-900` range remains reserved for temporary bootstrap-only artifacts and
must not become the architectural home for simulator requirements.

## A-001 — Configuration and Validation
- **Title**: Deterministic configuration and validation subsystem
- **Satisfies**: [R-001, R-017, R-020, R-021, R-022, R-025, R-028, R-029, R-030, R-031, R-032, R-033]
- **Status**: IN_PROGRESS
- **Responsibility**: Parse, validate, normalize, and expose simulator, provider, artifact, and scenario configuration before execution begins.
- **Owned Concepts**: `SimulatorConfig`; provider selection metadata; schema validation; unit-bearing and frame-bearing field definitions; provider validity metadata; unsupported-scope rejection.
- **Inputs**: Machine-readable run configuration; batch definitions; external artifact metadata; selected provider and model identifiers.
- **Outputs**: Validated in-memory configuration; deterministic validation diagnostics; normalized run metadata inputs.
- **Depends On**: Core validation helpers and documented process policies for units, state conventions, and provenance.
- **Must Not Depend On**: Mechanics stepping logic; hydro or aero algorithm internals; scenario execution state.
- **Invariants**: Invalid or ambiguous configuration never reaches runtime stepping; accepted values are normalized deterministically; unsupported scope is rejected explicitly.
- **Allocation Rationale**: Centralizes all boundary validation so requirements about deterministic rejection, units, provider selection, and scope control do not fragment across runtime subsystems.
- **Future Absorption**: Additional model toggles, boat-class expansion gates, richer artifact schemas, and future frame-bearing configuration definitions should be absorbed here before touching runtime logic.
- **Interfaces**: File-backed and in-memory JSON configuration loading contract returning validated `SimulatorConfig`, deterministic diagnostics, and normalized configuration metadata suitable for later runtime orchestration.

## A-002 — Simulation Orchestrator
- **Title**: Headless simulation orchestration subsystem
- **Satisfies**: [R-002, R-003, R-004, R-016, R-020, R-023, R-024, R-025]
- **Status**: IN_PROGRESS
- **Responsibility**: Coordinate one or more simulation runs, lifecycle transitions, provider wiring, failure handling, and deterministic replay.
- **Owned Concepts**: Run lifecycle; in-memory simulation API; CLI entry wiring; provider registry binding; batch execution flow.
- **Inputs**: Validated configuration; selected runtime providers; time-varying inputs; scenario harness requests.
- **Outputs**: Run status; structured result objects; deterministic failure codes; per-case batch outcomes.
- **Depends On**: Configuration and validation; mechanics; hydro; aero; control; numerical integration; output and diagnostics contracts.
- **Must Not Depend On**: Provider-specific physics internals; calibration artifact generation; scenario acceptance logic internals.
- **Invariants**: The same validated run definition executes through the same orchestration path for CLI and in-memory use; failures terminate with stable diagnostics; batch case ordering remains deterministic.
- **Allocation Rationale**: Groups lifecycle and composition concerns into one owner instead of scattering them across feature-specific requirements and runtime providers.
- **Future Absorption**: Parallel batch scheduling, checkpoint/restart, and richer orchestration policies should extend this subsystem.
- **Interfaces**: CLI entry contract and in-memory run API that share one
  deterministic single-run execution path, plus injected hydro and aero stub
  provider seams, stable run-result metadata, and exit-code mapping for the
  first headless baseline.

## A-003 — Mechanics Subsystem
- **Title**: 3D mechanics core for hull, oars, and seat motion
- **Satisfies**: [R-005, R-006, R-007, R-008, R-012, R-027, R-028, R-030, R-031, R-032]
- **Status**: IN_PROGRESS
- **Responsibility**: Own the constrained 3D mechanical state for hull, oars, seat, optional flexible oar behavior, and optional stabilization control coupling points.
- **Owned Concepts**: Hull rigid-body state; oar kinematics; seat travel state; prescribed stroke state; consistent initial mechanical state; optional balance control interfaces; optional rigid vs flexible oar representation.
- **Inputs**: Validated geometry and mass properties; control or prescribed stroke inputs; external hydro and aero loads.
- **Outputs**: 3D state trajectories; state-initialization and constraint-residual diagnostics; blade and hull state needed by load providers.
- **Depends On**: Core math/contracts; control and stroke input; hydro and aero load contracts.
- **Must Not Depend On**: Output formatting internals; calibration dataset ingestion; scenario acceptance logic; hard-coded solver-backend policy.
- **Invariants**: Mechanical state remains finite in nominal runs; constraints remain bounded by documented tolerances; startup assembly exposes enough information for consistent initialization; optional mechanics modes do not silently change the baseline rigid path.
- **Allocation Rationale**: Keeps the mechanics backbone as the central physical state owner rather than leaking motion ownership into individual requirement-driven feature slices.
- **Future Absorption**: Expanded body representations, future crew support, and deeper rower dynamics should be absorbed here behind stable state contracts.
- **Interfaces**: Mechanics state contract, external-load application contract, and subsystem initialization contract for state advancement. The current realization slice establishes boundary-visible hull, oar, seat, and stroke state behind the orchestrator seam using a deterministic internal baseline implementation that remains independent of concrete Chrono types.

## A-004 — Hydro Runtime Models
- **Title**: Reduced hydrodynamic runtime models
- **Satisfies**: [R-009, R-010, R-011, R-012, R-020, R-021, R-022, R-024, R-029, R-033]
- **Status**: OPEN
- **Responsibility**: Compute hydrostatic and reduced hydrodynamic loads for the hull and blades during default runtime execution.
- **Owned Concepts**: Hull flotation model; hull resistance providers; blade-water load providers; provider validity metadata; disturbance-to-water-load coupling points.
- **Inputs**: Mechanics state; validated provider selection; optional calibration datasets; optional disturbance definitions.
- **Outputs**: Hull and blade load vectors; immersion diagnostics; provider metadata for outputs and validity reporting.
- **Depends On**: Core math/contracts; mechanics state contracts; configuration/provider selection; optional calibration contracts.
- **Must Not Depend On**: Aero internals; orchestrator lifecycle logic; high-fidelity truth-model toolchains in the default runtime path.
- **Invariants**: Reduced-model providers remain callable without optional high-fidelity tooling; dry or zero-speed limits behave deterministically; provider outputs stay finite or fail deterministically; each provider carries explicit validity metadata suitable for run reporting.
- **Allocation Rationale**: Concentrates water-load logic into one subsystem so runtime hydro behavior evolves through replaceable providers instead of requirement-specific patches.
- **Future Absorption**: Additional resistance, blade, wave, and lookup-driven hydro providers should be absorbed here behind shared contracts.
- **Interfaces**: Planned hull-load provider and blade-load provider contracts with runtime-selectable implementations.

## A-005 — Aero Runtime Models
- **Title**: Reduced aerodynamic runtime models
- **Satisfies**: [R-013, R-014, R-020, R-021, R-022, R-023, R-024, R-029, R-031, R-033]
- **Status**: OPEN
- **Responsibility**: Compute apparent wind and reduced aerodynamic loads for routine runtime execution.
- **Owned Concepts**: Apparent wind computation; aero load providers; wind-direction interpretation at runtime boundaries; provider validity metadata; steady and time-varying wind handling; disturbance-to-aero coupling.
- **Inputs**: Mechanics state; ambient wind definitions; optional calibration datasets; selected aero provider.
- **Outputs**: Apparent wind state; aerodynamic force and moment outputs; provider metadata for diagnostics and validity reporting.
- **Depends On**: Core math/contracts; mechanics state contracts; configuration/provider selection; optional calibration contracts.
- **Must Not Depend On**: Hydro internals; orchestrator lifecycle policy; optional external CFD toolchains for the baseline runtime path.
- **Invariants**: Zero apparent wind yields near-zero loads; mirrored crosswind directions produce deterministic sign changes under the documented frame conventions; runtime behavior stays independent of optional truth-model tooling.
- **Allocation Rationale**: Keeps aero behavior replaceable and decoupled from hydro and orchestration instead of creating mixed fluid-load feature containers.
- **Future Absorption**: Gust models, richer aero coefficients, and calibrated aero providers should extend this subsystem.
- **Interfaces**: Planned apparent-wind contract and aerodynamic load provider contract.

## A-006 — Control and Stroke Input
- **Title**: Stroke scheduling and low-order control subsystem
- **Satisfies**: [R-008, R-023, R-027]
- **Status**: OPEN
- **Responsibility**: Generate deterministic prescribed stroke inputs and optional low-order stabilization control outputs.
- **Owned Concepts**: Stroke schedule definition; phase timing; controller mode selection; wind-aware input scheduling hooks.
- **Inputs**: Validated schedule and control configuration; optional time-varying inputs; mechanics state feedback when control mode requires it.
- **Outputs**: Stroke commands; optional control outputs; mode metadata for run reporting.
- **Depends On**: Core math/contracts; configuration and validation; mechanics feedback contracts.
- **Must Not Depend On**: Hydro or aero algorithm internals; scenario result evaluation; output serialization internals.
- **Invariants**: Prescribed schedules replay deterministically; invalid schedules are rejected before runtime; optional controllers remain finite and can be disabled cleanly.
- **Allocation Rationale**: Separates rower-input generation from mechanics and load-provider ownership so input strategies can evolve independently.
- **Future Absorption**: Richer control modes and eventual deeper rower representations should be absorbed here until a stronger biomechanics boundary is justified.
- **Interfaces**: Planned stroke schedule contract and optional controller contract.

## A-007 — Output and Diagnostics
- **Title**: Structured outputs and runtime diagnostics subsystem
- **Satisfies**: [R-003, R-004, R-015, R-016, R-022, R-025, R-031, R-033]
- **Status**: OPEN
- **Responsibility**: Capture machine-readable summaries, time series, metadata, and actionable diagnostics for single and batch runs.
- **Owned Concepts**: Run metadata; summary outputs; time-series emission; force and power accounting channels; frame and unit annotations; failure diagnostics; provenance and validity propagation into outputs.
- **Inputs**: Run lifecycle events; mechanics, hydro, aero, control, and numerical integration outputs; calibration provenance identifiers.
- **Outputs**: Summary artifacts; time-series artifacts; diagnostic records; per-case batch result summaries.
- **Depends On**: Orchestrator lifecycle contracts; subsystem output contracts; configuration metadata; provenance metadata.
- **Must Not Depend On**: Mechanics stepping internals; provider algorithms; scenario pass/fail policy logic.
- **Invariants**: Output ordering is deterministic; enabled outputs include required identifiers, provenance, validity metadata, and frame or unit annotations for vector quantities; failure diagnostics remain stable and actionable.
- **Allocation Rationale**: Prevents metadata, diagnostics, and serialization concerns from leaking into every runtime subsystem.
- **Future Absorption**: Richer artifact formats, streamed diagnostics, and analysis-facing summaries should grow here behind stable output contracts.
- **Interfaces**: Planned result object contract, summary writer contract, and time-series emission contract.

## A-008 — Scenario Harness and Validation
- **Title**: Scenario definition and validation subsystem
- **Satisfies**: [R-018, R-019, R-024, R-026, R-029]
- **Status**: OPEN
- **Responsibility**: Own named reference scenarios, acceptance envelopes, characterization baselines, and workflow-facing validation structure.
- **Owned Concepts**: Named validation scenarios; acceptance envelopes; scenario metadata; quick vs broader validation lanes; characterization baselines.
- **Inputs**: Scenario configurations; run outputs; documented acceptance envelopes; process test-lane policy.
- **Outputs**: Scenario pass/fail results; validation metadata; protected baseline definitions for regression and characterization work.
- **Depends On**: Orchestrator result contracts; output artifacts; process policies for numerics and fidelity.
- **Must Not Depend On**: Runtime subsystem internals beyond their public outputs; calibration generation internals; CLI-specific code paths.
- **Invariants**: Named baseline scenarios remain headlessly runnable; acceptance checks are deterministic; quick verification lanes stay independent of optional high-fidelity tooling.
- **Allocation Rationale**: Gives validation and scenario ownership one stable home instead of scattering it across tests, scripts, and individual feature requirements.
- **Future Absorption**: Expanded scenario catalogs, acceptance dashboards, and protected characterization suites should extend this subsystem.
- **Interfaces**: Planned scenario manifest contract and acceptance-evaluation contract.

## A-009 — External Calibration Integration
- **Title**: External calibration and artifact integration subsystem
- **Satisfies**: [R-021, R-022, R-024]
- **Status**: OPEN
- **Responsibility**: Own ingestion and metadata handling for external calibration datasets and derived model artifacts while keeping the runtime path loosely coupled.
- **Owned Concepts**: Calibration dataset loading; artifact provenance metadata; re-imported lookup or surrogate artifacts; offline truth-model handoff boundaries.
- **Inputs**: External machine-readable datasets; artifact identifiers; source hashes and versions; provider lookup requests.
- **Outputs**: Queryable calibration data contracts; validated provenance metadata; imported artifact handles for runtime providers.
- **Depends On**: Configuration and validation; output/provenance contracts; hydro and aero provider contracts.
- **Must Not Depend On**: Orchestrator lifecycle logic; mechanics state ownership; mandatory installation of offline truth-model toolchains.
- **Invariants**: Imported artifacts carry deterministic provenance; malformed artifacts are rejected before use; default runtime remains usable without optional calibration-generation tooling.
- **Allocation Rationale**: Isolates external data and provenance concerns so runtime subsystems can consume calibrated artifacts without becoming responsible for artifact lifecycle policy.
- **Future Absorption**: Surrogate-model ingestion, richer dataset schemas, and offline export/import workflows should extend this subsystem.
- **Interfaces**: Planned calibration dataset contract, artifact metadata contract, and provider-facing query contract.

## A-010 — Numerical Integration and State Advancement
- **Title**: Numerical integration and consistent state-advancement subsystem
- **Satisfies**: [R-004, R-016, R-032]
- **Status**: IN_PROGRESS
- **Responsibility**: Own consistent initialization, solver-backend abstraction, state-advancement policy, and solver-facing diagnostics while keeping concrete numerical backends replaceable.
- **Owned Concepts**: Consistent initialization; step-size and tolerance policy; solver backend selection; convergence and termination diagnostics; replay-stable state advancement.
- **Inputs**: Validated initial conditions and tolerances; mechanics state and constraint contracts; time span requests; orchestrator lifecycle requests.
- **Outputs**: Advanced state snapshots; startup validity status; convergence and residual diagnostics; deterministic solver termination reasons.
- **Depends On**: Configuration and validation; mechanics state and constraint contracts; documented numerics policy; orchestrator lifecycle contracts.
- **Must Not Depend On**: Hydro or aero provider internals beyond public load contracts; output serialization internals; optional offline truth-model toolchains.
- **Invariants**: Concrete solver choice remains hidden behind a stable contract; consistent initialization occurs before runtime stepping; solver failures map to deterministic diagnostics; replay expectations remain scoped to the same executable and platform unless a broader guarantee is documented.
- **Allocation Rationale**: Separates numerical backend ownership and startup validity from physical-state ownership so mechanics models and solver strategies can evolve independently without leaking backend choices into product requirements.
- **Future Absorption**: Alternative integrators, sensitivity analysis support, adaptive stepping policies, and richer DAE initialization helpers should extend this subsystem.
- **Interfaces**: Consistent-initialization contract, state-advancement contract, and solver-diagnostic contract. The current realization slice establishes a stable advancer interface plus deterministic internal startup and stepping behavior while deferring Chrono or SUNDIALS integration behind that seam.
