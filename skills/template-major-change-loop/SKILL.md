---
name: template-major-change-loop
description: Execute the repository's required workflow for cross-cutting, architectural, migratory, or semantic multi-requirement changes. Use when a change is too broad for the ordinary feature TDD loop and needs explicit architecture and preservation planning.
---

# Template Major-Change Loop

## Classify Change
- Use this workflow for:
  - architectural refactors,
  - migrations or deprecations,
  - semantic multi-requirement changes,
  - cross-cutting changes touching multiple subsystems.

## Run Workflow
1. Classify the change and name the affected subsystem seams.
2. Build an impact map across `R-*`, `A-*`, `D-*`, tests, and touched files.
3. Record the architecture delta before implementation begins.
4. Add characterization tests for preserved behavior before invasive edits.
5. Implement seam-first when the change can be split into transitional steps.
6. Remove transitional code explicitly once the replacement path is stable.
7. Run a drift review after the change to catch documentation or ownership gaps.

## Produce Outputs
- Leave a reviewable architecture delta, not just a green patch.
- Preserve or improve subsystem boundaries as part of the change.
- Update traceability, AI context, and process docs when the change affects them.
