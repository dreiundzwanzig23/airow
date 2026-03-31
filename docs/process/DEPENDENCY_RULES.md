# DEPENDENCY_RULES.md

This document defines allowed include-direction dependencies.

## Allowed dependencies
- `include -> include`
- `src/lib -> include, src/lib`
- `src/app -> include, src/lib`
- `tests/unit -> include, src/lib, tests/unit`
- `tests/integration -> include, src/lib, tests/integration`
- `tests/system -> include, src/lib, tests/system`

## Forbidden examples
- `src/lib -> src/app`
- `include -> src/*`
- `src/app -> tests/*`

## Validation
Run:
```bash
./scripts/depcheck.sh
```
