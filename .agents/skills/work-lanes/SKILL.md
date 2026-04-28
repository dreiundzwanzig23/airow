---
name: work-lanes
description: Classify repository work into the lightest sufficient lane before editing. Use at the start of tasks when scope, gates, or documentation obligations are unclear.
---

# Work Lanes

## Goal

Keep AIRow strict where it matters and lean where it can be lean. Select the smallest lane that satisfies the user request and the repository contract.

## Start

Before editing, answer these questions:

1. Did the user ask for exploration only?
2. Does the change alter functional behavior?
3. Does it alter a public contract, artifact, scenario, example, or user-facing doc?
4. Does it cross subsystem seams, change backend policy, or modify multiple requirement semantics?
5. Is this a release, merge, milestone, or handoff task?

## Lane Selection

### Lane 0 — Explore

Use when the task is inspection, explanation, diagnosis, impact estimation, or planning only.

Rules:

- Do not edit files.
- Do not update docs or traceability.
- List inspected files and recommend the next lane if implementation is needed.

### Lane 1 — Tiny Local Fix

Use for typo-level fixes, small local cleanup, or one small non-public correction.

Rules:

- Avoid requirement, architecture, README, CHANGELOG, and `docs/ai/*` edits unless the tiny fix directly triggers them.
- Run the narrowest relevant check.
- Escalate to Lane 2 if observable behavior changes.

### Lane 2 — Functional Behavior Slice

Use for ordinary feature or defect work that changes observable simulator or library behavior.

Rules:

- Use failing-tests-first red/green/refactor.
- Identify existing `R-*`, `A-*`, and `D-*` owners where applicable.
- Prefer extending existing coherent architecture owners.
- Run `./scripts/test_tdd.sh` after refactor.
- Update traceability only when trace links or evidence metadata changed.

### Lane 3 — Public Interface or Artifact

Use when the change affects configs, CLI behavior, examples, scenarios, output schemas, reports, visualization artifacts, HDF5/JSON contracts, public headers, or user-facing docs.

Rules:

- Use TDD for behavior.
- Update public-contract docs that are actually triggered.
- Run full local completion gates before declaring completion.
- Regenerate traceability when evidence links changed.

### Lane 4 — Major Architecture or Migration

Use only for cross-cutting, migratory, backend, architectural, seam-splitting, deprecation, semantic multi-requirement, or evidence-promotion work.

Rules:

- Load `.agents/skills/major-change-loop/SKILL.md`.
- Build an impact map before invasive edits.
- Add characterization tests for preserved behavior.
- Finish with drift review and full gates.

### Lane 5 — Release or Handoff

Use for milestone closure, release preparation, merge readiness, or durable session handoff.

Rules:

- Load `.agents/skills/release-doc-sync/SKILL.md` and `.agents/skills/workflow-audit/SKILL.md`.
- Update release/context docs only where triggered.
- Run `./scripts/verify.sh` when merge-ready confidence is required.

## Escalation Rules

Escalate when the actual change proves the current lane is too small. Do not escalate merely because:

- the task feels important,
- generated trace output changed,
- docs mention many IDs,
- Codex read many files,
- a small process doc was edited.

## Required Output

At the end of the task, include a compact lane note:

```markdown
Work lane: 0 | 1 | 2 | 3 | 4 | 5
Why: <one sentence>
Gates run: <commands or none>
Docs updated: <triggered artifacts or none>
Trace updated: yes | no, with reason
```

## Finish

If the selected lane required a skill, mention which skill was used. If no edits were made, do not invent evidence.
