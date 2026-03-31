# AGENTS.md — Codex Operating Contract

This repository is a domain-agnostic template for AI-guided C++ development.
Autonomous work must be traceable, test-driven, and resumable.

## Policy Core

### Source of truth
- Product intent and acceptance: `docs/process/REQUIREMENTS.md`
- Architecture intent: `docs/process/ARCHITECTURE.md`
- Design intent: code Doxygen `@design` blocks
- Verification intent: tests Doxygen `@test` blocks
- Session continuity: `docs/ai/*`

### Context efficiency defaults
- Start broad orientation with `./scripts/project_stats.py --format agent`.
- Retrieve context with a locate-then-slice pattern:
  - use `rg --files` / targeted `rg -n` first,
  - then read only the needed file section with `sed -n` or equivalent.
- Default search exclusions unless the task explicitly needs them:
  - `docs/ai/archive/**`
  - `docs/process/TRACEABILITY.md`
  - `build/**`, `build-*/**`, `build/logs/**`, `**/*.log`
- Keep active AI docs compact and non-duplicative:
  - `SESSION_CONTEXT.md` = current state and guardrails only
  - `HANDOFF.md` = latest-session delta only
  - `ROADMAP.md` = open/future direction only
  - `DECISIONS.md` = active constraints only; older ADRs belong in archive

### Non-negotiable workflow
1. Select actionable requirement work in this order:
   - requirements with `Needs-Review: yes`
   - then highest-priority `OPEN` requirement
2. Confirm/update architecture mapping (`A-*` satisfies `R-*`).
3. Add/adjust failing tests first for functional changes.
4. Implement minimal code to pass tests.
5. Refactor behavior-preserving.
6. Run required quality gates.
7. Update traceability/context artifacts when triggered.

Never skip failing-tests-first for functional behavior changes.

### Lightweight requirement-change policy
- Requirement metadata fields:
  - `Change-Type: none | editorial | semantic`
  - `Needs-Review: yes | no`
  - `Change-Note: optional`
- `Change-Type: semantic` must set `Needs-Review: yes`.
- A requirement with `Needs-Review: yes` is not fully aligned even if `Status: DONE`.
- Newer requirement wording overrides older notes/examples.
- Removed or replaced terms must be removed from affected architecture, tests,
  README, and examples/docs.
- Semantic requirement changes require a drift review across affected
  requirements, architecture, tests, and user-facing docs.
- A task touching `Needs-Review` items is complete only when each touched item
  is either cleared to `Needs-Review: no` or explicitly left flagged with a
  documented follow-up.
- Keep this lightweight: do not introduce formal approval workflows,
  baselines, or revision machinery.

### Traceability model
- IDs: `R-###`, `A-###`, `D-###`, `UT-###`, `IT-###`, `QT-###`
- Layer mapping:
  - `UT-*` verifies one-or-more `D-*`
  - `IT-*` verifies one-or-more `A-*`
  - `QT-*` verifies one-or-more `R-*`
- Optional overlay:
  - `@aux yes` marks non-evidence tests that are reported separately and
    excluded from `DONE` evidence gates
- Evidence gates:
  - `D-*` needs at least one `UT-*`
  - `A-*` (`CODE`) needs at least one `D-*` and one `IT-*`
  - `A-*` (`ASSET`) needs at least one linked `QT-*` via satisfied `R-*`
  - `R-*` needs at least one `A-*` and one `QT-*`
- Validate via:
  - `python3 tools/tracecheck.py --write`

### Required quality gates
Run all before considering work complete:
```bash
./scripts/format.sh
./scripts/lint.sh
./scripts/build.sh
./scripts/test.sh
./scripts/depcheck.sh
python3 tools/tracecheck.py --write
```

`./scripts/test.sh` is the default full local gate.
`./scripts/test_tdd.sh` is the fast local iteration lane.
`./scripts/test_aux.sh` is the auxiliary contract lane.
`./scripts/verify.sh` is the aggregate pre-merge run.

### Context/document completion rules
A task is complete only when:
- Tests and required gates pass.
- Trace check passes.
- Relevant docs and AI context files are updated.
- `CHANGELOG.md` and `README.md` reflect user/process-visible changes.

Update `docs/ai/*` when milestone-level change occurs:
- requirement status changes (`R-*`)
- architecture status changes (`A-*`)
- workflow, gate, or public interface contract changes

## Change Boundaries
- `include/`: public headers and stable interfaces
- `src/lib/`: library logic
- `src/app/`: program entry
- `scripts/`: deterministic local/CI-compatible automation
- `tools/`: traceability and policy guardrails

Avoid adding domain-specific template policy unless it is reusable.
Avoid non-apt dependencies unless explicitly approved.

## Legacy
- Do not preserve outdated template workflow just for compatibility.
- Prefer deleting stale process guidance over adding wrappers around it.

## Repository Skills (Load On Demand)
- Skill index: `skills/README.md`
- `skills/template-tdd-loop/SKILL.md`
- `skills/template-trace-maintenance/SKILL.md`
- `skills/template-test-lanes/SKILL.md`
- `skills/template-release-doc-sync/SKILL.md`

Policy stays here. Operational playbooks live in skills and should be loaded
only when relevant to the task.
