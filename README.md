# AIRow Repository

AIRow is an open-source C++ rowing simulator project focused on a physically
serious single-scull simulator with strict red-green-refactor TDD, traceability
(`R -> A -> D -> UT/IT/QT`), and deterministic local gates.

The `v0.1` cut line is now complete: the repository ships a deterministic
headless single-scull baseline with validated configuration loading, shared
in-memory and CLI execution, mechanics-backed startup and stepping, reduced
hydro and steady-wind aero runtime models, machine-readable outputs, and
runtime-backed passive-float, tow, calm-water, headwind, and crosswind
evidence.

The broader rowing simulator direction remains defined in the requirements,
architecture, technology stack, and decision records, including explicit
state-convention and numerical-integration ownership. Post-`v0.1` work has now
landed the observability slice, the runtime provider-selection slice, multiple
hydro and steady-wind aero fidelity refinements on the existing built-in
provider ids, and the first backend-selection slice for built-in
state-advancer selection with a required `sundials_ida` default path plus an
optional guarded Chrono path, now validated on Chrono-capable builds against
passive-float and tow scenario evidence; the roadmap now keeps reduced-model
fidelity and backend wiring separate before deferred calibration or
time-varying environment workflows.

## Quick Start

Install dependencies:
```bash
./scripts/setup.sh
```

This now installs the required Ubuntu SUNDIALS development package
(`libsundials-dev`) for the default `sundials_ida` runtime path.

Build:
```bash
./scripts/build.sh
```

Chrono-enabled build notes:
```bash
# Auto-detected when Chrono is installed under ~/.local
./scripts/build.sh

# Fallback for non-default install prefixes
CMAKE_PREFIX_PATH=/path/to/chrono/prefix ./scripts/build.sh
```

To run with Chrono-backed rigid-body stepping once Chrono support is enabled in
the build, set `"simulation.state_advancer": "chrono_rigidbody"` in the run
config.

Run one headless baseline case:
```bash
./build/project_app --config /path/to/config.json
```

Run one case with the human-readable compact report:
```bash
./build/project_app --config /path/to/config.json --report compact
```

Runnable example catalog:
```bash
./examples/run_example.sh passive_float
./examples/run_example.sh tow_test
./examples/run_example.sh calm_water_stroke
```

See `examples/README.md` for the checked-in CLI configs, output locations, and
the distinction between direct example configs and the scenario-harness
artifacts under `scenarios/`.

Current implemented library surface:
- `include/project/aero/baseline_providers.hpp`
- `include/project/configuration/provider_catalog.hpp`
- `include/project/configuration/simulator_config.hpp`
- `include/project/hydro/baseline_providers.hpp`
- `include/project/mechanics/state.hpp`
- `include/project/numerics/backend_catalog.hpp`
- `include/project/numerics/state_advancement.hpp`
- `include/project/orchestrator/simulation_run.hpp`
- `include/project/orchestrator/cli.hpp`
- `include/project/orchestrator/scenario_harness.hpp`
- `include/project/output/run_analysis.hpp`
- JSON file or in-memory loading for baseline simulation, hull, oar, seat, and
  prescribed-stroke startup fields
- deterministic diagnostics and normalized configuration metadata for `R-001`
- deterministic mechanics startup and state-advancement seams for the first
  `A-003` and `A-010` slice
- reusable in-memory single-run API with injected hydro, aero, and
  state-advancer seams plus structured state/load history
- top-level config-driven built-in provider selection for
  `hull_resistance`, `blade_force`, and `aero_load`
- top-level config-driven built-in state-advancer selection for
  required `sundials_ida` by default, explicit `deterministic_baseline`
  fallback, and deterministic rejection of `chrono_rigidbody` when Chrono
  support is unavailable in the build plus Chrono-backed rigid-body stepping
  when a discoverable Chrono package is present
- deterministic machine-readable summary and time-series artifacts with
  explicit unit or frame annotations for boundary-visible channels
- structured hydro force or moment vectors, final passive-float equilibrium
  diagnostics, and explicit blade-immersion or blade-tip-velocity channels in
  runtime outputs
- explicit world-frame ambient-wind configuration plus structured
  apparent-wind and aerodynamic-moment channels in runtime outputs
- output-format selection (`json`, `hdf5`, or both) with deterministic
  configuration rejection when HDF5 is requested but unavailable in the build
- configuration-controlled high-frequency time-series emission
- headless CLI wrapper with stable exit-code behavior for `R-002` and `R-003`
- stable run-result contract now lives in `include/project/output/run_result.hpp`
  with output-writer seams in `include/project/output/run_output.hpp`
- derived single-run analysis summaries and human-readable report formatting in
  `include/project/output/run_analysis.hpp`
- deterministic scenario-definition loading and acceptance-envelope evaluation
  for checked-in passive-float, tow, calm-water stroke, headwind stroke, and
  crosswind stroke artifacts in `scenarios/*.json`
- deterministic hydro baseline providers for passive float, tow drag, and
  calm-water stroke propulsion behind the shared `HydroProvider` seam, now
  including reduced hydrostatic restoring loads, low-speed-damped built-in
  hull resistance, and phase-shaped immersion-aware blade forces with
  backward-slip gating and zero-load catch or release boundaries
- deterministic aero baseline provider for steady apparent-wind, low-speed-
  sensitive headwind drag, lateral crosswind force, and speed-shaped yaw-
  moment reporting behind the shared `AeroProvider` seam
- structured provider metadata in run summaries with per-role provider ids plus
  validity identifiers and descriptions
- optional `--report compact|full` CLI output for human-readable run-state and
  load-envelope inspection
- offline `python3 tools/run_analysis.py --summary <path> --time-series <path>
  --output-dir <dir>` report generation with static HTML and SVG plots from
  emitted JSON artifacts

## Project Direction

Primary planning and process sources:
- `docs/process/REQUIREMENTS.md`
- `docs/process/ARCHITECTURE.md`
- `docs/process/ARCHITECTURE_POLICY.md`
- `docs/process/TECHNOLOGY_STACK.md`
- `docs/ai/DECISIONS.md`

Architecture note: `docs/process/ARCHITECTURE.md` now starts with a compact
overview of system context, building blocks, runtime flows, cross-cutting
concepts, and current implementation status before the normative `A-*`
ownership catalog.

Current intent:
- single-scull simulator first,
- headless executable plus reusable library API,
- 3D mechanics core with reduced hydro and aero runtime models,
- explicit world-frame, sign, and orientation conventions at simulator
  boundaries,
- explicit numerical integration and startup-validity ownership separate from
  mechanics ownership,
- optional high-fidelity calibration workflows kept outside the default runtime,
- real simulator code should now land inside the hardened architecture-first
  workflow rather than extending the bootstrap sample.

Current implementation status:
- the `v0.1` roadmap cut line is complete at the requirement level,
- `A-001 Configuration and Validation` is now in progress with a real public
  contract for deterministic JSON loading and validation of mechanics-startup
  inputs plus the top-level built-in provider-selection schema,
- `A-002 Simulation Orchestrator` is now in progress with a shared single-run
  path for CLI and in-memory execution plus config-driven built-in provider
  construction when injected seams are absent,
- `A-003 Mechanics Subsystem` and `A-010 Numerical Integration and State
  Advancement` are now in progress through a deterministic internal baseline
  state-advancer seam plus the first config-driven built-in backend-selection
  path for a guarded Chrono rigid-body advancer, now validated on
  Chrono-capable builds with passive-float and tow scenario coverage,
- `A-007 Output and Diagnostics` is now in progress with deterministic
  machine-readable summary/time-series artifact emission, structured provider
  metadata propagation, and optional HDF5 parity behind the same output
  contract,
- `A-005 Aero Runtime Models` is now in progress with the first steady-wind
  apparent-wind and aerodynamic-load slice, runtime-selectable built-in
  provider wiring, and an in-place steady-wind fidelity refinement on the
  existing built-in aero id,
- `A-008 Scenario Harness and Validation` is now in progress with a public
  scenario-harness API and runtime-backed passive-float/tow/calm-water/
  headwind/crosswind `QT-*` evidence,
- bootstrap-only placeholder code has been removed from the compiled targets,
- richer runtime provider fidelity is now in progress on the existing
  `A-004` and `A-005` seams through in-place hydro and steady-wind aero
  baseline refinements plus re-characterized wind-backed scenario envelopes,
  while the first `A-010` backend-selection slice now lands separately through
  built-in state-advancer selection and a compile-time-guarded Chrono path
  rather than mixing solver adoption into provider-selection work.

## Validation Lanes

Fast local TDD loop:
```bash
./scripts/test_tdd.sh
```

Standard full local testing:
```bash
./scripts/test.sh
```

Dedicated sanitized test lane:
```bash
./scripts/test_sanitized.sh
```

Dedicated GCC portability lane:
```bash
./scripts/test_gcc.sh
```

Auxiliary script/tool contracts:
```bash
./scripts/test_aux.sh
```

Dedicated test-quality lint lane:
```bash
./scripts/lint_tests.sh
```

Aggregate pre-merge validation:
```bash
./scripts/verify.sh
```

RGR evidence check (warning-only by default):
```bash
./scripts/check_rgr_evidence.sh
```

Required local completion gates:
```bash
./scripts/format.sh
./scripts/lint.sh
./scripts/build.sh
./scripts/test.sh
./scripts/depcheck.sh
python3 tools/tracecheck.py --write
```

## Process Model

Core artifacts:
- `AGENTS.md`
- `docs/process/REQUIREMENTS.md`
- `docs/process/ARCHITECTURE.md`
- `docs/process/TECHNOLOGY_STACK.md`
- `docs/process/ARCHITECTURE_POLICY.md`
- `docs/process/WORKFLOW.md`
- `docs/process/TEST_STRATEGY.md`
- `docs/process/SCENARIOS.md`
- `docs/process/TRACEABILITY.md`
- `docs/process/MAINTENANCE.md`
- `docs/process/LLM_DRIFT_REVIEW.md`
- `docs/process/ARCHITECTURE_HEALTH.md`
- `docs/process/MODEL_FIDELITY.md`
- `docs/process/STATE_CONVENTIONS.md`
- `docs/process/NUMERICS_POLICY.md`
- `docs/process/CALIBRATION_PROVENANCE.md`
- `docs/ai/SESSION_CONTEXT.md`
- `docs/ai/HANDOFF.md`
- `docs/ai/ROADMAP.md`
- `docs/ai/DECISIONS.md`

Repo-local skills:
- `.agents/skills/README.md`
- `.agents/skills/tdd-loop/SKILL.md`
- `.agents/skills/unit-test-design/SKILL.md`
- `.agents/skills/trace-maintenance/SKILL.md`
- `.agents/skills/test-lanes/SKILL.md`
- `.agents/skills/release-doc-sync/SKILL.md`
- `.agents/skills/major-change-loop/SKILL.md`

Traceability note: non-aux `UT/IT/QT` Doxygen `@test` blocks must verify one
or more same-layer IDs (`UT->D`, `IT->A`, `QT->R`). Use optional `@aux yes`
for informational tests that should be excluded from evidence gates.

Technology note: `docs/process/TECHNOLOGY_STACK.md` is the source of truth for
approved core libraries and formats. `docs/ai/DECISIONS.md` records the
rationale and durable technical constraints behind those choices.

## Validation Output

Validation scripts default to compact summaries. Use `VALIDATION_OUTPUT=verbose`
for live command output.

Artifacts:
- logs: `build/logs/validation/`
- JSON summaries: `build/logs/validation/*.json`
- interrupted runs keep a `*.json.steps.tsv` scratch file instead of leaving a
  stale summary behind

Useful environment variables:
- `TEST_BUILD_DIR`
- `LINT_BUILD_DIR`
- `VALIDATION_OUTPUT`
- `VALIDATION_LOG_DIR`
- `VALIDATION_SUMMARY_DIR`
- `VALIDATION_SUMMARY_PATH`
- `RGR_ENFORCEMENT_MODE` (`warn`, `strict`, `off`)
- `RGR_EVIDENCE_TEXT`
- `RGR_EVIDENCE_FILE`

## AI Context and Archive

Active AI docs are intentionally compact. Historical ADRs are archived under
`docs/ai/archive/`.

Maintenance commands:
```bash
./scripts/maintenance.sh
./scripts/project_stats.py --format agent
```

## Gate Highlights

- `./scripts/lint.sh`: strict `clang-tidy` and `lizard` gates over the current
  `src/` and `include/` translation-unit tree, including newly added source
  files, with
  aligned naming and function-size thresholds plus stronger guidance checks for
  const-correctness, braces, magic numbers, declaration isolation,
  LLVM-native include-cleaner coverage, and related agent-facing code quality
  issues.
- `./scripts/test.sh`: full validation now includes auxiliary tooling
  contracts, changed-scope test-quality linting, a dedicated sanitized runtime
  lane, a dedicated GCC portability lane, and unit coverage over `src/lib/**`
  with stricter 90% region and 80% branch gates.
- `./scripts/lint_tests.sh`: separate test-quality linting with banned-pattern
  checks for implementation-coupled or nondeterministic tests plus tighter
  test-only structural thresholds (default max `900` lines and `14` test
  cases per file) plus narrow legacy-file overrides where the repo still has
  explicit aggregation debt to split later.
- `./scripts/test_aux.sh`: auxiliary coverage now includes the dedicated
  test-quality lint lane, tooling contracts, and public-header self-containment
  compilation, plus a regression that checks validation summaries report
  nested child failures truthfully.
- `./scripts/test_tdd.sh` and `./scripts/test.sh`: coverage enforcement on
  `src/lib/**` plus changed-file coverage ratchets against merge-base
  baselines.
- `./scripts/depcheck.sh`: dependency rules, ADR archival, and instruction
  coherence checks, including public-header-only cross-component access,
  realized component cycle detection, and component-orphan guardrails tied to
  architecture ownership and non-aux test coverage.
- `python3 tools/tracecheck.py --write`: strict traceability and evidence checks.
