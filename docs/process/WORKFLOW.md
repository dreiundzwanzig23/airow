# WORKFLOW.md - Codex Execution Policy Index

## Purpose
Keep a compact index for autonomous and semi-autonomous work. Mandatory policy
lives in `AGENTS.md`; this file must not redefine it.

## Canonical Sources
- Operating contract and completion gates: `AGENTS.md`
- Architecture allocation: `docs/process/ARCHITECTURE_POLICY.md`
- Approved technologies: `docs/process/TECHNOLOGY_STACK.md`
- Durable rationale: `docs/ai/DECISIONS.md`
- Requirements and acceptance: `docs/process/REQUIREMENTS.md`
- Architecture ownership: `docs/process/ARCHITECTURE.md`

## Execution Shape
- Pick requirement work using the order in `AGENTS.md`.
- Allocate architecture before tests.
- Use failing-tests-first red/green/refactor for each distinct functional
  behavior slice, not once per broad milestone.
- If a new behavior is discovered while implementing, pause implementation and
  add the next failing test before continuing.
- Keep RGR evidence markers ordered as `rgr:red`, `rgr:green`,
  `rgr:refactor`; repeat that sequence for multi-slice work.
- Regenerate traceability with `python3 tools/tracecheck.py --write` when
  trace-relevant files change.
- Update release/context docs only when triggered by the change.

## Operational Playbooks
- Ordinary TDD: `.agents/skills/tdd-loop/SKILL.md`
- Unit test design: `.agents/skills/unit-test-design/SKILL.md`
- Cross-cutting or migratory work: `.agents/skills/major-change-loop/SKILL.md`
- Trace repair or promotion: `.agents/skills/trace-maintenance/SKILL.md`
- Test lane selection: `.agents/skills/test-lanes/SKILL.md`
- Release/context docs: `.agents/skills/release-doc-sync/SKILL.md`

## Context Hygiene
- Start broad orientation with `./scripts/project_stats.py --format agent`.
- Use locate-then-slice retrieval with `rg` and targeted `sed`.
- Exclude archives, generated trace output, build outputs, and logs unless a
  task explicitly needs them.
