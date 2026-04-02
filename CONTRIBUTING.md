# Contributing

- Conventional commits: `feat(scope): summary` (include RequirementID when relevant).
- Code style: see `.clang-format` and `.clang-tidy`.
- All new work: TDD. Failing test → minimal pass → refactor.
- Keep trace metadata current: `R-*`, `A-*`, `D-*`, `UT-*`, `IT-*`, `QT-*`.
- Keep `docs/process/TECHNOLOGY_STACK.md` and `docs/ai/DECISIONS.md` aligned
  when changing approved technologies, solver direction, file formats, or
  external-tool integration policy.
- Semantic requirement wording changes must set `Change-Type: semantic` and
  `Needs-Review: yes`.
- Update `README.md`, `CHANGELOG.md`, and relevant `docs/ai/*` files for
  user-visible or process-visible changes.
- Run the required local gates before considering work complete:
  - `./scripts/format.sh`
  - `./scripts/lint.sh`
  - `./scripts/build.sh`
  - `./scripts/test.sh`
  - `./scripts/depcheck.sh`
  - `python3 tools/tracecheck.py --write`
