# System tests

Use conventional filenames (no IDs in filenames). Annotate each test case/group
with a Doxygen block:

```cpp
/**
 * @test QT-001
 * @verifies [R-123]
 * @notes Given full scenario ... When run end-to-end ... Then requirement is
 * satisfied.
 */
TEST(MySuite, MyCase) {
    // ...
}
```

- **QT** tests verify requirement items (**R-...**).
- Multiple `QT-*` tests may verify the same `R-*` item.
