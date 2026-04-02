# Repository Skills

These skills are repo-local operational playbooks with real skill metadata.
They live in `.agents/skills` so Codex can discover them as project skills.
Load them on demand instead of embedding all procedure detail in always-loaded
policy docs.

Available skills:
- `.agents/skills/tdd-loop/SKILL.md`
- `.agents/skills/major-change-loop/SKILL.md`
- `.agents/skills/trace-maintenance/SKILL.md`
- `.agents/skills/test-lanes/SKILL.md`
- `.agents/skills/release-doc-sync/SKILL.md`

Use rule:
- Keep `AGENTS.md` as policy core.
- Load only the skill(s) needed for the current task.
