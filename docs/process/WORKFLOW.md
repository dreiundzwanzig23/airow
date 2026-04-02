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
3. Write failing tests first (`UT/IT/QT` by layer).
4. Implement minimal production code.
5. Iterate failing-test -> implement -> refactor as new risks are discovered.
6. Run required quality gates.
7. Regenerate traceability (`python3 tools/tracecheck.py --write`).
8. Update changelog/readme/context docs when triggered.

For cross-cutting refactors, migrations, and semantic multi-requirement work,
use `skills/template-major-change-loop/SKILL.md` rather than the ordinary
feature loop.

## Operational Playbooks (Skills)
- TDD execution loop: `skills/template-tdd-loop/SKILL.md`
- Major-change loop: `skills/template-major-change-loop/SKILL.md`
- Trace maintenance: `skills/template-trace-maintenance/SKILL.md`
- Test lane choice: `skills/template-test-lanes/SKILL.md`
- Release/doc sync: `skills/template-release-doc-sync/SKILL.md`

## Enforcement Summary
- Architecture allocation must happen before failing-tests-first work.
- `A-*` items are subsystem ownership boundaries, not requirement mirrors.
- Skipping TDD for functional changes is not allowed.
- Missing trace tags or evidence is not allowed.
- Duplicate `@design` or `@test` IDs are not allowed.
- Non-trivial `src/lib` functions require explicit `@design D-*` mapping.
- `DONE` items require minimum evidence by layer (`D->UT`, `A->IT`, `R->QT`).
