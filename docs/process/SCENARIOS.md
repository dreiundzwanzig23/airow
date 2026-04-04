# SCENARIOS.md — Reference Scenario Baselines

This document records checked-in baseline scenario artifacts and their
acceptance-envelope intent.

## Active v0.1 Scenario Artifacts

### Passive Float (`scenarios/passive_float.json`)
- Scenario id: `passive-float`
- Current hydro provider mode: deterministic `passive_placeholder`
- Acceptance envelope:
  - `max_abs_distance_m = 1e-9`
  - `max_abs_mean_speed_mps = 1e-9`
- Verification lane: `QT-009`

### Tow Test (`scenarios/tow_test.json`)
- Scenario id: `tow-test`
- Current hydro provider mode: deterministic `tow_placeholder`
- Provider parameter:
  - `drag_coefficient_n_s2_per_m2 = 6.0`
- Acceptance envelope:
  - `min_distance_m = 2.5`
  - `max_final_speed_mps = 1.0`
  - `drag_speed_samples_mps = [0.0, 0.5, 1.0, 2.0, 3.0]`
- Verification lane: `QT-010` plus deterministic replay check `QT-011`

### Calm-Water Stroke (`scenarios/calm_water_stroke.json`)
- Scenario id: `calm-water-stroke`
- Current hydro provider mode: deterministic `stroke_propulsion_placeholder`
- Provider parameter:
  - `blade_force_coefficient_n_s_per_m = 4.0`
- Acceptance envelope:
  - `min_distance_m = 1.5`
  - `min_mean_speed_mps = 0.6`
- Verification lane: `QT-012` plus propulsion-disabled comparison `QT-013`

### Headwind Stroke (`scenarios/headwind_stroke.json`)
- Scenario id: `headwind-stroke`
- Current hydro provider mode: deterministic `stroke_propulsion_placeholder`
- Current aero provider mode: deterministic `steady_wind_placeholder`
- Provider parameters:
  - `blade_force_coefficient_n_s_per_m = 4.0`
  - `drag_coefficient_n_s2_per_m2 = 1.5`
  - `yaw_moment_coefficient_n_m_s2_per_m2 = 0.75`
- Ambient wind:
  - `ambient_wind_world_mps = [-3.0, 0.0, 0.0]`
- Acceptance envelope:
  - `min_distance_m = 1.2`
  - `max_mean_speed_mps = 0.9`
- Verification lane: `QT-016` plus finite wind-channel check `QT-018`

### Crosswind Stroke (`scenarios/crosswind_stroke.json`)
- Scenario id: `crosswind-stroke`
- Current hydro provider mode: deterministic `stroke_propulsion_placeholder`
- Current aero provider mode: deterministic `steady_wind_placeholder`
- Provider parameters:
  - `blade_force_coefficient_n_s_per_m = 4.0`
  - `drag_coefficient_n_s2_per_m2 = 1.5`
  - `yaw_moment_coefficient_n_m_s2_per_m2 = 0.75`
- Ambient wind:
  - `ambient_wind_world_mps = [0.0, 2.0, 0.0]`
- Acceptance envelope:
  - `min_distance_m = 1.5`
  - `expected_yaw_moment_z_sign = positive`
  - `min_abs_yaw_moment_z_n_m = 0.8`
- Verification lane: `QT-017`
