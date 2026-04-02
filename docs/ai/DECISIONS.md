# DECISIONS.md

Historical archives: `docs/ai/archive/` (`DECISIONS_pre_YYYY-MM-DD.md` when
active-window rollover is needed).

Archived ADR index: `docs/ai/archive/DECISIONS_INDEX.md`.

## ADR-2026-03-23-001
- **Date**: 2026-03-23
- **Context**: The original template process model had drifted behind the
  evolved repository workflow and no longer captured the stricter generic
  gates, AI-doc hygiene, or lightweight requirement-drift handling.
- **Decision**:
  - adopt a stronger policy-core `AGENTS.md`,
  - require requirement drift metadata and aux-test aware traceability,
  - add repo-local skills, validation summaries, and AI decision archival.
- **Consequences**:
  - the template is stricter on process drift by default,
  - active AI context stays smaller and easier to resume,
  - project-specific runtime or backend gates remain intentionally out of
    scope for the template.
## ADR-2026-04-02-001
- **Date**: 2026-04-02
- **Context**: The rowing simulator is a multiphysics project with evolving
  subsystem boundaries. A requirement-driven agent can otherwise drift into a
  fragile 1:1 `R -> A` mapping and patch-oriented growth.
- **Decision**:
  - treat architecture allocation as a mandatory step before TDD,
  - require `A-*` items to represent cohesive subsystem responsibilities rather
    than requirement mirrors,
  - require new `A-*` items to carry explicit allocation rationale and future
    absorption,
  - distinguish ordinary feature work from major-change work in the workflow.
- **Consequences**:
  - architecture synthesis becomes an explicit deliverable,
  - Codex work packets should target architectural increments, not raw
    requirement mirroring,
  - trace tooling and workflow docs need follow-up updates to enforce this.
## ADR-2026-04-02-002
- **Date**: 2026-04-02
- **Context**: The simulator needs stable top-level ownership boundaries early
  so that mechanics, loads, calibration, and validation do not accrete in one
  undifferentiated code path.
- **Decision**:
  - seed the project with the following primary simulator component themes:
    `Configuration and Validation`, `Simulation Orchestrator`, `Mechanics`,
    `Hydro Runtime Models`, `Aero Runtime Models`, `Control and Stroke Input`,
    `Output and Diagnostics`, `Scenario Harness and Validation`, and
    `External Calibration Integration`,
  - prefer adding `D-*` realizations inside these themes over creating new
    top-level `A-*` items.
- **Consequences**:
  - new requirements have an obvious architectural home,
  - dependency rules and tests can be tightened around known subsystem seams,
  - major changes are more likely to evolve existing components than create
    narrow feature containers.
## ADR-2026-04-02-003
- **Date**: 2026-04-02
- **Context**: The rowing simulator may use different numerical techniques over
  time, and concrete solver choices are engineering decisions rather than
  product requirements unless externally mandated.
- **Decision**:
  - capture solver and time-integration needs in requirements through
    observable behavior and constraints,
  - model integration as its own architectural responsibility behind a stable
    contract,
  - record concrete numerical-method choices in decision records and design
    items rather than embedding them directly in `R-*` items.
- **Consequences**:
  - requirements remain technology-neutral and testable,
  - integrator or backend changes can be evaluated and replaced with less
    requirement churn,
  - numerical-method changes should update ADRs before large implementation
    work proceeds.
## ADR-2026-04-02-004
- **Date**: 2026-04-02
- **Context**: Full high-fidelity fluid simulation of water and air is too
  costly and too brittle to make the default runtime path for an agent-driven
  simulator project.
- **Decision**:
  - center the project on one 3D mechanics runtime simulation,
  - treat hydro and aero primarily as load-provider subsystems around that
    mechanics core,
  - keep high-fidelity SPH/CFD or other truth-model workflows optional and
    offline for calibration, comparison, or surrogate generation.
- **Consequences**:
  - the runtime loop stays executable without heavyweight external solvers,
  - calibration workflows can mature independently of the main simulator,
  - requirements and architecture should preserve a strict runtime vs
    truth-model separation.
## ADR-2026-04-02-005
- **Date**: 2026-04-02
- **Context**: The earliest useful simulator should be small enough to validate
  and evolve, while still exercising the main architectural seams of the
  project.
- **Decision**:
  - scope the initial simulator target to a headless single-scull baseline,
  - start with full 3D rigid-body mechanics for hull and oars, a prescribed
    stroke input, reduced hydro and aero runtime models, deterministic outputs,
    and named validation scenarios,
  - defer full musculoskeletal actuation, online high-fidelity CFD, and richer
    flexible-body effects to later stages.
- **Consequences**:
  - the first implementation target remains ambitious but buildable,
  - validation scenarios such as passive float, tow test, calm-water stroke,
    headwind stroke, and crosswind stroke become first-class progress markers,
  - later fidelity increases should be absorbed through existing component
    seams rather than replacing the initial workflow model.
## ADR-2026-04-02-006
- **Date**: 2026-04-02
- **Context**: The project will mix analytic models, calibrated models, and
  optional external truth data. Without explicit provenance and scenario-based
  verification, physics changes can become difficult to trust.
- **Decision**:
  - make scenario-based validation a first-class quality lane,
  - require machine-readable outputs and deterministic replay envelopes for the
    baseline scenarios,
  - treat calibration datasets and derived model artifacts as versioned inputs
    with explicit provenance rather than incidental files.
- **Consequences**:
  - quality evidence stays tied to named physical scenarios instead of only
    low-level tests,
  - calibrated models can be swapped or updated with traceable provenance,
  - future process docs should add numerics, fidelity, and provenance policies
    tailored to the simulator domain.
## ADR-2026-04-02-007
- **Date**: 2026-04-02
- **Context**: The project needed one explicit place where the approved core
  libraries and frameworks for the simulator are named, instead of leaving
  those choices implicit in chat history, build files, or architecture notes.
- **Decision**:
  - define the open-source best-bet technology stack as:
    - core language: `C++`,
    - mechanics backend: `Project Chrono`,
    - preferred constrained integrator family: `SUNDIALS IDA/IDAS`,
    - first blade/water truth-model path: `DualSPHysics`,
    - secondary CFD / atmospheric path: `OpenFOAM`,
    - optional future biomechanics stack: `OpenSim`,
    - data / visualization / geometry stack: `HDF5`, `ParaView`, and `Gmsh`,
  - record these choices in a dedicated `docs/process/TECHNOLOGY_STACK.md`
    file and keep `DECISIONS.md` as the rationale-bearing companion.
- **Consequences**:
  - the project now has an explicit source of truth for approved core
    technologies,
  - future library changes must update both the decision record and the stack
    document,
  - requirements remain technology-neutral unless an external mandate requires
    otherwise.
