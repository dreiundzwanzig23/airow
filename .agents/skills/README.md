# Repository Skills

These repo-local skills are operational playbooks for Codex. Keep `AGENTS.md` as the policy core and load only the skill or skills required by the selected work lane.

## Available Skills

- `.agents/skills/work-lanes/SKILL.md` — classify the task and choose the lightest sufficient workflow lane.
- `.agents/skills/tdd-loop/SKILL.md` — execute failing-tests-first behavior work.
- `.agents/skills/unit-test-design/SKILL.md` — design narrow, behavior-focused `UT-*` coverage.
- `.agents/skills/major-change-loop/SKILL.md` — manage cross-cutting, migratory, backend, architectural, or semantic multi-requirement changes.
- `.agents/skills/trace-maintenance/SKILL.md` — repair, promote, or regenerate traceability links.
- `.agents/skills/test-lanes/SKILL.md` — choose the right test command for the selected work lane.
- `.agents/skills/release-doc-sync/SKILL.md` — update release-facing and AI-context docs only when triggered.
- `.agents/skills/workflow-audit/SKILL.md` — audit lane discipline, gates, traceability, evidence, and docs before handoff or merge.
- `.agents/skills/simulation-evidence-design/SKILL.md` — design evidence for simulator physics, visualization, calibration, validation, and optimization claims.
- `.agents/skills/karpathy-guidelines/SKILL.md` — apply the repository's concise AI-coding style guidance when requested or relevant.

## Use Rule

1. Classify the lane first.
2. Load the smallest relevant skill set.
3. Do not load major-change, release-doc, or trace-maintenance skills unless their triggers are met.
