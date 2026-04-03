# Changelog

## [Unreleased]
### Changed
- Replaced the bootstrap `string_utils` sample with the first simulator-facing
  `R-001` configuration slice under `A-001`, including a public JSON loading
  and validation contract.
- Landed `R-002` and `R-003` inside `A-002` with a shared single-run
  orchestration path, injected hydro and aero stub seams, and a minimal
  `project_app --config <path>` CLI contract.
- Clarified the current wording around `R-002` and `R-003` so "simulation run"
  is explicitly understood as orchestration infrastructure, not yet a
  mechanics-backed physics model.
- Tightened `./scripts/lint.sh` so the `all` scope discovers translation units
  from the filesystem under `src/` instead of only from tracked git entries,
  making newly added source files part of the immediate lint feedback loop.
- Enabled the configured identifier-naming checks in `.clang-tidy` and aligned
  the `lizard` structural thresholds with the active function-size policy.
- Added high-signal `clang-tidy` guidance checks for braces, const-correctness,
  member-function constness, magic numbers, special member functions, branch
  clone detection, and declaration isolation.
- Tightened compiler and test policy with stronger warning flags
  (`-Wshadow`, conversion-family warnings, `-Wnon-virtual-dtor`), debug
  hardening (`-fno-omit-frame-pointer`, stdlib assertions), explicit CTest
  timeouts, a dedicated sanitized preset or lane, and an auxiliary tooling
  contract check.
- Added a dedicated GCC portability lane, public-header self-containment aux
  compilation, and LLVM-native `misc-include-cleaner` coverage in
  `clang-tidy` so include drift and compiler-specific regressions are caught
  earlier in the local workflow without a separate IWYU lane.
- Tightened `depcheck` with architecture-health enforcement for
  public-header-only cross-component access, realized component cycle
  detection, and orphan checks that tie component code back to owning `A-*`
  items and non-aux architecture evidence.
- Split test linting away from production linting with a dedicated
  `lint_tests.sh` lane, tighter test-only structural thresholds, and static
  banned-pattern checks for implementation includes, private-public hacks,
  `FRIEND_TEST`, direct wall-clock reads, and sleep-based timing.
- Raised the `src/lib/**` unit coverage gates to 90% region coverage and 80%
  branch coverage and added the missing unit-path tests needed to sustain the
  stricter baseline.
- Expanded the simulator trace surface to `D-001..D-014`,
  `UT-001..UT-012`, `IT-001..IT-005`, and `QT-001..QT-005`.
- Added the first real simulator trace evidence: `D-001..D-009`,
  `UT-001..UT-007`, `IT-001..IT-002`, and `QT-001..QT-002`.
- Activated the first component-prefixed production code path in
  `configuration`, fixed component self-dependency policy, and added the
  `nlohmann-json3-dev` setup dependency.
- Added an explicit `v0.1` milestone cut line in `docs/ai/ROADMAP.md` centered
  on the deterministic headless single-run baseline.
- Reprioritized calibration ingestion, calibration provenance, time-varying
  wind, and batch sweeps behind `v0.1` and marked those requirements for
  review.
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
