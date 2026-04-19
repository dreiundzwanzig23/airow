#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string make_calibrated_config_json(std::string_view config_id,
                                        std::string_view artifact_path) {
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
          "ambient_wind_world_mps": [-2.0, 1.5, 0.0]
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
        }
      })";
  return stream.str();
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

} // namespace

/**
 * @test IT-023
 * @verifies [A-009]
 * @notes Given a file-backed calibrated aero provider selection and a valid
 * external calibration artifact, when the shared file-backed run path
 * executes, then the run records the imported artifact provenance and uses the
 * calibrated coefficients on the first aero sample.
 */
TEST(CalibrationIntegration, FileBackedRunUsesCalibratedAeroArtifact) {
  const auto artifact_path = write_temp_file(
      "airow-it-calibration-artifact.json",
      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-it",
        "artifact_version": "2026-04-19",
        "content_hash": "sha256:it-calibration",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.5,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.5
          }
        }
      })");
  const auto config_path = write_temp_file(
      "airow-it-calibration-config.json",
      make_calibrated_config_json("it-calibration", artifact_path.string()));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 8h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.providers.aero_load.id, "steady_wind_calibrated");
  ASSERT_EQ(result.metadata.external_artifacts.size(), 1U);
  EXPECT_EQ(result.metadata.external_artifacts.front().kind, "calibration");
  EXPECT_EQ(result.metadata.external_artifacts.front().usage, "aero_load");
  EXPECT_EQ(result.metadata.external_artifacts.front().source_id,
            "wind-tunnel-it");
  EXPECT_EQ(result.metadata.external_artifacts.front().artifact_version,
            "2026-04-19");
  EXPECT_EQ(result.metadata.external_artifacts.front().content_hash,
            "sha256:it-calibration");
  EXPECT_EQ(result.metadata.external_artifacts.front().schema_id,
            "steady_wind_aero_calibration.v1");

  ASSERT_FALSE(result.load_history.empty());
  const auto &first_load = result.load_history.front();
  EXPECT_DOUBLE_EQ(first_load.apparent_wind_world_mps.x, -3.0);
  EXPECT_DOUBLE_EQ(first_load.apparent_wind_world_mps.y, 2.0);
  EXPECT_DOUBLE_EQ(first_load.aero_force_world_n.x, -29.66663456597992);
  EXPECT_DOUBLE_EQ(first_load.aero_force_world_n.y, 9.888878188659973);
  EXPECT_DOUBLE_EQ(first_load.aero_moment_world_n_m.z, 10.816653826391967);

  remove_file_if_present(config_path);
  remove_file_if_present(artifact_path);
}
