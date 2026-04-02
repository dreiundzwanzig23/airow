# AIRow Bootstrap Repository

AIRow is an open-source C++ rowing simulator project focused on a physically
serious single-scull simulator with strict TDD, traceability
(`R -> A -> D -> UT/IT/QT`), and deterministic local gates.

The repository is still in bootstrap state. The current tracked code includes a
small placeholder sample, while the rowing simulator direction is defined in
the requirements, architecture bootstrap, technology stack, and decision
records.

## Quick Start

Install dependencies:
```bash
./scripts/setup.sh
```

Build:
```bash
./scripts/build.sh
```

Run the current bootstrap app:
```bash
./build/project_app
```

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
- optional high-fidelity calibration workflows kept outside the default runtime.

Current bootstrap note:
- the `string_utils` sample code remains as a temporary placeholder until the
  first real simulator implementation slice replaces it,
- bootstrap-only sample artifacts live in reserved `900`-series trace IDs and
  do not represent simulator capability.

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
- `docs/process/TECHNOLOGY_STACK.md`
- `docs/process/ARCHITECTURE_POLICY.md`
- `docs/process/WORKFLOW.md`
- `docs/process/TEST_STRATEGY.md`
- `docs/process/TRACEABILITY.md`
- `docs/process/MAINTENANCE.md`
- `docs/process/LLM_DRIFT_REVIEW.md`
- `docs/process/ARCHITECTURE_HEALTH.md`
- `docs/process/MODEL_FIDELITY.md`
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

- `./scripts/lint.sh`: strict `clang-tidy` and `lizard` gates.
- `./scripts/test_tdd.sh` and `./scripts/test.sh`: coverage enforcement on
  `src/lib/**`.
- `./scripts/depcheck.sh`: dependency rules, ADR archival, and instruction
  coherence checks.
- `python3 tools/tracecheck.py --write`: strict traceability and evidence checks.
