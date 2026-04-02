# CODEX_PROCESS_OPTIMIZATION_EXECUTION_PLAN.md

## Purpose

This is the execution-ready plan for hardening the repository around stable
architecture ownership.

The central shift is:

> move from `requirement -> mapping -> tests -> code`
> to `requirement -> architecture allocation -> architecture delta -> tests -> design -> code`

The repo is already beyond the original template-replacement stage. This plan
therefore starts from the current rowing-simulator bootstrap state and focuses
on making the architecture the main project anchor.

## Current baseline

The repo already has:
- rowing-simulator requirements,
- explicit technology-stack and decision documents,
- strict TDD, traceability, and gate policy,
- a bootstrap placeholder sample still living in the main trace namespace,
- no real simulator architecture inventory yet,
- no major-change playbook yet,
- no architecture-health or simulator-specific policy docs yet.

The bootstrap sample must be isolated first so the real simulator architecture
can occupy `A-001..A-009` cleanly.

## Outcomes

After this plan:
- architecture allocation is mandatory before TDD,
- `A-*` items encode subsystem responsibility and boundaries,
- bootstrap sample artifacts are isolated into reserved `*-900` IDs,
- simulator architecture themes exist as the primary ownership surface,
- major-change work has an explicit playbook,
- trace tooling warns on fragmentation with deterministic heuristics,
- dependency rules support component-level seams,
- architecture health, fidelity, numerics, and calibration provenance have
  stable homes in process docs.

## Execution packets

Implement in this order.

### Packet 1 — Rebaseline and bootstrap trace decoupling

#### Goal
Move the placeholder sample out of the real simulator namespace.

#### Required changes
- Rewrite this plan file to reflect the current repo state.
- Add a bootstrap placeholder requirement:
  - `R-900 Bootstrap Placeholder Utility`
- Migrate the sample trace IDs:
  - `A-001 -> A-900`
  - `D-001 -> D-900`
  - `UT-001 -> UT-900`
  - `IT-001 -> IT-900`
  - `QT-001 -> QT-900`
- Point the sample system test to `R-900`, not `R-001`.
- Reserve `*-900` IDs for temporary bootstrap or sample artifacts.
- Exclude `*-900` IDs from architecture-health heuristics and scorecard metrics.

#### Acceptance criteria
- The sample utility no longer appears to satisfy simulator requirement
  `R-001`.
- `A-001..A-009` are free for real simulator architecture entries.
- Traceability stays green after the migration.

### Packet 2 — Workflow hardening

#### Goal
Require architecture allocation before tests or code.

#### Required changes
- Update:
  - `AGENTS.md`
  - `docs/process/WORKFLOW.md`
  - `docs/process/AGENTS.md`
  - `.agents/skills/tdd-loop/SKILL.md`
- Replace weak “confirm/update architecture mapping” wording with:
  - identify candidate owning `A-*` items,
  - evaluate reuse before creating a new `A-*`,
  - record the architecture delta before tests,
  - justify any new `A-*` with cohesion and reuse intent.
- State explicitly:
  - do not create 1:1 `R -> A` mappings unless justified,
  - prefer extending a stable subsystem over a narrow new one.
- Add a new major-change skill:
  - `.agents/skills/major-change-loop/SKILL.md`
- Update `.agents/skills/README.md`, `AGENTS.md`, `README.md`, and
  `docs/process/WORKFLOW.md` to reference the new skill.

#### Major-change skill requirements
The playbook must require:
1. change classification,
2. impact map across `R/A/D/tests/files`,
3. architecture delta before implementation,
4. characterization tests for preserved behavior,
5. seam-first migration when appropriate,
6. explicit removal of transitional code,
7. drift review after the change.

#### Acceptance criteria
- Core workflow docs and skills require architecture allocation before TDD.
- Large cross-cutting changes have a distinct documented playbook.

### Packet 3 — Architecture schema and simulator seed

#### Goal
Make `A-*` items the stable ownership surface for the simulator.

#### Required changes
- Extend `docs/process/ARCHITECTURE.md` so `Evidence Profile: CODE` requires:
  - `Responsibility`
  - `Owned Concepts`
  - `Inputs`
  - `Outputs`
  - `Depends On`
  - `Must Not Depend On`
  - `Invariants`
  - `Allocation Rationale`
  - `Future Absorption`
  - `Interfaces`
- Add `docs/process/ARCHITECTURE_POLICY.md` as the canonical architecture
  allocation policy.
- Seed these real simulator architecture items:
  - `A-001 Configuration and Validation`
  - `A-002 Simulation Orchestrator`
  - `A-003 Mechanics Subsystem`
  - `A-004 Hydro Runtime Models`
  - `A-005 Aero Runtime Models`
  - `A-006 Control and Stroke Input`
  - `A-007 Output and Diagnostics`
  - `A-008 Scenario Harness and Validation`
  - `A-009 External Calibration Integration`
- Keep `A-900` as the bootstrap placeholder component.
- Allocate the existing rowing requirements across those `A-*` items so they
  have obvious homes before implementation starts.

#### Acceptance criteria
- `ARCHITECTURE.md` contains the real simulator subsystem inventory.
- Every seeded simulator `A-*` item includes full boundary fields.
- `ARCHITECTURE_POLICY.md` is referenced from core process docs.

### Packet 4 — Traceability and scorecard hardening

#### Goal
Detect fragmentation deterministically without inventing subjective gates.

#### Required `tracecheck.py` changes
- Hard-fail if a `CODE` `A-*` item is missing required boundary fields.
- Hard-fail if a `DONE` `CODE` `A-*` item has no `Invariants`.
- Hard-fail if a `DONE` `CODE` `A-*` item has no `Interfaces`.
- Hard-fail if a non-bootstrap `CODE` `A-*` with `Satisfies` populated has no
  `Allocation Rationale`.
- Keep architecture-smell heuristics warning-only.

#### Required warning rules
- Warn if a non-bootstrap `A-*` title normalizes exactly to any satisfied
  requirement title.
- Warn if at least five non-bootstrap mapped requirements exist and more than
  80% of them map to exactly one `A-*`.
- Warn if a non-bootstrap `CODE` `A-*` has:
  - exactly one satisfied `R-*`,
  - exactly one `D-*`,
  - exactly one `IT-*`,
  - empty `Future Absorption`.
- Warn if two non-bootstrap `A-*` items have identical normalized titles.

#### Required trace output changes
- Keep the current summary table.
- Add an architecture detail section containing the richer architecture fields.
- Add an “Architecture Warnings” section.
- Exclude `*-900` IDs from numbering warnings and architecture-health metrics.

#### Required scorecard changes
- Update `docs/process/QUALITY_SCORECARD.md` to track:
  - counts of `R/A/D/UT/IT/QT`,
  - one-to-one `R -> A` ratio excluding bootstrap IDs,
  - average requirements per non-bootstrap `A-*`,
  - count of non-bootstrap `A-*` with only one `D-*`,
  - count of non-bootstrap `A-*` with empty `Future Absorption`.

#### Acceptance criteria
- `tracecheck.py` still validates baseline trace integrity.
- Trace output includes architecture ownership detail and warning reporting.
- The scorecard now reflects architecture/process health instead of the sample
  utility.

### Packet 5 — Component dependency controls

#### Goal
Add component-level boundary enforcement without requiring an immediate code
layout rewrite.

#### Required changes
- Extend `docs/process/DEPENDENCY_RULES.md` with a second rule section for
  logical component dependencies.
- Extend `tools/depcheck.py` to classify files by include/source prefix under:
  - `project/core`
  - `project/configuration`
  - `project/orchestrator`
  - `project/mechanics`
  - `project/hydro`
  - `project/aero`
  - `project/control`
  - `project/output`
  - `project/calibration`
- Enforce component rules only for files that actually live under those
  prefixes.

#### Initial allowed component dependencies
- `core -> core`
- `configuration -> core`
- `mechanics -> core`
- `hydro -> core, mechanics`
- `aero -> core, mechanics`
- `control -> core, mechanics`
- `output -> core, configuration, mechanics, hydro, aero, control, orchestrator`
- `orchestrator -> core, configuration, mechanics, hydro, aero, control, output`
- `calibration -> core, mechanics, hydro, aero, output`

#### Explicitly forbidden
- `hydro -> aero`
- `aero -> hydro`
- subsystem internals depending on `orchestrator`

#### Acceptance criteria
- `depcheck.py` supports both directory-level and component-level rules.
- Current code stays green even though no component-prefixed files exist yet.

### Packet 6 — Architecture health and simulator-specific policy

#### Goal
Create durable homes for long-lived structural and scientific-process guidance.

#### Required files to add
- `docs/process/ARCHITECTURE_HEALTH.md`
- `docs/process/MODEL_FIDELITY.md`
- `docs/process/NUMERICS_POLICY.md`
- `docs/process/CALIBRATION_PROVENANCE.md`

#### Required file updates
- `docs/process/LLM_DRIFT_REVIEW.md`
- `docs/process/MAINTENANCE.md`
- `README.md`
- `docs/ai/SESSION_CONTEXT.md`
- `docs/ai/HANDOFF.md`
- `docs/ai/ROADMAP.md`

#### Required content
- `ARCHITECTURE_HEALTH.md`
  - overlapping responsibilities,
  - fragile temporary seams,
  - tolerated duplication,
  - next-change pressure points,
  - no-go growth areas.
- `MODEL_FIDELITY.md`
  - runtime reduced models vs offline truth models,
  - default runtime independence from optional high-fidelity toolchains,
  - validity-range and calibration-source expectations.
- `NUMERICS_POLICY.md`
  - SI-only policy,
  - non-finite detection,
  - tolerance policy,
  - deterministic comparison guidance,
  - stable local regression expectations.
- `CALIBRATION_PROVENANCE.md`
  - artifact identity,
  - version/hash expectations,
  - storage expectations,
  - required provenance metadata.

#### Acceptance criteria
- The repo now has stable docs for architecture health, fidelity, numerics, and
  calibration provenance.
- These docs are referenced by the active workflow and maintenance docs.

### Packet 7 — Requirement allocation readiness and test strategy completion

#### Goal
Finish the process upgrade by making the existing requirements and test model
fit the seeded architecture.

#### Required changes
- Review `docs/process/REQUIREMENTS.md` for allocation readiness rather than
  wholesale replacement.
- Edit only where wording still encourages direct `R -> A` mirroring or
  conflicts with the seeded subsystem ownership.
- Update `docs/process/TEST_STRATEGY.md` and
  `.agents/skills/test-lanes/SKILL.md` so they explicitly cover:
  - scenario-oriented `QT-*`,
  - characterization tests for major changes,
  - subsystem-contract focused `IT-*`,
  - named baseline scenarios:
    - passive float,
    - tow test,
    - calm-water stroke,
    - headwind stroke,
    - crosswind stroke.

#### Acceptance criteria
- The requirements read as allocation-ready against the seeded `A-*` items.
- The test strategy supports both scenario validation and major-change
  characterization work.

## Guardrail updates required across packets

When adding new core process docs or repo-local skills, update:
- `tools/check_instruction_coherence.py`
- `README.md`
- `AGENTS.md`
- `docs/process/WORKFLOW.md`

When changing traceability schemas or metadata expectations, update:
- `tools/tracecheck.py`
- generated `docs/process/TRACEABILITY.md`
- relevant examples and sample test annotations

## Definition of done

This process optimization effort is complete when:
- architecture allocation is mandatory before TDD in both policy docs and
  skills,
- simulator subsystem ownership is seeded in `A-001..A-009`,
- bootstrap sample artifacts live in reserved `*-900` IDs,
- architecture policy and major-change workflow docs exist,
- tracecheck hard-fails on missing architecture boundary fields and emits
  deterministic fragmentation warnings,
- depcheck supports component-level rules,
- architecture health, fidelity, numerics, and calibration provenance docs
  exist,
- test strategy explicitly supports scenarios and characterization tests,
- all required local gates pass.

## Final note to Codex

Do not try to compensate for weak architecture by adding more tests alone.
Stable subsystem ownership and reviewable architecture deltas come first; TDD
and implementation should grow inside those boundaries.
