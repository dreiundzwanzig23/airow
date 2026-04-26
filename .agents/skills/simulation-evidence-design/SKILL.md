---
name: simulation-evidence-design
description: Design evidence for simulator physics, visualization, calibration, validation, and optimization behavior before implementation. Use when work makes a physical accuracy claim, changes model fidelity, adds diagnostic channels, or affects how users visually understand rowing simulation results.
---

# Simulation Evidence Design

## Start
- Use this skill before implementing hydro, aero, mechanics, rower/oar coupling,
  visualization, calibration, validation, uncertainty, or optimization behavior.
- Treat every simulator behavior as a claim that needs an oracle, validity range,
  diagnostics, and inspectable outputs.
- Separate runtime reduced-order behavior from calibrated, validated, and
  high-fidelity reference behavior.

## Design The Evidence
1. State the physical or interpretive claim:
   - what behavior should the simulator reproduce,
   - what the user should be able to understand visually,
   - what the simulator must not claim outside its validity range.
2. Assign a fidelity level:
   - placeholder,
   - reduced,
   - calibrated,
   - validated,
   - high-fidelity reference.
3. Define the validity range:
   - boat speed,
   - stroke rate,
   - rigging/geometry assumptions,
   - wind/current/wave assumptions,
   - rower/actuation assumptions,
   - numerical timestep or solver limits.
4. Pick the oracle:
   - analytic relation,
   - conservation/accounting invariant,
   - measured data,
   - calibrated coefficient table,
   - reference CFD/SPH artifact,
   - regression envelope,
   - visual/diagnostic invariant.
5. Define required outputs:
   - time-series channels,
   - units,
   - frames,
   - force/moment vectors,
   - energy and power terms,
   - diagnostic flags,
   - visualization overlays.
6. Select evidence lanes:
   - `UT-*` for local model contracts and invariants,
   - `IT-*` for coupled subsystem behavior,
   - `QT-*` for scenario-level requirement evidence.
7. Name failure modes:
   - rejected inputs,
   - low-confidence warnings,
   - out-of-range validity labels,
   - visual diagnostics needed to explain the failure.

## Required Output
Leave a simulation evidence note before implementation:

```markdown
## Simulation Evidence Design

Physical or interpretive claim:
-

Fidelity level:
- placeholder | reduced | calibrated | validated | high-fidelity reference

Validity range:
- speed:
- stroke rate:
- wind/current/wave assumptions:
- geometry assumptions:
- solver/timestep assumptions:

Oracle:
- analytic | measured data | regression envelope | conservation/accounting | reference artifact | visual invariant
- details:

Required outputs:
- time-series channels:
- units:
- frames:
- visualization vectors:
- diagnostics:

Tests:
- UT:
- IT:
- QT:

Failure modes:
- rejected:
- low-confidence:
- visually inspectable:
```

## Finish
- Do not implement a physics or visualization behavior whose evidence design is
  only "simulation runs".
- Preserve honesty labels. A placeholder model may be useful, but it must remain
  labeled as placeholder or reduced until calibrated and validated.
- Make the result inspectable: every important simulated force, state, and
  direction should have a unit, frame, diagnostic channel, or visual overlay.
