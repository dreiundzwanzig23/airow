# AIRow

AIRow is an open-source C++ rowing simulator focused on deterministic,
inspectable single-scull studies. The current runtime is a headless simulator
with reduced hydro, blade, aero, stroke, rower-coupling, calibration, and
reporting surfaces. It is built to say clearly what each run can and cannot
claim.

The active supported scope is a reduced single-scull baseline. Crew boats,
sweep rowing, waves, flexible oars, full 6-DOF validation, interactive
playback, and full-3D water reference workflows are roadmap items rather than
current runtime capability.

## Quick Start

Install dependencies:

```bash
./scripts/setup.sh
```

Build:

```bash
./scripts/build.sh
```

Run a checked-in example:

```bash
./examples/run_example.sh passive_float
./examples/run_example.sh tow_test
./examples/run_example.sh calm_water_stroke
```

Each example writes JSON outputs under `examples/output/<example-name>/`.
See `examples/README.md` for the example catalog and output locations.

## Run A Config Directly

After building, run the CLI from the repository root:

```bash
./build/project_app --config examples/calm_water_stroke/config.json
```

For a compact human-readable summary:

```bash
./build/project_app --config examples/calm_water_stroke/config.json --report compact
```

For an interactive HTML report from emitted JSON artifacts:

```bash
python3 tools/run_analysis.py \
  --summary examples/output/calm_water_stroke/summary.json \
  --time-series examples/output/calm_water_stroke/time_series.json \
  --visualization examples/output/calm_water_stroke/visualization.json \
  --output-dir examples/output/calm_water_stroke/report
```

The CLI also supports ordered batch configs through the same entry point:

```bash
./build/project_app --config /path/to/batch.json
```

## Outputs

Typical runs can emit:

- machine-readable summary JSON;
- machine-readable time-series JSON;
- optional visualization artifact JSON for downstream playback or analysis
  tooling;
- optional HDF5 output when built with HDF5 support;
- optional compact or full terminal reports;
- optional static or interactive analysis reports generated from JSON
  artifacts.

Run summaries include provider metadata, selected mechanics and integration
backends, diagnostics, units or frame annotations for boundary-visible
channels, and trust-envelope metadata. The trust fields are the intended place
to check whether a run supports a study claim.

Visualization artifacts use the `airow.visualization.v1` schema and can be
validated with `python3 tools/validate_visualization_artifact.py <path>`.
When a visualization artifact is provided, `tools/run_analysis.py` also writes
report-control metadata to `metrics.json` and exposes offline playback,
top/side/end projections, frame labels, world-frame and hull-body-frame vector
toggles, disabled unavailable channels, selectable linked plots, plot-click
seeking, and derived event markers for peaks, zero crossings, stroke
boundaries, and trust warnings.

## Current Capabilities

The current reduced runtime supports deterministic studies around:

- single-scull configuration loading and validation;
- headless single-run and batch execution;
- passive float, tow, calm-water stroke, headwind, crosswind, and gust-headwind
  scenario evidence;
- reduced hull-water resistance and hydrostatic restoring loads;
- reduced blade-water propulsion with slip, work, and efficiency reporting;
- reduced steady apparent-wind aero with headwind and crosswind behavior;
- prescribed, force-driven, and power-driven low-order stroke actuation;
- optional low-order rower center-of-mass coupling;
- file-backed calibration, measurement-bundle, and measured-trial imports;
- capability and trust reporting in machine-readable and human-readable output.

Capability status and limitations are summarized in
`docs/process/CAPABILITY_MATRIX.md`.

## Configuration Notes

The default scope is `single_scull`. A config may make that explicit:

```json
{
  "boat_class": "single_scull"
}
```

Unsupported boat classes are rejected before runtime startup.

The preferred standard runtime is:

```json
{
  "simulation": {
    "mechanics_backend": "chrono_rigidbody",
    "integration_backend": "sundials_ida"
  }
}
```

Supported fallback modes are documented in the process and architecture docs.
Checked-in examples show the normal config shape for direct simulator use.

## Development Entry Points

For users who want to inspect or contribute to the project:

- product requirements: `docs/process/REQUIREMENTS.md`;
- architecture: `docs/process/ARCHITECTURE.md`;
- architecture allocation policy: `docs/process/ARCHITECTURE_POLICY.md`;
- full-simulation roadmap: `docs/process/ROADMAP_FULL_SIMULATION.md`;
- approved technologies: `docs/process/TECHNOLOGY_STACK.md`;
- visualization artifact schema: `docs/process/VISUALIZATION_ARTIFACT.md`;
- state and frame conventions: `docs/process/STATE_CONVENTIONS.md`;
- workflow and test strategy: `docs/process/WORKFLOW.md` and
  `docs/process/TEST_STRATEGY.md`;
- cross-cutting change playbook: `.agents/skills/major-change-loop/SKILL.md`;
- durable decisions: `docs/ai/DECISIONS.md`.

Useful local validation commands:

```bash
./scripts/test_tdd.sh
./scripts/test.sh
./scripts/verify.sh
```

Required completion gates for repository changes are defined in `AGENTS.md`.
