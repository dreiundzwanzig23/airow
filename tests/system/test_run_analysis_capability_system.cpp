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

bool json_array_contains_string(const Json &items, std::string_view value) {
  for (const auto &item : items) {
    if (item.is_string() && item.get<std::string>() == value) {
      return true;
    }
  }
  return false;
}

std::string make_valid_config_json(std::string_view config_id,
                                   std::string_view summary_path,
                                   std::string_view time_series_path,
                                   std::string_view visualization_path) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
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
        "providers": {
          "hull_resistance": "quadratic_drag_placeholder",
          "blade_force": "stroke_propulsion_placeholder",
          "aero_load": "steady_wind_placeholder"
        },
        "output": {
          "summary_path": ")"
         << summary_path << R"(",
          "time_series_path": ")"
         << time_series_path << R"(",
          "visualization_path": ")"
         << visualization_path << R"(",
          "formats": ["json"],
          "high_frequency_time_series": true
        },
        "environment": {
          "wind_time_series": [
            {"time_s": 0.0, "ambient_wind_world_mps": [0.0, 0.0, 0.0]}
          ]
        }
      })";
  return stream.str();
}

struct ReportRun {
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

ReportRun make_report_run(std::string_view suffix, std::string_view config_id) {
  const std::string name{suffix};
  const auto root = std::filesystem::temp_directory_path();
  const auto summary = root / ("airow-" + name + "-summary.json");
  const auto time_series = root / ("airow-" + name + "-timeseries.json");
  const auto visualization = root / ("airow-" + name + "-artifact.json");
  return {
      .config_path = write_temp_file(
          "airow-" + name + "-config.json",
          make_valid_config_json(config_id, summary.string(),
                                 time_series.string(), visualization.string())),
      .summary_path = summary,
      .time_series_path = time_series,
      .visualization_path = visualization,
      .cli_stdout_path = root / ("airow-" + name + "-cli.stdout"),
      .cli_stderr_path = root / ("airow-" + name + "-cli.stderr"),
      .report_dir = root / ("airow-" + name + "-report"),
      .tool_stdout_path = root / ("airow-" + name + "-tool.stdout"),
      .tool_stderr_path = root / ("airow-" + name + "-tool.stderr"),
  };
}

void prepare_report_run(const ReportRun &run) {
  remove_file_if_present(run.summary_path);
  remove_file_if_present(run.time_series_path);
  remove_file_if_present(run.visualization_path);
  remove_file_if_present(run.cli_stdout_path);
  remove_file_if_present(run.cli_stderr_path);
  remove_file_if_present(run.tool_stdout_path);
  remove_file_if_present(run.tool_stderr_path);
  remove_directory_if_present(run.report_dir);
}

void execute_report_run(const ReportRun &run) {
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

void execute_report_run_with_vtk(const ReportRun &run,
                                 const std::filesystem::path &paraview_dir) {
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
      " --output-dir " + shell_quote(run.report_dir.string()) +
      " --vtk-output-dir " + shell_quote(paraview_dir.string()) + " > " +
      shell_quote(run.tool_stdout_path.string()) + " 2> " +
      shell_quote(run.tool_stderr_path.string());
  EXPECT_EQ(decode_exit_code(std::system(tool_command.c_str())), 0);
  EXPECT_TRUE(read_file(run.tool_stderr_path).empty());
}

void cleanup_report_run(const ReportRun &run) {
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
 * @test QT-056
 * @verifies [R-071, R-070]
 * @notes Given emitted summary, time-series, and visualization artifacts, when
 * the repository analysis tool builds an offline inspection report, then the
 * report entry page and metrics metadata expose physics capability, trust,
 * supported study questions, limitations, and provider explanation links
 * before users inspect or export the run.
 */
TEST(RunAnalysisCapabilitySystem, PythonToolReportsPhysicsCapabilityEntryPage) {
  const auto run = make_report_run("qt-analysis-capability-entry",
                                   "qt-analysis-capability-entry");
  prepare_report_run(run);
  execute_report_run(run);

  const auto html = read_file(run.report_dir / "index.html");
  EXPECT_NE(html.find("Physics Capability and Trust"), std::string::npos);
  EXPECT_NE(html.find("docs/process/CAPABILITY_MATRIX.md"), std::string::npos);
  EXPECT_NE(html.find("validated_reduced_baseline"), std::string::npos);
  EXPECT_NE(html.find("Supported Study Questions"), std::string::npos);
  EXPECT_NE(html.find("Limitations"), std::string::npos);
  EXPECT_NE(html.find("Provider Capability"), std::string::npos);
  EXPECT_NE(html.find("full 3D water or optimization claim"),
            std::string::npos);

  const auto metrics = read_json_file(run.report_dir / "metrics.json");
  const auto &capability = metrics.at("physics_capability_and_trust");
  EXPECT_EQ(capability.at("fidelity_tier").get<std::string>(),
            "validated_reduced_baseline");
  EXPECT_EQ(capability.at("capability_matrix").get<std::string>(),
            "docs/process/CAPABILITY_MATRIX.md");
  EXPECT_TRUE(json_array_contains_string(
      capability.at("supported_study_questions"),
      "Default single-scull reduced-model smoke, tow, calm-water stroke, "
      "headwind, and crosswind studies."));
  EXPECT_TRUE(json_array_contains_string(
      capability.at("claim_guidance"),
      "This report explains reduced-runtime capability and trust status; it is "
      "not a full 3D water or optimization claim."));
  EXPECT_TRUE(capability.at("providers").contains("hull_resistance"));
  EXPECT_EQ(capability.at("providers")
                .at("hull_resistance")
                .at("fidelity_level")
                .get<std::string>(),
            "reduced");

  cleanup_report_run(run);
}

/**
 * @test QT-057
 * @verifies [R-056, R-070]
 * @notes Given emitted summary, time-series, and visualization artifacts, when
 * the repository analysis tool exports a reduced ParaView bundle, then the
 * bundle includes deterministic loading guidance and the report/export
 * metadata links that guidance to the generated VTK files.
 */
TEST(RunAnalysisCapabilitySystem, PythonToolLinksParaViewLoadingGuide) {
  const auto run = make_report_run("qt-analysis-paraview-guide",
                                   "qt-analysis-paraview-guide");
  const auto paraview_dir = std::filesystem::temp_directory_path() /
                            "airow-qt-analysis-paraview-guide";
  prepare_report_run(run);
  remove_directory_if_present(paraview_dir);
  execute_report_run_with_vtk(run, paraview_dir);

  const auto guide_path = paraview_dir / "paraview_loading_guide.md";
  ASSERT_TRUE(std::filesystem::exists(guide_path));
  const auto guide = read_file(guide_path);
  EXPECT_NE(guide.find("Open `airow_geometry.vtk`"), std::string::npos);
  EXPECT_NE(guide.find("Open `airow_vectors.vtk`"), std::string::npos);
  EXPECT_NE(guide.find("airow_metadata.json"), std::string::npos);
  EXPECT_NE(guide.find("reduced visualization export"), std::string::npos);

  const auto metadata = read_json_file(paraview_dir / "airow_metadata.json");
  EXPECT_TRUE(json_array_contains_string(metadata.at("files"),
                                         "paraview_loading_guide.md"));
  EXPECT_EQ(metadata.at("loading_guide_path").get<std::string>(),
            "paraview_loading_guide.md");

  const auto metrics = read_json_file(run.report_dir / "metrics.json");
  EXPECT_EQ(
      metrics.at("paraview_export").at("loading_guide_path").get<std::string>(),
      guide_path.string());
  const auto html = read_file(run.report_dir / "index.html");
  EXPECT_NE(html.find("ParaView Loading Guide"), std::string::npos);
  EXPECT_NE(html.find("paraview_loading_guide.md"), std::string::npos);

  cleanup_report_run(run);
  remove_directory_if_present(paraview_dir);
}
