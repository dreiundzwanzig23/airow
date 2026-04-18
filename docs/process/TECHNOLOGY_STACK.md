# TECHNOLOGY_STACK.md

This document is the source of truth for the approved technology stack for the
rowing simulator project.

Use this file to record:
- the approved core libraries and frameworks,
- their role in the system,
- current adoption status,
- platform or integration notes,
- the decision record that explains why the choice was made.

Use `docs/ai/DECISIONS.md` for rationale and strategic decision history.
Use build manifests such as `CMakeLists.txt` and `CMakePresets.json` for the
currently wired implementation truth.

## Status meanings
- **approved**: the preferred project choice for the stated role.
- **optional**: supported or planned, but not required for the baseline runtime.
- **deferred**: intentionally postponed until the project needs it.
- **trial**: under evaluation; not yet the default project choice.

## Core runtime

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Core language | C++ | approved | Main implementation language for the runtime simulator and core libraries | Chosen for strong systems control, mature scientific libraries, and compatibility with the preferred mechanics and solver stack | ADR-2026-04-02-007 |
| Build system | CMake | approved | Primary build orchestration | Existing repo baseline; remains the source of truth for compilation and dependency wiring | ADR-2026-03-23-001 |
| Test framework | GoogleTest | approved | Unit, integration, and system verification lanes | Existing repo baseline; keep lane separation and trace tagging expectations | ADR-2026-03-23-001 |

## Mechanics and integration

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Mechanics backend | Project Chrono | approved | 3D multibody backbone for hull, oars, seat, constraints, and external loads | Preferred mechanics backend for the standard non-libc++ runtime build; libc++ sanitizer and coverage lanes remain explicit no-Chrono exceptions | ADR-2026-04-18-009 |
| Integration library | SUNDIALS IDA/IDAS | approved | Preferred constrained integration path for runtime stepping and sensitivities | Preferred supported runtime pair is `chrono_rigidbody + sundials_ida`; fallback modes remain behind the same project contract | ADR-2026-04-18-009 |
| Numerical backend policy | Integrator behind a project contract | approved | Hide concrete stepping backend behind stable subsystem interfaces | Prevents solver choice from leaking into requirements or broad implementation surfaces | ADR-2026-04-02-003 |

## Runtime physics model strategy

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Mechanics-centric runtime | 3D mechanics core with hydro and aero load providers | approved | Main simulation structure for routine runs | Runtime loop is centered on constrained mechanics; water and air primarily act as load-provider subsystems | ADR-2026-04-02-004 |
| Initial runtime scope | Single-scull, headless baseline | approved | First implementation target | Includes full 3D rigid-body mechanics, prescribed stroke input, reduced hydro/aero runtime models, and scenario-based validation | ADR-2026-04-02-005 |

## High-fidelity truth models and calibration

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Blade/water truth model | DualSPHysics | approved | First high-fidelity calibration path for blade and free-surface water interaction | Preferred first truth-model workflow because of free-surface focus and good fit for blade/water studies | ADR-2026-04-02-007 |
| Secondary CFD path | OpenFOAM | optional | Secondary truth-model path for blade/water, aero, and atmospheric or calibration studies | Not required for the baseline runtime; may also support richer wind/aero investigations later | ADR-2026-04-02-007 |
| Truth-model usage mode | Offline calibration / comparison / surrogate generation | approved | Keep heavy high-fidelity workflows outside the default runtime loop | High-fidelity tools must remain optional to preserve a lightweight baseline runtime | ADR-2026-04-02-004 |

## Human model strategy

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Baseline human actuation | Prescribed stroke / reduced control model | approved | Initial runtime rower representation | Keeps the baseline simulator tractable and testable while still exercising major architecture seams | ADR-2026-04-02-005 |
| Musculoskeletal / biomechanics stack | OpenSim | deferred | Optional future biomechanics or joint-load analysis path | Introduce only when the project needs muscle/joint-level questions; not part of the baseline runtime | ADR-2026-04-02-007 |

## Data, visualization, and geometry

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Scientific data format | HDF5 | approved | Structured output and persisted scientific data artifacts | Preferred durable format for machine-readable outputs, scenario data, and calibration artifacts | ADR-2026-04-02-007 |
| Post-processing / visualization | ParaView | approved | Primary visualization and analysis workflow | Preferred open-source visualization frontend for scenario review and post-processing | ADR-2026-04-02-007 |
| Geometry / meshing | Gmsh | approved | Geometry and meshing workflow support | Preferred open-source mesh and geometry tool for project assets and future calibration/truth-model workflows | ADR-2026-04-02-007 |

## Supporting languages and tooling

| Area | Choice | Status | Role | Notes | Decision |
|---|---|---|---|---|---|
| Workflow scripting | Python | approved | Trace tooling, automation scripts, data preparation, and report generation | Already used in repo tooling; suitable for glue code and offline utilities, but not the primary runtime language | ADR-2026-03-23-001 |

## Adoption rules

- New core libraries or framework changes must update both
  `docs/ai/DECISIONS.md` and this file.
- Architecture items should reference this file for realization notes rather
  than embedding technology selection directly into `R-*` items.
- Requirements should express observable behavior, constraints, and quality
  properties, not specific third-party library names, unless externally
  mandated.
- High-fidelity truth-model tools must remain optional unless an explicit
  decision record changes the project baseline.
