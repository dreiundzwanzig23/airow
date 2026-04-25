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

## Architecture Overview

This document has two roles:
- provide a compact architecture narrative for humans,
- provide the stable `A-*` ownership catalog used by traceability and TDD.

Use the overview sections below to understand the current system shape quickly.
Use the `A-*` sections as the normative ownership and allocation record.

## System Context

The baseline system is a headless single-scull rowing simulator with one shared
runtime path for CLI and in-memory execution.

Primary external inputs:
- machine-readable run configuration,
- checked-in scenario definitions and acceptance envelopes,
- optional future calibration or truth-model artifacts.

Primary external outputs:
- run status and diagnostics,
- machine-readable summary and time-series artifacts,
- scenario pass or fail results for automated verification.

Protected external boundaries:
- the default runtime must remain usable without optional high-fidelity or
  calibration toolchains,
- requirements stay technology-neutral even when preferred libraries are named
  elsewhere,
- frame, sign, unit, and validity metadata must stay explicit at simulator
  boundaries.

## Building Blocks

The current stable building-block view is organized around ten subsystem owners.

| ID | Subsystem | Current role |
|---|---|---|
| `A-001` | Configuration and Validation | Parse, validate, normalize, and reject unsupported scope before runtime starts |
| `A-002` | Simulation Orchestrator | Own run lifecycle, composition, CLI and in-memory entry points, and stable failure handling |
| `A-003` | Mechanics Subsystem | Own hull, oars, seat, stroke state, and mechanics-facing startup contracts |
| `A-004` | Hydro Runtime Models | Own reduced hull and blade water loads for the default runtime path |
| `A-005` | Aero Runtime Models | Own apparent wind and reduced aerodynamic loads |
| `A-006` | Control and Stroke Input | Own prescribed stroke scheduling and later low-order control |
| `A-007` | Output and Diagnostics | Own summary and time-series artifacts, metadata, and stable diagnostics |
| `A-008` | Scenario Harness and Validation | Own named scenarios, envelopes, and validation pass or fail logic |
| `A-009` | External Calibration Integration | Own optional artifact ingestion and provenance semantics |
| `A-010` | Numerical Integration and State Advancement | Own consistent initialization, backend abstraction, and solver-facing diagnostics |

Current implementation emphasis:
- active: `A-001`, `A-002`, `A-003`, `A-004`, `A-005`, `A-007`, `A-008`,
  `A-009`, `A-010`,
- open: `A-006`.

## Runtime View

### Baseline Single Run
1. `A-001` validates and normalizes the input configuration.
2. `A-002` stamps metadata, wires dependencies, and owns lifecycle behavior.
3. `A-010` initializes the first valid mechanics state through a stable
   advancer contract.
4. `A-003` provides the boundary-visible mechanical state carried through the
   run loop.
5. `A-002` samples hydro and aero provider seams and advances the state.
6. `A-007` emits structured outputs and stable diagnostics.

### Scenario Validation Run
1. `A-008` loads a checked-in scenario definition and acceptance envelope.
2. `A-002` executes the scenario through the same shared run path used by the
   CLI and in-memory API.
3. `A-008` evaluates the resulting summary and history against deterministic
   acceptance rules.

### Backend Evolution Path
- The current state-advancement boundary is a composed mechanics plus
  integration contract: the shared standard runtime prefers
  `chrono_rigidbody + sundials_ida`, preserves
  `internal_baseline + sundials_ida` as the supported fallback, and keeps
  `internal_baseline + deterministic_baseline` as the explicit debug fallback.
- Concrete backends remain hidden behind `A-010` and are exposed externally
  only through stable mechanics-backend or integration-backend selection ids,
  policy metadata, and solver-status fields.
- Higher-fidelity hydro, aero, and calibration paths are intended to plug in
  behind their subsystem contracts without changing requirement wording or the
  shared orchestration path.

## Cross-Cutting Concepts

### Determinism and Replay
- The same validated run definition should execute through the same
  orchestration path for CLI and in-memory use.
- Deterministic behavior is scoped to the same executable and platform unless a
  broader guarantee is documented.

### Traceability and Verification
- Requirements allocate into cohesive subsystem owners, not one-to-one feature
  containers.
- `UT-*`, `IT-*`, and `QT-*` evidence attach to design, architecture, and
  requirement layers separately.
- Named scenarios are first-class evidence, not only convenience regression
  cases.

### Runtime vs Truth-Model Separation
- The default runtime path must stay independent of optional SPH, CFD, and
  calibration-generation toolchains.
- External truth-model workflows are expected to support calibration,
  comparison, or surrogate generation rather than become mandatory runtime
  dependencies.

### State, Frame, and Numerics Discipline
- Frame and sign conventions are explicit and must remain stable at boundaries.
- Numerical integration ownership is separate from mechanics ownership.
- Validity metadata and deterministic diagnostics are part of the boundary
  contract, not implementation details.

## Implementation Status Snapshot

Implemented baseline today:
- deterministic JSON configuration loading and validation,
- shared headless CLI and in-memory single-run orchestration,
- boundary-visible hull, oar, seat, and stroke state contracts,
- deterministic internal startup and state advancement seam,
- structured hydro-provider samples with deterministic reduced passive, tow,
  and calm-water propulsion behavior, including hydro moments and blade
  immersion diagnostics,
- structured JSON output and optional HDF5 output contract,
- checked-in passive-float, tow-test, calm-water stroke, headwind stroke, and
  crosswind stroke scenario evaluation.

Still planned or incomplete:
- the current reduced hydro and steady-wind aero built-in providers are the
  supported default-runtime baseline for the closed Slice 2 packet,
- future hydro or aero expansion must be scoped as a new packet without
  changing the current provider-selection or metadata contracts by default,
- the closed Slice 4A packet adds one deterministic file-backed calibration
  artifact path and one explicit calibrated aero provider without changing the
  existing default-runtime baseline provider ids,
- the closed Slice 4B packet adds deterministic time-varying ambient-wind
  inputs plus one checked-in gust scenario without changing the current
  steady-wind provider ids or the orchestrator-to-aero seam,
- `A-009` remains in progress for broader artifact schemas and future hydro-
  side consumers beyond the first calibrated aero path,
- the next full-simulation phase is now staged in
  `docs/process/ROADMAP_FULL_SIMULATION.md`, with `R-050..R-071` merged into
  `docs/process/REQUIREMENTS.md` as the active extension backlog,
- the highest-value near-term packet is capability and trust visibility plus
  visualization-ready artifact contracts before deeper geometry or physics
  providers are reopened,
- the active next packet should preserve the current reduced runtime while
  exposing what is active, reduced, validated, unavailable, or planned through
  outputs, reports, and analysis workflows,
- deeper backend diagnostics, richer stepping policy, or future composed-
  backend expansion behind the existing seams.

## Next Full-Simulation Allocation

The next major phase extends the existing subsystem map instead of creating new
top-level owners.

- Capability, trust, explanation, and analysis artifacts (`R-050`, `R-052`,
  `R-062`, `R-070`, `R-071`) should start from `A-007`, with `A-001`,
  `A-008`, and `A-009` support where configuration, scenarios, or provenance
  are involved.
- Interactive playback, linked plots, comparison views, debug replay, and
  ParaView/VTK export (`R-051`, `R-053`, `R-054`, `R-055`, `R-056`) should
  extend `A-007` and `A-008` before introducing any new viewer-facing owner.
- Geometry, 6-DOF hull behavior, 3D blade loads, coupled rower/oar mechanics,
  interface loads, environment effects, and flexible-oar readiness
  (`R-057..R-064`) should be allocated across `A-003`, `A-004`, `A-006`,
  `A-009`, and `A-010` according to existing ownership boundaries.
- Truth-model studies, imports, validation scorecards, calibration lifecycle,
  and optimization-safe outputs (`R-065..R-069`) should reuse `A-008` and
  `A-009`, with `A-007` carrying report and output contracts.

Allocation guardrails:
- keep `A-004` focused on reduced runtime hydro behavior and `A-009` focused
  on external artifact contracts, provenance, and offline-loop boundaries,
- keep `A-010` focused on constrained initialization, stepping, and solver
  diagnostics needed by richer coupling rather than on hydro or control model
  semantics,
- keep `A-006` limited to low-order actuation and control rather than turning
  it into a full biomechanics container.

## A-001 — Configuration and Validation
- **Title**: Deterministic configuration and validation subsystem
- **Satisfies**: [R-001, R-017, R-020, R-021, R-022, R-023, R-025, R-028, R-029, R-030, R-031, R-032, R-033, R-036, R-037, R-038, R-039, R-048, R-049]
- **Status**: IN_PROGRESS
- **Responsibility**: Parse, validate, normalize, and expose simulator, provider, artifact, and scenario configuration before execution begins.
- **Owned Concepts**: `SimulatorConfig`; provider selection metadata; schema validation; unit-bearing and frame-bearing field definitions; provider validity metadata; unsupported-scope rejection; constant and time-varying ambient-wind input definitions; measurement or rigging or athlete dataset boundary contracts; measured-trial import metadata; trust-envelope-bearing configuration definitions.
- **Inputs**: Machine-readable run configuration; batch definitions; external artifact metadata; selected provider and model identifiers.
- **Outputs**: Validated in-memory configuration; deterministic validation diagnostics; normalized run metadata inputs.
- **Depends On**: Core validation helpers and documented process policies for units, state conventions, and provenance.
- **Must Not Depend On**: Mechanics stepping logic; hydro or aero algorithm internals; scenario execution state.
- **Invariants**: Invalid or ambiguous configuration never reaches runtime stepping; accepted values are normalized deterministically; unsupported scope is rejected explicitly.
- **Allocation Rationale**: Centralizes all boundary validation so requirements about deterministic rejection, units, provider selection, and scope control do not fragment across runtime subsystems.
- **Future Absorption**: Additional model toggles, boat-class expansion gates, richer artifact schemas, measurement or rigging parameter-set contracts, measured-trial import definitions, truth-model exchange identifiers, and future frame-bearing configuration definitions should be absorbed here before touching runtime logic.
- **Interfaces**: File-backed and in-memory JSON configuration loading contract returning validated `SimulatorConfig`, deterministic diagnostics, and normalized configuration metadata suitable for later runtime orchestration.
  The same boundary also absorbs ordered batch definitions through a top-level
  `batch.cases` contract that applies deterministic per-case override objects
  onto one validated shared base configuration before runtime execution.
  The same boundary now also owns the optional top-level `boat_class`
  selector, defaulting omitted configs to `single_scull` and rejecting any
  explicit unsupported boat class deterministically before runtime stepping so
  future scope expansion cannot silently alias onto the baseline runtime.
  The current slice also validates the optional top-level `providers` block
  (`hull_resistance`, `blade_force`, `aero_load`) against the built-in runtime
  provider catalog and rejects unknown selections before execution. The closed
  backend slice extends the same boundary with
  `simulation.mechanics_backend` and `simulation.integration_backend`
  selection for built-in backend ids, defaulting to the preferred supported
  runtime for the current build and rejecting unknown, unavailable, or
  unsupported backend pairs deterministically. The current `R-024` guardrail
  packet extends the same boundary with one optional
  `output.truth_model_export_path` field that is disabled by default, echoed
  through normalized metadata when present, and used only to request a
  deterministic offline truth-model handoff artifact without changing the
  normal runtime path. The active fidelity packet extends the same config
  boundary with separate `artifacts.measurement_bundle` and
  `artifacts.measured_trial` references plus an explicit `stroke.actuation`
  object. That object owns mode selection
  (`prescribed_kinematic`, `force_driven`, or `power_driven`), bounded command
  inputs, low-speed power-to-force floor policy, and deterministic rejection of
  missing, incompatible, or overconstrained actuation definitions before
  runtime stepping. When a measurement bundle is present, this boundary also
  validates its reference-contract metadata and applies bundle-owned hull,
  rigging, and athlete mass-distribution values onto the effective runtime
  configuration before startup validation while leaving run-specific initial
  conditions, environment, and output paths in the main config. The `R-038`
  close-out proof reuses this same boundary rather than adding a new owner:
  parameter-sensitive evidence is generated by running the shared path against
  multiple valid measurement bundles and comparing the resulting trim or
  performance outputs.

## A-002 — Simulation Orchestrator
- **Title**: Headless simulation orchestration subsystem
- **Satisfies**: [R-002, R-003, R-004, R-016, R-020, R-023, R-024, R-025, R-034]
- **Status**: IN_PROGRESS
- **Responsibility**: Coordinate one or more simulation runs, lifecycle transitions, provider wiring, failure handling, and deterministic replay.
- **Owned Concepts**: Run lifecycle; in-memory simulation API; CLI entry wiring; provider registry binding; batch execution flow.
- **Inputs**: Validated configuration; selected runtime providers; time-varying inputs; scenario harness requests.
- **Outputs**: Run status; structured result objects; deterministic failure codes; per-case batch outcomes.
- **Depends On**: Configuration and validation; imported artifact contracts; mechanics; hydro; aero; control; numerical integration; output and diagnostics contracts.
- **Must Not Depend On**: Provider-specific physics internals; calibration artifact generation; scenario acceptance logic internals.
- **Invariants**: The same validated run definition executes through the same orchestration path for CLI and in-memory use; failures terminate with stable diagnostics; batch case ordering remains deterministic.
- **Allocation Rationale**: Groups lifecycle and composition concerns into one owner instead of scattering them across feature-specific requirements and runtime providers.
- **Future Absorption**: Parallel batch scheduling, checkpoint/restart, and richer orchestration policies should extend this subsystem.
- **Interfaces**: CLI entry contract and in-memory run API that share one
  deterministic single-run execution path, plus injected hydro and aero stub
  provider seams, stable run-result metadata, exit-code mapping for the first
  headless baseline, config-driven built-in provider construction when injected
  provider seams are absent, config-driven built-in mechanics and integration
  backend composition when an injected advancer seam is absent, one shared
  ambient-wind sampler that resolves legacy constant wind, replayed sampled
  wind, or authored wind profiles into the per-step world-frame ambient vector
  passed to the aero seam, one ordered batch executor that reuses the same
  shared single-run path for each resolved case while preserving deterministic
  case ordering and isolated per-case results, optional human-readable report
  rendering modes for successful single-run inspection, and one optional
  truth-model handoff export mode that emits a deterministic JSON input bundle
  when `output.truth_model_export_path` is configured while leaving the
  ordinary runtime lifecycle unchanged and free of mandatory external
  truth-model tooling. The active fidelity packet also makes the shared run
  path the composition root for imported measurement-bundle, measured-trial,
  and calibration artifacts after those files have been validated against the
  `A-009` public contracts, so runtime overlay and provenance binding happen in
  one place instead of being duplicated across provider or output subsystems.

## A-003 — Mechanics Subsystem
- **Title**: 3D mechanics core for hull, oars, and seat motion
- **Satisfies**: [R-005, R-006, R-007, R-008, R-012, R-027, R-028, R-030, R-031, R-032, R-038, R-039, R-040, R-044]
- **Status**: IN_PROGRESS
- **Responsibility**: Own the constrained 3D mechanical state for hull, oars, seat, optional flexible oar behavior, low-order rower inertial coupling points, and optional stabilization control coupling points.
- **Owned Concepts**: Hull rigid-body state; oar kinematics; seat travel state; prescribed stroke state; consistent initial mechanical state; low-order rower center-of-mass coupling points; optional balance control interfaces; optional rigid vs flexible oar representation.
- **Inputs**: Validated geometry and mass properties; control or prescribed stroke inputs; external hydro and aero loads.
- **Outputs**: 3D state trajectories; state-initialization and constraint-residual diagnostics; blade and hull state needed by load providers.
- **Depends On**: Core math/contracts; control and stroke input; hydro and aero load contracts.
- **Must Not Depend On**: Output formatting internals; calibration dataset ingestion; scenario acceptance logic; hard-coded solver-backend policy.
- **Invariants**: Mechanical state remains finite in nominal runs; constraints remain bounded by documented tolerances; startup assembly exposes enough information for consistent initialization; optional mechanics modes do not silently change the baseline rigid path; unsupported boat classes are rejected at the configuration boundary rather than silently entering the single-scull mechanics path.
- **Allocation Rationale**: Keeps the mechanics backbone as the central physical state owner rather than leaking motion ownership into individual requirement-driven feature slices.
- **Future Absorption**: Expanded body representations, future crew support, low-order rower inertial coupling, hull or rigging parameter-set realization, and deeper rower dynamics should be absorbed here behind stable state contracts.
- **Interfaces**: Mechanics state contract, external-load application contract, and subsystem initialization contract for state advancement. The current realization slice establishes boundary-visible hull, oar, seat, and stroke state behind the orchestrator seam using a deterministic internal baseline implementation that remains independent of concrete Chrono types.
  The closed backend slice promotes Chrono-backed rigid-body realization to the
  preferred standard runtime build while keeping the same boundary-visible
  state contract and preserving the deterministic internal baseline path as a
  fallback and cross-check implementation. The active fidelity packet extends
  the same state contract with low-order rower center-of-mass position and
  velocity plus the resulting inertial load contribution derived from seat
  motion under bounded configuration. Mechanics remains responsible for
  realizing those coupling points and any effective hull-load redistribution
  they imply, while keeping the disabled path behavior-compatible with the
  existing reduced baseline. `R-038` parameter sensitivity relies on this same
  realization seam to turn imported hull, rigging, and athlete mass properties
  into deterministic state and load deltas without introducing a separate hull-
  study-only mechanics path.

## A-004 — Hydro Runtime Models
- **Title**: Reduced hydrodynamic runtime models
- **Satisfies**: [R-009, R-010, R-011, R-012, R-020, R-021, R-022, R-024, R-029, R-033, R-041, R-042, R-043, R-044, R-046, R-049]
- **Status**: IN_PROGRESS
- **Responsibility**: Compute hydrostatic and reduced hydrodynamic loads for the hull and blades during default runtime execution.
- **Owned Concepts**: Hull flotation model; hull resistance providers; blade-water load providers; provider validity metadata; blade slip or propulsion-efficiency metric inputs; calibrated reduced hull or blade provider variants; disturbance-to-water-load coupling points.
- **Inputs**: Mechanics state; validated provider selection; optional calibration datasets; optional disturbance definitions.
- **Outputs**: Hull and blade load vectors; immersion diagnostics; provider metadata for outputs and validity reporting.
- **Depends On**: Core math/contracts; mechanics state contracts; configuration/provider selection; optional calibration contracts.
- **Must Not Depend On**: Aero internals; orchestrator lifecycle logic; high-fidelity truth-model toolchains in the default runtime path.
- **Invariants**: Reduced-model providers remain callable without optional high-fidelity tooling; dry or zero-speed limits behave deterministically; provider outputs stay finite or fail deterministically; each provider carries explicit validity metadata suitable for run reporting.
- **Allocation Rationale**: Concentrates water-load logic into one subsystem so runtime hydro behavior evolves through replaceable providers instead of requirement-specific patches.
- **Future Absorption**: Additional resistance, blade, wave, lookup-driven hydro providers, calibrated reduced blade models, calibrated reduced hull-performance models, and trim or off-axis penalty terms should be absorbed here behind shared contracts.
- **Interfaces**: Public hydro-provider seam in the shared run path that now
  returns structured hull force or moment plus per-blade world-force and
  immersion samples for deterministic reduced passive, tow, and calm-water
  propulsion providers. The current realization slice composes always-on
  passive restoring behavior with independently selected built-in hull
  resistance and blade-force roles, couples longitudinal and vertical
  hydro-force components plus roll or pitch restoring moments into the
  internal baseline advancer, and leaves fuller sway or yaw hydro dynamics for
  later work. Slice 2 closure keeps the existing built-in
  `quadratic_drag_placeholder` and `stroke_propulsion_placeholder` ids stable
  as the supported reduced default-runtime baseline with low-speed hull
  damping, speed-squared resistance, and phase-, immersion-, and backward-
  slip-shaped blade propulsion behavior. Future hydro expansion remains
  provider-only and should preserve the current `A-010` load-to-motion
  coupling boundary unless a later architecture packet says otherwise.

## A-005 — Aero Runtime Models
- **Title**: Reduced aerodynamic runtime models
- **Satisfies**: [R-013, R-014, R-020, R-021, R-022, R-023, R-024, R-029, R-031, R-033]
- **Status**: IN_PROGRESS
- **Responsibility**: Compute apparent wind and reduced aerodynamic loads for routine runtime execution.
- **Owned Concepts**: Apparent wind computation; aero load providers; wind-direction interpretation at runtime boundaries; provider validity metadata; steady and time-varying wind handling; disturbance-to-aero coupling.
- **Inputs**: Mechanics state; ambient wind definitions; optional calibration datasets; selected aero provider.
- **Outputs**: Apparent wind state; aerodynamic force and moment outputs; provider metadata for diagnostics and validity reporting.
- **Depends On**: Core math/contracts; mechanics state contracts; configuration/provider selection; optional calibration contracts.
- **Must Not Depend On**: Hydro internals; orchestrator lifecycle policy; optional external CFD toolchains for the baseline runtime path.
- **Invariants**: Zero apparent wind yields near-zero loads; mirrored crosswind directions produce deterministic sign changes under the documented frame conventions; runtime behavior stays independent of optional truth-model tooling.
- **Allocation Rationale**: Keeps aero behavior replaceable and decoupled from hydro and orchestration instead of creating mixed fluid-load feature containers.
- **Future Absorption**: Gust models, richer aero coefficients, and calibrated aero providers should extend this subsystem.
- **Interfaces**: Public aero-provider seam in the shared run path now returns
  structured apparent-wind, aerodynamic-force, and aerodynamic-moment samples
  for deterministic steady-wind baseline behavior. The current realization
  slice resolves the built-in `steady_wind_placeholder` or `none` provider from
  configuration, propagates full aero vectors and yaw-sign information through
  outputs and scenario evaluation, and keeps dynamic state advancement coupled
  only to the longitudinal aero-force component. Slice 2 closure keeps the
  existing built-in `steady_wind_placeholder` id stable as the supported
  reduced steady-wind default-runtime baseline with explicit headwind drag
  sensitivity plus deterministic crosswind lateral and yaw behavior. Slice 4B
  extends the same provider seam to consume orchestrator-sampled time-varying
  ambient-wind vectors without changing the built-in provider ids or
  broadening the current state-advancer coupling boundary. Slice 4A adds a
  separate built-in
  `steady_wind_calibrated` provider id that consumes a validated imported
  calibration artifact through `A-009` while leaving the baseline id and its
  default-runtime contract unchanged.

## A-006 — Control and Stroke Input
- **Title**: Stroke scheduling and low-order control subsystem
- **Satisfies**: [R-008, R-023, R-027, R-039, R-040, R-041, R-045]
- **Status**: IN_PROGRESS
- **Responsibility**: Generate deterministic prescribed stroke inputs plus bounded low-order actuation and optional stabilization control outputs.
- **Owned Concepts**: Stroke schedule definition; phase timing; force-driven or power-driven actuation modes; controller mode selection; wind-aware input scheduling hooks.
- **Inputs**: Validated schedule and control configuration; optional time-varying inputs; mechanics state feedback when control mode requires it.
- **Outputs**: Stroke commands; optional control outputs; mode metadata for run reporting.
- **Depends On**: Core math/contracts; configuration and validation; mechanics feedback contracts.
- **Must Not Depend On**: Hydro or aero algorithm internals; scenario result evaluation; output serialization internals.
- **Invariants**: Prescribed schedules replay deterministically; invalid schedules are rejected before runtime; optional controllers remain finite and can be disabled cleanly.
- **Allocation Rationale**: Separates rower-input generation from mechanics and load-provider ownership so input strategies can evolve independently.
- **Future Absorption**: Richer low-order actuation modes, bounded rower-coupling controls, technique-comparison input families, and eventual deeper rower representations should be absorbed here until a stronger biomechanics boundary is justified.
- **Interfaces**: Planned stroke schedule contract and optional controller
  contract. Slice 4B keeps time-varying wind ownership out of this subsystem
  for now, but preserves future wind-aware scheduling hooks so later control or
  pacing work can consume the same validated time-varying input definitions
  without moving ambient-wind sampling out of `A-002`. The active fidelity
  packet realizes the first bounded actuation contract behind the same seam by
  emitting deterministic per-step stroke-command metadata for prescribed,
  `force_driven`, and `power_driven` modes without taking ownership of hydro
  provider internals. `A-006` owns command shaping, mode metadata, and
  low-speed power-floor policy; downstream runtime subsystems consume those
  commands through stable mechanics and load contracts. The current realized
  slice adds one public control helper that maps validated configuration into a
  narrow per-step stroke-command packet, keeping the full configuration schema
  out of mechanics-facing runtime context while preserving deterministic mode
  selection and bounded command inputs.

## A-007 — Output and Diagnostics
- **Title**: Structured outputs and runtime diagnostics subsystem
- **Satisfies**: [R-003, R-004, R-015, R-016, R-022, R-025, R-031, R-033, R-034, R-035, R-041, R-045, R-046, R-047, R-049]
- **Status**: IN_PROGRESS
- **Responsibility**: Capture machine-readable summaries, time series, metadata, actionable diagnostics, and human-readable derived analysis for single and batch runs.
- **Owned Concepts**: Run metadata; summary outputs; time-series emission; force and power accounting channels; frame and unit annotations; failure diagnostics; provenance and validity propagation into outputs; derived run-analysis summaries; comparison metrics; trust-envelope and uncertainty reporting; offline report generation contracts.
- **Inputs**: Run lifecycle events; mechanics, hydro, aero, control, and numerical integration outputs; calibration provenance identifiers.
- **Outputs**: Summary artifacts; time-series artifacts; diagnostic records; per-case batch result summaries.
- **Depends On**: Orchestrator lifecycle contracts; subsystem output contracts; configuration metadata; provenance metadata.
- **Must Not Depend On**: Mechanics stepping internals; provider algorithms; scenario pass/fail policy logic.
- **Invariants**: Output ordering is deterministic; enabled outputs include required identifiers, provenance, validity metadata, and frame or unit annotations for vector quantities; failure diagnostics remain stable and actionable.
- **Allocation Rationale**: Prevents metadata, diagnostics, and serialization concerns from leaking into every runtime subsystem.
- **Future Absorption**: Richer artifact formats, streamed diagnostics, analysis-facing summaries, comparison reports, slip or efficiency metrics, measured-trial agreement outputs, and trust or uncertainty annotations should grow here behind stable output contracts.
- **Interfaces**: Stable run-result contract with deterministic JSON summary and
  time-series artifact emission plus optional HDF5 artifact emission selected
  at runtime configuration (`json`, `hdf5`, or both). The output contract
  preserves explicit unit/frame annotations for boundary-visible vectors,
  structured hull/blade force channels, force/power accounting channels,
  deterministic sampling policy, structured per-role provider metadata with
  validity descriptors in JSON or HDF5 summaries, stable diagnostics when
  requested formats are unavailable, additive derived-analysis summaries
  suitable for CLI and offline single-run inspection, and one deterministic
  batch-summary artifact that records ordered per-case identifiers, statuses,
  headline metrics, diagnostics, and emitted per-case artifact locations.
  Hydro fidelity work must preserve the existing structured provider metadata
  and emitted artifact schema while updating only the numeric contents implied
  by the richer runtime providers. The current `R-024` packet adds one optional
  JSON truth-model handoff artifact with its own explicit schema id and emitted
  path metadata, but keeps it disabled by default and separate from imported
  external-artifact provenance reporting. The `R-038` close-out proof lands in
  this subsystem through existing summary metrics and identifier metadata; it
  does not require a new output schema, only requirement-level evidence that
  those reported metrics change when the imported parameter set changes. The
  the closed `R-041` packet extends the same summary, time-series, HDF5, and
  human-readable analysis contracts with propulsion-metric support metadata,
  deterministic slip or work channels, and reusable comparison-facing metric
  names rather than creating a separate output subsystem for stroke-dynamics
  studies.

## A-008 — Scenario Harness and Validation
- **Title**: Scenario definition and validation subsystem
- **Satisfies**: [R-018, R-019, R-024, R-026, R-029, R-035, R-045, R-046, R-047, R-049]
- **Status**: IN_PROGRESS
- **Responsibility**: Own named reference scenarios, acceptance envelopes, characterization baselines, and workflow-facing validation structure.
- **Owned Concepts**: Named validation scenarios; acceptance envelopes; scenario metadata; quick vs broader validation lanes; characterization baselines; technique-comparison suites; hull or rigging sensitivity suites; measured-trial comparison suites.
- **Inputs**: Scenario configurations; run outputs; documented acceptance envelopes; process test-lane policy.
- **Outputs**: Scenario pass/fail results; validation metadata; protected baseline definitions for regression and characterization work.
- **Depends On**: Orchestrator result contracts; output artifacts; process policies for numerics and fidelity.
- **Must Not Depend On**: Runtime subsystem internals beyond their public outputs; calibration generation internals; CLI-specific code paths.
- **Invariants**: Named baseline scenarios remain headlessly runnable; acceptance checks are deterministic; quick verification lanes stay independent of optional high-fidelity tooling.
- **Allocation Rationale**: Gives validation and scenario ownership one stable home instead of scattering it across tests, scripts, and individual feature requirements.
- **Future Absorption**: Expanded scenario catalogs, acceptance dashboards, protected characterization suites, technique-comparison packs, hull or rigging sensitivity packs, and measured-trial agreement suites should extend this subsystem.
- **Interfaces**: Public scenario-harness contract in
  `include/project/orchestrator/scenario_harness.hpp` for deterministic
  checked-in scenario definition loading and acceptance-envelope evaluation
  against runtime results, with passive-float, tow, calm-water stroke,
  headwind stroke, and crosswind stroke scenario artifacts under
  `scenarios/`. The workflow-facing validation lane wrappers under
  `scripts/` also belong to this subsystem: they must propagate nested step
  failures without rewriting the exit status, and any emitted validation
  summary JSON must report step status and exit codes truthfully enough to be
  used as machine-readable local-gate evidence. The current `R-026` guardrail
  packet extends the same public surface with a checked-in
  `scenarios/performance_budgets.json` manifest for the protected core
  reference scenarios plus a deterministic budget-evaluation contract used by a
  dedicated performance-validation lane. `R-038` close-out evidence is owned
  here as a fidelity-packet comparison scenario: two valid parameter bundles
  run through the same deterministic CLI path and prove that trim or
  performance reporting changes without changing the baseline provider
  contracts. The first reusable comparison surface also stays here: `D-058`
  extends the public harness with one shared-baseline technique-comparison
  scenario type that reuses batch-style override payloads, requires an explicit
  comparison window plus varied-parameter paths, and reports deterministic
  baseline-versus-variant delta metrics without moving metric computation or
  serialization ownership out of `A-007`; broader acceleration- and
  technique-study coverage remains open under `R-045`.

## A-009 — External Calibration Integration
- **Title**: External calibration and artifact integration subsystem
- **Satisfies**: [R-021, R-022, R-024, R-036, R-037, R-043, R-044, R-048, R-049]
- **Status**: IN_PROGRESS
- **Responsibility**: Own ingestion and metadata handling for external calibration datasets and derived model artifacts while keeping the runtime path loosely coupled.
- **Owned Concepts**: Calibration dataset loading; measured-trial artifact loading; artifact provenance metadata; re-imported lookup or surrogate artifacts; offline truth-model handoff boundaries; export or import lineage management.
- **Inputs**: External machine-readable datasets; artifact identifiers; source hashes and versions; provider lookup requests.
- **Outputs**: Queryable calibration data contracts; validated provenance metadata; imported artifact handles for runtime providers.
- **Depends On**: Configuration and validation; output/provenance contracts; hydro and aero provider contracts.
- **Must Not Depend On**: Orchestrator lifecycle logic; mechanics state ownership; mandatory installation of offline truth-model toolchains.
- **Invariants**: Imported artifacts carry deterministic provenance; malformed artifacts are rejected before use; default runtime remains usable without optional calibration-generation tooling.
- **Allocation Rationale**: Isolates external data and provenance concerns so runtime subsystems can consume calibrated artifacts without becoming responsible for artifact lifecycle policy.
- **Future Absorption**: Surrogate-model ingestion, richer dataset schemas, measured-trial artifacts, hydro-side calibrated providers, and offline export/import workflows should extend this subsystem.
- **Interfaces**: Calibration dataset contract, artifact metadata contract, and
  provider-facing query contract. The active Slice 4A realization introduces a
  file-backed machine-readable calibration artifact boundary with required
  provenance fields (`source_id`, `artifact_version`, `content_hash`, and
  `schema_id`), deterministic validation before runtime stepping, and a first
  provider-facing coefficient lookup path for one explicit calibrated aero
  provider without changing the existing default built-in provider ids. Future
  `A-009` work remains responsible for richer schemas, additional runtime
  consumers, and offline import or export evolution beyond this first path. The
  current `R-024` packet keeps re-import on the existing validated calibration
  artifact path while formalizing the first exported offline handoff artifact
  schema for truth-model studies. The active fidelity packet extends the same
  artifact boundary with versioned `measurement_bundle` and `measured_trial`
  schemas that share explicit provenance, units, state-convention, and study-
  identifier fields (`boat_id`, `rigging_id`, `athlete_id`, and `trial_id`
  where applicable). `A-009` remains responsible for shared lineage and
  reference-contract checks across imported artifacts and the truth-model
  handoff path so identifier, unit, and frame mismatches are rejected
  deterministically before runtime use.

## A-010 — Numerical Integration and State Advancement
- **Title**: Numerical integration and consistent state-advancement subsystem
- **Satisfies**: [R-004, R-016, R-032, R-039, R-040]
- **Status**: IN_PROGRESS
- **Responsibility**: Own consistent initialization, solver-backend abstraction, state-advancement policy, and solver-facing diagnostics while keeping concrete numerical backends replaceable.
- **Owned Concepts**: Consistent initialization; step-size and tolerance policy; solver backend selection; convergence and termination diagnostics; replay-stable state advancement.
- **Inputs**: Validated initial conditions and tolerances; mechanics state and constraint contracts; time span requests; orchestrator lifecycle requests.
- **Outputs**: Advanced state snapshots; startup validity status; convergence and residual diagnostics; deterministic solver termination reasons.
- **Depends On**: Configuration and validation; mechanics state and constraint contracts; documented numerics policy; orchestrator lifecycle contracts.
- **Must Not Depend On**: Hydro or aero provider internals beyond public load contracts; output serialization internals; optional offline truth-model toolchains.
- **Invariants**: Concrete solver choice remains hidden behind a stable contract; consistent initialization occurs before runtime stepping; solver failures map to deterministic diagnostics; replay expectations remain scoped to the same executable and platform unless a broader guarantee is documented.
- **Allocation Rationale**: Separates numerical backend ownership and startup validity from physical-state ownership so mechanics models and solver strategies can evolve independently without leaking backend choices into product requirements.
- **Future Absorption**: Alternative integrators, sensitivity analysis support, adaptive stepping policies, richer DAE initialization helpers, and the constrained startup or stepping support needed by low-order actuation and rower inertial coupling should extend this subsystem.
- **Interfaces**: Consistent-initialization contract, state-advancement
  contract, and solver-diagnostic contract. The current realization slice
  establishes a stable advancer interface plus deterministic internal startup
  and stepping behavior, explicit blade-immersion or blade-tip-velocity state,
  and widened hydro-force coupling. The closed backend slice adds stable
  built-in mechanics-backend and integration-backend catalogs, a preferred
  `chrono_rigidbody + sundials_ida` standard runtime with fixed built-in
  SUNDIALS tolerances, explicit supported fallback pairs, deterministic
  rejection of unsupported backend combinations, and stable startup and
  runtime solver-status plus split backend-policy metadata propagation through
  the shared run path and machine-readable outputs.
