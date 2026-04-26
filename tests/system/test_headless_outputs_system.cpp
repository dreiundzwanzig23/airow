#include <gtest/gtest.h>

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/output/run_output.hpp"

namespace {

using Json = nlohmann::json;

#ifndef PROJECT_APP_PATH
#error "PROJECT_APP_PATH is required for headless system tests"
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary);
  output << contents;
  output.close();
  return path;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string shell_quote(std::string_view text) {
  std::string quoted;
  quoted.reserve(text.size() + 2U);
  quoted.push_back('\'');
  for (const char ch : text) {
    if (ch == '\'') {
      quoted.append("'\\''");
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int decode_exit_code(int status) {
  if (status == -1) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string
make_valid_config_json(std::string_view config_id,
                       std::string_view summary_path,
                       std::string_view time_series_path,
                       std::string_view formats_json = R"(["json"])",
                       std::string_view hdf5_path = "",
                       std::string_view truth_model_export_path = "") {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 1.0,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
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
        "output": {
          "summary_path": ")"
         << summary_path << R"(",
          "time_series_path": ")"
         << time_series_path << R"(",
          "formats": )"
         << formats_json << R"(,
          "hdf5_path": ")"
         << hdf5_path << R"(",
          "truth_model_export_path": ")"
         << truth_model_export_path << R"(",
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

std::string make_provider_selection_config_json(
    std::string_view config_id, std::string_view summary_path,
    std::string_view time_series_path, std::string_view providers_json,
    std::string_view ambient_wind_json = "[0.0, 0.0, 0.0]",
    double initial_speed_x_mps = 0.0) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 1.0,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [)"
         << initial_speed_x_mps << R"(, 0.0, 0.0],
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
            {"time_s": 0.0, "ambient_wind_world_mps": )"
         << ambient_wind_json << R"(
            }
          ]
        },
        "providers": )"
         << providers_json << R"(,
        "output": {
          "summary_path": ")"
         << summary_path << R"(",
          "time_series_path": ")"
         << time_series_path << R"(",
          "formats": ["json"],
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

} // namespace

/**
 * @test QT-007
 * @verifies [R-015]
 * @notes Given a headless run config that sets structured output paths and
 * high-frequency output, when the CLI executes, then summary and time-series
 * machine-readable artifacts are emitted with explicit required metadata and
 * deterministic record count.
 */
TEST(HeadlessOutputsSystem, CliEmitsStructuredOutputArtifacts) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-output-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-output-timeseries.json";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path =
      write_temp_file("airow-qt-output-config.json",
                      make_valid_config_json("qt-output", summary_path.string(),
                                             time_series_path.string()));

  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-output.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-output.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());

  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(time_series_path));

  const Json summary = Json::parse(read_file(summary_path));
  const Json time_series = Json::parse(read_file(time_series_path));

  EXPECT_EQ(summary.at("config_id").get<std::string>(), "qt-output");
  EXPECT_FALSE(summary.at("simulator_version").get<std::string>().empty());
  EXPECT_EQ(time_series.at("high_frequency_time_series").get<bool>(), true);

  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), 5U);
  const auto &aero_load =
      records.front().at("aerodynamic_load_world_n").at("vector");
  EXPECT_EQ(aero_load.at("unit").get<std::string>(), "N");
  EXPECT_EQ(aero_load.at("frame").get<std::string>(), "world");

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-008
 * @verifies [R-015]
 * @notes Given HDF5 output configured through the CLI path, when execution
 * runs on builds with and without HDF5 support, then behavior is deterministic
 * for both availability states.
 */
TEST(HeadlessOutputsSystem, CliHandlesHdf5OutputAvailabilityDeterministically) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5-timeseries.json";
  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5-output.h5";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);

  const auto config_path = write_temp_file(
      "airow-qt-hdf5-config.json",
      make_valid_config_json("qt-output-hdf5", summary_path.string(),
                             time_series_path.string(), R"(["hdf5"])",
                             hdf5_path.string()));

  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());
  const auto exit_code = decode_exit_code(status);
  const auto stderr_text = read_file(stderr_path);

  if (project::hdf5_output_supported()) {
    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(stderr_text.empty());
    EXPECT_TRUE(std::filesystem::exists(hdf5_path));
  } else {
    EXPECT_EQ(exit_code, 2);
    EXPECT_NE(stderr_text.find("unsupported_value"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(hdf5_path));
  }

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
}

/**
 * @test QT-029
 * @verifies [R-020]
 * @notes Given explicit runtime provider selections, when the CLI executes a
 * run then selected provider ids are preserved in metadata, and when an
 * unknown provider id is configured then configuration validation rejects it
 * deterministically.
 */
TEST(HeadlessOutputsSystem, CliSupportsRuntimeProviderSelectionAndRejection) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-provider-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-provider-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path =
      write_temp_file("airow-qt-provider-config.json",
                      make_provider_selection_config_json(
                          "qt-provider-selection", summary_path.string(),
                          time_series_path.string(),
                          R"({
            "hull_resistance": "quadratic_drag_placeholder",
            "blade_force": "stroke_propulsion_placeholder",
            "aero_load": "steady_wind_placeholder"
          })",
                          "[-2.0, 1.0, 0.0]", 1.0));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-provider.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-provider.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  const Json summary = Json::parse(read_file(summary_path));
  const auto &providers = summary.at("metadata").at("providers");
  EXPECT_EQ(providers.at("hull_resistance").at("id").get<std::string>(),
            "quadratic_drag_placeholder");
  EXPECT_EQ(providers.at("blade_force").at("id").get<std::string>(),
            "stroke_propulsion_placeholder");
  EXPECT_EQ(providers.at("aero_load").at("id").get<std::string>(),
            "steady_wind_placeholder");

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto invalid_config_path =
      write_temp_file("airow-qt-provider-invalid-config.json",
                      make_provider_selection_config_json(
                          "qt-provider-invalid", summary_path.string(),
                          time_series_path.string(),
                          R"({
            "aero_load": "unsupported_provider"
          })"));
  const auto invalid_stdout_path = std::filesystem::temp_directory_path() /
                                   "airow-qt-provider-invalid.stdout";
  const auto invalid_stderr_path = std::filesystem::temp_directory_path() /
                                   "airow-qt-provider-invalid.stderr";
  const auto invalid_command =
      shell_quote(kProjectAppPath.string()) + " --config " +
      shell_quote(invalid_config_path.string()) + " > " +
      shell_quote(invalid_stdout_path.string()) + " 2> " +
      shell_quote(invalid_stderr_path.string());
  const auto invalid_status = std::system(invalid_command.c_str());

  EXPECT_EQ(decode_exit_code(invalid_status), 2);
  const auto invalid_stderr = read_file(invalid_stderr_path);
  EXPECT_NE(invalid_stderr.find("$.providers.aero_load"), std::string::npos);

  remove_file_if_present(invalid_config_path);
  remove_file_if_present(invalid_stdout_path);
  remove_file_if_present(invalid_stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-030
 * @verifies [R-033]
 * @notes Given selectable built-in runtime providers, when the CLI emits a
 * summary artifact then each selected provider includes non-empty validity
 * metadata in the machine-readable output.
 */
TEST(HeadlessOutputsSystem, CliEmitsProviderValidityMetadata) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-validity-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-validity-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path =
      write_temp_file("airow-qt-validity-config.json",
                      make_provider_selection_config_json(
                          "qt-provider-validity", summary_path.string(),
                          time_series_path.string(),
                          R"({
            "hull_resistance": "quadratic_drag_placeholder",
            "blade_force": "stroke_propulsion_placeholder",
            "aero_load": "steady_wind_placeholder"
          })",
                          "[-2.0, 1.0, 0.0]", 1.0));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-validity.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-validity.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  const Json summary = Json::parse(read_file(summary_path));
  const auto &providers = summary.at("metadata").at("providers");
  for (const auto &role : {"hull_resistance", "blade_force", "aero_load"}) {
    const auto &provider = providers.at(role);
    EXPECT_FALSE(provider.at("validity_id").get<std::string>().empty());
    EXPECT_FALSE(
        provider.at("validity_description").get<std::string>().empty());
  }

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-047
 * @verifies [R-071]
 * @notes Given a CLI run using the default reduced built-in providers, when
 * the summary artifact is emitted, then every active provider reports
 * non-empty machine-readable capability metadata for support, fidelity,
 * validation, and human-facing summary text.
 */
TEST(HeadlessOutputsSystem, CliEmitsProviderCapabilityMetadata) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-capability-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-capability-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path =
      write_temp_file("airow-qt-capability-config.json",
                      make_provider_selection_config_json(
                          "qt-provider-capability", summary_path.string(),
                          time_series_path.string(),
                          R"({
            "hull_resistance": "quadratic_drag_placeholder",
            "blade_force": "stroke_propulsion_placeholder",
            "aero_load": "steady_wind_placeholder"
          })",
                          "[-2.0, 1.0, 0.0]", 1.0));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-capability.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-capability.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());
  const Json summary = Json::parse(read_file(summary_path));
  const auto &providers = summary.at("metadata").at("providers");
  for (const auto &role : {"hull_resistance", "blade_force", "aero_load"}) {
    const auto &capability = providers.at(role).at("capability");
    EXPECT_FALSE(capability.at("support_status").get<std::string>().empty());
    EXPECT_FALSE(capability.at("fidelity_level").get<std::string>().empty());
    EXPECT_FALSE(capability.at("validation_status").get<std::string>().empty());
    EXPECT_FALSE(
        capability.at("capability_summary").get<std::string>().empty());
  }

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-041
 * @verifies [R-024]
 * @notes Given a default-runtime CLI config that requests the optional
 * truth-model handoff export path, when the run succeeds without any optional
 * external truth-model tooling present, then the shared runtime still
 * succeeds and emits the deterministic handoff bundle.
 */
TEST(HeadlessOutputsSystem, CliEmitsTruthModelHandoffArtifact) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-truth-model-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-truth-model-timeseries.json";
  const auto export_path = std::filesystem::temp_directory_path() /
                           "airow-qt-truth-model-export.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(export_path);

  const auto config_path = write_temp_file(
      "airow-qt-truth-model-config.json",
      make_valid_config_json("qt-truth-model", summary_path.string(),
                             time_series_path.string(), R"(["json"])", "",
                             export_path.string()));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-truth-model.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-truth-model.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  ASSERT_EQ(decode_exit_code(status), 0);
  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(time_series_path));
  ASSERT_TRUE(std::filesystem::exists(export_path));

  const Json summary = Json::parse(read_file(summary_path));
  const Json handoff = Json::parse(read_file(export_path));

  EXPECT_EQ(summary.at("outputs")
                .at("truth_model_export")
                .at("path")
                .get<std::string>(),
            export_path.string());
  EXPECT_EQ(
      summary.at("outputs").at("truth_model_export").at("written").get<bool>(),
      true);
  EXPECT_EQ(handoff.at("schema_id").get<std::string>(),
            "truth_model_input_handoff.v1");
  EXPECT_EQ(handoff.at("config_id").get<std::string>(), "qt-truth-model");
  EXPECT_EQ(handoff.at("state_conventions").at("body_frame").get<std::string>(),
            "x_forward_y_starboard_z_up");

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(export_path);
}
