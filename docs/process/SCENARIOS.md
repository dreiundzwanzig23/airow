# SCENARIOS.md - Reference Scenario Baselines

This file indexes checked-in scenario artifacts. Scenario JSON files are the
source of truth for run configuration and acceptance envelopes.

## Scenario Catalog
| Scenario | Artifact | Purpose |
|---|---|---|
| Passive float | `scenarios/passive_float.json` | Hydrostatic initialization and calm-water equilibrium |
| Tow test | `scenarios/tow_test.json` | Reduced hull-water resistance behavior |
| Calm-water stroke | `scenarios/calm_water_stroke.json` | Self-propelled reduced baseline propulsion |
| Headwind stroke | `scenarios/headwind_stroke.json` | Apparent-wind and aero-load interaction |
| Crosswind stroke | `scenarios/crosswind_stroke.json` | Asymmetric environmental loading behavior |
| Gust headwind stroke | `scenarios/gust_headwind_stroke.json` | Time-varying wind replay/profile behavior |
| Technique comparison | `scenarios/technique_comparison_actuation_modes.json` | Shared-baseline actuation-mode comparison |
| Performance budgets | `scenarios/performance_budgets.json` | Protected scenario runtime budget manifest |

## Usage
- Scenario artifacts are consumed by the scenario harness and validation lanes.
- Direct user examples live under `examples/`; see `examples/README.md`.
- Performance budgets are evaluated by `./scripts/test_performance.sh`.
- Broader lane-selection guidance lives in `.agents/skills/test-lanes/SKILL.md`.

## Maintenance Rules
- Keep scenario names stable once referenced by evidence.
- Keep acceptance envelopes deterministic and bounded.
- Do not duplicate detailed JSON fields here; update the scenario artifact
  itself.
- Preserve the current reduced-runtime baseline unless a selected requirement
  explicitly changes runtime physics.
