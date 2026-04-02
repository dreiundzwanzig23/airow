# NUMERICS_POLICY.md

This document defines the baseline numerics policy for the simulator project.

## Units
- Use SI units at all simulator boundaries.
- Unit-bearing configuration and output fields must document their units
  explicitly.

## Numeric safety
- Detect non-finite values at configuration boundaries and during runtime.
- Treat non-finite subsystem outputs as deterministic failure conditions.
- Reject ambiguous or invalid numeric inputs before time stepping starts.

## Tolerances and comparisons
- Use explicit tolerances when comparing floating-point results.
- Keep deterministic replay expectations scoped to the same executable and
  platform unless a broader guarantee is documented separately.
- Scenario acceptance envelopes should document whether they are exact,
  tolerance-based, or trend-based.

## Regression expectations
- Local regression lanes should remain stable and bounded in runtime.
- Quick verification lanes must remain usable without optional high-fidelity
  tooling.
