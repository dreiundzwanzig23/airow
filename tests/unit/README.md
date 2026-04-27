# Unit tests

Use conventional filenames (no IDs in filenames). Annotate each test case/group
with a Doxygen block:

```cpp
/**
 * @test UT-001
 * @verifies [D-001]
 * @case nominal
 * @oracle exact
 * @notes Given ... When ... Then ...
 */
TEST(MySuite, MyCase) {
    // ...
}
```

- **UT** tests verify design items (**D-...**).
- Multiple `UT-*` tests may verify the same `D-*` item.
- New or changed `UT-*` blocks must include exactly one `@case` tag:
  `nominal`, `equivalence`, `boundary`, `edge`, or `invalid`.
- New or changed `UT-*` blocks must include exactly one `@oracle` tag:
  `exact`, `tolerance`, `invariant`, `monotonic`, `accounting`,
  `diagnostic`, or `rejection`.
- Keep each `UT-*` focused on one observable behavior. Split distinct
  equivalence classes, boundaries, edge cases, and invalid-input reasons into
  separate tests.
- Prefer one primary `D-*` target per new or changed unit test unless the
  behavior has crossed into integration scope.
- Keep tests deterministic and fast.
