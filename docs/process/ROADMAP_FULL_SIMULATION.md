# airow Full-Simulation Roadmap

This is the active long-range roadmap for `R-050..R-071`. Detailed acceptance
criteria live in `docs/process/REQUIREMENTS.md`; this file exists to keep the
sequence and product intent easy to load during agent work.

## Target Outcome
- Physically honest simulator output: every run states what is modeled,
  reduced, calibrated, validated, unavailable, or unsupported.
- Inspectable simulation artifacts: users can reason about state, frames,
  loads, energy, comparisons, and failure modes.
- Deterministic reduced runtime first, with optional high-fidelity truth-model
  workflows used for calibration and validation rather than the normal loop.

## Strategy
- Preserve the current reduced single-scull baseline unless a selected
  requirement explicitly changes numerical physics.
- Build observability and trust reporting before deeper physics or custom
  viewers.
- Prefer calibrated reduced models for routine local studies; keep CFD/SPH or
  full-3D water workflows offline and optional.
- Treat validation, provenance, uncertainty, and capability limits as product
  behavior rather than documentation afterthoughts.

## Phase Overview
| Phase | Theme | Primary outcome | Requirements |
|---|---|---|---|
| 1 | Trust and capability visibility | Every run/report/view explains supported claims | `R-035`, `R-049`, `R-071` |
| 2 | Visualization artifact foundation | Stable run artifacts for replay and comparison | `R-050`, `R-052`, `R-062`, `R-070` |
| 3 | Visual inspection tooling | Replay, scrub, plot, compare, export, and debug runs | `R-051`, `R-053`, `R-054`, `R-055`, `R-056` |
| 4 | Geometry foundation | Hull, blade, oar, rigging, and surfaces become assets | `R-038`, `R-057` |
| 5 | Coupled 6-DOF mechanics | Hull, rower, oars, gates, loads, and energy couple coherently | `R-039`, `R-040`, `R-058`, `R-060`, `R-061`, `R-062` |
| 6 | Realistic blade-water model | Blade loads become 3D, phase-aware, inspectable, and calibratable | `R-041`, `R-042`, `R-043`, `R-059` |
| 7 | Validation and calibration loop | Measured/truth data drive scorecards and provider lifecycle | `R-036`, `R-037`, `R-047`, `R-049`, `R-067`, `R-068` |
| 8 | Optimization-safe studies | Technique and hull studies become comparable and validity-aware | `R-045`, `R-046`, `R-054`, `R-069` |
| 9 | Environment and secondary realism | Waves, current, and flexible-oar readiness enter safely | `R-063`, `R-064` |
| 10 | Full-3D water reference workflow | Optional truth-model studies feed validation and reduced models | `R-048`, `R-065`, `R-066` |

## Recommended Near-Term Packet
- Clear or explicitly carry forward `Needs-Review` items before selecting new
  work.
- Continue Phase 1/2 by allocating `R-071`, `R-050`, `R-052`, `R-062`, and
  `R-070` against existing `A-*` owners before writing tests.
- Keep capability/trust surfaces and artifact contracts additive until a
  requirement explicitly calls for physics behavior changes.

## Anti-Goals
- Do not make visual output imply validated realism by itself.
- Do not add full-3D water tooling to the default runtime path.
- Do not duplicate requirement acceptance text here; update
  `docs/process/REQUIREMENTS.md` instead.
- Do not use archived roadmap snapshots as active implementation guidance.
