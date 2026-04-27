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
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
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

std::string make_valid_config_json(std::string_view config_id,
                                   std::string_view summary_path,
                                   std::string_view time_series_path,
                                   std::string_view visualization_path = "") {
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
         << time_series_path << R"(")";
  if (!visualization_path.empty()) {
    stream << R"(,
          "visualization_path": ")"
           << visualization_path << R"(")";
  }
  stream << R"(,
          "formats": ["json"],
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

void expect_interactive_report_html(const std::filesystem::path &report_dir) {
  ASSERT_TRUE(std::filesystem::exists(report_dir / "index.html"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "metrics.json"));

  const auto html = read_file(report_dir / "index.html");
  EXPECT_NE(html.find("data-airow-viewer"), std::string::npos);
  EXPECT_NE(html.find("airowViewerData"), std::string::npos);
  EXPECT_NE(html.find("projection-top"), std::string::npos);
  EXPECT_NE(html.find("projection-side"), std::string::npos);
  EXPECT_NE(html.find("plot-cursor"), std::string::npos);
  EXPECT_NE(html.find("playback-toggle"), std::string::npos);
  EXPECT_NE(html.find("hull_hydro_force_world_n"), std::string::npos);
  EXPECT_NE(html.find("gate_loads"), std::string::npos);
  EXPECT_NE(html.find("unavailable"), std::string::npos);
  EXPECT_NE(html.find("trust-status"), std::string::npos);
}

void expect_interactive_report_metrics(const std::filesystem::path &report_dir,
                                       std::string_view config_id) {
  const auto metrics = read_json_file(report_dir / "metrics.json");
  EXPECT_EQ(metrics.at("config_id").get<std::string>(), config_id);
  EXPECT_TRUE(metrics.at("interactive_visualization").get<bool>());
}

bool json_array_contains_id(const Json &items, std::string_view id) {
  for (const auto &item : items) {
    if (item.is_object() && item.contains("id") &&
        item.at("id").get<std::string>() == id) {
      return true;
    }
  }
  return false;
}

bool json_array_contains_string(const Json &items, std::string_view value) {
  for (const auto &item : items) {
    if (item.is_string() && item.get<std::string>() == value) {
      return true;
    }
  }
  return false;
}

struct InteractiveReportRun {
  std::filesystem::path config_path;
  std::filesystem::path summary_path;
  std::filesystem::path time_series_path;
  std::filesystem::path visualization_path;
  std::filesystem::path cli_stdout_path;
  std::filesystem::path cli_stderr_path;
  std::filesystem::path report_dir;
  std::filesystem::path tool_stdout_path;
  std::filesystem::path tool_stderr_path;
};

InteractiveReportRun make_interactive_report_run(std::string_view suffix,
                                                 std::string_view config_id) {
  const std::string name{suffix};
  InteractiveReportRun run{
      .config_path = write_temp_file(
          "airow-" + name + "-config.json",
          make_valid_config_json(config_id,
                                 (std::filesystem::temp_directory_path() /
                                  ("airow-" + name + "-summary.json"))
                                     .string(),
                                 (std::filesystem::temp_directory_path() /
                                  ("airow-" + name + "-timeseries.json"))
                                     .string(),
                                 (std::filesystem::temp_directory_path() /
                                  ("airow-" + name + "-artifact.json"))
                                     .string())),
      .summary_path = std::filesystem::temp_directory_path() /
                      ("airow-" + name + "-summary.json"),
      .time_series_path = std::filesystem::temp_directory_path() /
                          ("airow-" + name + "-timeseries.json"),
      .visualization_path = std::filesystem::temp_directory_path() /
                            ("airow-" + name + "-artifact.json"),
      .cli_stdout_path = std::filesystem::temp_directory_path() /
                         ("airow-" + name + "-cli.stdout"),
      .cli_stderr_path = std::filesystem::temp_directory_path() /
                         ("airow-" + name + "-cli.stderr"),
      .report_dir = std::filesystem::temp_directory_path() /
                    ("airow-" + name + "-report"),
      .tool_stdout_path = std::filesystem::temp_directory_path() /
                          ("airow-" + name + "-tool.stdout"),
      .tool_stderr_path = std::filesystem::temp_directory_path() /
                          ("airow-" + name + "-tool.stderr"),
  };
  return run;
}

void prepare_interactive_report_run(const InteractiveReportRun &run) {
  remove_file_if_present(run.summary_path);
  remove_file_if_present(run.time_series_path);
  remove_file_if_present(run.visualization_path);
  remove_file_if_present(run.cli_stdout_path);
  remove_file_if_present(run.cli_stderr_path);
  remove_file_if_present(run.tool_stdout_path);
  remove_file_if_present(run.tool_stderr_path);
  remove_directory_if_present(run.report_dir);
}

void execute_interactive_report_run(const InteractiveReportRun &run) {
  const auto cli_command = shell_quote(kProjectAppPath.string()) +
                           " --config " +
                           shell_quote(run.config_path.string()) + " > " +
                           shell_quote(run.cli_stdout_path.string()) + " 2> " +
                           shell_quote(run.cli_stderr_path.string());
  ASSERT_EQ(decode_exit_code(std::system(cli_command.c_str())), 0);
  ASSERT_TRUE(read_file(run.cli_stderr_path).empty());

  const auto tool_path = kProjectSourceDir / "tools" / "run_analysis.py";
  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --summary " + shell_quote(run.summary_path.string()) +
      " --time-series " + shell_quote(run.time_series_path.string()) +
      " --visualization " + shell_quote(run.visualization_path.string()) +
      " --output-dir " + shell_quote(run.report_dir.string()) + " > " +
      shell_quote(run.tool_stdout_path.string()) + " 2> " +
      shell_quote(run.tool_stderr_path.string());
  EXPECT_EQ(decode_exit_code(std::system(tool_command.c_str())), 0);
  EXPECT_TRUE(read_file(run.tool_stderr_path).empty());
}

void cleanup_interactive_report_run(const InteractiveReportRun &run) {
  remove_file_if_present(run.config_path);
  remove_file_if_present(run.summary_path);
  remove_file_if_present(run.time_series_path);
  remove_file_if_present(run.visualization_path);
  remove_file_if_present(run.cli_stdout_path);
  remove_file_if_present(run.cli_stderr_path);
  remove_file_if_present(run.tool_stdout_path);
  remove_file_if_present(run.tool_stderr_path);
  remove_directory_if_present(run.report_dir);
}

} // namespace

/**
 * @test QT-027
 * @verifies [R-034]
 * @notes Given a valid headless run config, when the CLI is asked to print the
 * compact report mode, then it emits a human-readable analysis report with
 * stable section headings.
 */
TEST(RunAnalysisSystem, CliCanPrintCompactHumanReadableReport) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-timeseries.json";
  const auto config_path = write_temp_file(
      "airow-qt-analysis-config.json",
      make_valid_config_json("qt-analysis", summary_path.string(),
                             time_series_path.string()));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis.stderr";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) +
                       " --report compact > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());
  const auto stdout_text = read_file(stdout_path);
  EXPECT_NE(stdout_text.find("Run Analysis"), std::string::npos);
  EXPECT_NE(stdout_text.find("Coverage"), std::string::npos);
  EXPECT_NE(stdout_text.find("Final State"), std::string::npos);
  EXPECT_NE(stdout_text.find("Motion Envelope"), std::string::npos);

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-028
 * @verifies [R-034]
 * @notes Given emitted JSON summary and time-series artifacts, when the
 * repository analysis tool runs offline, then it creates a static report
 * bundle with summary metrics and deterministic SVG plots.
 */
TEST(RunAnalysisSystem, PythonToolGeneratesStaticReportBundle) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-analysis-tool-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-tool-timeseries.json";
  const auto config_path = write_temp_file(
      "airow-qt-analysis-tool-config.json",
      make_valid_config_json("qt-analysis-tool", summary_path.string(),
                             time_series_path.string()));
  const auto cli_stdout_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-tool-cli.stdout";
  const auto cli_stderr_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-tool-cli.stderr";
  const auto report_dir =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-report";
  const auto tool_stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-tool.stdout";
  const auto tool_stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-tool.stderr";
  const auto tool_path = kProjectSourceDir / "tools" / "run_analysis.py";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);

  const auto cli_command = shell_quote(kProjectAppPath.string()) +
                           " --config " + shell_quote(config_path.string()) +
                           " > " + shell_quote(cli_stdout_path.string()) +
                           " 2> " + shell_quote(cli_stderr_path.string());
  ASSERT_EQ(decode_exit_code(std::system(cli_command.c_str())), 0);
  ASSERT_TRUE(read_file(cli_stderr_path).empty());

  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --summary " + shell_quote(summary_path.string()) + " --time-series " +
      shell_quote(time_series_path.string()) + " --output-dir " +
      shell_quote(report_dir.string()) + " > " +
      shell_quote(tool_stdout_path.string()) + " 2> " +
      shell_quote(tool_stderr_path.string());
  const auto tool_status = std::system(tool_command.c_str());

  EXPECT_EQ(decode_exit_code(tool_status), 0);
  EXPECT_TRUE(read_file(tool_stderr_path).empty());
  ASSERT_TRUE(std::filesystem::exists(report_dir / "index.html"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "metrics.json"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "boat_speed.svg"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "stroke_power.svg"));

  const auto metrics = read_json_file(report_dir / "metrics.json");
  EXPECT_EQ(metrics.at("config_id").get<std::string>(), "qt-analysis-tool");
  EXPECT_GE(metrics.at("record_count").get<std::size_t>(), 1U);

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);
}

/**
 * @test QT-050
 * @verifies [R-052, R-053]
 * @notes Given emitted summary, time-series, and visualization artifacts, when
 * the repository analysis tool runs offline, then it creates an interactive
 * inspection report with synchronized playback controls, 2D state projections,
 * vector-overlay metadata, plot cursor hooks, and explicit trust or channel
 * availability labels.
 */
TEST(RunAnalysisSystem, PythonToolGeneratesInteractiveVisualizationReport) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-analysis-viz-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-viz-timeseries.json";
  const auto visualization_path = std::filesystem::temp_directory_path() /
                                  "airow-qt-analysis-viz-artifact.json";
  const auto config_path = write_temp_file(
      "airow-qt-analysis-viz-config.json",
      make_valid_config_json("qt-analysis-viz", summary_path.string(),
                             time_series_path.string(),
                             visualization_path.string()));
  const auto cli_stdout_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-viz-cli.stdout";
  const auto cli_stderr_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-viz-cli.stderr";
  const auto report_dir =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-viz-report";
  const auto tool_stdout_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-viz-tool.stdout";
  const auto tool_stderr_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-viz-tool.stderr";
  const auto tool_path = kProjectSourceDir / "tools" / "run_analysis.py";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(visualization_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);

  const auto cli_command = shell_quote(kProjectAppPath.string()) +
                           " --config " + shell_quote(config_path.string()) +
                           " > " + shell_quote(cli_stdout_path.string()) +
                           " 2> " + shell_quote(cli_stderr_path.string());
  ASSERT_EQ(decode_exit_code(std::system(cli_command.c_str())), 0);
  ASSERT_TRUE(read_file(cli_stderr_path).empty());

  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --summary " + shell_quote(summary_path.string()) + " --time-series " +
      shell_quote(time_series_path.string()) + " --visualization " +
      shell_quote(visualization_path.string()) + " --output-dir " +
      shell_quote(report_dir.string()) + " > " +
      shell_quote(tool_stdout_path.string()) + " 2> " +
      shell_quote(tool_stderr_path.string());
  const auto tool_status = std::system(tool_command.c_str());

  EXPECT_EQ(decode_exit_code(tool_status), 0);
  EXPECT_TRUE(read_file(tool_stderr_path).empty());
  expect_interactive_report_html(report_dir);
  expect_interactive_report_metrics(report_dir, "qt-analysis-viz");

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(visualization_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);
}

/**
 * @test QT-052
 * @verifies [R-052, R-053, R-070]
 * @notes Given emitted summary, time-series, and visualization artifacts, when
 * the repository analysis tool builds an offline inspection report, then the
 * generated report exposes vector toggles, projection and frame controls,
 * broader linked plot channel selection, plot-click seek hooks, unavailable
 * channel disabling, trust labels, and machine-readable control metadata.
 */
TEST(RunAnalysisSystem, PythonToolReportsConfigurableVisualizationControls) {
  const auto run = make_interactive_report_run("qt-analysis-controls",
                                               "qt-analysis-controls");
  prepare_interactive_report_run(run);
  execute_interactive_report_run(run);

  const auto html = read_file(run.report_dir / "index.html");
  EXPECT_NE(html.find("projection-mode"), std::string::npos);
  EXPECT_NE(html.find("projection-end"), std::string::npos);
  EXPECT_NE(html.find("reference-frame"), std::string::npos);
  EXPECT_NE(html.find("hull_body"), std::string::npos);
  EXPECT_NE(html.find("waterline"), std::string::npos);
  EXPECT_NE(html.find("vector-toggle-hull_hydro_force_world_n"),
            std::string::npos);
  EXPECT_NE(html.find("vector-toggle-port_blade_force_world_n"),
            std::string::npos);
  EXPECT_NE(html.find("vector-toggle-gate_loads"), std::string::npos);
  EXPECT_NE(html.find("data-availability=\"unavailable\""), std::string::npos);
  EXPECT_NE(html.find("data-plot-click-seek=\"true\""), std::string::npos);
  EXPECT_NE(html.find("plot-channel-select"), std::string::npos);
  EXPECT_NE(html.find("hull_position_z_m"), std::string::npos);
  EXPECT_NE(html.find("apparent_wind_speed_mps"), std::string::npos);
  EXPECT_NE(html.find("trust-status"), std::string::npos);
  EXPECT_NE(html.find("metrics_metadata"), std::string::npos);

  const auto metrics = read_json_file(run.report_dir / "metrics.json");
  const auto &controls = metrics.at("interactive_controls");
  EXPECT_TRUE(controls.at("enabled").get<bool>());
  EXPECT_TRUE(controls.at("plot_click_seek").get<bool>());
  EXPECT_TRUE(
      json_array_contains_string(controls.at("projection_modes"), "top"));
  EXPECT_TRUE(
      json_array_contains_string(controls.at("projection_modes"), "side"));
  EXPECT_TRUE(
      json_array_contains_string(controls.at("projection_modes"), "end"));
  EXPECT_TRUE(
      json_array_contains_string(controls.at("reference_frames"), "world"));
  EXPECT_TRUE(
      json_array_contains_string(controls.at("reference_frames"), "hull_body"));
  EXPECT_TRUE(
      json_array_contains_string(controls.at("reference_frames"), "waterline"));
  EXPECT_TRUE(json_array_contains_id(controls.at("vector_channels"),
                                     "hull_hydro_force_world_n"));
  EXPECT_TRUE(json_array_contains_id(controls.at("vector_channels"),
                                     "port_blade_force_world_n"));
  EXPECT_TRUE(json_array_contains_id(controls.at("unavailable_channels"),
                                     "gate_loads"));
  EXPECT_TRUE(
      json_array_contains_id(controls.at("plot_channels"), "boat_speed_mps"));
  EXPECT_TRUE(json_array_contains_id(controls.at("plot_channels"),
                                     "hull_position_z_m"));
  EXPECT_TRUE(json_array_contains_id(controls.at("plot_channels"),
                                     "apparent_wind_speed_mps"));
  EXPECT_GE(controls.at("plot_channels").size(), 6U);

  cleanup_interactive_report_run(run);
}

/**
 * @test QT-051
 * @verifies [R-050]
 * @notes Given emitted summary and time-series artifacts but a malformed
 * visualization artifact, when the repository analysis tool runs offline, then
 * it rejects the unsupported visualization schema deterministically instead of
 * generating an ambiguous inspection report.
 */
TEST(RunAnalysisSystem, PythonToolRejectsMalformedVisualizationArtifact) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-analysis-bad-viz-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-bad-viz-timeseries.json";
  const auto bad_visualization_path =
      write_temp_file("airow-qt-analysis-bad-viz-artifact.json",
                      R"({"schema_id":"airow.visualization.v0"})");
  const auto config_path = write_temp_file(
      "airow-qt-analysis-bad-viz-config.json",
      make_valid_config_json("qt-analysis-bad-viz", summary_path.string(),
                             time_series_path.string()));
  const auto cli_stdout_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-bad-viz-cli.stdout";
  const auto cli_stderr_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-bad-viz-cli.stderr";
  const auto report_dir = std::filesystem::temp_directory_path() /
                          "airow-qt-analysis-bad-viz-report";
  const auto tool_stdout_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-bad-viz-tool.stdout";
  const auto tool_stderr_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-bad-viz-tool.stderr";
  const auto tool_path = kProjectSourceDir / "tools" / "run_analysis.py";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);

  const auto cli_command = shell_quote(kProjectAppPath.string()) +
                           " --config " + shell_quote(config_path.string()) +
                           " > " + shell_quote(cli_stdout_path.string()) +
                           " 2> " + shell_quote(cli_stderr_path.string());
  ASSERT_EQ(decode_exit_code(std::system(cli_command.c_str())), 0);
  ASSERT_TRUE(read_file(cli_stderr_path).empty());

  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --summary " + shell_quote(summary_path.string()) + " --time-series " +
      shell_quote(time_series_path.string()) + " --visualization " +
      shell_quote(bad_visualization_path.string()) + " --output-dir " +
      shell_quote(report_dir.string()) + " > " +
      shell_quote(tool_stdout_path.string()) + " 2> " +
      shell_quote(tool_stderr_path.string());
  const auto tool_status = std::system(tool_command.c_str());

  EXPECT_EQ(decode_exit_code(tool_status), 1);
  const auto stderr_text = read_file(tool_stderr_path);
  EXPECT_NE(stderr_text.find("invalid visualization artifact"),
            std::string::npos);
  EXPECT_NE(stderr_text.find("$.schema_id"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(report_dir / "index.html"));

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(bad_visualization_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);
}
