#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_output.hpp"

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <H5Dpublic.h>
#include <H5Fpublic.h>
#include <H5Gpublic.h>
#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5Spublic.h>
#include <H5Tpublic.h>
#endif

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

struct EnergyStateSample {
  double time_s{};
  double hull_speed_mps{};
  double yaw_rate_radps{};
  double rower_speed_mps{};
  double blade_speed_mps{};
};

project::MechanicalStateSnapshot make_energy_state(
    const EnergyStateSample &sample) {
  return {
      .time_s = sample.time_s,
      .hull =
          {
              .position_world_m = {.x = sample.time_s, .y = 0.0, .z = 0.0},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = {.x = sample.hull_speed_mps,
                                            .y = 0.0,
                                            .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0,
                                              .y = 0.0,
                                              .z = sample.yaw_rate_radps},
          },
      .port_oar =
          {
              .handle_angle_rad = 0.0,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = sample.time_s,
                                             .y = -1.0,
                                             .z = 0.0},
              .blade_tip_velocity_world_mps = {.x = sample.blade_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .starboard_oar =
          {
              .handle_angle_rad = 0.0,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = sample.time_s,
                                             .y = 1.0,
                                             .z = 0.0},
              .blade_tip_velocity_world_mps = {.x = sample.blade_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .seat = {.rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0}},
      .stroke = {},
      .rower_center_of_mass_velocity_world_mps = {.x = sample.rower_speed_mps,
                                                  .y = 0.0,
                                                  .z = 0.0},
  };
}

project::SimulationRunResult make_energy_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.metadata.config_id = "ut-energy";
  result.metadata.simulator_version = "0.2.0";
  result.metadata.actuation_mode = "power_driven";
  result.metadata.rower_coupling_enabled = true;
  result.metadata.hull_mass_kg = 10.0;
  result.metadata.hull_inertia_kg_m2 = {.x = 1.0, .y = 1.5, .z = 2.0};
  result.metadata.rower_mass_kg = 80.0;
  result.metadata.providers.blade_force.id = "stroke_propulsion_placeholder";
  result.metadata.providers.blade_force.validity_id =
      "baseline-blade-force-v1";
  result.state_history.push_back(make_energy_state(
      {.time_s = 0.0,
       .hull_speed_mps = 1.0,
       .yaw_rate_radps = 1.0,
       .rower_speed_mps = 0.5,
       .blade_speed_mps = -1.0}));
  result.state_history.push_back(make_energy_state(
      {.time_s = 1.0,
       .hull_speed_mps = 3.0,
       .yaw_rate_radps = 2.0,
       .rower_speed_mps = 1.5,
       .blade_speed_mps = -1.0}));
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .commanded_power_w = 100.0,
      .hull_force_world_n = {.x = -3.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = 10.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 10.0, .y = 0.0, .z = 0.0},
      .aero_force_world_n = {.x = -2.0, .y = 0.0, .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 1.0,
      .commanded_power_w = 300.0,
      .hull_force_world_n = {.x = -6.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = 20.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 10.0, .y = 0.0, .z = 0.0},
      .aero_force_world_n = {.x = -4.0, .y = 0.0, .z = 0.0},
  });
  return result;
}

project::SimulatorConfig make_power_config(std::string_view config_id) {
  project::SimulatorConfig config;
  config.config_id = std::string(config_id);
  config.simulation.duration_s = 0.5;
  config.simulation.time_step_s = 0.25;
  config.hull.mass_kg = 14.0;
  config.hull.center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0};
  config.hull.inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2};
  config.hull.initial_orientation_xyzw = {.x = 0.0,
                                          .y = 0.0,
                                          .z = 0.0,
                                          .w = 1.0};
  config.oars.port.inboard_length_m = 0.88;
  config.oars.port.outboard_length_m = 1.98;
  config.oars.port.oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18};
  config.oars.starboard.inboard_length_m = 0.88;
  config.oars.starboard.outboard_length_m = 1.98;
  config.oars.starboard.oarlock_position_m = {.x = 0.25,
                                              .y = 0.82,
                                              .z = 0.18};
  config.seat.rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0};
  config.seat.min_position_m = -0.4;
  config.seat.max_position_m = 0.4;
  config.stroke.cycle_duration_s = 1.2;
  config.stroke.drive_duration_s = 0.48;
  config.stroke.catch_angle_rad = -0.9;
  config.stroke.release_angle_rad = 0.6;
  config.stroke.actuation.mode = "power_driven";
  config.stroke.actuation.peak_drive_power_w = 650.0;
  config.stroke.actuation.power_mode_speed_floor_mps = 0.35;
  config.stroke.rower_coupling.enabled = true;
  config.stroke.rower_coupling.rower_mass_kg = 80.0;
  config.stroke.rower_coupling.body_center_of_mass_m = {.x = -0.1,
                                                        .y = 0.0,
                                                        .z = 0.35};
  config.stroke.rower_coupling.seat_position_to_com_scale = 1.0;
  config.providers.hull_resistance = "quadratic_drag_placeholder";
  config.providers.blade_force = "stroke_propulsion_placeholder";
  config.providers.aero_load = "steady_wind_placeholder";
  config.output.emit_json = true;
  config.output.high_frequency_time_series = true;
  return config;
}

class FixedClock final : public project::Clock {
public:
  explicit FixedClock(
      std::vector<std::chrono::system_clock::time_point> instants)
      : instants_(std::move(instants)) {}

  std::chrono::system_clock::time_point now_utc() override {
    if (index_ >= instants_.size()) {
      return instants_.back();
    }
    return instants_[index_++];
  }

private:
  std::vector<std::chrono::system_clock::time_point> instants_;
  std::size_t index_{0};
};

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
class H5ScopedHandle final {
public:
  H5ScopedHandle() = default;
  H5ScopedHandle(hid_t value, herr_t (*close_fn)(hid_t)) noexcept
      : id(value), close_fn_(close_fn) {}
  ~H5ScopedHandle() { reset(); }
  H5ScopedHandle(const H5ScopedHandle &) = delete;
  H5ScopedHandle &operator=(const H5ScopedHandle &) = delete;

  [[nodiscard]] bool valid() const noexcept { return id >= 0; }

  void reset() noexcept {
    if (valid() && close_fn_ != nullptr) {
      close_fn_(id);
    }
    id = -1;
    close_fn_ = nullptr;
  }

  hid_t id{-1};

private:
  herr_t (*close_fn_)(hid_t){nullptr};
};

std::vector<double> read_hdf5_double_dataset(hid_t group, const char *name) {
  const H5ScopedHandle dataset(H5Dopen2(group, name, H5P_DEFAULT), H5Dclose);
  EXPECT_TRUE(dataset.valid());
  const H5ScopedHandle space(H5Dget_space(dataset.id), H5Sclose);
  EXPECT_TRUE(space.valid());
  const hssize_t count = H5Sget_simple_extent_npoints(space.id);
  std::vector<double> values(static_cast<std::size_t>(count));
  EXPECT_GE(H5Dread(dataset.id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, values.data()),
            0);
  return values;
}
#endif

} // namespace

/**
 * @test UT-383
 * @verifies [D-064]
 * @case nominal
 * @oracle accounting
 * @notes Given a minimal power-driven run with finite state and load samples,
 * when reduced energy accounting is analyzed, then rower input work, blade
 * work, kinetic-energy deltas, losses, residual status, and the unavailable
 * oar kinetic-energy term are reported deterministically.
 */
TEST(EnergyAccounting, DerivesReducedTermsAndResidual) {
  const auto accounting =
      project::analyze_energy_accounting(make_energy_result());

  EXPECT_TRUE(accounting.rower_input_work.supported);
  EXPECT_TRUE(accounting.blade_work.supported);
  EXPECT_TRUE(accounting.hull_kinetic_energy_change.supported);
  EXPECT_TRUE(accounting.rower_seat_kinetic_energy_change.supported);
  EXPECT_FALSE(accounting.oar_kinetic_energy_change.supported);
  EXPECT_DOUBLE_EQ(accounting.run_metrics.rower_input_work_j, 200.0);
  EXPECT_DOUBLE_EQ(accounting.run_metrics.blade_work_j, 80.0);
  EXPECT_DOUBLE_EQ(accounting.run_metrics.hull_kinetic_energy_change_j, 43.0);
  EXPECT_DOUBLE_EQ(accounting.run_metrics.rower_seat_kinetic_energy_change_j,
                   80.0);
  EXPECT_DOUBLE_EQ(accounting.run_metrics.aerodynamic_loss_j, 7.0);
  EXPECT_DOUBLE_EQ(accounting.run_metrics.hull_water_loss_j, 10.5);
  EXPECT_EQ(accounting.energy_residual.support_status,
            "reported_unbounded_reduced_model");
  EXPECT_NE(accounting.oar_kinetic_energy_change.reason.find("not modeled"),
            std::string::npos);
}

/**
 * @test UT-384
 * @verifies [D-064]
 * @case equivalence
 * @oracle diagnostic
 * @notes Given a finite run that is not power-driven and has no enabled rower
 * coupling, when reduced energy accounting is analyzed, then rower input,
 * rower/seat kinetic energy, and oar kinetic energy remain unavailable with
 * deterministic reasons while supported reduced boat terms remain available.
 */
TEST(EnergyAccounting, ReportsUnsupportedRowerAndOarTermsDeterministically) {
  auto result = make_energy_result();
  result.metadata.actuation_mode = "force_driven";
  result.metadata.rower_coupling_enabled = false;
  result.metadata.rower_mass_kg = 0.0;

  const auto accounting = project::analyze_energy_accounting(result);

  EXPECT_FALSE(accounting.rower_input_work.supported);
  EXPECT_FALSE(accounting.rower_seat_kinetic_energy_change.supported);
  EXPECT_FALSE(accounting.oar_kinetic_energy_change.supported);
  EXPECT_TRUE(accounting.blade_work.supported);
  EXPECT_TRUE(accounting.hull_kinetic_energy_change.supported);
  EXPECT_NE(accounting.rower_input_work.reason.find("power_driven"),
            std::string::npos);
  EXPECT_NE(accounting.rower_seat_kinetic_energy_change.reason.find(
                "enabled rower coupling"),
            std::string::npos);
}

/**
 * @test UT-388
 * @verifies [D-064]
 * @case edge
 * @oracle diagnostic
 * @notes Given invalid energy-accounting inputs, when the analyzer receives an
 * invalid time window or a blade provider without propulsion support, then it
 * reports deterministic unsupported diagnostics instead of implying complete
 * accounting.
 */
TEST(EnergyAccounting, ReportsInvalidWindowAndUnsupportedBladeProvider) {
  auto result = make_energy_result();

  const auto invalid_window = project::analyze_energy_accounting(
      result, project::EnergyAccountingWindow{.start_time_s = 1.0,
                                              .end_time_s = 0.5});
  EXPECT_FALSE(invalid_window.availability.supported);
  EXPECT_NE(invalid_window.availability.reason.find("end_time_s"),
            std::string::npos);

  result.metadata.providers.blade_force.id = "none";
  const auto unsupported_provider = project::analyze_energy_accounting(result);
  EXPECT_FALSE(unsupported_provider.blade_work.supported);
  EXPECT_NE(unsupported_provider.blade_work.reason.find("blade-force provider"),
            std::string::npos);
}

/**
 * @test UT-385
 * @verifies [D-064]
 * @case nominal
 * @oracle exact
 * @notes Given a successful power-driven file-backed run, when JSON outputs
 * and the human-readable report are produced, then summary analysis,
 * time-series records, and report text expose the Energy Flow contract.
 */
TEST(EnergyAccounting, EmitsJsonAndReportEnergyFlowContract) {
  auto config = make_power_config("ut-energy-output");
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-energy-output-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-energy-output-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 27} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 27} + 8h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto summary = read_json_file(summary_path);
  const auto time_series = read_json_file(time_series_path);
  ASSERT_TRUE(summary.at("analysis").contains("energy_accounting"));
  EXPECT_EQ(summary.at("analysis")
                .at("energy_accounting")
                .at("terms")
                .at("oar_kinetic_energy_change_j")
                .at("support_status")
                .get<std::string>(),
            "unavailable");
  ASSERT_FALSE(time_series.at("records").empty());
  EXPECT_TRUE(time_series.at("records")
                  .front()
                  .at("energy_accounting")
                  .contains("blade_power_w"));

  const auto report = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::compact);
  EXPECT_NE(report.find("Energy Flow"), std::string::npos);
  EXPECT_NE(report.find("energy_residual_j"), std::string::npos);
  EXPECT_NE(report.find("unavailable_terms"), std::string::npos);

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-386
 * @verifies [D-064]
 * @case nominal
 * @oracle exact
 * @notes Given a successful power-driven HDF5-enabled run, when the artifact
 * is written on an HDF5-capable build, then summary energy accounting and
 * matching time-series power datasets are present.
 */
TEST(EnergyAccounting, EmitsHdf5EnergyAccountingGroupsWhenSupported) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_power_config("ut-energy-hdf5");
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-energy-hdf5-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-energy-hdf5-timeseries.json";
  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-ut-energy-hdf5.h5";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.hdf5_path = hdf5_path.string();
  config.output.emit_hdf5 = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 27} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 27} + 9h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  const H5ScopedHandle energy_group(
      H5Gopen2(file.id, "/summary/energy_accounting", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(energy_group.valid());
  const H5ScopedHandle time_series_group(
      H5Gopen2(file.id, "/time_series", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(time_series_group.valid());
  EXPECT_FALSE(
      read_hdf5_double_dataset(energy_group.id, "blade_work_j").empty());
  EXPECT_FALSE(
      read_hdf5_double_dataset(time_series_group.id, "blade_power_w").empty());
#endif

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
}
