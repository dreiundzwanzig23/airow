# AGENTS.md — src

## Scope
Implementation source files for application and library behavior.

## Rules
- Library/domain logic goes to `src/lib/`.
- CLI entry and wiring go to `src/app/`.
- Every non-trivial implementation unit should include Doxygen
  `@design D-*` and `@satisfies [A-*]`.

## Guardrails
- Keep implementations small and composable.
- Avoid embedding architecture decisions without matching docs updates.
