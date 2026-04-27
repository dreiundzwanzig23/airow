---
name: unit-test-design
description: Design or tighten repository unit tests (`UT-*`) for design-level behavior. Use when Codex is adding or revising unit coverage, selecting edge cases, boundary values, or equivalence classes, splitting broad unit tests into small maintainable cases, or turning a `D-*` contract into behavior-focused Given/When/Then tests with explicit expected outputs.
---

# Unit Test Design

## Start
- Use this skill when `UT-*` is the narrowest correct evidence lane.
- Pair it with `.agents/skills/tdd-loop/SKILL.md` during red-phase test design.
- Pair it with `.agents/skills/major-change-loop/SKILL.md` when preserved local
  behavior needs characterization before refactor.
- Keep repo rules aligned with:
  - `docs/process/TEST_STRATEGY.md`
  - `tests/AGENTS.md`
  - `tests/unit/README.md`

## Design Cases
1. Map the target test surface to the owning `D-*` item before writing code.
2. Derive cases from the design contract, not the implementation shape:
   - observable outputs,
   - invariants,
   - deterministic diagnostics,
   - normalization rules,
   - contract-level error handling.
3. Identify the oracle before writing the assertion. Prefer one of:
   - exact value,
   - tolerance-bounded numeric expectation,
   - invariant,
   - monotonic relation,
   - conservation or accounting relation,
   - deterministic diagnostic,
   - invalid-input rejection.
4. Partition inputs into meaningful equivalence classes.
5. Hit the boundaries that can change behavior:
   - minimum, maximum, empty, missing, zero, sign, ordering, duplicate, and
     just-inside or just-outside values,
   - finite versus non-finite numeric inputs where the contract exposes them.
6. Add nominal coverage plus only the edge cases that can fail for distinct
   reasons.
7. Prefer one behavior axis per test. Split cases instead of building one
   oversized assertion bundle.
8. For changed or new `UT-*` blocks, choose exactly one `@case` classification:
   `nominal`, `equivalence`, `boundary`, `edge`, or `invalid`.
9. For changed or new `UT-*` blocks, choose exactly one `@oracle`
   classification: `exact`, `tolerance`, `invariant`, `monotonic`,
   `accounting`, `diagnostic`, or `rejection`.

## Write Tests
- Keep tests behavior-focused. Do not couple them to private structure,
  implementation-file includes, `FRIEND_TEST`, wall-clock reads, sleeps, or
  other backdoors.
- Keep files intentionally small under the repo defaults:
  - max `900` lines per file,
  - max `14` `TEST*` cases per file,
  - use overrides only with explicit rationale.
- Use names that describe the contract outcome, not the internal mechanism.
- Use Doxygen blocks with `@test`, `@verifies`, and `@notes`.
- For new or changed unit tests, include exactly one `@case` and one `@oracle`
  tag in the Doxygen block.
- Prefer one primary `D-*` target per new or changed `UT-*`; move broader
  boundary behavior to `IT-*` when a single design owner is no longer the
  narrowest correct lane.
- Write `@notes` in Given/When/Then form.
- Make the Then clause concrete. State the expected output, diagnostic, or
  invariant instead of only saying that the call "succeeds" or "fails".
- Avoid weak oracles. "Does not crash" is auxiliary smoke coverage unless the
  design contract explicitly defines non-crashing as the observable behavior.
- Keep fixtures local and minimal. Extract helpers only when they reduce
  repetition without hiding the test intent.

## Required Output
For non-trivial unit-test work, leave a compact design note:

```markdown
## Unit Test Design

Target:
- Design: D-###
- Test: UT-###

Cases:
- nominal:
- boundary:
- invalid/error:

Oracle:
- exact | tolerance | invariant | monotonic | accounting | diagnostic | rejection
- expected observable:

Doxygen:
- @case:
- @oracle:

Red expectation:
- why this test should fail before implementation:
```

## Review
- Check that each case has a unique reason to exist.
- Check that nominal, boundary, and invalid classes are all represented where
  the design contract exposes them.
- Check that assertions verify public behavior and stable diagnostics.
- Check that the test still reads linearly from Given to Then.
- Check that the test would stay valid after an internal refactor that preserves
  behavior.

## Finish
- In red phase, make the targeted `UT-*` fail for the intended reason first.
- In green phase, keep the test surface narrow enough that failures localize to
  one design contract.
- In refactor phase, remove duplication and rename for clarity without
  weakening coverage intent.
- Run the narrowest useful lane during iteration, then the required full gates
  before concluding work.
