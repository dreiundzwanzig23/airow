# DECISIONS Archive — pre 2026-04-02

Archived ADR blocks moved from `docs/ai/DECISIONS.md`.
## ADR-2026-03-23-001
- **Date**: 2026-03-23
- **Context**: The original template process model had drifted behind the
  evolved repository workflow and no longer captured the stricter generic
  gates, AI-doc hygiene, or lightweight requirement-drift handling.
- **Decision**:
  - adopt a stronger policy-core `AGENTS.md`,
  - require requirement drift metadata and aux-test aware traceability,
  - add repo-local skills, validation summaries, and AI decision archival.
- **Consequences**:
  - the template is stricter on process drift by default,
  - active AI context stays smaller and easier to resume,
  - project-specific runtime or backend gates remain intentionally out of
    scope for the template.
