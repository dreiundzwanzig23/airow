# Repository Skills

These skills are repo-local operational playbooks with real skill metadata. They
live in `.agents/skills` so Codex can discover them as project skills. Load them
on demand instead of embedding all procedure detail in always-loaded policy docs.

## Available skills

| Skill | Use when |
|---|---|
| `.agents/skills/tdd-loop/SKILL.md` | Implementing features or fixing behavior with strict red/green/refactor discipline. |
| `.agents/skills/unit-test-design/SKILL.md` | Designing or tightening `UT-*` coverage for `D-*` behavior. |
| `.agents/skills/major-change-loop/SKILL.md` | Handling cross-cutting, architectural, migratory, or semantic multi-requirement work. |
| `.agents/skills/trace-maintenance/SKILL.md` | Updating `R/A/D/test` metadata, generated traceability, or evidence links. |
| `.agents/skills/test-lanes/SKILL.md` | Choosing the correct test lane for iteration, auxiliary checks, regression, or pre-merge verification. |
| `.agents/skills/release-doc-sync/SKILL.md` | Synchronizing README, CHANGELOG, and AI context docs when a change triggers documentation obligations. |
| `.agents/skills/workflow-audit/SKILL.md` | Auditing whether a branch or task is process-clean before handoff or merge. |
| `.agents/skills/simulation-evidence-design/SKILL.md` | Designing evidence for physics, visualization, calibration, validation, or optimization claims before implementation. |
| `.agents/skills/karpathy-guidelines/SKILL.md` | Applying lightweight coding guardrails against overcomplication, unclear assumptions, and unverifiable changes. |

## Use rule

- Keep `AGENTS.md` as policy core.
- Load only the skill(s) needed for the current task.
- Prefer the most specific skill that matches the work.
- Use `workflow-audit` before handoff when process cleanliness is uncertain.
