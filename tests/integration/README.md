# Integration tests

Use conventional filenames (no IDs in filenames). Annotate each test case/group
with a Doxygen block:

```cpp
/**
 * @test IT-001
 * @verifies [A-123]
 * @notes Given subsystem coupling ... When executed ... Then expected
 * interaction holds.
 */
TEST(MySuite, MyCase) {
    // ...
}
```

- **IT** tests verify architecture items (**A-...**).
- Multiple `IT-*` tests may verify the same `A-*` item.
