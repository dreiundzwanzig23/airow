# AGENTS.md — tests

## Scope
Verification code and fixtures across unit, integration, and system levels.

## Rules
- Use deterministic setups and bounded runtime.
- All tests must include Doxygen `@test` and `@verifies` tags.
- Use clear Given/When/Then notes in `@notes`.
- Use `@aux yes` only for informational tests that should not count as
  evidence.

## Guardrails
- Keep layer responsibilities separate: UT for design, IT for architecture,
  QT for requirements.
- Non-aux tests must verify same-layer IDs only.
