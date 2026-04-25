#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/provider_catalog.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_output.hpp"

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <H5Apublic.h>
#include <H5Dpublic.h>
#include <H5Fpublic.h>
#include <H5Gpublic.h>
#include <H5Ppublic.h>
#include <H5Spublic.h>
#include <H5Tpublic.h>
#endif

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

project::MechanicalStateSnapshot make_state(double time_s,
                                            double boat_speed_mps,
                                            double port_slip_speed_mps,
                                            double starboard_slip_speed_mps) {
  return {
      .time_s = time_s,
      .hull =
          {
              .position_world_m = {.x = boat_speed_mps * time_s,
                                   .y = 0.0,
                                   .z = 0.0},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = {.x = boat_speed_mps,
                                            .y = 0.0,
                                            .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = 0.1,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = time_s,
                                             .y = -0.82,
                                             .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = -port_slip_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.12,
          },
      .starboard_oar =
          {
              .handle_angle_rad = 0.1,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = time_s, .y = 0.82, .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = -starboard_slip_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.12,
          },
      .seat =
          {
              .rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
              .position_along_rail_m = 0.0,
              .velocity_along_rail_mps = 0.0,
          },
      .stroke =
          {
              .phase = project::StrokePhase::drive,
              .phase_time_s = time_s,
              .cycle_time_s = time_s,
          },
      .constraint_residual_max = 0.0,
  };
}

project::SimulationRunResult make_supported_propulsion_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.metadata.config_id = "ut-propulsion-analysis";
  result.metadata.startup_status = "success";
  result.metadata.providers.blade_force = project::ProviderMetadata{
      .id = "stroke_propulsion_placeholder",
      .validity_id = "baseline-blade-force-v1",
      .validity_description =
          "Supported immersion-aware reduced blade-force baseline.",
  };

  result.state_history.push_back(make_state(0.0, 1.0, 0.6, 0.2));
  result.state_history.push_back(make_state(0.5, 1.5, 0.8, 1.0));
  result.state_history.push_back(make_state(1.0, 2.0, 0.4, 0.0));

  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .port_blade_force_world_n = {.x = 100.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 80.0, .y = 0.0, .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.5,
      .port_blade_force_world_n = {.x = 120.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 100.0, .y = 0.0, .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 1.0,
      .port_blade_force_world_n = {.x = 60.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 70.0, .y = 0.0, .z = 0.0},
  });
  return result;
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

struct RuntimeJsonPaths {
  std::string_view summary_path;
  std::string_view time_series_path;
};

Json make_runtime_hull_json() {
  return Json{{"mass_kg", 14.0},
              {"center_of_mass_m", Json::array({0.0, 0.0, 0.0})},
              {"inertia_kg_m2", Json::array({1.1, 7.8, 8.2})},
              {"initial_position_m", Json::array({0.0, 0.0, 0.0})},
              {"initial_orientation_xyzw", Json::array({0.0, 0.0, 0.0, 1.0})},
              {"initial_linear_velocity_mps", Json::array({0.8, 0.0, 0.0})},
              {"initial_angular_velocity_radps", Json::array({0.0, 0.0, 0.0})}};
}

Json make_runtime_oars_json() {
  return Json{
      {"port", Json{{"inboard_length_m", 0.88},
                    {"outboard_length_m", 1.98},
                    {"oarlock_position_m", Json::array({0.25, -0.82, 0.18})}}},
      {"starboard",
       Json{{"inboard_length_m", 0.88},
            {"outboard_length_m", 1.98},
            {"oarlock_position_m", Json::array({0.25, 0.82, 0.18})}}},
  };
}

Json make_runtime_seat_json() {
  return Json{{"rail_axis", Json::array({1.0, 0.0, 0.0})},
              {"min_position_m", -0.4},
              {"max_position_m", 0.4},
              {"initial_position_m", 0.0}};
}

Json make_runtime_stroke_json(std::string_view actuation_mode) {
  return Json{{"cycle_duration_s", 1.2},
              {"drive_duration_s", 0.48},
              {"catch_angle_rad", -0.9},
              {"release_angle_rad", 0.6},
              {"drive_blade_depth_m", 0.12},
              {"recovery_blade_depth_m", 0.0},
              {"actuation", Json{{"mode", actuation_mode}}}};
}

void apply_actuation_mode_overrides(Json &root,
                                    std::string_view actuation_mode) {
  auto &actuation = root["stroke"]["actuation"];
  if (actuation_mode == "force_driven") {
    actuation["peak_drive_force_n"] = 420.0;
  }
  if (actuation_mode == "power_driven") {
    actuation["peak_drive_power_w"] = 650.0;
    actuation["power_mode_speed_floor_mps"] = 0.35;
  }
}

Json make_runtime_config_json(std::string_view config_id,
                              const RuntimeJsonPaths &paths,
                              std::string_view blade_force_provider,
                              std::string_view actuation_mode) {
  Json root{
      {"config_id", config_id},
      {"simulation", Json{{"duration_s", 1.2}, {"time_step_s", 0.12}}},
      {"hull", make_runtime_hull_json()},
      {"oars", make_runtime_oars_json()},
      {"seat", make_runtime_seat_json()},
      {"stroke", make_runtime_stroke_json(actuation_mode)},
      {"providers", Json{{"hull_resistance", "quadratic_drag_placeholder"},
                         {"blade_force", blade_force_provider},
                         {"aero_load", "none"}}},
      {"output", Json{{"summary_path", paths.summary_path},
                      {"time_series_path", paths.time_series_path},
                      {"formats", Json::array({"json"})},
                      {"high_frequency_time_series", true}}},
  };
  apply_actuation_mode_overrides(root, actuation_mode);
  return root;
}

Json make_hdf5_config_json(std::string_view config_id,
                           std::string_view hdf5_path) {
  auto root =
      make_runtime_config_json(config_id, RuntimeJsonPaths{"", ""},
                               "stroke_propulsion_placeholder", "power_driven");
  root["output"] = Json{{"formats", Json::array({"hdf5"})},
                        {"hdf5_path", hdf5_path},
                        {"high_frequency_time_series", true}};
  return root;
}

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
class H5ScopedHandle final {
public:
  H5ScopedHandle() = default;

  H5ScopedHandle(hid_t value, herr_t (*close_fn)(hid_t)) noexcept
      : id(value), closer(close_fn) {}

  ~H5ScopedHandle() { reset(); }

  H5ScopedHandle(const H5ScopedHandle &) = delete;
  H5ScopedHandle &operator=(const H5ScopedHandle &) = delete;

  H5ScopedHandle(H5ScopedHandle &&other) noexcept
      : id(other.id), closer(other.closer) {
    other.id = -1;
    other.closer = nullptr;
  }

  H5ScopedHandle &operator=(H5ScopedHandle &&other) noexcept {
    if (this != &other) {
      reset();
      id = other.id;
      closer = other.closer;
      other.id = -1;
      other.closer = nullptr;
    }
    return *this;
  }

  [[nodiscard]] bool valid() const noexcept { return id >= 0; }

  void reset() noexcept {
    if (id >= 0 && closer != nullptr) {
      static_cast<void>(closer(id));
    }
    id = -1;
    closer = nullptr;
  }

  hid_t id{-1};

private:
  herr_t (*closer)(hid_t){nullptr};
};

std::string read_hdf5_string_attribute(hid_t object, const char *name) {
  const H5ScopedHandle attribute(H5Aopen(object, name, H5P_DEFAULT), H5Aclose);
  EXPECT_TRUE(attribute.valid());
  const H5ScopedHandle type(H5Aget_type(attribute.id), H5Tclose);
  EXPECT_TRUE(type.valid());
  const auto size = static_cast<std::size_t>(H5Tget_size(type.id));
  std::string value(size, '\0');
  EXPECT_GE(H5Aread(attribute.id, type.id, value.data()), 0);
  const auto null_pos = value.find('\0');
  if (null_pos != std::string::npos) {
    value.resize(null_pos);
  }
  return value;
}

int read_hdf5_int_attribute(hid_t object, const char *name) {
  const H5ScopedHandle attribute(H5Aopen(object, name, H5P_DEFAULT), H5Aclose);
  EXPECT_TRUE(attribute.valid());
  int value = 0;
  EXPECT_GE(H5Aread(attribute.id, H5T_NATIVE_INT, &value), 0);
  return value;
}

std::vector<double> read_hdf5_double_dataset(hid_t group, const char *name) {
  const H5ScopedHandle dataset(H5Dopen2(group, name, H5P_DEFAULT), H5Dclose);
  EXPECT_TRUE(dataset.valid());
  const H5ScopedHandle space(H5Dget_space(dataset.id), H5Sclose);
  EXPECT_TRUE(space.valid());
  hsize_t dims[1] = {0};
  EXPECT_EQ(H5Sget_simple_extent_ndims(space.id), 1);
  EXPECT_GE(H5Sget_simple_extent_dims(space.id, dims, nullptr), 0);
  std::vector<double> values(static_cast<std::size_t>(dims[0]), 0.0);
  if (!values.empty()) {
    EXPECT_GE(H5Dread(dataset.id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL,
                      H5P_DEFAULT, values.data()),
              0);
  }
  return values;
}

std::vector<int> read_hdf5_int_dataset(hid_t group, const char *name) {
  const H5ScopedHandle dataset(H5Dopen2(group, name, H5P_DEFAULT), H5Dclose);
  EXPECT_TRUE(dataset.valid());
  const H5ScopedHandle space(H5Dget_space(dataset.id), H5Sclose);
  EXPECT_TRUE(space.valid());
  hsize_t dims[1] = {0};
  EXPECT_EQ(H5Sget_simple_extent_ndims(space.id), 1);
  EXPECT_GE(H5Sget_simple_extent_dims(space.id, dims, nullptr), 0);
  std::vector<int> values(static_cast<std::size_t>(dims[0]), 0);
  if (!values.empty()) {
    EXPECT_GE(H5Dread(dataset.id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                      values.data()),
              0);
  }
  return values;
}
#endif

} // namespace

/**
 * @test UT-348
 * @verifies [D-030]
 * @notes Given finite propulsion-capable state and load samples, when the
 * derived run analysis computes propulsion metrics, then it reports stable
 * slip, work, efficiency, and support metadata from the existing run result.
 */
TEST(PropulsionMetrics, DerivesSlipWorkEfficiencyAndAvailability) {
  const auto result = make_supported_propulsion_result();

  const auto metrics = project::analyze_propulsion_metrics(result);
  const auto analysis = project::analyze_run_result(result);

  EXPECT_TRUE(metrics.availability.supported);
  EXPECT_EQ(metrics.availability.reason, "");
  EXPECT_EQ(metrics.availability.provider_id, "stroke_propulsion_placeholder");
  EXPECT_EQ(metrics.availability.provider_validity_id,
            "baseline-blade-force-v1");
  EXPECT_DOUBLE_EQ(metrics.run_metrics.mean_port_blade_slip_speed_mps, 0.6);
  EXPECT_DOUBLE_EQ(metrics.run_metrics.peak_port_blade_slip_speed_mps, 0.8);
  EXPECT_DOUBLE_EQ(metrics.run_metrics.mean_starboard_blade_slip_speed_mps,
                   0.4);
  EXPECT_DOUBLE_EQ(metrics.run_metrics.peak_starboard_blade_slip_speed_mps,
                   1.0);
  EXPECT_DOUBLE_EQ(metrics.run_metrics.effective_propulsive_work_j, 275.0);
  EXPECT_DOUBLE_EQ(metrics.run_metrics.slip_loss_work_j, 123.0);
  EXPECT_NEAR(metrics.run_metrics.propulsion_efficiency, 275.0 / 398.0, 1e-12);
  EXPECT_EQ(analysis.propulsion_metrics, metrics);
}

/**
 * @test UT-349
 * @verifies [D-030]
 * @notes Given a run whose blade-force provider cannot support propulsion
 * metrics, when propulsion analysis runs, then the support block is marked
 * unsupported with a deterministic reason and the human-readable report
 * records that status.
 */
TEST(PropulsionMetrics, ReportsUnsupportedProviderDeterministically) {
  auto result = make_supported_propulsion_result();
  result.metadata.providers.blade_force = project::ProviderMetadata{
      .id = "none",
      .validity_id = "not_applicable",
      .validity_description = "No blade-force runtime provider is selected.",
  };

  const auto metrics = project::analyze_propulsion_metrics(result);
  const auto report = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::full);

  EXPECT_FALSE(metrics.availability.supported);
  EXPECT_EQ(metrics.availability.provider_id, "none");
  EXPECT_EQ(metrics.availability.provider_validity_id, "not_applicable");
  EXPECT_EQ(metrics.availability.reason,
            "blade-force provider does not support propulsion metrics");
  EXPECT_NE(report.find("Propulsion Metrics"), std::string::npos);
  EXPECT_NE(report.find("supported=false"), std::string::npos);
  EXPECT_NE(
      report.find("blade-force provider does not support propulsion metrics"),
      std::string::npos);
}

/**
 * @test UT-350
 * @verifies [D-021, D-022]
 * @notes Given a propulsion-capable run with JSON output enabled, when the
 * structured summary and time-series artifacts are emitted, then both include
 * stable propulsion-metric definitions, support metadata, and per-record
 * propulsion channels.
 */
TEST(PropulsionMetrics, EmitsJsonSummaryAndTimeSeriesBlocks) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-propulsion-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-propulsion-timeseries.json";
  const auto config_path = std::filesystem::temp_directory_path() /
                           "airow-ut-propulsion-config.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(config_path);

  std::ofstream output(config_path, std::ios::binary | std::ios::trunc);
  output << make_runtime_config_json(
                "ut-propulsion-json",
                RuntimeJsonPaths{summary_path.string(),
                                 time_series_path.string()},
                "stroke_propulsion_placeholder", "power_driven")
                .dump(2);
  output.close();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 9h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto summary = read_json_file(summary_path);
  const auto time_series = read_json_file(time_series_path);
  const auto &propulsion_metrics =
      summary.at("analysis").at("propulsion_metrics");
  const auto &availability = propulsion_metrics.at("availability");
  const auto &definitions = propulsion_metrics.at("definitions");
  const auto &run_metrics = propulsion_metrics.at("run_metrics");

  EXPECT_TRUE(availability.at("supported").get<bool>());
  EXPECT_EQ(availability.at("reason").get<std::string>(), "");
  EXPECT_EQ(availability.at("provider_id").get<std::string>(),
            "stroke_propulsion_placeholder");
  EXPECT_EQ(availability.at("provider_validity_id").get<std::string>(),
            "baseline-blade-force-v1");
  EXPECT_TRUE(definitions.contains("port_blade_slip_speed_mps"));
  EXPECT_TRUE(definitions.contains("effective_propulsive_power_w"));
  EXPECT_TRUE(run_metrics.contains("effective_propulsive_work_j"));
  EXPECT_TRUE(run_metrics.contains("slip_loss_work_j"));
  EXPECT_TRUE(run_metrics.contains("propulsion_efficiency"));

  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());
  for (const auto &record : records) {
    const auto &metrics = record.at("propulsion_metrics");
    EXPECT_TRUE(metrics.at("supported").get<bool>());
    EXPECT_FALSE(metrics.contains("reason"));
    EXPECT_TRUE(metrics.contains("port_blade_slip_speed_mps"));
    EXPECT_TRUE(metrics.contains("starboard_blade_slip_speed_mps"));
    EXPECT_TRUE(metrics.contains("effective_propulsive_power_w"));
    EXPECT_TRUE(metrics.contains("slip_loss_power_w"));
    EXPECT_TRUE(metrics.contains("propulsion_efficiency"));
  }

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-351
 * @verifies [D-021, D-022]
 * @notes Given a run whose blade-force path cannot support propulsion
 * metrics, when JSON output is emitted, then summary availability metadata and
 * per-record support flags distinguish unsupported metrics from numeric zero.
 */
TEST(PropulsionMetrics, EmitsUnsupportedJsonSupportFlagsForNonPropulsionPath) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-propulsion-unsupported-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() /
      "airow-ut-propulsion-unsupported-timeseries.json";
  const auto config_path = std::filesystem::temp_directory_path() /
                           "airow-ut-propulsion-unsupported-config.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(config_path);

  std::ofstream output(config_path, std::ios::binary | std::ios::trunc);
  output << make_runtime_config_json(
                "ut-propulsion-unsupported",
                RuntimeJsonPaths{summary_path.string(),
                                 time_series_path.string()},
                "none", "prescribed_kinematic")
                .dump(2);
  output.close();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 10h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto summary = read_json_file(summary_path);
  const auto time_series = read_json_file(time_series_path);
  const auto &availability =
      summary.at("analysis").at("propulsion_metrics").at("availability");

  EXPECT_FALSE(availability.at("supported").get<bool>());
  EXPECT_EQ(availability.at("provider_id").get<std::string>(), "none");
  EXPECT_EQ(availability.at("provider_validity_id").get<std::string>(),
            "not_applicable");
  EXPECT_EQ(availability.at("reason").get<std::string>(),
            "blade-force provider does not support propulsion metrics");

  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());
  for (const auto &record : records) {
    const auto &metrics = record.at("propulsion_metrics");
    EXPECT_FALSE(metrics.at("supported").get<bool>());
    EXPECT_EQ(metrics.at("reason").get<std::string>(),
              "blade-force provider does not support propulsion metrics");
    EXPECT_TRUE(metrics.at("port_blade_slip_speed_mps").is_null());
    EXPECT_TRUE(metrics.at("starboard_blade_slip_speed_mps").is_null());
    EXPECT_TRUE(metrics.at("effective_propulsive_power_w").is_null());
    EXPECT_TRUE(metrics.at("slip_loss_power_w").is_null());
    EXPECT_TRUE(metrics.at("propulsion_efficiency").is_null());
  }

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-352
 * @verifies [D-021, D-022]
 * @notes Given HDF5 output enabled on an HDF5-capable build, when a
 * propulsion-capable run emits the artifact, then the summary and time-series
 * groups carry the same propulsion-metric channels plus deterministic support
 * metadata attributes.
 */
TEST(PropulsionMetrics, EmitsHdf5SummaryAndTimeSeriesDatasets) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-ut-propulsion.h5";
  const auto config_path =
      std::filesystem::temp_directory_path() / "airow-ut-propulsion-hdf5.json";
  remove_file_if_present(hdf5_path);
  remove_file_if_present(config_path);

  std::ofstream output(config_path, std::ios::binary | std::ios::trunc);
  output << make_hdf5_config_json("ut-propulsion-hdf5", hdf5_path.string())
                .dump(2);
  output.close();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 11h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.hdf5_written);

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  const H5ScopedHandle summary_group(
      H5Gopen2(file.id, "/summary/propulsion_metrics", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(summary_group.valid());
  EXPECT_EQ(read_hdf5_int_attribute(summary_group.id, "supported"), 1);
  EXPECT_EQ(read_hdf5_string_attribute(summary_group.id, "provider_id"),
            "stroke_propulsion_placeholder");
  EXPECT_EQ(
      read_hdf5_string_attribute(summary_group.id, "provider_validity_id"),
      "baseline-blade-force-v1");
  EXPECT_EQ(read_hdf5_string_attribute(summary_group.id, "reason"), "");
  const auto effective_work =
      read_hdf5_double_dataset(summary_group.id, "effective_propulsive_work_j");
  const auto slip_loss_work =
      read_hdf5_double_dataset(summary_group.id, "slip_loss_work_j");
  ASSERT_EQ(effective_work.size(), 1U);
  ASSERT_EQ(slip_loss_work.size(), 1U);
  EXPECT_GT(effective_work.front(), 0.0);
  EXPECT_GT(slip_loss_work.front(), 0.0);

  const H5ScopedHandle time_series_group(
      H5Gopen2(file.id, "/time_series", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(time_series_group.valid());
  const auto supported = read_hdf5_int_dataset(time_series_group.id,
                                               "propulsion_metrics_supported");
  const auto port_slip = read_hdf5_double_dataset(time_series_group.id,
                                                  "port_blade_slip_speed_mps");
  const auto efficiency =
      read_hdf5_double_dataset(time_series_group.id, "propulsion_efficiency");
  ASSERT_EQ(supported.size(), result.state_history.size());
  ASSERT_EQ(port_slip.size(), result.state_history.size());
  ASSERT_EQ(efficiency.size(), result.state_history.size());
  EXPECT_TRUE(std::all_of(supported.begin(), supported.end(),
                          [](int value) { return value == 1; }));
#endif

  remove_file_if_present(config_path);
  remove_file_if_present(hdf5_path);
}

/**
 * @test UT-356
 * @verifies [D-030]
 * @notes Given an invalid propulsion comparison window, when propulsion
 * analysis runs, then it reports deterministic unsupported metadata rather
 * than integrating over an ill-formed interval.
 */
TEST(PropulsionMetrics, RejectsInvalidComparisonWindow) {
  const auto result = make_supported_propulsion_result();

  const auto metrics = project::analyze_propulsion_metrics(
      result,
      project::PropulsionMetricWindow{.start_time_s = 0.7, .end_time_s = 0.7});

  EXPECT_FALSE(metrics.availability.supported);
  EXPECT_EQ(metrics.availability.reason,
            "comparison window must have end_time_s greater than start_time_s");
}

/**
 * @test UT-357
 * @verifies [D-021, D-022]
 * @notes Given a propulsion-capable provider but non-finite state samples,
 * when JSON outputs are emitted, then the per-record propulsion block remains
 * explicitly unsupported instead of collapsing to numeric zero.
 */
TEST(PropulsionMetrics, EmitsUnsupportedRecordWhenSampleIsNonFinite) {
  auto result = make_supported_propulsion_result();
  result.state_history.at(1).port_oar.blade_tip_velocity_world_mps.x =
      std::numeric_limits<double>::quiet_NaN();

  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-propulsion-nan-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-propulsion-nan-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  project::SimulatorConfig config;
  config.config_id = "ut-propulsion-nan";
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = true;
  config.output.emit_json = true;
  config.output.emit_hdf5 = false;

  project::emit_run_outputs(config, result);

  ASSERT_TRUE(result.ok());
  const auto time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_GE(records.size(), 2U);
  const auto &metrics = records.at(1).at("propulsion_metrics");
  EXPECT_FALSE(metrics.at("supported").get<bool>());
  EXPECT_EQ(metrics.at("reason").get<std::string>(),
            "propulsion metrics require finite state and load samples");
  EXPECT_TRUE(metrics.at("port_blade_slip_speed_mps").is_null());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}
