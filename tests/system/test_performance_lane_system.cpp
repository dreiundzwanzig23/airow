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

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif

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

} // namespace

/**
 * @test QT-042
 * @verifies [R-026]
 * @notes Given the dedicated scenario-performance validation lane, when it
 * runs against the checked-in protected scenarios, then it emits a separate
 * validation summary and machine-readable budget report for the documented
 * runtime envelopes.
 */
TEST(PerformanceLaneSystem, ScriptEmitsSeparateBudgetReport) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-performance-summary.json";
  const auto report_path =
      std::filesystem::temp_directory_path() / "airow-qt-performance-report.json";
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-performance.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-performance.stderr";
  remove_file_if_present(summary_path);
  remove_file_if_present(report_path);

  const auto command = "cd " + shell_quote(kProjectSourceDir.string()) +
                       " && TEST_BUILD_DIR=build VALIDATION_SUMMARY_PATH=" +
                       shell_quote(summary_path.string()) +
                       " PERFORMANCE_BUDGET_REPORT_PATH=" +
                       shell_quote(report_path.string()) +
                       " ./scripts/test_performance.sh > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  ASSERT_EQ(decode_exit_code(status), 0);
  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(report_path));

  const Json summary = Json::parse(read_file(summary_path));
  const Json report = Json::parse(read_file(report_path));

  EXPECT_EQ(summary.at("script").get<std::string>(), "test_performance");
  EXPECT_EQ(report.at("status").get<std::string>(), "pass");
  EXPECT_EQ(report.at("manifest").at("schema_id").get<std::string>(),
            "scenario_performance_budgets.v1");
  ASSERT_EQ(report.at("scenario_results").size(), 5U);
  for (const auto &entry : report.at("scenario_results")) {
    EXPECT_EQ(entry.at("status").get<std::string>(), "pass");
    EXPECT_FALSE(entry.at("scenario_id").get<std::string>().empty());
    EXPECT_FALSE(entry.at("step_name").get<std::string>().empty());
  }

  remove_file_if_present(summary_path);
  remove_file_if_present(report_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
}
