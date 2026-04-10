# Changelog

## [Unreleased]
### Changed
- Continued Slice 2 on `A-005` by deepening the existing built-in
  `steady_wind_placeholder` aero id in place: the provider now adds stronger
  low-apparent-wind headwind drag sensitivity, explicit lateral crosswind
  force, and speed-shaped yaw behavior while preserving the existing provider
  selection and structured metadata contracts; the checked-in headwind and
  crosswind scenario envelopes were re-characterized to the richer
  deterministic wind baseline with new evidence (`D-038`, `D-039`,
  `UT-129`, `UT-130`).
- Started the reduced-model fidelity slice on `A-004` by deepening the
  existing built-in hydro ids in place: `quadratic_drag_placeholder` now adds
  low-speed damping on top of monotonic speed-squared drag,
  `stroke_propulsion_placeholder` now shapes blade force across drive phase,
  and the checked-in calm-water or headwind or crosswind scenario envelopes
  were re-characterized to the new deterministic hydro baselines with new
  evidence (`D-036`, `D-037`, `UT-127`, `UT-128`).
- Closed the provider-selection slice (`R-020`, `R-033`) with a top-level
  `providers` config block, built-in reduced hull-resistance or blade-force or
  aero provider construction on the shared run path, structured provider
  validity metadata in JSON/HDF5 summaries, updated example configs, and new
  evidence (`D-032..D-034`, `UT-122..UT-126`, `IT-013..IT-015`,
  `QT-029..QT-030`).
- Added a human-readable run-analysis slice (`R-034`) with a public
  `run_analysis` contract, additive summary-analysis JSON, optional
  `project_app --report compact|full` output, and a dependency-light
  `tools/run_analysis.py` static HTML or SVG post-processing tool for
  single-run inspection.
- Replaced the generic post-`v0.1` backlog in `docs/ai/ROADMAP.md` with four
  explicit follow-on slices and clarified across the roadmap, session, handoff,
  and README docs that Chrono or SUNDIALS belong to a later external-backend
  slice under `A-010`, not to the near-term hydro or aero provider-selection
  work.
- Closed the remaining `v0.1` cut-line requirements with new
  requirement-level evidence (`QT-019..QT-026`) covering hull or oar or seat
  mechanics closure, ten-cycle stroke replay, runtime diagnostics, unit or
  traceability checks, and startup validity, promoting `R-005`, `R-006`,
  `R-007`, `R-008`, `R-016`, `R-017`, `R-019`, and `R-032` to `DONE`.
- Tightened the shared run-path state validator so non-finite boundary-visible
  oar blade-tip velocity or immersion state now fails deterministically under
  `state_advancement` rather than slipping through runtime validation.
- Updated `tools/tracecheck.py --json` to emit clean JSON for automation
  consumers and synchronized the roadmap, session, handoff, and README docs to
  record that the `v0.1` cut line is complete.
- Raised the baseline `A-004` hydro slice from placeholder behavior to
  deterministic reduced hydrostatic restoring, widened hydro force or moment
  propagation, explicit blade immersion or blade-tip-velocity state, and
  immersion-aware blade-water loads with new evidence (`D-028`, `D-029`,
  `UT-105..UT-112`, `IT-011`) that closes `R-009`, `R-010`, and `R-011`.
- Landed the first `A-005` steady-wind aero slice with explicit ambient-wind
  configuration, a public baseline aero-provider header, structured
  apparent-wind plus aerodynamic force or moment samples in run outputs, and
  new evidence (`D-026`, `UT-075..UT-079`, `UT-104`, `IT-010`) that closes
  `R-013` and `R-014`; the same built-in aero id is now being refined in
  place rather than replaced in later slices.
- Completed the `v0.1` reference scenario set with checked-in
  `scenarios/headwind_stroke.json` and `scenarios/crosswind_stroke.json`,
  extended scenario-harness loading or evaluation for wind-backed envelopes,
  and new evidence (`D-027`, `UT-080..UT-103`, `QT-016..QT-018`) that closes
  `R-018` and `R-031`.
- Expanded `docs/process/ARCHITECTURE.md` with an arc42-lite overview section
  ahead of the traceable `A-*` catalog, adding compact system-context,
  building-block, runtime-view, cross-cutting-concept, and
  implemented-versus-planned summaries for faster architectural orientation.
- Added a dedicated `.agents/skills/unit-test-design/SKILL.md` playbook for
  behavior-focused `UT-*` design, boundary and equivalence-class case
  selection, Given/When/Then notes, and explicit expected-output guidance,
  with integration points from the TDD and major-change loops.
- Tightened the workflow from generic failing-tests-first to explicit
  red-green-refactor enforcement with a mandatory refactor phase, aligned
  policy and skill playbooks, and a new warning-only
  `./scripts/check_rgr_evidence.sh` hook in `test_tdd.sh` and `verify.sh`
  for `rgr:red`, `rgr:green`, and `rgr:refactor` marker checks.
- Added the first `A-008` scenario-harness slice with a public
  `scenario_harness` contract, checked-in passive-float and tow scenario
  artifacts, deterministic acceptance-envelope evaluation, and new evidence
  coverage (`D-023`, `D-024`, `UT-052..UT-070`, `QT-009..QT-011`) while
  intentionally staging hydro behavior through deterministic placeholder
  providers.
- Landed the first real `A-004` hydro runtime slice with structured hydro load
  samples, deterministic passive/tow/calm-water placeholder providers, real
  blade-load propagation into run outputs, and a checked-in calm-water stroke
  scenario with new evidence (`D-025`, `UT-071..UT-074`, `IT-009`,
  `QT-012..QT-013`) that closes `R-012`.
- Added a tracked `examples/` catalog with runnable passive-float, tow-test,
  and calm-water CLI configs, a `run_example.sh` helper, documented output
  locations under `examples/output/`, and system coverage for both the helper
  script and direct example-config execution.
- Completed the next `A-007` output slice and closed `R-015` with
  format-selectable machine-readable artifacts (`json`, `hdf5`, or both),
  deterministic JSON summary/time-series emission, optional HDF5 artifact
  emission, explicit unit/frame channel annotations, force/power accounting
  channels, and deterministic rejection when HDF5 output is requested on
  unsupported builds.
- Landed the first mechanics-backed runtime slice for `A-003` and `A-010`
  with expanded startup configuration, public mechanics and state-advancer
  contracts, deterministic startup-validity diagnostics, and in-memory hull,
  oar, seat, and stroke state history.
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
- Hardened workflow enforcement by removing the `trace: trivial` function
  bypass, requiring helper refinement blocks to target exactly one parent
  design item, adding split unit files for mechanics-heavy coverage, and
  enforcing changed-file coverage ratchets in both `test_tdd.sh` and
  `test.sh`.
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
