# FIDELITY_GAP_MAP.md

This document translates the current reduced-model simulator baseline into a
milestone plan for research-oriented single-scull studies.

## Current Baseline Position

The repository has a validated deterministic baseline, not yet a calibrated
research simulator.

What is mature today:
- a headless single-scull runtime with shared CLI and in-memory execution,
- explicit subsystem ownership across configuration, mechanics, hydro, aero,
  orchestration, numerics, outputs, scenarios, and external artifacts,
- a preferred `chrono_rigidbody + sundials_ida` runtime behind stable project
  contracts,
- deterministic reduced hydro and steady-wind aero providers with named
  baseline scenarios and protected performance budgets,
- one initial external-calibration ingestion path plus an offline truth-model
  handoff export boundary.

What is still reduced or placeholder in study terms:
- the default blade and hull providers remain low-order reduced models,
- stroke generation is still prescribed rather than actuation-driven,
- boat, rigging, athlete, and measured-trial data contracts are not yet rich
  enough for serious calibration or comparison work,
- scenario evidence proves deterministic baseline behavior, not yet agreement
  against measured rowing studies,
- outputs carry provider validity metadata but not yet uncertainty, fit
  quality, or explicit trust-envelope reporting for study claims.

## Dominant Fidelity Gaps

### Hull Performance Studies

The current runtime can replay reduced propulsion and resistance behavior, but
it cannot yet support serious hull-performance questions about:
- how hull or crew mass distribution changes trim and resistance,
- how rigging changes alter speed, efficiency, or off-axis penalties,
- how calibrated reduced hull behavior compares against measured or offline
  reference data,
- how trustworthy a given hull or rigging conclusion is outside the validated
  baseline scenarios.

The main blockers are:
- missing boat, rigging, and athlete measurement contracts,
- no calibrated reduced hull model for resistance, trim, or off-axis effects,
- no sensitivity-scenario packet for hull or rigging comparisons,
- no uncertainty or validity-range reporting for study outputs.

### Stroke Dynamics Studies

The current runtime can replay a prescribed stroke schedule, but it cannot yet
support serious stroke-dynamics questions about:
- how force application, seat motion, and rower inertia change propulsion,
- how catch or release timing affects blade slip and intra-stroke losses,
- how simulated strokes compare with measured trials,
- how much confidence to place in technique conclusions from a given reduced
  model.

The main blockers are:
- no force-driven or power-driven actuation mode,
- no low-order rower center-of-mass coupling model,
- no calibrated reduced blade-water model with richer entry or extraction
  behavior,
- no comparison metrics for slip, efficiency, or measured-trial agreement.

## Fidelity Layers

The next phase must keep three layers explicit.

### Layer 0 — Validated Reduced Baseline
- Current status: implemented and validated.
- Role: deterministic single-scull baseline for architecture, workflow,
  debugging, and bounded reduced-model studies.
- Runtime expectation: built-in reduced providers remain usable without
  external artifacts or truth-model tooling.

### Layer 1 — Calibrated Reduced Runtime
- Current status: planned next-phase target.
- Role: research-capable runtime for hull-performance and stroke-dynamics
  studies using richer reduced models, measured data contracts, and imported
  calibration artifacts.
- Runtime expectation: still runs inside the default headless simulator
  without invoking SPH or CFD tooling online.

### Layer 2 — Offline Truth Models and Surrogate Generation
- Current status: boundary only.
- Role: export inputs, run optional external studies offline, fit or derive
  reduced artifacts, and re-import them through explicit artifact contracts.
- Runtime expectation: remains optional, offline, and decoupled from the
  default runtime.

## Milestones

### Milestone 1 — Fidelity Target and Trust Envelope
- Purpose: define the supported study questions, fidelity tiers, validity
  claims, and trust-reporting rules for the next phase.
- Why it matters: without an explicit trust envelope, richer outputs would
  still not tell users which hull or stroke conclusions are defensible.
- Primary owners: `A-007`, `A-008`.
- Supporting owners: `A-001`, `A-009`.
- Likely artifacts touched: `docs/process/REQUIREMENTS.md`,
  `docs/process/ARCHITECTURE.md`, `docs/process/MODEL_FIDELITY.md`,
  `docs/ai/ROADMAP.md`, output metadata contracts, scenario manifests.
- Expected deliverables:
  - fidelity-layer and study-question definitions,
  - machine-readable trust or validity reporting contract,
  - scenario/report guidance for when a run is in-baseline, calibrated-study,
    or outside-validity use.
- Validation and evidence expectations:
  - documented trust-envelope rules in process docs,
  - output-contract tests for fidelity or trust metadata,
  - scenario/report evidence that ties at least one study claim to an explicit
    envelope.
- Requirement packet: `R-035`, `R-049`.
- Non-goals:
  - changing runtime physics by itself,
  - expanding into sweep, crew, or GUI-first work.
- Exit criteria:
  - the repo can state what study claims are intended,
  - outputs and reports can identify the trust category of a run,
  - future packets have a stable reporting target.

### Milestone 2 — Measurement and Calibration Foundation
- Purpose: define data contracts for boat, rigging, athlete, and measured-trial
  inputs that later calibrated reduced models can rely on.
- Why it matters: hull and stroke studies are blocked until measured data can
  be ingested, validated, versioned, and traced.
- Primary owners: `A-001`, `A-009`.
- Supporting owners: `A-007`, `A-008`.
- Likely artifacts touched: configuration schemas, artifact schemas, provider
  lookup contracts, report metadata, scenario fixtures, example measurement
  bundles.
- Expected deliverables:
  - versioned boat or rigging or athlete parameter-set contracts,
  - versioned measured-trial time-series contract,
  - deterministic validation and provenance rules,
  - imported dataset identifiers propagated to outputs.
- Validation and evidence expectations:
  - unit and integration tests for schema rejection and normalized metadata,
  - at least one minimal valid measurement bundle,
  - one traceable comparison-ready artifact path through outputs.
- Requirement packet: `R-036`, `R-037`, `R-038`, `R-048`.
- Non-goals:
  - online truth-model execution,
  - broad multi-boat asset management.
- Exit criteria:
  - later calibration or comparison packets can consume measured inputs
    without inventing new boundary contracts,
  - runtime outputs preserve dataset lineage and identifiers.

### Milestone 3 — Low-Order Actuation and Rower Coupling
- Purpose: move from pure stroke replay to a low-order actuation model that is
  still reduced, deterministic, and bounded.
- Why it matters: stroke-dynamics studies need more than phase replay to test
  force timing, power delivery, and rower inertial effects.
- Primary owners: `A-006`, `A-003`.
- Supporting owners: `A-010`, `A-007`.
- Likely artifacts touched: stroke-input schemas, mechanics or control
  contracts, state-advancement coupling, output channels, technique scenarios.
- Expected deliverables:
  - selectable kinematic vs force/power-driven actuation modes,
  - low-order rower mass or center-of-mass coupling,
  - output channels for commanded and realized actuation plus rower inertial
    effects.
- Validation and evidence expectations:
  - deterministic configuration rejection for invalid actuation packets,
  - startup and stepping evidence under the preferred runtime,
  - comparison scenarios showing actuation-mode effects without full
    biomechanics.
- Requirement packet: `R-039`, `R-040`, `R-041`.
- Non-goals:
  - full musculoskeletal modeling,
  - OpenSim integration in the default runtime.
- Exit criteria:
  - the simulator can run at least one non-kinematic stroke mode,
  - rower coupling changes loads or motion in a traceable way,
  - the reduced baseline path remains selectable and stable.

### Milestone 4 — Calibrated Reduced Blade-Water Model
- Purpose: replace the current placeholder-style propulsion logic with a richer
  reduced blade model calibrated from explicit data sources.
- Why it matters: blade slip, catch or release behavior, and intra-stroke
  losses are central to stroke-dynamics studies.
- Primary owners: `A-004`, `A-009`.
- Supporting owners: `A-007`, `A-008`, `A-010`.
- Likely artifacts touched: hydro providers, calibration artifact schemas,
  blade diagnostics, validation scenarios, comparison reports.
- Expected deliverables:
  - calibrated reduced blade provider ids and contracts,
  - explicit catch, drive, extraction, and dry-transition behavior,
  - slip, work, and efficiency reporting tied to provider validity metadata.
- Validation and evidence expectations:
  - unit and integration coverage for transitions and finite loads,
  - at least one calibrated artifact fixture,
  - agreement evidence against reference blade or propulsion data.
- Requirement packet: `R-041`, `R-042`, `R-043`.
- Non-goals:
  - online SPH inside the default runtime,
  - flexible oars.
- Exit criteria:
  - blade-study outputs are no longer limited to phase-shaped placeholder
    propulsion,
  - calibrated blade runs carry provenance, validity range, and agreement
    metrics.

### Milestone 5 — Calibrated Reduced Hull-Performance Model
- Purpose: extend hull-water modeling toward meaningful studies of resistance,
  trim, loading, and rigging sensitivity.
- Why it matters: hull-performance studies require more than speed-squared drag
  with fixed restoring terms.
- Primary owners: `A-004`.
- Supporting owners: `A-003`, `A-009`, `A-007`, `A-008`.
- Likely artifacts touched: hull resistance providers, mass-property inputs,
  calibration artifacts, sensitivity scenarios, analysis reports.
- Expected deliverables:
  - reduced hull model that responds to speed, attitude, loading, and rigging
    parameters,
  - calibrated trim or off-axis penalty terms when supported by artifacts,
  - outputs that expose the terms needed for hull comparisons.
- Validation and evidence expectations:
  - deterministic sensitivity scenarios,
  - comparison evidence against measured or offline reference hull data,
  - protected reporting of validity range and calibration lineage.
- Requirement packet: `R-038`, `R-044`, `R-046`.
- Non-goals:
  - broad seaway or disturbance modeling,
  - new top-level hydro/truth-model architecture.
- Exit criteria:
  - the simulator can compare at least two hull or rigging setups through a
    calibrated reduced runtime path,
  - reported conclusions include trim or resistance context and validity tags.

### Milestone 6 — Coupled Validation Scenarios for Real Rowing Questions
- Purpose: turn richer physics and imported data into scenario evidence for
  technique, rigging, hull, and measured-trial comparison questions.
- Why it matters: without scenario-backed evidence, new fidelity packets would
  remain untrusted implementation detail.
- Primary owners: `A-008`, `A-007`.
- Supporting owners: `A-004`, `A-006`, `A-009`.
- Likely artifacts touched: scenario manifests, acceptance envelopes,
  comparison tooling, reports, protected validation lanes.
- Expected deliverables:
  - technique-comparison scenario family,
  - hull or rigging sensitivity scenario family,
  - measured-vs-sim comparison scenario family,
  - deterministic delta-metric reporting.
- Validation and evidence expectations:
  - checked-in scenario artifacts for each study family,
  - requirement-level `QT-*` evidence for delta metrics and agreement metrics,
  - stable machine-readable comparison summaries.
- Requirement packet: `R-045`, `R-046`, `R-047`.
- Non-goals:
  - replacing unit or integration evidence with scenario-only checks,
  - GUI-led analysis workflows.
- Exit criteria:
  - the repo can answer at least one technique question and one hull or rigging
    question through named validation artifacts,
  - measured-trial comparison exists as a first-class scenario path.

### Milestone 7 — Real Offline Truth-Model Loop
- Purpose: make the export, offline-study, surrogate-fit, and re-import loop
  concrete for hydro calibration work.
- Why it matters: calibrated reduced blade and hull providers need a durable
  lineage from runtime inputs to offline study artifacts and back again.
- Primary owners: `A-009`.
- Supporting owners: `A-001`, `A-007`, `A-004`.
- Likely artifacts touched: export schema, artifact lineage metadata,
  provider-facing fit artifacts, validation fixtures, workflow docs.
- Expected deliverables:
  - stable identifiers across export and re-import artifacts,
  - provider-facing reduced-fit artifact contracts for hydro-side consumers,
  - documented offline study workflow for blade and hull calibration loops.
- Validation and evidence expectations:
  - regression tests for identifier, unit, and frame matching,
  - at least one end-to-end offline lineage fixture,
  - output reporting that shows provenance from runtime to fitted artifact.
- Requirement packet: `R-043`, `R-044`, `R-048`.
- Non-goals:
  - running external truth-model tooling from the headless runtime,
  - replacing the reduced runtime with a truth-model runtime.
- Exit criteria:
  - the offline loop is explicit enough that later calibration packets are
    bounded engineering work rather than process invention,
  - hydro-side calibrated artifacts can be re-imported deterministically.

### Milestone 8 — Uncertainty and Trust Reporting
- Purpose: expose uncertainty, fit quality, validity-range status, and
  confidence annotations in outputs and reports.
- Why it matters: research-facing study results need more than deterministic
  numbers; they need clear statements of trust and limits.
- Primary owners: `A-007`, `A-009`.
- Supporting owners: `A-008`, `A-001`.
- Likely artifacts touched: output schemas, report generation, scenario
  summaries, provider metadata catalogs, comparison tooling.
- Expected deliverables:
  - machine-readable validity and confidence fields,
  - report formatting for fit quality and out-of-envelope warnings,
  - scenario-level annotations that state whether a conclusion is inside the
    documented trust envelope.
- Validation and evidence expectations:
  - output-contract tests for degraded-trust and out-of-validity cases,
  - at least one validation artifact that intentionally triggers low-confidence
    reporting,
  - report examples that show confidence and validity annotations.
- Requirement packet: `R-035`, `R-049`.
- Non-goals:
  - probabilistic digital-twin ambitions beyond the reduced-model scope,
  - new mandatory external dependencies.
- Exit criteria:
  - reports can distinguish trusted from exploratory conclusions,
  - model validity or fit quality is surfaced rather than implied.

## Explicit Deferrals

The next fidelity phase still defers:
- full OpenSim or musculoskeletal runtime modeling,
- flexible oars as a required study path,
- wave or disturbance expansion beyond tightly scoped future placeholders,
- multi-rower or sweep support,
- GUI-first workflows.

## Recommended First Implementation Packet

The first concrete packet should combine Milestone 2 with the first slice of
Milestone 3.

Why this packet first:
- richer blade or hull calibration is blocked without measured-data contracts,
- stroke-dynamics studies need more than prescribed replay before blade-model
  calibration can answer technique questions,
- it extends `A-001`, `A-009`, `A-006`, and `A-003` without reopening the
  closed reduced baseline providers prematurely.

Recommended near-term scope:
- land the versioned boat or rigging or athlete parameter-set contract,
- land the versioned measured-trial time-series import contract,
- add one non-kinematic actuation mode with deterministic output reporting,
- add the minimum output channels needed for later measured-trial comparison,
- keep the current reduced blade and hull providers intact as the baseline
  fallback path.

Recommended evidence target:
- schema-validation `UT-*` coverage for new dataset contracts,
- one `IT-*` path from imported measurement bundle to normalized runtime use,
- one `QT-*` scenario comparing prescribed replay against the first low-order
  actuation mode,
- updated traceability and report wording that keep the baseline, calibrated
  runtime, and offline truth-model loop separate.
