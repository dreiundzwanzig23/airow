# AGENTS.md — Codex Operating Contract

This repository bootstraps an AI-guided C++ rowing simulator project.
Autonomous work must be traceable, test-driven, and resumable.

## Policy Core

### Source of truth
- Product intent and acceptance: `docs/process/REQUIREMENTS.md`
- Architecture intent: `docs/process/ARCHITECTURE.md`
- Architecture allocation policy: `docs/process/ARCHITECTURE_POLICY.md`
- State and frame conventions: `docs/process/STATE_CONVENTIONS.md`
- Approved core technologies: `docs/process/TECHNOLOGY_STACK.md`
- Durable technical decisions: `docs/ai/DECISIONS.md`
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
2. Perform architecture allocation before writing tests:
   - identify candidate owning `A-*` items,
   - prefer extending an existing coherent subsystem,
   - record the architecture delta before TDD,
   - justify any new `A-*` with explicit cohesion and reuse intent.
3. Red: add/adjust failing tests first for functional changes and capture test
   failure evidence.
4. Green: implement the minimum code needed to make the targeted failing tests
   pass.
5. Refactor: run a mandatory behavior-preserving cleanup pass after green; if
   no structural changes are needed, record an explicit no-op rationale.
6. Re-run the fast lane (`./scripts/test_tdd.sh`) after refactor before full
   completion gates.
7. Run required quality gates.
8. Update traceability/context artifacts when triggered.

Never skip failing-tests-first for functional behavior changes, and never skip
the refactor phase for functional loops.
Use explicit `rgr:red`, `rgr:green`, and `rgr:refactor` markers in commit
messages or evidence notes.
`./scripts/check_rgr_evidence.sh` is warning-only by default (`warn` mode) and
is wired into `test_tdd.sh` and `verify.sh`; set
`RGR_ENFORCEMENT_MODE=strict` to promote missing markers to a failing check.
Do not create a 1:1 `R -> A` mapping unless `Allocation Rationale` makes the
need explicit.
When a task affects technology choice, solver direction, file-format policy, or
external-tool integration, align with `docs/process/TECHNOLOGY_STACK.md` and
`docs/ai/DECISIONS.md` before implementation.
When a change is cross-cutting, migratory, or architectural, use
`.agents/skills/major-change-loop/SKILL.md` instead of the ordinary TDD loop.

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
`./scripts/check_rgr_evidence.sh` checks for `rgr:red`, `rgr:green`, and
`rgr:refactor` evidence markers.

### Context/document completion rules
A task is complete only when:
- Tests and required gates pass.
- Trace check passes.
- Relevant docs and AI context files are updated.
- `CHANGELOG.md` and `README.md` reflect user/process-visible changes.

Update `docs/ai/*` when milestone-level change occurs:
- requirement status changes (`R-*`)
- architecture status changes (`A-*`)
- approved technology direction changes
- durable technical decision changes
- workflow, gate, or public interface contract changes

## Change Boundaries
- `include/`: public headers and stable interfaces
- `src/lib/`: library logic
- `src/app/`: program entry
- `scripts/`: deterministic local/CI-compatible automation
- `tools/`: traceability and policy guardrails

Avoid adding narrow process policy unless it is reusable or clearly needed by
the simulator project.
Avoid non-apt dependencies unless explicitly approved.

## Legacy
- Do not preserve outdated bootstrap or template workflow just for
  compatibility.
- Prefer deleting stale process guidance over adding wrappers around it.

## Codex, OPENAI, ChatGPT
Always use the OpenAI developer documentation MCP server if you need to work with the OpenAI API, ChatGPT Apps SDK, Codex,… without me having to explicitly ask.

## Repository Skills (Load On Demand)
- Skill index: `.agents/skills/README.md`
- `.agents/skills/tdd-loop/SKILL.md`
- `.agents/skills/unit-test-design/SKILL.md`
- `.agents/skills/trace-maintenance/SKILL.md`
- `.agents/skills/test-lanes/SKILL.md`
- `.agents/skills/release-doc-sync/SKILL.md`
- `.agents/skills/major-change-loop/SKILL.md`

Policy stays here. Operational playbooks live in skills and should be loaded
only when relevant to the task.
