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

## Startup validity and solver diagnostics
- Perform consistent initialization before routine time stepping begins.
- Treat non-converged startup conditions as deterministic startup failures, not
  as recoverable nominal runtime states.
- Report failure categories through stable diagnostics so repeated failures
  remain actionable and comparable across runs.
- When backend-specific detail is needed, expose it through dedicated
  solver-status metadata rather than changing the stable diagnostic code set.

## Built-in backend policy
- The public runtime backend contract is split into
  `simulation.mechanics_backend` and `simulation.integration_backend`.
- The preferred supported runtime pair is
  `chrono_rigidbody + sundials_ida`.
- The built-in SUNDIALS IDA policy for the preferred runtime path uses fixed
  `rtol = 1e-10` and `atol = 1e-10`.
- `internal_baseline + sundials_ida` remains the supported fallback and
  cross-check pair.
- `internal_baseline + deterministic_baseline` remains the explicit
  deterministic debug fallback.
- `chrono_rigidbody + deterministic_baseline` is not a supported backend
  combination.
- Stable diagnostic codes remain generic; backend-specific detail is reported
  through startup and runtime solver-status fields.

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
