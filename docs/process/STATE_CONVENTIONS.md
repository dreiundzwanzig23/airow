# STATE_CONVENTIONS.md

This document defines the baseline simulator conventions for frames, signs,
and boundary-visible state quantities.

## Scope
- These conventions apply to configuration, machine-readable outputs, and
  public runtime contracts unless a narrower interface documents an explicit
  local override.
- Named scenarios such as headwind, tailwind, and crosswind must be interpreted
  through explicit vectors that follow these conventions rather than informal
  natural-language direction labels alone.

## World Frame
- Use a right-handed world frame.
- `+X` is the nominal forward direction for the baseline single-scull setup.
- `+Y` points to starboard.
- `+Z` points upward.
- Gravity acts in the `-Z` direction.

## Body and Local Frames
- The baseline hull body frame is right-handed and coincides with the world
  frame when the boat is initialized at zero orientation.
- Port corresponds to negative `Y`; starboard corresponds to positive `Y`.
- Positive yaw, pitch, and roll follow the right-hand rule about `+Z`, `+Y`,
  and `+X` respectively when derived from the documented orientation state.
- Configuration-defined local axes such as the seat rail direction must be
  normalized and reported explicitly when they appear at runtime boundaries.

## Orientation and State Representation
- The canonical orientation state should be represented as a unit quaternion in
  runtime contracts and machine-readable outputs unless a narrower interface
  documents a different internal-only representation.
- If Euler angles are emitted for diagnostics or user-facing summaries, they
  must be documented as derived quantities with an explicit rotation order.
- Vector states and load quantities emitted at simulator boundaries must carry
  both SI units and reference-frame annotations.

## Wind and Load Interpretation
- Wind inputs represent air-velocity vectors in the world frame.
- Apparent wind is the local air velocity relative to the moving body point or
  aggregate state under evaluation.
- A headwind case uses an air-velocity vector with a negative `X` component in
  the world frame; a tailwind case uses a positive `X` component.
- Crosswind direction cases must state the explicit world-frame vector so that
  mirrored port and starboard cases are unambiguous.

## Guardrails
- Do not mix frame or sign conventions silently across subsystems.
- Do not emit boundary-visible vector quantities without frame annotations.
- Do not rely on colloquial direction words alone when scenario or regression
  data needs a precise sign convention.
