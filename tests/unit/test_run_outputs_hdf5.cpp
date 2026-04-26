#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/numerics/backend_catalog.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_output.hpp"

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <H5Apublic.h>
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

std::string expected_default_mechanics_backend_id() {
  return project::chrono_mechanics_backend_supported() ? "chrono_rigidbody"
                                                       : "internal_baseline";
}

std::string expected_default_mechanics_policy_id() {
  return project::chrono_mechanics_backend_supported() ? "chrono-rigidbody-v2"
                                                       : "internal-baseline-v1";
}

std::string expected_default_mechanics_policy_description() {
  return project::chrono_mechanics_backend_supported()
             ? "Preferred rigid-body mechanics backend for the standard "
               "runtime build."
             : "Internal deterministic mechanics backend for fallback and "
               "cross-check runtime operation.";
}

project::SimulatorConfig make_config(std::string_view config_id = "ut-output",
                                     double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = std::string(config_id),
      .simulation =
          {
              .duration_s = duration_s,
              .time_step_s = time_step_s,
          },
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .initial_linear_velocity_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .oars =
          {
              .port =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18},
                  },
              .starboard =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = 0.82, .z = 0.18},
                  },
          },
      .seat =
          {
              .rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0},
              .min_position_m = -0.4,
              .max_position_m = 0.4,
              .initial_position_m = 0.0,
          },
      .stroke =
          {
              .cycle_duration_s = 1.2,
              .drive_duration_s = 0.48,
              .catch_angle_rad = -0.9,
              .release_angle_rad = 0.6,
          },
      .environment = {},
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
              .truth_model_export_path = {},
              .high_frequency_time_series = false,
              .emit_json = true,
              .emit_hdf5 = false,
          },
  };
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

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

std::string read_binary_prefix(const std::filesystem::path &path,
                               std::size_t byte_count) {
  std::ifstream input(path, std::ios::binary);
  std::string bytes(byte_count, '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(byte_count));
  bytes.resize(static_cast<std::size_t>(input.gcount()));
  return bytes;
}

std::filesystem::path write_temp_marker_file(std::string_view file_name) {
  const auto path =
      std::filesystem::temp_directory_path() / std::string(file_name);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << "marker";
  output.close();
  return path;
}

std::string make_calibrated_hdf5_config_json(std::string_view config_id,
                                             std::string_view artifact_path,
                                             std::string_view hdf5_path) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 0.25,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [1.0, -0.5, 0.0],
          "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
        },
        "oars": {
          "port": {
            "inboard_length_m": 0.88,
            "outboard_length_m": 1.98,
            "oarlock_position_m": [0.25, -0.82, 0.18]
          },
          "starboard": {
            "inboard_length_m": 0.88,
            "outboard_length_m": 1.98,
            "oarlock_position_m": [0.25, 0.82, 0.18]
          }
        },
        "seat": {
          "rail_axis": [1.0, 0.0, 0.0],
          "min_position_m": -0.4,
          "max_position_m": 0.4,
          "initial_position_m": 0.0
        },
        "stroke": {
          "cycle_duration_s": 1.2,
          "drive_duration_s": 0.48,
          "catch_angle_rad": -0.9,
          "release_angle_rad": 0.6
        },
        "environment": {
          "wind_time_series": [
            {"time_s": 0.0, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]}
          ]
        },
        "providers": {
          "hull_resistance": "quadratic_drag_placeholder",
          "blade_force": "stroke_propulsion_placeholder",
          "aero_load": "steady_wind_calibrated"
        },
        "artifacts": {
          "calibration": {
            "path": ")"
         << artifact_path << R"("
          }
        },
        "output": {
          "formats": ["hdf5"],
          "hdf5_path": ")"
         << hdf5_path << R"("
        }
      })";
  return stream.str();
}

std::string
make_time_varying_hdf5_config_json(std::string_view config_id,
                                   std::string_view hdf5_path,
                                   std::string_view environment_json) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 0.5,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [1.0, -0.5, 0.0],
          "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
        },
        "oars": {
          "port": {
            "inboard_length_m": 0.88,
            "outboard_length_m": 1.98,
            "oarlock_position_m": [0.25, -0.82, 0.18]
          },
          "starboard": {
            "inboard_length_m": 0.88,
            "outboard_length_m": 1.98,
            "oarlock_position_m": [0.25, 0.82, 0.18]
          }
        },
        "seat": {
          "rail_axis": [1.0, 0.0, 0.0],
          "min_position_m": -0.4,
          "max_position_m": 0.4,
          "initial_position_m": 0.0
        },
        "stroke": {
          "cycle_duration_s": 1.2,
          "drive_duration_s": 0.48,
          "catch_angle_rad": -0.9,
          "release_angle_rad": 0.6
        },
        "environment": )"
         << environment_json << R"(,
        "providers": {
          "hull_resistance": "quadratic_drag_placeholder",
          "blade_force": "stroke_propulsion_placeholder",
          "aero_load": "steady_wind_placeholder"
        },
        "output": {
          "formats": ["hdf5"],
          "hdf5_path": ")"
         << hdf5_path << R"("
        }
      })";
  return stream.str();
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

private:
  void reset() noexcept {
    if (id >= 0 && closer != nullptr) {
      static_cast<void>(closer(id));
    }
    id = -1;
    closer = nullptr;
  }

public:
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

struct ExpectedBackendMetadata {
  const char *group_path;
  std::string_view id;
  std::string_view policy_id;
  std::string_view policy_description;
};

void expect_hdf5_backend_metadata(hid_t file_id,
                                  const ExpectedBackendMetadata &expected) {
  const H5ScopedHandle group(
      H5Gopen2(file_id, expected.group_path, H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(group.valid());
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "id"), expected.id);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "policy_id"),
            expected.policy_id);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "policy_description"),
            expected.policy_description);
}
#endif

} // namespace

/**
 * @test UT-048
 * @verifies [D-022, D-034, D-040]
 * @notes Given HDF5 output enabled on a supported build, when the run
 * executes, then deterministic HDF5 artifacts are emitted with the expected
 * file signature and structured state-advancer metadata.
 */
TEST(RunOutputsHdf5, EmitsHdf5ArtifactWhenEnabled) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5", 1.0, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-hdf5-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-hdf5-timeseries.json";
  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-ut-output-hdf5.h5";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.hdf5_path = hdf5_path.string();
  config.output.emit_json = true;
  config.output.emit_hdf5 = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  ASSERT_TRUE(result.outputs.hdf5_written);
  ASSERT_TRUE(std::filesystem::exists(hdf5_path));
  EXPECT_EQ(read_binary_prefix(hdf5_path, 8U),
            std::string("\x89HDF\r\n\x1a\n", 8U));

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  const H5ScopedHandle metadata_group(
      H5Gopen2(file.id, "/metadata", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(metadata_group.valid());
  EXPECT_EQ(read_hdf5_string_attribute(metadata_group.id,
                                       "state_advancement_solver_status"),
            "sundials-ida");
  expect_hdf5_backend_metadata(
      file.id,
      ExpectedBackendMetadata{
          .group_path = "/metadata/mechanics_backend",
          .id = expected_default_mechanics_backend_id(),
          .policy_id = expected_default_mechanics_policy_id(),
          .policy_description = expected_default_mechanics_policy_description(),
      });
  expect_hdf5_backend_metadata(
      file.id,
      ExpectedBackendMetadata{
          .group_path = "/metadata/integration_backend",
          .id = "sundials_ida",
          .policy_id = "sundials-ida-fixed-tolerances-v2",
          .policy_description =
              "Preferred constrained integration backend with fixed relative "
              "and absolute tolerances of 1e-10 for Chrono-backed runtime "
              "stepping.",
      });
#endif

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
}

/**
 * @test UT-231
 * @verifies [D-045]
 * @notes Given a calibrated run that emits HDF5 output, when the artifact is
 * written on an HDF5-capable build, then the imported external artifact
 * provenance is recorded under the metadata external-artifacts group.
 */
TEST(RunOutputsHdf5, EmitsExternalArtifactMetadataInHdf5Output) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  const auto artifact_path =
      write_temp_file("airow-ut-output-hdf5-calibration-artifact.json",
                      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-hdf5",
        "artifact_version": "2026-04-19",
        "content_hash": "sha256:ut-hdf5-calibration",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.5,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.5
          }
        }
      })");
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-hdf5-calibration.h5";
  remove_file_if_present(hdf5_path);
  const auto config_path =
      write_temp_file("airow-ut-output-hdf5-calibration-config.json",
                      make_calibrated_hdf5_config_json(
                          "ut-output-hdf5-calibration", artifact_path.string(),
                          hdf5_path.string()));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.hdf5_written);
  ASSERT_TRUE(std::filesystem::exists(hdf5_path));

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  const H5ScopedHandle artifact_group(
      H5Gopen2(file.id, "/metadata/external_artifacts/artifact_0", H5P_DEFAULT),
      H5Gclose);
  ASSERT_TRUE(artifact_group.valid());
  EXPECT_EQ(read_hdf5_string_attribute(artifact_group.id, "kind"),
            "calibration");
  EXPECT_EQ(read_hdf5_string_attribute(artifact_group.id, "usage"),
            "aero_load");
  EXPECT_EQ(read_hdf5_string_attribute(artifact_group.id, "source_id"),
            "wind-tunnel-hdf5");
  EXPECT_EQ(read_hdf5_string_attribute(artifact_group.id, "artifact_version"),
            "2026-04-19");
  EXPECT_EQ(read_hdf5_string_attribute(artifact_group.id, "content_hash"),
            "sha256:ut-hdf5-calibration");
  EXPECT_EQ(read_hdf5_string_attribute(artifact_group.id, "schema_id"),
            "steady_wind_aero_calibration.v1");
#endif

  remove_file_if_present(config_path);
  remove_file_if_present(artifact_path);
  remove_file_if_present(hdf5_path);
}

/**
 * @test UT-190
 * @verifies [D-022]
 * @notes Given HDF5-only output on a startup-failed run, when emission runs on
 * an HDF5-capable build, then the artifact is still created deterministically
 * with the HDF5-only schema contract.
 */
TEST(RunOutputsHdf5, EmitsHdf5ArtifactForStartupFailureWithoutSamples) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5-startup-fail", 1.0, 0.25);
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-hdf5-startup-fail.h5";
  remove_file_if_present(hdf5_path);
  config.output.emit_json = false;
  config.output.emit_hdf5 = true;
  config.output.hdf5_path = hdf5_path.string();
  config.hull.initial_orientation_xyzw = {
      .x = 0.0, .y = 0.0, .z = 0.0, .w = 0.0};

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 13h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 13h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(result.outputs.hdf5_written);
  EXPECT_EQ(result.outputs.schema_version, "a007-hdf5-v2");
  ASSERT_TRUE(std::filesystem::exists(hdf5_path));
  EXPECT_EQ(read_binary_prefix(hdf5_path, 8U),
            std::string("\x89HDF\r\n\x1a\n", 8U));

  remove_file_if_present(hdf5_path);
}

/**
 * @test UT-161
 * @verifies [D-021]
 * @notes Given mixed JSON and HDF5 output on an HDF5-capable build and an
 * invalid HDF5 parent path, when emission runs, then JSON artifacts still
 * succeed while HDF5 failure is reported deterministically.
 */
TEST(RunOutputsHdf5, EmitsJsonAndReportsHdf5DirectoryFailureWhenSupported) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5-dir-fail", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-hdf5-dir-fail-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-hdf5-dir-fail-timeseries.json";
  const auto parent_marker =
      write_temp_marker_file("airow-ut-output-hdf5-parent-marker");
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.hdf5_path = (parent_marker / "run.h5").string();
  config.output.emit_json = true;
  config.output.emit_hdf5 = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 14h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 14h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  EXPECT_FALSE(result.outputs.hdf5_written);
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.back().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.back().path, "$.output.hdf5_path");

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(parent_marker);
}

/**
 * @test UT-162
 * @verifies [D-021]
 * @notes Given HDF5-only output on an HDF5-capable build and a file-creation
 * target under `/proc`, when emission runs, then HDF5 file-creation failure is
 * reported deterministically.
 */
TEST(RunOutputsHdf5, ReportsHdf5FileCreationFailureWhenSupported) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5-create-fail", 0.5, 0.25);
  config.output.emit_json = false;
  config.output.emit_hdf5 = true;
  config.output.hdf5_path = "/proc/airow-ut-output-create-fail.h5";

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 15h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 15h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  EXPECT_FALSE(result.outputs.hdf5_written);
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.back().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.back().path, "$.output.hdf5_path");
}

/**
 * @test UT-050
 * @verifies [D-021]
 * @notes Given HDF5 output requested in-memory on a build without HDF5,
 * when execution runs, then output emission reports deterministic runtime
 * diagnostics.
 */
TEST(RunOutputsHdf5, InMemoryHdf5RequestFailsDeterministicallyWhenUnavailable) {
  if (project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support available on this build";
  }

  auto config = make_config("ut-output-hdf5-runtime-unavailable", 0.5, 0.25);
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-hdf5-runtime-unavailable.h5";
  remove_file_if_present(hdf5_path);

  config.output.emit_json = false;
  config.output.emit_hdf5 = true;
  config.output.hdf5_path = hdf5_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 20h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 20h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  EXPECT_EQ(result.diagnostics.back().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.back().path, "$.output.hdf5_path");
  EXPECT_FALSE(result.outputs.hdf5_written);
  EXPECT_FALSE(std::filesystem::exists(hdf5_path));
}

/**
 * @test UT-051
 * @verifies [D-021]
 * @notes Given mixed JSON and HDF5 output requested without HDF5 support,
 * when execution runs, then JSON artifacts are emitted and HDF5 failure is
 * reported with stable artifact metadata.
 */
TEST(RunOutputsHdf5, EmitsJsonAndReportsHdf5FailureWhenUnavailable) {
  if (project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support available on this build";
  }

  auto config = make_config("ut-output-mixed-unavailable", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-mixed-unavailable-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() /
      "airow-ut-output-mixed-unavailable-timeseries.json";
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-mixed-unavailable.h5";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);

  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.hdf5_path = hdf5_path.string();
  config.output.emit_json = true;
  config.output.emit_hdf5 = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  ASSERT_FALSE(result.outputs.hdf5_written);
  const Json summary = read_json_file(summary_path);
  const auto &formats = summary.at("outputs").at("formats");
  ASSERT_EQ(formats.size(), 2U);
  EXPECT_EQ(formats.at(0).get<std::string>(), "json");
  EXPECT_EQ(formats.at(1).get<std::string>(), "hdf5");
  EXPECT_EQ(summary.at("outputs").at("hdf5_path").get<std::string>(),
            hdf5_path.string());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
}

/**
 * @test UT-254
 * @verifies [D-022]
 * @notes Given a time-varying wind series on an HDF5-capable build, when HDF5
 * output is emitted, then the effective ambient-wind datasets are written with
 * one scalar channel per world-frame component.
 */
TEST(RunOutputsHdf5, EmitsEffectiveAmbientWindDatasets) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-ut-output-wind.h5";
  remove_file_if_present(hdf5_path);
  const auto config_path =
      write_temp_file("airow-ut-output-wind-hdf5-config.json",
                      make_time_varying_hdf5_config_json("ut-output-wind-hdf5",
                                                         hdf5_path.string(),
                                                         R"({
          "wind_time_series": [
            {"time_s": 0.0, "ambient_wind_world_mps": [-1.0, 0.0, 0.0]},
            {"time_s": 0.25, "ambient_wind_world_mps": [-3.0, 0.0, 0.0]}
          ]
        })"));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 14h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 14h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.hdf5_written);

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  const H5ScopedHandle time_series_group(
      H5Gopen2(file.id, "/time_series", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(time_series_group.valid());
  const auto ambient_x = read_hdf5_double_dataset(time_series_group.id,
                                                  "ambient_wind_world_mps_x");
  const auto ambient_y = read_hdf5_double_dataset(time_series_group.id,
                                                  "ambient_wind_world_mps_y");
  const auto ambient_z = read_hdf5_double_dataset(time_series_group.id,
                                                  "ambient_wind_world_mps_z");
  ASSERT_EQ(ambient_x.size(), 2U);
  ASSERT_EQ(ambient_y.size(), 2U);
  ASSERT_EQ(ambient_z.size(), 2U);
  EXPECT_DOUBLE_EQ(ambient_x.at(0), -1.0);
  EXPECT_DOUBLE_EQ(ambient_x.at(1), -3.0);
  EXPECT_DOUBLE_EQ(ambient_y.at(0), 0.0);
  EXPECT_DOUBLE_EQ(ambient_y.at(1), 0.0);
  EXPECT_DOUBLE_EQ(ambient_z.at(0), 0.0);
  EXPECT_DOUBLE_EQ(ambient_z.at(1), 0.0);
#endif

  remove_file_if_present(config_path);
  remove_file_if_present(hdf5_path);
}
