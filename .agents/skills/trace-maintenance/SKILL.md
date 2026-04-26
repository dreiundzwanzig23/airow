---
name: trace-maintenance
description: Maintain repository traceability metadata and evidence links for requirements, architecture, design, and tests. Use when changing `@design`, `@test`, `@verifies`, or `@aux` tags, or when editing process artifacts that affect trace mappings or evidence coverage.
---

# Trace Maintenance

## Modes
- `repair`: fix broken, stale, missing, or lane-inconsistent trace links after a
  code, test, or process-doc change.
- `promotion`: move work from auxiliary/non-evidence status into evidence-bearing
  `UT-*`, `IT-*`, or `QT-*` status.

## Apply Rules
- Keep `@design` IDs in `D-*` form and include `@satisfies [A-*]`.
- Keep `@test` IDs unique within each lane (`UT-*`, `IT-*`, `QT-*`).
- Keep `@verifies` targets lane-consistent:
  - `UT-* -> D-*`
  - `IT-* -> A-*`
  - `QT-* -> R-*`
- Use `@aux yes` only for informational tests that should not satisfy evidence
  gates.
- Do not hand-edit generated `docs/process/TRACEABILITY.md` content.

## Execute
1. Edit metadata near the behavior under test or specification being changed.
2. Regenerate trace artifacts and validate:
   - `python3 tools/tracecheck.py --write`
3. Review the generated trace diff:
   - `git diff -- docs/process/TRACEABILITY.md`
4. Resolve all trace errors before concluding work.
5. Keep `docs/process/TRACEABILITY.md` generated only; do not hand-edit it.

## Required Output
Leave a compact trace note when links changed:

```markdown
## Trace Maintenance

Mode:
- repair | promotion

Changed links:
- R-###:
- A-###:
- D-###:
- UT/IT/QT-###:

Tracecheck:
- command: python3 tools/tracecheck.py --write
- result:

Generated trace diff reviewed:
- yes | no, with reason
```

## Finish
- End with `Trace OK`.
- Leave changed `R-*`, `A-*`, `D-*`, `UT-*`, `IT-*`, `QT-*` items fully linked.
