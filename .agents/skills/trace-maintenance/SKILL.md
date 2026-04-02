---
name: trace-maintenance
description: Maintain repository traceability metadata and evidence links for requirements, architecture, design, and tests. Use when changing `@design`, `@test`, `@verifies`, or `@aux` tags, or when editing process artifacts that affect trace mappings or evidence coverage.
---

# Trace Maintenance

## Apply Rules
- Keep `@design` IDs in `D-*` form and include `@satisfies [A-*]`.
- Keep `@test` IDs unique within each lane (`UT-*`, `IT-*`, `QT-*`).
- Keep `@verifies` targets lane-consistent:
  - `UT-* -> D-*`
  - `IT-* -> A-*`
  - `QT-* -> R-*`
- Use `@aux yes` only for informational tests that should not satisfy evidence
  gates.

## Execute
1. Edit metadata near the behavior under test or specification being changed.
2. Regenerate trace artifacts and validate:
   - `python3 tools/tracecheck.py --write`
3. Resolve all trace errors before concluding work.
4. Keep `docs/process/TRACEABILITY.md` generated only; do not hand-edit it.

## Finish
- End with `Trace OK`.
- Leave changed `R-*`, `A-*`, `D-*`, `UT-*`, `IT-*`, `QT-*` items fully linked.
