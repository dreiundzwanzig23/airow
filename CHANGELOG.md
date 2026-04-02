# Changelog

## [Unreleased]
### Changed
- Added a dedicated numerical integration and state-advancement architecture
  boundary (`A-010`) plus a new `docs/process/STATE_CONVENTIONS.md` source of
  truth for simulator frame, sign, and orientation conventions.
- Expanded the simulator requirements to cover startup validity, runtime
  provider validity metadata, and richer machine-readable output contracts for
  load or power accounting and frame-aware vector quantities.
- Replaced the earlier architecture optimization draft with an execution-ready
  hardening plan and seeded the simulator subsystem architecture surface.
- Reserved the `900`-series trace IDs for bootstrap-only sample artifacts and
  moved the placeholder utility evidence out of the real simulator namespace.
- Added architecture policy, architecture health, model fidelity, numerics,
  and calibration provenance process docs.
- Added a major-change skill and strengthened workflow wording around
  architecture allocation before TDD.
- Completed the process-optimization hardening effort, retired the temporary
  execution-plan tracker, and finished the remaining test-strategy alignment
  for scenario baselines, subsystem-contract `IT-*`, and major-change
  characterization coverage.
- Renamed repo-local `template-*` skills to real operational skill names,
  moved them to `.agents/skills`, and added `agents/openai.yaml` metadata for
  Codex skill discovery.
- Extended traceability and dependency guardrails toward architecture-health
  enforcement and component-level dependency policy.
- Reframed the repository documentation from generic template wording to a
  rowing simulator bootstrap project and documented that the current sample
  code is temporary placeholder state.
- Added explicit workflow and contribution references to
  `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md`.
- Marked `docs/process/ARCHITECTURE.md` as bootstrap placeholder
  documentation pending the first simulator architecture seeding task.
- Extended instruction coherence checks to treat
  `docs/process/TECHNOLOGY_STACK.md` as a core process artifact.
- Refreshed the template process model to the evolved generic workflow:
  - moved policy into a stronger top-level `AGENTS.md`,
  - added requirement drift metadata (`Change-Type`, `Needs-Review`,
    `Change-Note`) and lightweight drift-review docs,
  - introduced repo-local skills and AI-doc archive scaffolding,
  - added validation step logging with JSON summaries,
  - expanded `tracecheck.py` with aux-test support and stricter evidence rules,
  - added ADR archival and instruction coherence tooling,
  - updated README, contributing guidance, maintenance docs, and AI context
    templates to match the stricter workflow.
