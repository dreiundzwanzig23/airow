# AI-Guided C++ Template

A C++ template for autonomous or semi-autonomous development with strict TDD,
traceability (`R -> A -> D -> UT/IT/QT`), and deterministic local gates.

## Quick Start

Install dependencies:
```bash
./scripts/setup.sh
```

Build:
```bash
./scripts/build.sh
```

Run the sample app:
```bash
./build/project_app
```

## Validation Lanes

Fast local TDD loop:
```bash
./scripts/test_tdd.sh
```

Standard full local testing:
```bash
./scripts/test.sh
```

Auxiliary script/tool contracts:
```bash
./scripts/test_aux.sh
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
- `docs/process/WORKFLOW.md`
- `docs/process/TEST_STRATEGY.md`
- `docs/process/TRACEABILITY.md`
- `docs/process/MAINTENANCE.md`
- `docs/process/LLM_DRIFT_REVIEW.md`
- `docs/ai/SESSION_CONTEXT.md`
- `docs/ai/HANDOFF.md`
- `docs/ai/ROADMAP.md`
- `docs/ai/DECISIONS.md`

Repo-local skills:
- `skills/README.md`
- `skills/template-tdd-loop/SKILL.md`
- `skills/template-trace-maintenance/SKILL.md`
- `skills/template-test-lanes/SKILL.md`
- `skills/template-release-doc-sync/SKILL.md`

Traceability note: non-aux `UT/IT/QT` Doxygen `@test` blocks must verify one
or more same-layer IDs (`UT->D`, `IT->A`, `QT->R`). Use optional `@aux yes`
for informational tests that should be excluded from evidence gates.

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

- `./scripts/lint.sh`: strict `clang-tidy` and `lizard` gates.
- `./scripts/test_tdd.sh` and `./scripts/test.sh`: coverage enforcement on
  `src/lib/**`.
- `./scripts/depcheck.sh`: dependency rules, ADR archival, and instruction
  coherence checks.
- `python3 tools/tracecheck.py --write`: strict traceability and evidence checks.
