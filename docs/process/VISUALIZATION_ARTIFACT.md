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
- `channels`: available transform/vector channels and explicit unavailable
  channels such as current gate loads.
- `samples`: time-indexed transforms, vectors, and scalar values sampled with
  the same policy as time-series JSON.

Validate an artifact with:

```bash
python3 tools/validate_visualization_artifact.py path/to/visualization.json
```

Generate an offline interactive inspection report with synchronized 2D
projections, vector overlays, playback controls, and linked plot cursors:

```bash
python3 tools/run_analysis.py \
  --summary examples/output/calm_water_stroke/summary.json \
  --time-series examples/output/calm_water_stroke/time_series.json \
  --visualization examples/output/calm_water_stroke/visualization.json \
  --output-dir examples/output/calm_water_stroke/report
```

## Simulation Evidence Design

Physical or interpretive claim:
- Users can inspect a reduced-runtime run's state evolution, frame directions,
  selected world-frame vectors, and time-series relationships without changing
  the numerical result.

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
  unavailable channels remain labeled unavailable, and linked plots share the
  same sample index as the playback controls.

Required outputs:
- time-series channels: existing emitted records plus visualization samples.
- units: inherited from scalar and vector channel metadata.
- frames: world, hull-body, waterline, and port/starboard conventions.
- visualization vectors: hull hydro, blade, aero, wind, and rower inertial
  vectors when present.
- diagnostics: invalid visualization artifacts fail report generation with a
  deterministic error.

Tests:
- UT: existing output helper coverage for artifact/time-series shape.
- IT: existing output integration coverage for visualization metadata and
  sample alignment.
- QT: `QT-049`, `QT-050`, and `QT-051`.

Failure modes:
- rejected: unsupported or malformed visualization schema.
- low-confidence: trust-envelope labels remain visible in the report.
- visually inspectable: unavailable channels are listed rather than drawn as
  implied zero or supported vectors.
