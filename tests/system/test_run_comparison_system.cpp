#include <gtest/gtest.h>

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;

#ifndef PROJECT_APP_PATH
#error PROJECT_APP_PATH must be defined for system tests
#endif

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};
const std::filesystem::path kProjectSourceDir{PROJECT_SOURCE_DIR};

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

void write_json_file(const std::filesystem::path &path, const Json &document) {
  std::ofstream output(path, std::ios::binary);
  output << document.dump(2);
  output.close();
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void remove_directory_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove_all(path, error);
}

std::string shell_quote(std::string_view value) {
  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int decode_exit_code(int raw_status) {
  if (WIFEXITED(raw_status)) {
    return WEXITSTATUS(raw_status);
  }
  return -1;
}

struct RunConfigOptions {
  double initial_speed_x_mps{};
  std::string_view actuation_json{R"({"mode": "prescribed_kinematic"})"};
};

struct ComparisonRunPaths {
  std::filesystem::path config_path;
  std::filesystem::path summary_path;
  std::filesystem::path time_series_path;
  std::filesystem::path visualization_path;
  std::filesystem::path stdout_path;
  std::filesystem::path stderr_path;
};

std::string make_valid_config_json(std::string_view config_id,
                                   const ComparisonRunPaths &run,
                                   RunConfigOptions options = {}) {
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
         << options.initial_speed_x_mps << R"(, 0.0, 0.0],
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
          "release_angle_rad": 0.6,
          "actuation": )"
         << options.actuation_json << R"(
        },
        "providers": {
          "hull_resistance": "quadratic_drag_placeholder",
          "blade_force": "stroke_propulsion_placeholder",
          "aero_load": "none"
        },
        "output": {
          "summary_path": ")"
         << run.summary_path.string() << R"(",
          "time_series_path": ")"
         << run.time_series_path.string() << R"(",
          "visualization_path": ")"
         << run.visualization_path.string() << R"(",
          "formats": ["json"],
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

ComparisonRunPaths make_run_paths(std::string_view suffix) {
  const std::string name{suffix};
  const auto temp = std::filesystem::temp_directory_path();
  return ComparisonRunPaths{
      .config_path = temp / ("airow-" + name + "-config.json"),
      .summary_path = temp / ("airow-" + name + "-summary.json"),
      .time_series_path = temp / ("airow-" + name + "-timeseries.json"),
      .visualization_path = temp / ("airow-" + name + "-visualization.json"),
      .stdout_path = temp / ("airow-" + name + ".stdout"),
      .stderr_path = temp / ("airow-" + name + ".stderr"),
  };
}

void prepare_run(const ComparisonRunPaths &run) {
  remove_file_if_present(run.config_path);
  remove_file_if_present(run.summary_path);
  remove_file_if_present(run.time_series_path);
  remove_file_if_present(run.visualization_path);
  remove_file_if_present(run.stdout_path);
  remove_file_if_present(run.stderr_path);
}

void execute_run(const ComparisonRunPaths &run, std::string_view config_id,
                 RunConfigOptions options = {}) {
  write_json_file(run.config_path,
                  Json::parse(make_valid_config_json(config_id, run, options)));

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(run.config_path.string()) + " > " +
                       shell_quote(run.stdout_path.string()) + " 2> " +
                       shell_quote(run.stderr_path.string());
  ASSERT_EQ(decode_exit_code(std::system(command.c_str())), 0);
  ASSERT_TRUE(read_file(run.stderr_path).empty());
  ASSERT_TRUE(std::filesystem::exists(run.summary_path));
  ASSERT_TRUE(std::filesystem::exists(run.time_series_path));
  ASSERT_TRUE(std::filesystem::exists(run.visualization_path));
}

void cleanup_run(const ComparisonRunPaths &run) {
  remove_file_if_present(run.config_path);
  remove_file_if_present(run.summary_path);
  remove_file_if_present(run.time_series_path);
  remove_file_if_present(run.visualization_path);
  remove_file_if_present(run.stdout_path);
  remove_file_if_present(run.stderr_path);
}

Json manifest_run(std::string_view id, std::string_view label,
                  std::string_view role, const ComparisonRunPaths &run) {
  return Json{{"id", id},
              {"label", label},
              {"role", role},
              {"summary_path", run.summary_path.string()},
              {"time_series_path", run.time_series_path.string()},
              {"visualization_path", run.visualization_path.string()}};
}

} // namespace

/**
 * @test QT-059
 * @verifies [R-054]
 * @notes Given two emitted summary, time-series, and visualization artifact
 * bundles, when the offline run comparison tool aligns them by time, then it
 * writes deterministic metrics, HTML, and SVG outputs with headline deltas,
 * comparability metadata, provider/backend/trust metadata, and visual/vector
 * channel coverage.
 */
TEST(RunComparisonSystem, PythonToolGeneratesOfflineComparisonBundle) {
  const auto baseline = make_run_paths("qt-comparison-baseline");
  const auto variant = make_run_paths("qt-comparison-variant");
  const auto manifest_path = std::filesystem::temp_directory_path() /
                             "airow-qt-comparison-manifest.json";
  const auto output_dir =
      std::filesystem::temp_directory_path() / "airow-qt-comparison-report";
  const auto tool_stdout_path = std::filesystem::temp_directory_path() /
                                "airow-qt-comparison-tool.stdout";
  const auto tool_stderr_path = std::filesystem::temp_directory_path() /
                                "airow-qt-comparison-tool.stderr";

  prepare_run(baseline);
  prepare_run(variant);
  remove_file_if_present(manifest_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(output_dir);

  execute_run(baseline, "qt-comparison-baseline");
  execute_run(variant, "qt-comparison-force",
              {.initial_speed_x_mps = 0.35,
               .actuation_json = R"({"mode": "force_driven",
                                      "peak_drive_force_n": 420.0})"});

  const Json manifest{
      {"schema_id", "airow.run_comparison.v1"},
      {"comparison_id", "qt-059-baseline-vs-force"},
      {"alignment",
       Json{{"mode", "time"}, {"start_time_s", 0.0}, {"end_time_s", 1.0}}},
      {"runs",
       Json::array({manifest_run("baseline", "Baseline", "baseline", baseline),
                    manifest_run("force", "Force variant", "variant",
                                 variant)})}};
  write_json_file(manifest_path, manifest);

  const auto tool_path = kProjectSourceDir / "tools" / "compare_runs.py";
  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --manifest " + shell_quote(manifest_path.string()) +
      " --output-dir " + shell_quote(output_dir.string()) + " > " +
      shell_quote(tool_stdout_path.string()) + " 2> " +
      shell_quote(tool_stderr_path.string());
  const auto tool_status = std::system(tool_command.c_str());

  EXPECT_EQ(decode_exit_code(tool_status), 0);
  EXPECT_TRUE(read_file(tool_stderr_path).empty());
  ASSERT_TRUE(std::filesystem::exists(output_dir / "metrics.json"));
  ASSERT_TRUE(std::filesystem::exists(output_dir / "index.html"));
  ASSERT_TRUE(std::filesystem::exists(output_dir / "boat_speed.svg"));
  ASSERT_TRUE(std::filesystem::exists(output_dir / "blade_loads.svg"));
  ASSERT_TRUE(std::filesystem::exists(output_dir / "energy_power.svg"));

  const auto metrics = read_json_file(output_dir / "metrics.json");
  EXPECT_EQ(metrics.at("schema_id").get<std::string>(),
            "airow.run_comparison_report.v1");
  EXPECT_EQ(metrics.at("comparison_id").get<std::string>(),
            "qt-059-baseline-vs-force");
  EXPECT_EQ(metrics.at("alignment").at("mode").get<std::string>(), "time");
  EXPECT_GT(metrics.at("alignment").at("overlapping_sample_count")
                .get<std::size_t>(),
            0U);
  EXPECT_TRUE(metrics.at("comparability").at("software_comparable")
                  .get<bool>());
  EXPECT_TRUE(metrics.at("comparability").contains("calibration_comparable"));
  EXPECT_TRUE(metrics.at("comparability").contains("physical_comparable"));
  EXPECT_TRUE(metrics.at("runs").at(0).at("providers").is_object());
  EXPECT_TRUE(metrics.at("runs").at(0).at("backends").is_object());
  EXPECT_TRUE(metrics.at("runs").at(0).at("trust_envelope").is_object());
  EXPECT_GT(std::abs(metrics.at("headline_deltas")
                         .at("mean_speed_mps")
                         .at("delta")
                         .get<double>()),
            1.0e-9);
  EXPECT_TRUE(metrics.at("differences").contains("config"));
  EXPECT_TRUE(metrics.at("channel_coverage")
                  .at("visualization_vectors")
                  .contains("hull_hydro_force_world_n"));
  EXPECT_TRUE(metrics.at("channel_coverage").contains("unavailable_reasons"));

  const auto html = read_file(output_dir / "index.html");
  EXPECT_NE(html.find("Run Comparison"), std::string::npos);
  EXPECT_NE(html.find("Comparability"), std::string::npos);
  EXPECT_NE(html.find("boat_speed.svg"), std::string::npos);
  EXPECT_NE(html.find("hull_hydro_force_world_n"), std::string::npos);

  cleanup_run(baseline);
  cleanup_run(variant);
  remove_file_if_present(manifest_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(output_dir);
}

/**
 * @test QT-060
 * @verifies [R-054]
 * @notes Given an unsupported comparison manifest schema, when the offline run
 * comparison tool validates inputs, then it rejects the manifest with a
 * deterministic diagnostic before writing a report bundle.
 */
TEST(RunComparisonSystem, PythonToolRejectsUnsupportedManifestSchema) {
  const auto manifest_path = std::filesystem::temp_directory_path() /
                             "airow-qt-comparison-bad-manifest.json";
  const auto output_dir = std::filesystem::temp_directory_path() /
                          "airow-qt-comparison-bad-report";
  const auto tool_stdout_path = std::filesystem::temp_directory_path() /
                                "airow-qt-comparison-bad-tool.stdout";
  const auto tool_stderr_path = std::filesystem::temp_directory_path() /
                                "airow-qt-comparison-bad-tool.stderr";

  remove_file_if_present(manifest_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(output_dir);
  write_json_file(
      manifest_path,
      Json{{"schema_id", "airow.run_comparison.v0"},
           {"comparison_id", "qt-060-invalid"},
           {"alignment",
            Json{{"mode", "time"}, {"start_time_s", 0.0}, {"end_time_s", 1.0}}},
           {"runs", Json::array()}});

  const auto tool_path = kProjectSourceDir / "tools" / "compare_runs.py";
  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --manifest " + shell_quote(manifest_path.string()) +
      " --output-dir " + shell_quote(output_dir.string()) + " > " +
      shell_quote(tool_stdout_path.string()) + " 2> " +
      shell_quote(tool_stderr_path.string());
  const auto tool_status = std::system(tool_command.c_str());

  EXPECT_NE(decode_exit_code(tool_status), 0);
  EXPECT_NE(read_file(tool_stderr_path).find("invalid comparison manifest"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(output_dir / "metrics.json"));

  remove_file_if_present(manifest_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(output_dir);
}
