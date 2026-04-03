# QUALITY_SCORECARD.md

This scorecard tracks the structural baseline of the process model. Bootstrap
`900`-series IDs are excluded from the architecture metrics below.

| Metric | Current Baseline | Notes |
|--------|------------------|-------|
| Non-bootstrap requirements (`R-*`) | 33 | `R-001..R-033` define the simulator backlog. |
| Non-bootstrap architecture items (`A-*`) | 10 | Seeded subsystem ownership surface in `A-001..A-010`. |
| Non-bootstrap design items (`D-*`) | 9 | `D-001..D-009` now cover the first simulator-facing configuration boundary and its deterministic validation helpers. |
| Non-bootstrap evidence tests (`UT/IT/QT-*`) | 11 | `UT-001..UT-007`, `IT-001..IT-002`, and `QT-001..QT-002` provide the first simulator evidence. |
| One-to-one `R -> A` ratio | 0.45 | 15 of 33 simulator requirements currently allocate to exactly one subsystem owner. |
| Average requirements per non-bootstrap `A-*` | 7.20 | Initial seeded architecture is intentionally broad to avoid requirement mirroring. |
| Non-bootstrap `A-*` with exactly one `D-*` | 0 | `A-001` now owns multiple concrete design items while the rest of the simulator architecture remains planned. |
| Non-bootstrap `A-*` with empty `Future Absorption` | 0 | Every seeded subsystem states intended growth direction. |
| Traceability guardrails | Green when `tracecheck.py --write` passes | Generated trace report and architecture warnings are the source of truth. |
| Dependency guardrails | Green when `depcheck.sh` passes | Component rules are active for the configuration component and must track self-dependencies correctly. |
