# HANDOFF.md

## Handoff Timestamp
- 2026-04-19

## What Changed In This Session
- Closed the combined `R-024` / `R-026` guardrail packet.
- Public config now accepts optional `output.truth_model_export_path`, and the
  shared run path emits one deterministic JSON truth-model handoff bundle only
  when that path is configured.
- The default runtime still executes without optional truth-model tooling, and
  runtime re-import remains the existing calibrated-artifact path through
  `steady_wind_calibrated` plus `artifacts.calibration.path`.
- Added `scenarios/performance_budgets.json` for the five protected core
  scenarios plus public budget-loading and evaluation contracts in the
  scenario-harness API.
- Added `./scripts/test_performance.sh` and
  `tools/check_scenario_budgets.py` so protected-scenario runtime budgets are
  reported separately from ordinary functional failures with a machine-readable
  budget report.
- `test.sh` and `verify.sh` now run the dedicated performance lane; the quick
  `test_tdd.sh` lane remains unchanged.

## Current Technical Posture
- The truth-model handoff path is one-way JSON export only in the current
  packet.
- The protected performance-budget lane should remain separate from the quick
  TDD loop.
- Future calibration growth should stay under explicit `A-009` packets.

## Immediate Next Steps
1. Keep the closed truth-model handoff export and performance-budget lane
   stable while later backlog items settle.
2. Keep future calibration growth under explicit `A-009` packets so schema or
   consumer expansion does not erode the closed Slice 4A contract.
3. Treat the deferred `R-027..R-030` cluster as later backlog unless a
   smaller stop-the-line defect preempts it.
