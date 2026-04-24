# airow Full-Simulation Roadmap

This roadmap is rebased for the current `main` branch, where `docs/process/REQUIREMENTS.md` already contains `R-001` through `R-049`.

The current requirements already cover the first post-`v0.1` fidelity layer: fidelity trust envelopes, measurement and trial imports, boat/rigging/mass parameterization, force/power actuation hooks, reduced rower coupling, blade slip and propulsion metrics, blade-entry/extraction behavior, calibrated reduced blade/hull providers, technique and hull-sensitivity comparison scenarios, measured-vs-sim agreement, truth-model round-trip consistency, and uncertainty/validity reporting.

The missing dream-goal layer is mostly about **making the simulator visually understandable** and then deepening physics in a validation-safe way. This roadmap therefore treats observability as a prerequisite for realism work.

Related requirements:

- Current main: `R-035` through `R-049`.
- Full-simulation extension: `R-050` through `R-071`.

---

## 1. Target outcome

The long-term goal is a rowing simulation platform that supports credible reasoning about:

- boat speed;
- stroke technique;
- blade-water interaction;
- hull behavior;
- rigging and mass distribution;
- energy transfer;
- balance and 6-DOF dynamics;
- validation against measured data;
- calibrated reduced models;
- and optional full-3D water reference studies.

The simulator should be both:

1. **physically honest** — every result exposes what is modeled, reduced, calibrated, validated, placeholder, disabled, or unavailable; and
2. **easy to understand** — users can inspect the simulated state, directions, frames, loads, energy flow, comparison deltas, and failure modes visually.

A pretty boat animation must never imply validated physics by itself. Visual clarity and physical trust need to travel together.

---

## 2. Strategy

### 2.1 Preserve the current reduced baseline

The existing deterministic reduced single-scull baseline remains the anchor. New visual artifacts, reports, providers, and comparison tools should not change baseline numerical behavior unless a scoped physics requirement explicitly does so.

Relevant current requirements: `R-004`, `R-015`, `R-018`, `R-024`, `R-034`, `R-035`, `R-049`.

### 2.2 Build observability before deeper physics

A more sophisticated hydrodynamic model is much less useful if nobody can see its force directions, validity status, or energy consequences. The first roadmap phases therefore build artifact contracts, replay, overlays, linked plots, debug bundles, and analysis workflows.

Relevant extension requirements: `R-050` through `R-056`, `R-062`, `R-070`, `R-071`.

### 2.3 Use calibrated reduced models for normal runtime

The default runtime should stay local, deterministic, and reasonably fast. Full-3D water simulation should be an optional reference workflow for selected cases, calibration, and validation rather than the everyday inner loop.

Relevant requirements: `R-043`, `R-044`, `R-048`, `R-065`, `R-066`, `R-068`.

### 2.4 Validation is a product feature

Realism is not a claim the project grants itself. It is a measured property over a stated validity range, with explicit missing channels and uncertainty.

Relevant requirements: `R-035`, `R-047`, `R-049`, `R-067`, `R-068`, `R-069`.

---

## 3. Roadmap overview

| Phase | Theme | Primary outcome | Requirements |
|---|---|---|---|
| 0 | Rebase and requirement hygiene | Avoid duplicate `R-035`-series requirements and preserve current main | `R-035`..`R-049`, extension `R-050`..`R-071` |
| 1 | Trust and capability visibility | Every run/report/viewer says what can and cannot be claimed | `R-035`, `R-049`, `R-071` |
| 2 | Visualization artifact foundation | Runs emit stable visualization-ready and comparison-ready artifacts | `R-050`, `R-052`, `R-062`, `R-070` |
| 3 | Visual inspection tooling | Users replay, scrub, plot, compare, export, and debug runs | `R-051`, `R-053`, `R-054`, `R-055`, `R-056` |
| 4 | Geometry foundation | Hull, blade, oar, rigging, and surfaces become first-class assets | `R-038`, `R-057` |
| 5 | Coupled 6-DOF mechanics | Hull, rower, oars, gates, loads, and energy become physically coupled | `R-039`, `R-040`, `R-058`, `R-060`, `R-061`, `R-062` |
| 6 | Realistic blade-water model | Blade loads become 3D, phase-aware, inspectable, and calibratable | `R-041`, `R-042`, `R-043`, `R-059` |
| 7 | Validation and calibration loop | Measured/truth data drive scorecards, calibrated providers, and surrogate lifecycle | `R-036`, `R-037`, `R-047`, `R-049`, `R-067`, `R-068` |
| 8 | Optimization-safe studies | Technique and hull optimization are comparable, replayable, and validity-aware | `R-045`, `R-046`, `R-054`, `R-069` |
| 9 | Environment and secondary realism | Waves, current, and flexible-oar readiness enter without breaking the baseline | `R-063`, `R-064` |
| 10 | Full-3D water reference workflow | Optional CFD/SPH/full-3D water studies feed validation and reduced models | `R-048`, `R-065`, `R-066` |

---

## 4. Phase 0 — Rebase and requirement hygiene

### Purpose

The earlier dream-goal requirements draft used `R-035` through `R-066`. Current `main` now already owns `R-035` through `R-049`, so the old patch must not be applied as-is.

### Deliverables

1. Keep current `R-035` through `R-049` unchanged.
2. Introduce the full-simulation extension as `R-050` through `R-071`.
3. Keep visual/physics dream-goal requirements separated from already-merged measurement, calibration, comparison, and uncertainty requirements.
4. When these requirements are eventually appended into `docs/process/REQUIREMENTS.md`, keep them `Status: OPEN` until architecture and evidence are allocated.

### Acceptance gate

There are no duplicate requirement IDs and no obsolete instruction saying to paste a new `R-035` block after `R-034`.

### Suggested PR sequence

1. `requirements-rebase`: add the rebased full-simulation extension starting at `R-050`.
2. `roadmap-rebase`: add this roadmap with cross-references to current `R-035` through `R-049`.
3. `process-note`: optionally add a short note to `REQUIREMENTS.md` pointing to the extension document once the team is ready.

---

## 5. Phase 1 — Trust and capability visibility

### Purpose

Before richer physics or prettier views, airow needs a public and machine-readable way to explain what each run is allowed to claim.

### Deliverables

1. Run-level fidelity/trust metadata that aligns with `R-035` and `R-049`.
2. Provider capability declarations for hull, blade, aero, rower, environment, numerics, visualization, calibration, and truth-model workflows.
3. A capability matrix document or report section as required by `R-071`.
4. Human-readable explanation panels in reports and viewer entry pages.
5. Guardrails preventing placeholder/reduced output from being presented as validated physics.

### Acceptance gate

A default run must clearly report:

- active providers;
- active physical effects;
- reduced or placeholder effects;
- inactive or unavailable effects;
- selected backend and calibration artifacts;
- validation/trust status;
- and whether conclusions are physically supported.

### Suggested PR sequence

1. `provider-capability-metadata`.
2. `run-trust-report`.
3. `capability-matrix-doc`.
4. `human-readable-physics-explanations`.

---

## 6. Phase 2 — Visualization artifact foundation

### Purpose

Create the artifact contract that makes simulation output understandable outside the C++ runtime.

### Deliverables

1. A versioned visualization artifact schema.
2. Time-indexed transforms for hull, oars, blades, seat, rower reference masses, waterline, and coordinate frames.
3. Time-indexed vectors for loads, moments, velocities, accelerations, wind/current/environment, and total force/moment.
4. A channel registry with units, frames, sign conventions, provenance, support state, and validity status.
5. First-pass energy and power channels.
6. An analysis-first command that produces all core artifacts from one scenario run.

### Acceptance gate

One documented command runs a reference scenario and emits:

- summary output;
- time-series output;
- human-readable report;
- visualization artifact;
- energy/power summary;
- and clear validity/capability warnings.

### Suggested PR sequence

1. `visualization-schema-v1`.
2. `transform-streams`.
3. `vector-streams`.
4. `channel-registry`.
5. `energy-accounting-v1`.
6. `analysis-first-cli`.

---

## 7. Phase 3 — Visual inspection tooling

### Purpose

Make completed runs inspectable by a human, not just parsable by scripts.

### Deliverables

1. Minimal offline 3D replay viewer.
2. Toggleable axes, labels, frames, loads, moments, velocities, and waterline overlays.
3. Linked time-series plots with a shared simulation-time cursor.
4. Run-to-run comparison view with time/stroke-phase alignment.
5. Failure replay/debug bundle support.
6. Optional ParaView/VTK export and loading guide.

### Acceptance gate

A user can open a reference run, scrub through a stroke, see the boat direction and frames, inspect blade/hull/aero loads, jump from a plot peak to the matching 3D state, and export to ParaView without optional truth-model dependencies.

### Suggested PR sequence

1. `minimal-run-viewer`.
2. `frame-vector-overlays`.
3. `linked-plots`.
4. `comparison-view-v1`.
5. `debug-replay-bundles`.
6. `vtk-paraview-export`.

---

## 8. Phase 4 — Geometry foundation

### Purpose

Move from parameter-only abstractions toward geometry-aware hull, blade, oar, and rigging assets.

### Deliverables

1. Geometry asset schema for hull, blades, oars, riggers/gates, seat/rail references, and rower mass/reference markers.
2. Provenance, units, coordinate frame, version, and validation metadata.
3. Hull asset fields for displacement, waterline, wetted surface, hydrostatic coefficients, and visualization surface.
4. Blade asset fields for area, orientation, immersion region, center-of-pressure assumptions, and visualization surface.
5. Deterministic rejection of invalid or incompatible geometry.

### Acceptance gate

A reference scenario can load named geometry assets and the resulting report/viewer can show which geometry was used, whether it is valid for the selected physics, and which geometry-derived quantities affected the run.

### Suggested PR sequence

1. `geometry-schema-v1`.
2. `hull-asset-fixture`.
3. `blade-oar-rigging-fixtures`.
4. `geometry-validation`.
5. `geometry-linked-visualization`.

---

## 9. Phase 5 — Coupled 6-DOF mechanics

### Purpose

Give the hull, rower, oars, gates, and loads a physically meaningful coupling path.

### Deliverables

1. Geometry-aware 6-DOF hull hydrostatics/hydrodynamics provider contract.
2. Sway, heave, roll, pitch, yaw, and surge force/moment channels when supported.
3. Coupled force/power actuation that lets hull acceleration feed back into rower/oar/blade dynamics.
4. Oarlock/gate, handle, seat, rail, and foot-stretcher load channels.
5. Strong energy/power accounting and residual reporting.
6. Visual overlays for all supported forces and moments.

### Acceptance gate

A bounded coupled reference scenario remains finite and deterministic, reports load and energy channels, and shows a physically explainable difference between prescribed kinematics and coupled force/power actuation.

### Suggested PR sequence

1. `hull-6dof-provider-contract`.
2. `six-dof-load-channels`.
3. `coupled-actuation-v1`.
4. `interface-load-channels`.
5. `energy-residual-tests`.
6. `coupled-visualization-overlays`.

---

## 10. Phase 6 — Realistic blade-water model

### Purpose

Make blade-water interaction sufficiently rich for serious technique studies.

### Deliverables

1. 3D blade force and moment model using blade pose, immersion, local water velocity, boat/oar motion, and blade geometry.
2. Explicit support flags for lift, drag, added mass, free-surface correction, ventilation, wake memory, and empirical coefficients.
3. Phase-aware behavior for dry recovery, catch, partial immersion, drive, extraction, and slip-direction changes.
4. Center-of-pressure or effective force-application diagnostics.
5. Calibration hook for blade provider artifacts.
6. Visual display of blade load vectors and immersion state.

### Acceptance gate

Changing catch timing, blade depth, or blade orientation changes blade force histories and energy channels in a way that the linked 3D/plot viewer can explain.

### Suggested PR sequence

1. `blade-geometry-consumption`.
2. `blade-3d-load-contract`.
3. `phase-aware-blade-diagnostics`.
4. `blade-force-application-point`.
5. `calibrated-blade-provider-v1`.
6. `blade-visual-diagnostics`.

---

## 11. Phase 7 — Validation and calibration loop

### Purpose

Turn realism into something measured, not merely asserted.

### Deliverables

1. Measured dataset and measured trial ingestion from current `R-036` and `R-037`.
2. Agreement metrics from current `R-047`.
3. Validation scorecards from `R-067`.
4. Calibration/surrogate lifecycle artifacts from `R-068`.
5. Report and viewer integration so validation status is visible before conclusions.
6. Out-of-validity warnings that propagate through comparisons and optimization outputs.

### Acceptance gate

A validation fixture can state: “this run is inside/outside/insufficient-evidence for this study question,” with visible measured-vs-sim or reference-vs-sim metrics and missing-channel explanations.

### Suggested PR sequence

1. `measured-trial-loader-v1`.
2. `agreement-metrics-v1`.
3. `validation-scorecard-v1`.
4. `calibration-artifact-schema-v2`.
5. `scorecard-report-integration`.
6. `scorecard-viewer-integration`.

---

## 12. Phase 8 — Optimization-safe studies

### Purpose

Support hull and technique exploration without creating fake precision.

### Deliverables

1. Sweep/batch output that carries case deltas, validity, calibration, provider, backend, and comparability flags.
2. Ranking outputs that clearly mark invalid or non-comparable cases.
3. Replayable artifacts for selected cases.
4. Sensitivity summaries that separate numerical, provider, calibration, geometry, and technique variation.
5. Examples for technique timing and hull/rigging sensitivity.

### Acceptance gate

A small optimization-style fixture ranks multiple cases but refuses to present an out-of-validity fast case as a trustworthy physical optimum.

### Suggested PR sequence

1. `batch-validity-metadata`.
2. `comparison-and-ranking-schema`.
3. `selected-case-replay-artifacts`.
4. `technique-study-fixture`.
5. `hull-sensitivity-fixture`.

---

## 13. Phase 9 — Environment and secondary realism

### Purpose

Add waves, current, and flexible-oar readiness without destabilizing the baseline.

### Deliverables

1. Calm water, steady current, and deterministic disturbance contracts.
2. Water/environment channels delivered to providers.
3. Non-calm-water scenario fixture.
4. Flexible-oar mode boundary and rigid/flexible support flags.
5. Optional flexible-oar visualization channels.

### Acceptance gate

A non-calm-water scenario remains deterministic and produces visible environmental inputs, while existing calm-water baseline scenarios remain unchanged when the new features are disabled.

### Suggested PR sequence

1. `water-environment-contract`.
2. `current-and-disturbance-fixture`.
3. `provider-environment-inputs`.
4. `flexible-oar-mode-boundary`.
5. `environment-and-flex-visuals`.

---

## 14. Phase 10 — Full-3D water reference workflow

### Purpose

Create an optional path from airow runtime scenarios to high-fidelity water studies and back into validation/calibration.

### Deliverables

1. Study definition exports for hull-water, blade-water, and short coupled windows.
2. Stable identifiers for geometry, frames, boundary conditions, time windows, and comparison channels.
3. Adapter-friendly schema for OpenFOAM, SPH, or other high-fidelity tools.
4. Truth-model result import with solver/tool metadata and resolution information.
5. Reference-vs-reduced comparison reports and visualization overlays.
6. Calibration/surrogate generation workflow that consumes truth-model results.

### Acceptance gate

The repository can perform a dry-run truth-model export and import a fixture result, then compare that fixture against a reduced runtime run without requiring the optional high-fidelity solver to be installed.

### Suggested PR sequence

1. `truth-study-definition-v1`.
2. `truth-export-fixtures`.
3. `truth-result-schema-v1`.
4. `truth-result-importer`.
5. `truth-vs-runtime-comparison`.
6. `truth-visualization-overlays`.
7. `surrogate-generation-handoff`.

---

## 15. Recommended first implementation slice

The first slice should maximize clarity without changing physics behavior.

Recommended order:

1. `R-071` capability matrix and explanation language.
2. `R-050` visualization artifact schema v1.
3. `R-052` frame/vector channel registry.
4. `R-070` analysis-first CLI workflow.
5. `R-062` energy/power accounting v1.
6. `R-056` minimal VTK/ParaView export.
7. `R-051` minimal 3D replay fixture.
8. `R-055` failure replay fixture.

This gives the project an inspection layer before the next hydrodynamic upgrade. The result is less “mystery JSON river” and more “physics lab with labels on the equipment.”

---

## 16. What not to do next

Avoid these traps:

- Do not apply the older `R-035` through `R-066` requirements patch to current `main`.
- Do not build a polished viewer that hides fidelity/validity warnings.
- Do not start hull or technique optimization before energy accounting and validity/comparability flags exist.
- Do not make OpenFOAM/SPH/full-3D water tooling a hidden dependency of the default runtime.
- Do not add high-fidelity-looking geometry without labeling the physics model behind it.
- Do not call a calibrated model validated unless measured/reference agreement has actually been scored.

---

## 17. Apply guidance

For the current merged `main`, apply the rebased combined patch that adds:

```text
docs/process/REQUIREMENTS_FULL_SIMULATION_EXTENSION.md
docs/process/ROADMAP_FULL_SIMULATION.md
```

Then review whether you want the extension requirements copied into `docs/process/REQUIREMENTS.md`. If you do copy them into `REQUIREMENTS.md`, keep them after current `R-049` so the requirement numbering remains continuous.
