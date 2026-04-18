#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_output.hpp"

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <H5Apublic.h>
#include <H5Fpublic.h>
#include <H5Gpublic.h>
#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5Tpublic.h>
#endif

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

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

  const H5ScopedHandle state_advancer_group(
      H5Gopen2(file.id, "/metadata/state_advancer", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(state_advancer_group.valid());
  EXPECT_EQ(read_hdf5_string_attribute(state_advancer_group.id, "id"),
            "sundials_ida");
  EXPECT_EQ(read_hdf5_string_attribute(state_advancer_group.id, "policy_id"),
            "sundials-ida-fixed-tolerances-v1");
  EXPECT_EQ(
      read_hdf5_string_attribute(state_advancer_group.id, "policy_description"),
      "Required SUNDIALS IDA default-runtime backend with fixed relative and "
      "absolute tolerances of 1e-10 for Slice 3 closure.");
#endif

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
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
