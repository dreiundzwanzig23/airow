# AGENTS.md — tests/unit

## Scope
Fine-grained tests for implementation design units.

## Rules
- `UT-*` IDs are mandatory.
- `@verifies` must reference `D-*` IDs only.
- Prefer small fixtures and explicit tolerances.

## Guardrails
- Do not test cross-module integration behavior here.
