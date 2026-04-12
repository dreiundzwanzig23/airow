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
#error "PROJECT_APP_PATH is required for system tests"
#endif

#ifndef PROJECT_SOURCE_DIR
#error "PROJECT_SOURCE_DIR is required for system tests"
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};
const std::filesystem::path kProjectSourceDir{PROJECT_SOURCE_DIR};

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

void clear_example_output(std::string_view example_name) {
  const auto output_dir =
      kProjectSourceDir / "examples" / "output" / std::string(example_name);
  remove_file_if_present(output_dir / "summary.json");
  remove_file_if_present(output_dir / "time_series.json");
}

} // namespace

/**
 * @test QT-014
 * @verifies [R-002, R-015]
 * @notes Given the checked-in example helper script, when it is invoked from
 * the repo root for the calm-water example, then the headless CLI succeeds and
 * writes the documented machine-readable artifacts under `examples/output`.
 */
TEST(ExamplesSystem, HelperScriptRunsCalmWaterExample) {
  clear_example_output("calm_water_stroke");

  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-example-helper.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-example-helper.stderr";
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);

  const auto command = "cd " + shell_quote(kProjectSourceDir.string()) +
                       " && ./examples/run_example.sh calm_water_stroke > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());

  const auto stdout_text = read_file(stdout_path);
  EXPECT_NE(stdout_text.find("example=calm_water_stroke"), std::string::npos);
  EXPECT_NE(stdout_text.find("status=success"), std::string::npos);

  const auto summary_path =
      kProjectSourceDir / "examples/output/calm_water_stroke/summary.json";
  const auto time_series_path =
      kProjectSourceDir / "examples/output/calm_water_stroke/time_series.json";
  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(time_series_path));

  const auto summary = Json::parse(read_file(summary_path));
  EXPECT_EQ(summary.at("config_id").get<std::string>(), "calm-water-stroke");

  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  clear_example_output("calm_water_stroke");
}

/**
 * @test QT-015
 * @verifies [R-002, R-015]
 * @notes Given a checked-in example config, when the CLI is invoked directly
 * from the repo root with that config path, then the run succeeds and writes
 * the documented example artifacts.
 */
TEST(ExamplesSystem, DirectCliRunUsesCheckedInExampleConfig) {
  clear_example_output("passive_float");

  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-example-direct.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-example-direct.stderr";
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);

  const auto command = "cd " + shell_quote(kProjectSourceDir.string()) +
                       " && " + shell_quote(kProjectAppPath.string()) +
                       " --config examples/passive_float/config.json > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());
  EXPECT_NE(read_file(stdout_path).find("status=success"), std::string::npos);

  const auto summary_path =
      kProjectSourceDir / "examples/output/passive_float/summary.json";
  const auto time_series_path =
      kProjectSourceDir / "examples/output/passive_float/time_series.json";
  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(time_series_path));

  const auto summary = Json::parse(read_file(summary_path));
  EXPECT_EQ(summary.at("config_id").get<std::string>(), "passive-float");

  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  clear_example_output("passive_float");
}
