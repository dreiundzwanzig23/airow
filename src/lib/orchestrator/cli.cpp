#include "project/orchestrator/cli.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_result.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace project {

namespace {

/**
 * @design D-031 — Optional CLI report-mode selection
 * @title Deterministic parsing and rendering-mode selection for human-readable
 * run reports
 * @satisfies [A-002]
 */
std::optional<RunAnalysisReportMode> parse_report_mode(std::string_view value) {
  if (value == "compact") {
    return RunAnalysisReportMode::compact;
  }
  if (value == "full") {
    return RunAnalysisReportMode::full;
  }
  return std::nullopt;
}

} // namespace

/**
 * @design D-014 — Headless CLI argument handling and exit mapping
 * @title Minimal command-line entry wrapper for single-run orchestration
 * @satisfies [A-002]
 * @notes Parses the baseline `--config` CLI contract, delegates to the shared
 * run path, and maps result status to stable process exit codes and concise
 * terminal output.
 */
int run_headless_cli(std::span<const std::string_view> args, std::ostream &out,
                     std::ostream &err, const CliDependencies &dependencies) {
  std::string_view config_path;
  std::optional<RunAnalysisReportMode> report_mode;
  for (std::size_t index = 0; index < args.size();) {
    if (args[index] == "--config" && index + 1U < args.size() &&
        !args[index + 1U].empty()) {
      config_path = args[index + 1U];
      index += 2U;
      continue;
    }
    if (args[index] == "--report" && index + 1U < args.size()) {
      report_mode = parse_report_mode(args[index + 1U]);
      if (!report_mode.has_value()) {
        err << "usage: project_app --config <path> [--report <compact|full>]\n";
        return 64;
      }
      index += 2U;
      continue;
    }
    err << "usage: project_app --config <path> [--report <compact|full>]\n";
    return 64;
  }
  if (config_path.empty()) {
    err << "usage: project_app --config <path> [--report <compact|full>]\n";
    return 64;
  }

  const auto result = run_simulation_from_config_file(
      std::filesystem::path(config_path), dependencies.simulation);

  if (result.status == RunStatus::success) {
    out << "status=success" << " config_id=" << result.metadata.config_id
        << " version=" << result.metadata.simulator_version
        << " advancer=" << result.metadata.state_advancer_id
        << " startup=" << result.metadata.startup_status
        << " start=" << result.metadata.start_timestamp_utc
        << " end=" << result.metadata.end_timestamp_utc
        << " steps=" << result.summary.executed_step_count << '\n';
    if (report_mode.has_value()) {
      out << '\n' << format_run_analysis_report(result, *report_mode) << '\n';
    }
    return 0;
  }

  const auto exit_code =
      result.status == RunStatus::configuration_error ? 2 : 3;
  err << "status="
      << (result.status == RunStatus::configuration_error
              ? "configuration_error"
              : "runtime_error")
      << " version=" << result.metadata.simulator_version
      << " advancer=" << result.metadata.state_advancer_id
      << " startup=" << result.metadata.startup_status
      << " start=" << result.metadata.start_timestamp_utc
      << " end=" << result.metadata.end_timestamp_utc << '\n';
  for (const auto &diagnostic : result.diagnostics) {
    err << "diagnostic" << " code=" << diagnostic.code
        << " subsystem=" << diagnostic.subsystem << " path=" << diagnostic.path
        << " message=" << diagnostic.message << '\n';
  }
  return exit_code;
}

} // namespace project
