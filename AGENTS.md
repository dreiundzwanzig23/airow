# AGENTS.md — Codex Operating Contract

AIRow is an AI-guided C++ rowing simulator. Work must stay testable, traceable, and resumable, but the default path is the lightest lane that safely satisfies the task.

## Operating Principle

Be strict about behavior, public contracts, traceability, and completion gates. Be lean about context, documentation churn, and handoff artifacts. Do not turn every small change into a major-change workflow.

## Source of Truth

- Product intent and acceptance: `docs/process/REQUIREMENTS.md`
- Architecture intent: `docs/process/ARCHITECTURE.md`
- Architecture allocation policy: `docs/process/ARCHITECTURE_POLICY.md`
- State and frame conventions: `docs/process/STATE_CONVENTIONS.md`
- Approved core technologies: `docs/process/TECHNOLOGY_STACK.md`
- Durable technical decisions: `docs/ai/DECISIONS.md`
- Design intent: code Doxygen `@design` blocks
- Verification intent: test Doxygen `@test` blocks
- Compact workflow index: `docs/process/WORKFLOW.md`
- Operational playbooks: `.agents/skills/*/SKILL.md`

## Context Budget

Start with `./scripts/project_stats.py --format agent` only when broad orientation is useful. Retrieve context with locate-then-slice:

1. Search with `rg --files` or targeted `rg -n`.
2. Read only the needed sections with `sed -n` or an equivalent slice.
3. Load skill files only when the selected work lane requires them.

Default search exclusions unless explicitly needed:

- `docs/ai/archive/**`
- `docs/archive/**`
- `docs/process/TRACEABILITY.md`
- `build/**`, `build-*/**`, `build/logs/**`, `**/*.log`

Use `python3 tools/tracecheck.py --json` for full trace details instead of expanding generated trace markdown by hand.

## Work Lane Router

Before editing, classify the task and use the lightest sufficient lane. If the user directly specifies a task, that task overrides backlog selection.

| Lane | Use for | Required discipline | Typical gate |
| --- | --- | --- | --- |
| 0 Explore | Inspection, explanation, impact estimate, plan only | No edits. List inspected files and recommended lane. | None |
| 1 Tiny local fix | Typo, local cleanup, one small non-public fix | Targeted check. No requirement or architecture edits unless already implicated. | Targeted command or relevant narrow script |
| 2 Functional behavior slice | Feature or defect changing observable simulator/library behavior | Failing test first, green, mandatory refactor. Identify existing `R-*`, `A-*`, `D-*` owners. | `./scripts/test_tdd.sh` after refactor |
| 3 Public interface or artifact | Config, CLI, examples, scenarios, output JSON/HDF5, reports, visualization, user docs | TDD plus public-contract docs and trace updates when affected. | Full local gates before completion |
| 4 Major architecture or migration | Cross-subsystem seam work, backend replacement, semantic multi-requirement change, architecture split | Load major-change skill. Impact map and characterization tests before invasive edits. | Full gates plus drift review |
| 5 Release or handoff | Milestone wrap-up, merge/release preparation, durable handoff | Release-doc sync and workflow audit. | `./scripts/verify.sh` |

Escalate only when the actual change crosses the next lane's trigger. Do not use Lane 4 merely because a diff touches many docs or generated trace files.

## Non-Negotiables

- Functional behavior changes use failing-tests-first red/green/refactor.
- Repeat red/green/refactor for each distinct observable behavior slice.
- Keep `rgr:red`, `rgr:green`, and `rgr:refactor` markers in order for behavior-changing work.
- Never skip the refactor phase for a functional loop; record a no-op rationale when no cleanup is needed.
- Prefer existing coherent architecture owners before adding new `A-*` items.
- Do not create thin 1:1 `R -> A` mappings unless `Allocation Rationale` makes the cohesion need explicit.
- Align with `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md` before changing technology choice, solver direction, file-format policy, or external-tool integration.
- Use `.agents/skills/major-change-loop/SKILL.md` for cross-cutting, migratory, architectural, backend, or semantic multi-requirement changes.

## Requirement Change Policy

Requirement metadata fields:

- `Change-Type: none | editorial | semantic`
- `Needs-Review: yes | no`
- `Change-Note: optional`

Rules:

- `Change-Type: semantic` must set `Needs-Review: yes`.
- A requirement with `Needs-Review: yes` is not fully aligned even if `Status: DONE`.
- Newer requirement wording overrides older notes/examples.
- Removed or replaced terms must be removed from affected architecture, tests, README, examples, and docs.
- Semantic requirement changes require drift review across affected requirements, architecture, tests, and user-facing docs.
- A task touching `Needs-Review` items is complete only when each touched item is cleared to `Needs-Review: no` or explicitly left flagged with a follow-up.
- Keep this lightweight; do not add formal approval workflows, baselines, or revision machinery.

## Documentation Update Triggers

Update only when triggered by the actual change:

| Artifact | Update when |
| --- | --- |
| `docs/process/REQUIREMENTS.md` | Requirement wording, status, acceptance, semantics, or review flags change |
| `docs/process/ARCHITECTURE.md` | Subsystem ownership, boundaries, allocation, or architecture status changes |
| Code/test Doxygen IDs | Design or verification contract changes near code/tests |
| `docs/process/TRACEABILITY.md` | Regenerated by `python3 tools/tracecheck.py --write` after trace-relevant links/metadata change |
| `README.md` | Setup, CLI usage, examples, public behavior, or user-visible output changes |
| `CHANGELOG.md` | User-visible behavior, process-visible workflow, public artifact, or milestone state changes |
| `docs/ai/DECISIONS.md` | Durable technical decision or constraint changes |
| `docs/ai/SESSION_CONTEXT.md` | Persistent current-state guardrails or next actions change |
| `docs/ai/HANDOFF.md` | Session or milestone handoff is being prepared |
| `docs/ai/ROADMAP.md` | Backlog or milestone direction changes |

Do not update README, CHANGELOG, `docs/ai/*`, requirements, or architecture only to prove that work happened.

## Traceability Model

IDs:

- Requirements: `R-###`
- Architecture: `A-###`
- Design: `D-###`
- Tests: `UT-###`, `IT-###`, `QT-###`

Layer mapping:

- `UT-*` verifies one or more `D-*`
- `IT-*` verifies one or more `A-*`
- `QT-*` verifies one or more `R-*`
- `@aux yes` marks informational tests that are excluded from `DONE` evidence gates

Evidence gates:

- `D-*` needs at least one `UT-*`
- `A-*` with `CODE` status needs at least one `D-*` and one `IT-*`
- `A-*` with `ASSET` status needs at least one linked `QT-*` through satisfied `R-*`
- `R-*` needs at least one `A-*` and one `QT-*`

Validate traceability with:

```bash
python3 tools/tracecheck.py --write
```

Do not hand-edit generated trace output.

## Quality Gates

Use the lane table first. For merge-ready, release-ready, public-interface, or major work, run the full local completion gates:

```bash
./scripts/format.sh
./scripts/lint.sh
./scripts/build.sh
./scripts/test.sh
./scripts/depcheck.sh
python3 tools/tracecheck.py --write
```

Common lanes:

- Fast functional iteration: `./scripts/test_tdd.sh`
- Standard full local test: `./scripts/test.sh`
- Auxiliary script/tool contract checks: `./scripts/test_aux.sh`
- Protected performance guardrail: `./scripts/test_performance.sh`
- Aggregate pre-merge verification: `./scripts/verify.sh`
- RGR evidence check: `./scripts/check_rgr_evidence.sh`

`check_rgr_evidence.sh`, `test_tdd.sh`, and `verify.sh` are strict by default for RGR evidence. Use `RGR_ENFORCEMENT_MODE=warn` or `off` only as an explicit local override for lanes that do not change functional behavior.

## Completion Rules

A task is complete when:

- The selected lane is named.
- Required tests/gates for that lane pass, or blockers are reported clearly.
- Tracecheck is run when trace-relevant files changed.
- Documentation is updated only where the trigger table requires it.
- The final response lists changed areas and commands run.
- Repository-changing final responses include one line: `Commit message: ...`

## Change Boundaries

- `include/`: public headers and stable interfaces
- `src/lib/`: library logic
- `src/app/`: program entry
- `scripts/`: deterministic local/CI-compatible automation
- `tools/`: traceability, analysis, and policy guardrails
- `.agents/skills/`: on-demand operational playbooks

Avoid adding narrow process policy unless it is reusable or clearly needed by the simulator project. Prefer deleting stale process guidance over wrapping it.

Avoid non-apt dependencies unless explicitly approved.

## Codex, OpenAI, and ChatGPT

Always use the OpenAI developer documentation MCP server if you need to work with the OpenAI API, ChatGPT Apps SDK, Codex, or related OpenAI developer tooling.

## Repository Skills

Skill index: `.agents/skills/README.md`

Load on demand:

- `.agents/skills/work-lanes/SKILL.md`
- `.agents/skills/tdd-loop/SKILL.md`
- `.agents/skills/unit-test-design/SKILL.md`
- `.agents/skills/major-change-loop/SKILL.md`
- `.agents/skills/trace-maintenance/SKILL.md`
- `.agents/skills/test-lanes/SKILL.md`
- `.agents/skills/release-doc-sync/SKILL.md`
- `.agents/skills/workflow-audit/SKILL.md`
- `.agents/skills/simulation-evidence-design/SKILL.md`
- `.agents/skills/karpathy-guidelines/SKILL.md`

Policy stays here. Operational detail lives in skills and should be loaded only when relevant to the selected lane.
