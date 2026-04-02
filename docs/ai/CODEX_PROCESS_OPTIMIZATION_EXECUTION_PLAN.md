# CODEX_PROCESS_OPTIMIZATION_EXECUTION_PLAN.md

## Purpose

This plan upgrades the repository from a strong generic Codex template into a
better fit for a long-lived, architecture-sensitive, multiphysics simulator
project.

The central change is:

> move from `requirement -> mapping -> tests -> code`
> to `requirement -> architecture allocation -> architecture delta -> tests -> design -> code`

The plan is written to be executed incrementally by Codex inside this repo.
Each phase is intentionally small enough to be reviewed and merged on its own.

## Outcomes

After completing this plan, the repo should:

- force architecture allocation before TDD for functional work,
- reduce accidental 1:1 `R -> A` mirroring,
- distinguish ordinary feature work from major cross-cutting changes,
- enforce richer architecture evidence and boundary descriptions,
- protect against architecture drift during large updates,
- better support the rowing simulator as a multiphysics, multi-fidelity codebase.

## Operating principles

- Keep changes incremental and reviewable.
- Prefer updating existing process artifacts over adding many new ones.
- Keep requirements externally observable; keep architecture responsibility-based.
- Put most implementation granularity in `D-*`, not `A-*`.
- Introduce new gates only when they are deterministic and locally runnable.
- Avoid domain-specific runtime code in this phase unless needed for process bootstrapping.

## Execution strategy

Complete the phases in order. Do not collapse multiple phases into one large
Codex task unless the diff remains easy to review.

For each phase:
1. update process docs first,
2. add or adjust guardrail tooling,
3. add or update tests for the guardrail/tooling when practical,
4. run the full local gates,
5. update `docs/ai/*` only when milestone-level process changes occur.

---

## Phase 0 — Baseline and replacement scope

### Goal
Freeze the starting point and define which template artifacts will be upgraded
versus replaced.

### Files to inspect
- `AGENTS.md`
- `docs/process/WORKFLOW.md`
- `docs/process/ARCHITECTURE.md`
- `docs/process/REQUIREMENTS.md`
- `docs/process/TEST_STRATEGY.md`
- `docs/process/TRACEABILITY.md`
- `docs/process/DEPENDENCY_RULES.md`
- `docs/process/LLM_DRIFT_REVIEW.md`
- `skills/template-tdd-loop/SKILL.md`
- `tools/tracecheck.py`
- `tools/depcheck.py`

### Codex tasks
- Confirm the current template assumptions and note where the repo still
  rewards trace completeness without architecture synthesis.
- Record the intended process upgrade sequence in `docs/ai/HANDOFF.md`.

### Acceptance criteria
- Baseline process limitations are recorded in `docs/ai/HANDOFF.md`.
- There is a clear statement that the template sample requirement set is no
  longer the target product scope.

---

## Phase 1 — Make architecture allocation explicit in the workflow

### Goal
Force Codex to perform architecture allocation before writing tests or code.

### Files to update
- `AGENTS.md`
- `docs/process/WORKFLOW.md`
- `docs/process/AGENTS.md`
- `skills/template-tdd-loop/SKILL.md`

### Required changes
- Replace weak wording such as “confirm/update architecture mapping” with
  explicit architecture allocation language.
- Require the agent to:
  - identify candidate existing `A-*` owners,
  - scan neighboring requirements for shared concepts and interfaces,
  - reuse an existing `A-*` when coherent,
  - justify any new `A-*` with cohesion and future reuse,
  - record the architecture delta before TDD.
- State explicitly that `A-*` items represent subsystem responsibilities and
  boundaries, not requirement mirrors.

### Suggested policy text
Use wording equivalent to:
- “Perform architecture allocation before writing tests.”
- “Do not create a 1:1 `R -> A` mapping unless explicitly justified.”
- “Prefer extending a stable subsystem over introducing a new narrow one.”

### Acceptance criteria
- All primary workflow docs require architecture allocation before TDD.
- The workflow language distinguishes architecture synthesis from trace
  bookkeeping.
- The default agent loop no longer implies direct `R -> A -> code` mapping.

---

## Phase 2 — Enrich the architecture schema

### Goal
Make `A-*` items describe real subsystem boundaries.

### Files to update
- `docs/process/ARCHITECTURE.md`
- `docs/process/TRACEABILITY.md`

### Required schema changes
For `Evidence Profile: CODE`, require these fields:
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

Keep `Satisfies`, `Status`, and `Evidence Profile` as before.

### Intent
This change should make weak architecture visibly weak. If an `A-*` item cannot
state its responsibility, boundaries, and invariants, it is probably not a real
architecture item.

### Acceptance criteria
- The architecture template documents the richer schema.
- `TRACEABILITY.md` explains the role of `A-*` as stable subsystem ownership.
- Existing sample/template architecture entries are updated enough to remain
  valid under the new schema, or are clearly marked as template placeholders.

---

## Phase 3 — Seed stable architecture themes for the rowing simulator

### Goal
Prevent architecture from emerging one requirement at a time.

### Files to update
- `docs/process/ARCHITECTURE.md`
- `docs/ai/ROADMAP.md`

### Seed these `A-*` items
At minimum create initial `OPEN` items for:
- `A-001 Configuration and Validation Subsystem`
- `A-002 Simulation Orchestrator`
- `A-003 Mechanics Subsystem`
- `A-004 Hydro Runtime Models`
- `A-005 Aero Runtime Models`
- `A-006 Control and Stroke Input`
- `A-007 Output and Diagnostics`
- `A-008 Scenario Harness and Validation`
- `A-009 External Calibration Integration`

### Required content
Each seeded item must include:
- its responsibility,
- the core concepts it owns,
- what it depends on and must not depend on,
- likely future requirement absorption.

### Acceptance criteria
- The repository contains a stable first architecture surface for the rowing
  simulator.
- New simulator requirements have obvious homes before code implementation
  begins.

---

## Phase 4 — Add a dedicated architecture policy

### Goal
Make architecture allocation rules explicit and reusable.

### Files to add
- `docs/process/ARCHITECTURE_POLICY.md`

### Required content
The policy must state at least:
- `A-*` items represent cohesive subsystem responsibilities, not requirement
  containers.
- New requirements must first be evaluated against existing `A-*` ownership.
- A new `A-*` requires explicit cohesion justification.
- One `A-*` may satisfy many `R-*`; this is expected.
- One `R-*` may allocate to multiple `A-*`; this is expected.
- Most implementation granularity should go into `D-*`, not `A-*`.
- Architecture must optimize for cohesion, low coupling, stable interfaces,
  and test seams.

### Acceptance criteria
- The policy exists and is referenced from `AGENTS.md` and `WORKFLOW.md`.
- The repo has one canonical place describing architecture allocation rules.

---

## Phase 5 — Add a major-change workflow alongside the normal TDD loop

### Goal
Handle cross-cutting and architectural changes without falling into patch-to-green behavior.

### Files to add/update
- `skills/template-major-change-loop/SKILL.md`
- `skills/README.md`
- `AGENTS.md`
- `docs/process/WORKFLOW.md`

### Required behavior
Define a separate workflow for changes classified as:
- cross-cutting,
- architectural refactor,
- migration/deprecation,
- semantic multi-requirement update.

### Major-change loop must require
1. change classification,
2. impact map across `R/A/D/tests/files`,
3. architecture delta before implementation,
4. characterization tests for preserved behavior,
5. seam-first migration where appropriate,
6. explicit removal of transitional code,
7. drift review after the change.

### Acceptance criteria
- The repo distinguishes ordinary feature work from major change work.
- Large changes now have an explicit, Codex-loadable playbook.

---

## Phase 6 — Extend tracecheck with architecture-fitness guardrails

### Goal
Make tooling detect architecture fragmentation, not just missing links.

### Files to update
- `tools/tracecheck.py`
- optionally `scripts/verify.sh` if output handling changes

### Hard-fail checks to add
For `Evidence Profile: CODE`, fail if an `A-*` item is missing required boundary
fields introduced in Phase 2.

Fail if:
- a new `A-*` lacks `Allocation Rationale`,
- a `DONE` `A-*` lacks `Invariants`,
- a new `A-*` satisfies only one small requirement and has no `Future Absorption`,
- a `CODE` `A-*` has no `Interfaces` field.

### Warning-level checks to add
Warn if:
- an `A-*` title is nearly the same as an `R-*` title,
- multiple `A-*` items use overlapping vocabulary suggesting fragmentation,
- the ratio of one-to-one `R -> A` mappings grows,
- many `A-*` items have only one `D-*` and one `IT-*`.

### Acceptance criteria
- `tracecheck.py` still validates baseline trace integrity.
- It now also flags likely architecture anti-patterns.
- Warnings are readable and actionable.

---

## Phase 7 — Strengthen dependency control to component level

### Goal
Reduce ripple effects from large changes by enforcing clearer component boundaries.

### Files to update
- `docs/process/DEPENDENCY_RULES.md`
- `tools/depcheck.py`

### Required change
Keep the existing directory-level rules, but add a second layer for logical
component paths or namespaces.

### Initial component rules
Define allowed dependencies for at least these logical areas:
- core/configuration
- mechanics
- hydro
- aero
- control
- orchestrator
- outputs/diagnostics
- calibration adapters

### Examples
- hydro and aero may depend on shared core math/contracts
- hydro must not depend on aero internals
- aero must not depend on hydro internals
- orchestrator may depend on subsystem contracts but should not own subsystem
  algorithms

### Acceptance criteria
- Dependency rules go beyond broad directories.
- `depcheck.py` can flag forbidden cross-component dependencies.

---

## Phase 8 — Add process artifacts for architecture drift and hotspots

### Goal
Capture long-lived structural risk, not just requirement drift.

### Files to add
- `docs/process/ARCHITECTURE_HEALTH.md`
- or `docs/process/HOTSPOTS.md`

### Files to update
- `docs/process/LLM_DRIFT_REVIEW.md`
- `docs/process/MAINTENANCE.md`

### Required content
Track at least:
- overlapping subsystem responsibilities,
- fragile temporary seams,
- known duplication tolerated for now,
- likely next-change pressure points,
- places where agents should avoid growing complexity.

### Acceptance criteria
- There is a lightweight, durable place to record architecture health.
- Major changes can leave explicit follow-up notes without polluting active AI
  handoff docs.

---

## Phase 9 — Add simulator-specific process policy

### Goal
Adapt the generic agentic template to the rowing simulator domain.

### Files to add
- `docs/process/MODEL_FIDELITY.md`
- `docs/process/NUMERICS_POLICY.md`
- `docs/process/CALIBRATION_PROVENANCE.md`

### Required content
#### MODEL_FIDELITY.md
- distinguish runtime reduced models from offline truth-model workflows,
- require the main runtime loop to remain usable without high-fidelity
  toolchains,
- require every closure model to state validity range and calibration source.

#### NUMERICS_POLICY.md
- SI-units-only rule,
- non-finite detection requirements,
- tolerance handling policy,
- deterministic comparison guidance,
- expectation for stable local regression tests.

#### CALIBRATION_PROVENANCE.md
- how datasets, fitted parameters, and surrogate artifacts are identified,
- hash/version expectations,
- where calibration outputs live,
- what metadata must accompany imported artifacts.

### Acceptance criteria
- The repo process now acknowledges simulator-specific concerns.
- Codex has explicit guidance for fidelity, numerics, and calibration.

---

## Phase 10 — Replace template requirements with the simulator requirements set

### Goal
Review and update the requirements for a prcatical implementation of the rowing simulator

### Files to update
- `docs/process/REQUIREMENTS.md`

### Required work
- Check if the set of requiremnts make sense to form a rowing simulator using our process
- Preserve the repository requirement schema and metadata policy.
- Ensure the new requirements are grouped into meaningful, verifiable chunks.
- Ensure the backlog supports the seeded `A-*` themes rather than encouraging
  direct `R -> A` mirroring.

### Acceptance criteria
- `REQUIREMENTS.md` reflects the rowing simulator.
- The requirement set is ready for `R -> A -> D -> evidence` allocation.

---

## Phase 11 — Upgrade test strategy for scenario and characterization use

### Goal
Make the verification model fit simulator work, especially major changes.

### Files to update
- `docs/process/TEST_STRATEGY.md`
- `skills/template-test-lanes/SKILL.md`

### Required changes
- Clarify that `QT-*` for this project should be scenario-oriented.
- Require named scenario baselines such as:
  - passive float,
  - tow test,
  - calm-water stroke,
  - headwind stroke,
  - crosswind stroke.
- Add a characterization-test rule for refactor and cross-cutting changes.
- Clarify that `IT-*` must verify subsystem contracts, not just broad coupling.
- Hardening the test first (TDD) process for the new workflows

### Acceptance criteria
- The test strategy distinguishes scenario validation from unit/integration work.
- Refactor-classified work now protects old behavior before invasive changes.
- TDD is strong and preserved.

---

## Phase 12 — Add lightweight process metrics

### Goal
Make architecture drift visible before it becomes painful.

### Files to update
- `docs/process/QUALITY_SCORECARD.md`
- optionally `tools/tracecheck.py` or a small helper script if metrics are automated

### Suggested metrics
- count of `R-*`, `A-*`, `D-*`, `UT-*`, `IT-*`, `QT-*`,
- ratio of one-to-one `R -> A` mappings,
- average requirements per architecture item,
- architecture items with only one design item,
- architecture items with no future absorption,
- churn hotspots by touched files or components.

### Acceptance criteria
- The repo has at least a lightweight architecture/process health scorecard.
- Metrics help detect fragmentation trends over time.

---

## Recommended Codex execution packets

Use these as task boundaries.

### Packet 1
- Phase 1
- Phase 2
- minimal updates to cross-references

### Packet 2
- Phase 3
- Phase 4

### Packet 3
- Phase 5
- Phase 11

### Packet 4
- Phase 6
- Phase 12

### Packet 5
- Phase 7
- any `depcheck.py` tests or fixtures needed

### Packet 6
- Phase 8
- Phase 9

### Packet 7
- Phase 10

Keep packets smaller if the diff becomes hard to review.

---

## Definition of done for the process upgrade

The process optimization effort is complete when:

- architecture allocation is mandatory before TDD,
- the architecture schema encodes responsibility and boundaries,
- the repo has seeded simulator architecture themes,
- there is a major-change playbook,
- tracecheck guards against architecture fragmentation,
- dependency rules operate at component level,
- architecture drift and hotspots have a documented home,
- simulator-specific process policies exist,
- the rowing simulator requirements replace the template sample,
- test strategy explicitly supports scenarios and characterization tests,
- all updated process/tooling changes pass the standard local gates.
- TDD is harded and strong

## Final note to Codex

Do not attempt to solve all architecture quality problems through tests alone.
The process must first create stable subsystem ownership and reviewable
architecture deltas; only then should TDD and implementation proceed.
