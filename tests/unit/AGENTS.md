# AGENTS.md — tests/unit

## Scope
Fine-grained tests for implementation design units.

## Rules
- `UT-*` IDs are mandatory.
- `@verifies` must reference `D-*` IDs only.
- New or changed `UT-*` Doxygen blocks must include exactly one `@case` tag and
  exactly one `@oracle` tag.
- Each new or changed `UT-*` should cover one observable behavior. Split
  distinct equivalence classes, boundaries, edge cases, and rejection reasons.
- Prefer small fixtures and explicit tolerances.

## Guardrails
- Do not test cross-module integration behavior here.
