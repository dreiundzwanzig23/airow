# Examples

This directory contains runnable headless simulator examples for the current
baseline runtime.

## Prerequisites

From the repo root:

```bash
./scripts/setup.sh
./scripts/build.sh
```

## Run An Example

Use the helper script from the repo root:

```bash
./examples/run_example.sh passive_float
./examples/run_example.sh tow_test
./examples/run_example.sh calm_water_stroke
```

Each example writes machine-readable JSON artifacts under:

- `examples/output/passive_float/`
- `examples/output/tow_test/`
- `examples/output/calm_water_stroke/`

The helper script keeps the working directory anchored at the repo root so the
relative output paths in the example configs resolve consistently.

## Run The Config Directly

You can also call the CLI directly from the repo root:

```bash
./build/project_app --config examples/passive_float/config.json
./build/project_app --config examples/tow_test/config.json
./build/project_app --config examples/calm_water_stroke/config.json
```

## Relationship To `scenarios/`

The files under `scenarios/` are validation artifacts for the scenario
harness. They wrap a nested simulator `config` together with scenario metadata
and acceptance envelopes.

The files under `examples/` are plain simulator configs intended for direct CLI
execution.
