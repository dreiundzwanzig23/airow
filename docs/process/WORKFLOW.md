# WORKFLOW.md — Codex Execution Policy

## Objective
Define deterministic execution behavior for autonomous and semi-autonomous
changes.

## Canonical Policy
- Mandatory governance rules live in `AGENTS.md`.
- This file is an operational index and must not redefine conflicting policy.
- Use `docs/process/ARCHITECTURE_POLICY.md` as the canonical architecture
  allocation rule set.
- Use `docs/process/TECHNOLOGY_STACK.md` for approved core technologies and
  `docs/ai/DECISIONS.md` for durable technical rationale when a task touches
  libraries, solver direction, output formats, or external integrations.
- For token-efficient agent work, prefer compact repo orientation
  (`./scripts/project_stats.py --format agent`), locate-then-slice retrieval,
  and default exclusion of archive/generated/log paths unless the task
  explicitly needs history, generated trace output, or raw logs.

## Required Sequence
1. Select actionable requirement work in this order:
   - requirements with `Needs-Review: yes`,
   - then highest-priority `OPEN` requirement from `REQUIREMENTS.md`.
2. Perform architecture allocation in `ARCHITECTURE.md` before tests:
   - identify candidate owning `A-*` items,
   - reuse an existing subsystem when coherent,
   - record any architecture delta before TDD,
   - justify any new `A-*` in `Allocation Rationale`.
3. Red: write or adjust failing tests first (`UT/IT/QT` by layer) and preserve
   concrete failure evidence.
4. Green: implement minimal production code to make the targeted failing tests
   pass.
5. Refactor: execute a mandatory behavior-preserving cleanup pass after green.
   If no refactor edits are needed, record an explicit refactor no-op
   rationale.
6. Re-run `./scripts/test_tdd.sh` after refactor.
7. Run required quality gates.
8. Regenerate traceability (`python3 tools/tracecheck.py --write`).
9. Update changelog/readme/context docs when triggered.

For cross-cutting refactors, migrations, and semantic multi-requirement work,
use `.agents/skills/major-change-loop/SKILL.md` rather than the ordinary
feature loop.

## Operational Playbooks (Skills)
- TDD execution loop: `.agents/skills/tdd-loop/SKILL.md`
- Unit test design: `.agents/skills/unit-test-design/SKILL.md`
- Major-change loop: `.agents/skills/major-change-loop/SKILL.md`
- Trace maintenance: `.agents/skills/trace-maintenance/SKILL.md`
- Test lane choice: `.agents/skills/test-lanes/SKILL.md`
- Release/doc sync: `.agents/skills/release-doc-sync/SKILL.md`

## Enforcement Summary
- Architecture allocation must happen before failing-tests-first work.
- `A-*` items are subsystem ownership boundaries, not requirement mirrors.
- Skipping red, green, or refactor phases for functional changes is not
  allowed.
- Use explicit `rgr:red`, `rgr:green`, and `rgr:refactor` evidence markers for
  loop traceability (commit message or note).
- `./scripts/check_rgr_evidence.sh` runs in warning mode by default from
  `test_tdd.sh` and `verify.sh`; strict mode is opt-in via
  `RGR_ENFORCEMENT_MODE=strict`.
- Missing trace tags or evidence is not allowed.
- Duplicate `@design` or `@test` IDs are not allowed.
- Non-trivial `src/lib` functions require explicit `@design D-*` mapping.
- `trace: trivial` (or equivalent helper bypass tags) is not allowed.
- Helper-only `@design` refinements must target exactly one parent `D-*`.
- `DONE` items require minimum evidence by layer (`D->UT`, `A->IT`, `R->QT`).
- Fast and full coverage gates must include changed-file regression checks.
