# QUALITY_SCORECARD.md

This scorecard tracks the structural baseline of the process model. Bootstrap
`900`-series IDs are excluded from the architecture metrics below.

| Metric | Current Baseline | Notes |
|--------|------------------|-------|
| Non-bootstrap requirements (`R-*`) | 30 | `R-001..R-030` define the simulator backlog. |
| Non-bootstrap architecture items (`A-*`) | 9 | Seeded subsystem ownership surface in `A-001..A-009`. |
| Non-bootstrap design items (`D-*`) | 0 | No simulator implementation has landed yet; `D-900` is bootstrap-only. |
| Non-bootstrap evidence tests (`UT/IT/QT-*`) | 0 | Current compiled tests are bootstrap-only `900`-series evidence. |
| One-to-one `R -> A` ratio | 0.50 | 15 of 30 simulator requirements currently allocate to exactly one subsystem owner. |
| Average requirements per non-bootstrap `A-*` | 6.56 | Initial seeded architecture is intentionally broad to avoid requirement mirroring. |
| Non-bootstrap `A-*` with exactly one `D-*` | 0 | No simulator code owns a design surface yet. |
| Non-bootstrap `A-*` with empty `Future Absorption` | 0 | Every seeded subsystem states intended growth direction. |
| Traceability guardrails | Green when `tracecheck.py --write` passes | Generated trace report and architecture warnings are the source of truth. |
| Dependency guardrails | Green when `depcheck.sh` passes | Component rules are defined and activate as component-prefixed files are introduced. |
