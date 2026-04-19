# DECISIONS Archive — pre 2026-04-18

Archived ADR blocks moved from `docs/ai/DECISIONS.md`.
## ADR-2026-04-02-001
- **Date**: 2026-04-02
- **Context**: The rowing simulator is a multiphysics project with evolving
  subsystem boundaries. A requirement-driven agent can otherwise drift into a
  fragile 1:1 `R -> A` mapping and patch-oriented growth.
- **Decision**:
  - treat architecture allocation as a mandatory step before TDD,
  - treat functional delivery as an explicit red-green-refactor loop rather
    than an implicit failing-tests-first sequence,
  - require a mandatory behavior-preserving refactor pass after green (or an
    explicit refactor no-op rationale),
  - require loop evidence markers (`rgr:red`, `rgr:green`, `rgr:refactor`) in
    commit messages or evidence notes,
  - adopt hybrid enforcement first: warning-mode marker checks in local and
    pre-merge scripts, with opt-in strict mode,
  - require `A-*` items to represent cohesive subsystem responsibilities rather
    than requirement mirrors,
  - require new `A-*` items to carry explicit allocation rationale and future
    absorption,
  - distinguish ordinary feature work from major-change work in the workflow.
- **Consequences**:
  - architecture synthesis becomes an explicit deliverable,
  - workflow and skills now carry strict phase entry and exit criteria for
    red, green, and refactor,
  - script lanes expose missing RGR evidence early without blocking by default,
  - Codex work packets should target architectural increments, not raw
    requirement mirroring,
  - trace tooling and workflow docs need follow-up updates to enforce this.
