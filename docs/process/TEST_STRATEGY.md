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
- Focus: component coupling and contract interoperability.

### System (`tests/system/`)
- Verifies requirement-level acceptance (`QT-* -> R-*`).
- Focus: end-to-end deterministic behavior and scenario-level acceptance.

## Core Design Rules
- Keep tests deterministic and runtime-bounded.
- Use explicit tolerances when needed.
- Keep `@test` IDs unique and `@verifies` same-layer.
- One-or-more same-layer references per test block are allowed.
- Keep unit tests behavior-focused rather than integration-heavy.

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
Use `skills/template-test-lanes/SKILL.md` for lane-selection and execution
procedures.
