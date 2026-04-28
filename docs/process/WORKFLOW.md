# WORKFLOW.md — Codex Execution Policy Index

## Purpose

Keep a compact index for autonomous and semi-autonomous work. Mandatory policy lives in `AGENTS.md`; this file must not redefine it.

## Canonical Sources

- Operating contract, work-lane router, documentation triggers, and completion rules: `AGENTS.md`
- Architecture allocation: `docs/process/ARCHITECTURE_POLICY.md`
- Approved technologies: `docs/process/TECHNOLOGY_STACK.md`
- Durable rationale: `docs/ai/DECISIONS.md`
- Requirements and acceptance: `docs/process/REQUIREMENTS.md`
- Architecture ownership: `docs/process/ARCHITECTURE.md`

## Execution Shape

- Classify the task before editing and default to the lightest sufficient lane.
- Use Lane 0 for exploration-only work and do not edit files in that lane.
- Use Lane 1 for tiny local fixes that do not change public contracts.
- Use Lane 2 for ordinary functional behavior slices with failing-test-first red/green/refactor.
- Use Lane 3 for public interfaces, artifact formats, examples, scenarios, reports, or user-facing docs.
- Use Lane 4 only for cross-cutting, migratory, backend, architectural, or semantic multi-requirement changes.
- Use Lane 5 for release, merge, milestone, or durable handoff work.
- Escalate lanes only when the actual change triggers the heavier workflow.

## Documentation Discipline

- Update release/context docs only when the trigger table in `AGENTS.md` requires it.
- Do not update README, CHANGELOG, requirements, architecture, traceability, or `docs/ai/*` merely because files were touched.
- Regenerate traceability with `python3 tools/tracecheck.py --write` only when trace-relevant files or metadata changed.
- Keep generated trace markdown compact; use `python3 tools/tracecheck.py --json` for detailed investigation.

## Operational Playbooks

- Work lane selection: `.agents/skills/work-lanes/SKILL.md`
- Ordinary TDD: `.agents/skills/tdd-loop/SKILL.md`
- Unit test design: `.agents/skills/unit-test-design/SKILL.md`
- Cross-cutting or migratory work: `.agents/skills/major-change-loop/SKILL.md`
- Trace repair or promotion: `.agents/skills/trace-maintenance/SKILL.md`
- Test lane selection: `.agents/skills/test-lanes/SKILL.md`
- Release/context docs: `.agents/skills/release-doc-sync/SKILL.md`
- Workflow audit: `.agents/skills/workflow-audit/SKILL.md`
- Simulation evidence: `.agents/skills/simulation-evidence-design/SKILL.md`

## Context Hygiene

- Start broad orientation with `./scripts/project_stats.py --format agent` only when broad orientation is useful.
- Use locate-then-slice retrieval with `rg` and targeted `sed`.
- Exclude archives, generated trace output, build outputs, and logs unless a task explicitly needs them.
- Load skill files only after selecting a lane that needs them.
