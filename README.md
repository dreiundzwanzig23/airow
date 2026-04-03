# AIRow Repository

AIRow is an open-source C++ rowing simulator project focused on a physically
serious single-scull simulator with strict TDD, traceability
(`R -> A -> D -> UT/IT/QT`), and deterministic local gates.

The repository now includes the first two simulator-facing runtime slices:
deterministic JSON configuration loading plus a shared in-memory and CLI
single-run orchestration path for the baseline single-scull runtime. The
broader rowing simulator direction remains defined in the requirements,
architecture, technology stack, and decision records, including explicit
state-convention and numerical-integration ownership.

The current run path is orchestration-only. It validates configuration, runs a
bounded deterministic loop, and exposes structured metadata and diagnostics,
but it does not yet include real mechanics-backed rowing physics.

## Quick Start

Install dependencies:
```bash
./scripts/setup.sh
```

Build:
```bash
./scripts/build.sh
```

Run one headless baseline case:
```bash
./build/project_app --config /path/to/config.json
```

Current implemented library surface:
- `include/project/configuration/simulator_config.hpp`
- `include/project/orchestrator/simulation_run.hpp`
- `include/project/orchestrator/cli.hpp`
- JSON file or in-memory loading for `config_id`,
  `simulation.duration_s`, `simulation.time_step_s`, and `hull.mass_kg`
- deterministic diagnostics and normalized configuration metadata for `R-001`
- reusable in-memory single-run API with injected hydro and aero stubs
- headless CLI wrapper with stable exit-code behavior for `R-002` and `R-003`
- current run results are infrastructure-level placeholders until mechanics,
  hydro, aero, and startup-validity subsystems land

## Project Direction

Primary planning and process sources:
- `docs/process/REQUIREMENTS.md`
- `docs/process/ARCHITECTURE.md`
- `docs/process/ARCHITECTURE_POLICY.md`
- `docs/process/TECHNOLOGY_STACK.md`
- `docs/ai/DECISIONS.md`

Current intent:
- single-scull simulator first,
- headless executable plus reusable library API,
- 3D mechanics core with reduced hydro and aero runtime models,
- explicit world-frame, sign, and orientation conventions at simulator
  boundaries,
- explicit numerical integration and startup-validity ownership separate from
  mechanics ownership,
- optional high-fidelity calibration workflows kept outside the default runtime,
- real simulator code should now land inside the hardened architecture-first
  workflow rather than extending the bootstrap sample.

Current implemented slice:
- `A-001 Configuration and Validation` is now in progress with a real public
  contract for deterministic JSON loading and validation,
- `A-002 Simulation Orchestrator` is now in progress with a shared single-run
  path for CLI and in-memory execution,
- bootstrap-only placeholder code has been removed from the compiled targets,
- real mechanics, reduced runtime physics, and machine-readable run artifacts
  remain future work after the first headless orchestration baseline.

## Validation Lanes

Fast local TDD loop:
```bash
./scripts/test_tdd.sh
```

Standard full local testing:
```bash
./scripts/test.sh
```

Dedicated sanitized test lane:
```bash
./scripts/test_sanitized.sh
```

Dedicated GCC portability lane:
```bash
./scripts/test_gcc.sh
```

Auxiliary script/tool contracts:
```bash
./scripts/test_aux.sh
```

Dedicated test-quality lint lane:
```bash
./scripts/lint_tests.sh
```

Aggregate pre-merge validation:
```bash
./scripts/verify.sh
```

Required local completion gates:
```bash
./scripts/format.sh
./scripts/lint.sh
./scripts/build.sh
./scripts/test.sh
./scripts/depcheck.sh
python3 tools/tracecheck.py --write
```

## Process Model

Core artifacts:
- `AGENTS.md`
- `docs/process/REQUIREMENTS.md`
- `docs/process/ARCHITECTURE.md`
- `docs/process/TECHNOLOGY_STACK.md`
- `docs/process/ARCHITECTURE_POLICY.md`
- `docs/process/WORKFLOW.md`
- `docs/process/TEST_STRATEGY.md`
- `docs/process/TRACEABILITY.md`
- `docs/process/MAINTENANCE.md`
- `docs/process/LLM_DRIFT_REVIEW.md`
- `docs/process/ARCHITECTURE_HEALTH.md`
- `docs/process/MODEL_FIDELITY.md`
- `docs/process/STATE_CONVENTIONS.md`
- `docs/process/NUMERICS_POLICY.md`
- `docs/process/CALIBRATION_PROVENANCE.md`
- `docs/ai/SESSION_CONTEXT.md`
- `docs/ai/HANDOFF.md`
- `docs/ai/ROADMAP.md`
- `docs/ai/DECISIONS.md`

Repo-local skills:
- `.agents/skills/README.md`
- `.agents/skills/tdd-loop/SKILL.md`
- `.agents/skills/trace-maintenance/SKILL.md`
- `.agents/skills/test-lanes/SKILL.md`
- `.agents/skills/release-doc-sync/SKILL.md`
- `.agents/skills/major-change-loop/SKILL.md`

Traceability note: non-aux `UT/IT/QT` Doxygen `@test` blocks must verify one
or more same-layer IDs (`UT->D`, `IT->A`, `QT->R`). Use optional `@aux yes`
for informational tests that should be excluded from evidence gates.

Technology note: `docs/process/TECHNOLOGY_STACK.md` is the source of truth for
approved core libraries and formats. `docs/ai/DECISIONS.md` records the
rationale and durable technical constraints behind those choices.

## Validation Output

Validation scripts default to compact summaries. Use `VALIDATION_OUTPUT=verbose`
for live command output.

Artifacts:
- logs: `build/logs/validation/`
- JSON summaries: `build/logs/validation/*.json`

Useful environment variables:
- `TEST_BUILD_DIR`
- `LINT_BUILD_DIR`
- `VALIDATION_OUTPUT`
- `VALIDATION_LOG_DIR`
- `VALIDATION_SUMMARY_DIR`
- `VALIDATION_SUMMARY_PATH`

## AI Context and Archive

Active AI docs are intentionally compact. Historical ADRs are archived under
`docs/ai/archive/`.

Maintenance commands:
```bash
./scripts/maintenance.sh
./scripts/project_stats.py --format agent
```

## Gate Highlights

- `./scripts/lint.sh`: strict `clang-tidy` and `lizard` gates over the current
  `src/` and `include/` translation-unit tree, including newly added source
  files, with
  aligned naming and function-size thresholds plus stronger guidance checks for
  const-correctness, braces, magic numbers, declaration isolation,
  LLVM-native include-cleaner coverage, and related agent-facing code quality
  issues.
- `./scripts/test.sh`: full validation now includes auxiliary tooling
  contracts, a dedicated sanitized runtime lane, a dedicated GCC portability
  lane, and unit coverage over `src/lib/**` with stricter 90% region and 80%
  branch gates.
- `./scripts/lint_tests.sh`: separate test-quality linting with banned-pattern
  checks for implementation-coupled or nondeterministic tests plus tighter
  test-only structural thresholds.
- `./scripts/test_aux.sh`: auxiliary coverage now includes the dedicated
  test-quality lint lane, tooling contracts, and public-header self-containment
  compilation.
- `./scripts/test_tdd.sh` and `./scripts/test.sh`: coverage enforcement on
  `src/lib/**`.
- `./scripts/depcheck.sh`: dependency rules, ADR archival, and instruction
  coherence checks, including public-header-only cross-component access,
  realized component cycle detection, and component-orphan guardrails tied to
  architecture ownership and non-aux test coverage.
- `python3 tools/tracecheck.py --write`: strict traceability and evidence checks.
