# TEST_STRATEGY.md — Rowing Simulator Test Strategy

## Purpose
Define verification-layer responsibilities and quality intent. Detailed lane
execution playbooks are in skills.

## Test Layers
### Unit (`tests/unit/`)
- Verifies design-level behavior (`UT-* -> D-*`).
- Focus: local logic, invariants, deterministic helpers.

### Integration (`tests/integration/`)
- Verifies architecture-level interactions (`IT-* -> A-*`).
- Focus: subsystem contracts, boundary behavior, provider wiring, and contract
  interoperability.
- Preferred for characterization coverage when a major change preserves a
  subsystem seam but refactors its internals.

### System (`tests/system/`)
- Verifies requirement-level acceptance (`QT-* -> R-*`).
- Focus: end-to-end deterministic behavior and named scenario-level acceptance.
- This is the main evidence lane for passive float, tow, calm-water stroke,
  headwind stroke, and crosswind stroke baselines once runtime scenarios exist.

## Core Design Rules
- Keep tests deterministic and runtime-bounded.
- Use explicit tolerances when needed.
- Keep `@test` IDs unique and `@verifies` same-layer.
- One-or-more same-layer references per test block are allowed.
- Keep unit tests behavior-focused rather than integration-heavy.
- Add characterization coverage before invasive refactors or major-change work.
- Choose the narrowest characterization lane that protects the preserved
  behavior:
  - `UT-*` for local logic,
  - `IT-*` for subsystem seams,
  - `QT-*` for named end-to-end scenarios.
- Protect named baseline scenarios when changing runtime behavior.

## Scenario Baselines
Named requirement-level baselines for this project should center on:
- passive float for hydrostatic initialization and calm-water equilibrium,
- tow test for reduced hull-water resistance behavior,
- calm-water stroke for self-propelled baseline propulsion,
- headwind stroke for apparent-wind and aero-load interaction,
- crosswind stroke for asymmetric environmental loading behavior.

These scenarios should become the default `QT-*` acceptance surface once the
runtime implementation exists.

## Auxiliary Test Overlay
- Use optional `@aux yes` for non-evidence script/tool contract tests.
- `@aux` tests are excluded from evidence gates (`D->UT`, `A->IT`, `R->QT`) and
  are listed separately in generated traceability output.
- Non-aux tests keep strict lane mapping and required same-layer `@verifies`
  references.
- Execute auxiliary tests via `./scripts/test_aux.sh`.

## Required Gates
- `./scripts/test.sh` is required before completion.
- `./scripts/test_tdd.sh` is for fast local iteration only.
- Coverage gates on `src/lib/**` are enforced by test scripts.

## Optional/Specialized Lanes
- Auxiliary script/tool contracts: `./scripts/test_aux.sh`
- Regression lane: `./scripts/regression.sh`
- Aggregate verification: `./scripts/verify.sh`

## Operational Playbook
Use `.agents/skills/test-lanes/SKILL.md` for lane-selection and execution
procedures.
