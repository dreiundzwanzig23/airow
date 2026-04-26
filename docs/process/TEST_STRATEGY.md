# TEST_STRATEGY.md - Rowing Simulator Test Strategy

## Purpose
Define verification-layer intent. Detailed lane choice and execution belongs in
`.agents/skills/test-lanes/SKILL.md`.

## Test Layers
| Layer | Verifies | Main use |
|---|---|---|
| Unit (`tests/unit/`) | `UT-* -> D-*` | Local logic, invariants, deterministic helpers |
| Integration (`tests/integration/`) | `IT-* -> A-*` | Subsystem contracts, provider wiring, preserved seams |
| System (`tests/system/`) | `QT-* -> R-*` | End-to-end acceptance and named scenario behavior |
| Auxiliary (`@aux yes`) | Informational | Tooling, script, and header contracts outside evidence gates |

## Design Rules
- Keep tests deterministic, bounded, and behavior-focused.
- Keep `@test` IDs unique and `@verifies` targets lane-consistent.
- Use explicit tolerances when comparing numerics.
- Avoid implementation backdoors such as private-public hacks,
  `FRIEND_TEST`, implementation-file includes, wall-clock reads, or sleep
  timing.
- Add characterization coverage before invasive refactors or major-change work,
  using the narrowest layer that protects the preserved behavior.
- Preserve red/green/refactor evidence for functional work as required by
  `AGENTS.md`.

## Baseline Scenarios
Named requirement-level baselines currently center on:
- passive float;
- tow test;
- calm-water stroke;
- headwind stroke;
- crosswind stroke;
- gust-headwind stroke;
- protected performance budgets;
- shared-baseline technique comparison.

Current artifacts and acceptance envelopes are listed in
`docs/process/SCENARIOS.md`.

## Gates And Lanes
- Fast iteration: `./scripts/test_tdd.sh`
- Standard completion testing: `./scripts/test.sh`
- Aggregate pre-merge confidence: `./scripts/verify.sh`
- Trace validation/regeneration: `python3 tools/tracecheck.py --write`

For specialized lanes such as auxiliary tooling, performance budgets,
sanitizers, GCC portability, regression, and test-quality linting, use
`.agents/skills/test-lanes/SKILL.md`.
