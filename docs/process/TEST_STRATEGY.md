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
- Keep unit test files intentionally small by default:
  - max `900` lines per file,
  - max `14` `TEST*` cases per file,
  - use per-file overrides only with explicit rationale.
- Keep tests independent of implementation backdoors such as private-public
  access hacks, `FRIEND_TEST`, implementation-file includes, wall-clock reads,
  and sleep-based timing.
- Add characterization coverage before invasive refactors or major-change work.
- Choose the narrowest characterization lane that protects the preserved
  behavior:
  - `UT-*` for local logic,
  - `IT-*` for subsystem seams,
  - `QT-*` for named end-to-end scenarios.
- Protect named baseline scenarios when changing runtime behavior.
- Treat functional development as explicit red-green-refactor loops:
  - red: targeted failing test evidence exists,
  - green: targeted failure resolved with minimal behavior change,
  - refactor: mandatory behavior-preserving cleanup pass or explicit no-op
    rationale.
- Preserve loop evidence with `rgr:red`, `rgr:green`, and `rgr:refactor`
  markers in commit messages or evidence notes.

## Scenario Baselines
Named requirement-level baselines for this project should center on:
- passive float for hydrostatic initialization and calm-water equilibrium,
- tow test for reduced hull-water resistance behavior,
- calm-water stroke for self-propelled baseline propulsion,
- headwind stroke for apparent-wind and aero-load interaction,
- crosswind stroke for asymmetric environmental loading behavior.

These scenarios should become the default `QT-*` acceptance surface once the
runtime implementation exists.

Current checked-in scenario artifacts and acceptance envelopes are listed in
`docs/process/SCENARIOS.md`.

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
- `./scripts/test_performance.sh` is the dedicated protected-scenario
  performance-budget lane and reports its status separately from ordinary
  functional failures.
- `./scripts/check_rgr_evidence.sh` runs as a strict check in
  `./scripts/test_tdd.sh` and `./scripts/verify.sh`; `warn` or `off` are
  explicit local overrides only.
- `./scripts/test.sh` includes the repo-wide auxiliary tooling-contract lane,
  the protected-scenario performance lane, repo-wide test-quality linting with
  the default structural limits, a
  dedicated sanitized build or run lane, a dedicated GCC portability lane,
  and coverage gates on `src/lib/**` at 90% region coverage and 80% branch
  coverage plus changed-file coverage regression ratchets.
- `./scripts/test_tdd.sh` intentionally excludes the protected-scenario
  performance-budget checks so the quick loop stays fast.
- `./scripts/verify.sh` runs `./scripts/test_performance.sh` as its own logged
  step before the broader `./scripts/test.sh` aggregate gate.

## Optional/Specialized Lanes
- Auxiliary script/tool contracts and header self-containment:
  `./scripts/test_aux.sh`
- Dedicated protected-scenario performance-budget lane:
  `./scripts/test_performance.sh`
- Dedicated test-quality lint lane: `./scripts/lint_tests.sh`
- Dedicated sanitized runtime lane: `./scripts/test_sanitized.sh`
- Dedicated GCC portability lane: `./scripts/test_gcc.sh`
- Regression lane: `./scripts/regression.sh`
- Aggregate verification: `./scripts/verify.sh`

## Operational Playbook
Use `.agents/skills/test-lanes/SKILL.md` for lane-selection and execution
procedures.
Use `.agents/skills/unit-test-design/SKILL.md` when designing or tightening
`UT-*` behavior coverage.
