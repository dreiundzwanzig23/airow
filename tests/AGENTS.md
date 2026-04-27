# AGENTS.md — tests

## Scope
Verification code and fixtures across unit, integration, and system levels.

## Rules
- Use deterministic setups and bounded runtime.
- All tests must include Doxygen `@test` and `@verifies` tags.
- Use clear Given/When/Then notes in `@notes`.
- New or changed `UT-*` blocks must describe one observable behavior and carry
  exactly one `@case` tag and one `@oracle` tag.
- Use `@aux yes` only for informational tests that should not count as
  evidence.

## Guardrails
- Keep layer responsibilities separate: UT for design, IT for architecture,
  QT for requirements.
- Non-aux tests must verify same-layer IDs only.
- Split separate equivalence classes, boundaries, edge cases, and failure
  reasons into separate `UT-*` cases instead of bundling them into one broad
  unit test.
