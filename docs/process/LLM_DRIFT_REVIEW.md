# LLM_DRIFT_REVIEW.md

Use this prompt/checklist for lightweight semantic-drift reviews after
requirement wording changes.

## Prompt
Review this repository for requirement-to-implementation drift with focus on:
- contradictions inside `docs/process/REQUIREMENTS.md`,
- outdated or replaced terminology across requirements, architecture, tests,
  README, and process docs,
- architecture-allocation drift against `docs/process/ARCHITECTURE_POLICY.md`,
- architecture-health follow-ups captured in `docs/process/ARCHITECTURE_HEALTH.md`,
- `DONE` requirements that no longer appear fully supported,
- requirements with `Needs-Review: yes`,
- stale `R -> A -> D -> UT/IT/QT` mappings caused by semantic changes.

## Required output format
1. Issue list (short, actionable).
2. For each issue: affected IDs (`R-*`, `A-*`, `D-*`, `UT/IT/QT-*`) and files.
3. Minimal concrete edits required to fix each issue.
4. Whether each touched `Needs-Review: yes` requirement can be cleared
   (`yes` or `no`) and why.

## Review checklist
- Requirement text uses current canonical terminology.
- Architecture wording and interfaces match current requirement semantics.
- Architecture ownership boundaries still match the intended subsystem seams.
- Tests and examples do not preserve removed or replaced terms.
- `DONE` status is still justified by current evidence and behavior.
- Traceability links still reflect the current semantics.

## Constraints for reviewer
- Preserve stable IDs (`R-*`, `A-*`, `D-*`, `UT-*`, `IT-*`, `QT-*`).
- Keep edits minimal and markdown-first.
- Do not add formal change-management machinery.
