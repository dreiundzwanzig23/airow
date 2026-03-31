# AGENTS.md — scripts

## Scope
Developer and CI helper scripts for setup, format, lint, build, and test.

## Rules
- Scripts must be deterministic and fail fast.
- Preserve compatibility with Ubuntu apt-based environment.
- Document behavior changes in `README.md`.
- Keep validation scripts compatible with `scripts/validation_output.sh`.
- Keep summary/log outputs machine-readable and stable.

## Guardrails
- Avoid hidden state mutations.
- Avoid introducing external package-manager assumptions.
- Keep default lanes generic; do not hard-code project-specific runtimes.
