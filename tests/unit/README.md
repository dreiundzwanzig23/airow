# Unit tests

Use conventional filenames (no IDs in filenames). Annotate each test case/group
with a Doxygen block:

```cpp
/**
 * @test UT-001
 * @verifies [D-001]
 * @notes Given ... When ... Then ...
 */
TEST(MySuite, MyCase) {
    // ...
}
```

- **UT** tests verify design items (**D-...**).
- Multiple `UT-*` tests may verify the same `D-*` item.
- Keep tests deterministic and fast.
