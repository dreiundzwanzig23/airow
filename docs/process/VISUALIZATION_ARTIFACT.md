# Visualization Artifact

The optional visualization artifact is a JSON output for downstream playback
or analysis tooling. Configure it with:

```json
{
  "output": {
    "visualization_path": "examples/output/calm_water_stroke/visualization.json"
  }
}
```

The current schema id is `airow.visualization.v1`. It is additive: enabling the
artifact must not change the numerical result of a run.

## Contents

- `metadata`: config id, simulator version, provider metadata, backend
  metadata, trust envelope, and external artifact provenance.
- `frames`: world, hull-body, waterline, and port/starboard conventions.
- `channels`: available transform/vector channels, including world-frame and
  hull-body-frame variants for emitted vectors, plus explicit unavailable
  channels such as current gate loads.
- `samples`: time-indexed transforms, vectors, and scalar values sampled with
  the same policy as time-series JSON.

Validate an artifact with:

```bash
python3 tools/validate_visualization_artifact.py path/to/visualization.json
```

Generate an offline interactive inspection report with synchronized 2D
projections, vector overlays, playback controls, linked plot cursors, plot
click-to-seek hooks, derived event markers, a physics capability/trust entry
section, optional ParaView/VTK export, and report-control metadata in
`metrics.json`:

```bash
python3 tools/run_analysis.py \
  --summary examples/output/calm_water_stroke/summary.json \
  --time-series examples/output/calm_water_stroke/time_series.json \
  --visualization examples/output/calm_water_stroke/visualization.json \
  --output-dir examples/output/calm_water_stroke/report
```

Export a validated visualization artifact directly to a reduced ParaView/VTK
bundle. The bundle includes a minimal `paraview_loading_guide.md` alongside the
VTK files and metadata sidecar:

```bash
python3 tools/export_visualization_vtk.py \
  --visualization examples/output/calm_water_stroke/visualization.json \
  --output-dir examples/output/calm_water_stroke/paraview
```

Generate an offline comparison report from two or more already-emitted runs:

```bash
python3 tools/compare_runs.py \
  --manifest path/to/run_comparison_manifest.json \
  --output-dir path/to/comparison_report
```

The comparison manifest schema id is `airow.run_comparison.v1`:

```json
{
  "schema_id": "airow.run_comparison.v1",
  "comparison_id": "baseline-vs-variant",
  "alignment": {
    "mode": "time",
    "start_time_s": 0.0,
    "end_time_s": 10.0
  },
  "runs": [
    {
      "id": "baseline",
      "label": "Baseline",
      "role": "baseline",
      "summary_path": "baseline/summary.json",
      "time_series_path": "baseline/time_series.json",
      "visualization_path": "baseline/visualization.json"
    },
    {
      "id": "variant",
      "label": "Changed technique",
      "role": "variant",
      "summary_path": "variant/summary.json",
      "time_series_path": "variant/time_series.json",
      "visualization_path": "variant/visualization.json"
    }
  ]
}
```

Supported alignment modes are `time` and `stroke_phase`. Time alignment uses
emitted `time_s` samples inside the declared window. Stroke-phase alignment
uses emitted `stroke_input.phase` labels inside the same window and rejects
artifacts that omit those labels.

The output bundle schema id is `airow.run_comparison_report.v1`. It writes
`metrics.json`, `index.html`, `boat_speed.svg`, `blade_loads.svg`, and
`energy_power.svg`. `metrics.json` records run metadata, alignment metadata,
headline deltas, provider/backend/calibration/config differences,
software/calibration/physical comparability flags with deterministic false
reasons, shared channel coverage, visualization vector metadata, and
unavailable-channel reasons. This reduced comparison bundle is an offline
artifact inspection workflow; it does not add runtime physics, change solver
behavior, or claim full 3D comparison playback.

## Simulation Evidence Design

Physical or interpretive claim:
- Users can inspect a reduced-runtime run's state evolution, frame directions,
  selected world-frame and hull-body-frame vectors, report-side event markers,
  ParaView/VTK-compatible reduced export files and loading guidance, plus
  time-series relationships without changing the numerical result.

Fidelity level:
- reduced visualization of the current reduced runtime.

Validity range:
- speed: inherited from the active provider trust envelope.
- stroke rate: inherited from the emitted scenario/configuration.
- wind/current/wave assumptions: visualized only when emitted by current
  providers; waves/current remain unavailable unless future providers add them.
- geometry assumptions: single-scull reduced hull/oar/blade reference geometry
  from the artifact, not imported production surfaces.
- solver/timestep assumptions: sample policy matches the emitted time-series
  artifact.

Oracle:
- visual invariant and deterministic artifact validation.
- details: malformed artifacts are rejected, vectors retain units and frames,
  unavailable channels remain labeled unavailable and disabled, hull-body
  vector controls are derived only from emitted body-frame channels, linked
  plots share the same sample index as the playback controls, and event markers
  plus VTK files are derived from existing emitted samples or trust metadata
  rather than new runtime state.

Required outputs:
- time-series channels: existing emitted records plus visualization samples and
  derived report `plot_channels` metadata.
- units: inherited from scalar and vector channel metadata.
- frames: world, hull-body, waterline, and port/starboard conventions.
- visualization vectors: world-frame and hull-body-frame hull hydro, blade,
  aero, wind, moment, and rower inertial vectors when present.
- diagnostics: optional ParaView export writes deterministic
  `airow_geometry.vtk`, `airow_vectors.vtk`, `airow_metadata.json`, and
  `paraview_loading_guide.md` files.
- diagnostics: `metrics.json.physics_capability_and_trust` mirrors the
  report-entry explanation with trust-envelope, provider-capability, and
  capability-matrix metadata.
- diagnostics: `metrics.json.interactive_controls.event_markers` lists peak,
  zero-crossing, stroke-boundary, and trust-warning markers with sample index,
  time, channel, unit, frame, and source metadata.
- diagnostics: invalid visualization artifacts fail report generation with a
  deterministic error.
- diagnostics: comparison reports emit `airow.run_comparison_report.v1`
  metrics, deterministic SVG plots, comparability flags, and explicit
  unavailable-channel reasons from already-emitted artifacts.

Tests:
- UT: `UT-382` plus existing output helper coverage for artifact/time-series
  shape.
- IT: existing output integration coverage for visualization metadata and
  sample alignment.
- QT: `QT-049`, `QT-050`, `QT-051`, `QT-052`, `QT-053`, `QT-054`, `QT-055`,
  `QT-056`, `QT-057`, `QT-059`, and `QT-060`.

Failure modes:
- rejected: unsupported or malformed visualization schema.
- rejected: malformed comparison manifest, unsupported comparison schema,
  missing artifacts, empty records, invalid comparison windows, duplicate run
  ids, unsupported alignment modes, or missing stroke-phase labels.
- low-confidence: trust-envelope labels remain visible in the report.
- visually inspectable: unavailable channels are listed rather than drawn as
  implied zero or supported vectors.
