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

bool should_fallback_to_single_run(const BatchSimulationResult &result) {
  return result.status == RunStatus::configuration_error &&
         result.case_results.empty() && result.diagnostics.size() == 1U &&
         result.diagnostics.front().code == "missing_required_field" &&
         result.diagnostics.front().path == "$.batch";
}

constexpr std::string_view CLI_USAGE =
    "usage: project_app --config <path> [--report <compact|full>]\n";

struct ParsedCliArgs {
  std::string_view config_path;
  std::optional<RunAnalysisReportMode> report_mode;
};

std::string_view run_status_text(RunStatus status) {
  switch (status) {
  case RunStatus::success:
    return "success";
  case RunStatus::configuration_error:
    return "configuration_error";
  case RunStatus::runtime_error:
    return "runtime_error";
  }
  return "runtime_error";
}

std::optional<ParsedCliArgs>
parse_cli_args(std::span<const std::string_view> args, std::ostream &err) {
  ParsedCliArgs parsed;
  for (std::size_t index = 0; index < args.size();) {
    if (args[index] == "--config" && index + 1U < args.size() &&
        !args[index + 1U].empty()) {
      parsed.config_path = args[index + 1U];
      index += 2U;
      continue;
    }
    if (args[index] == "--report" && index + 1U < args.size()) {
      parsed.report_mode = parse_report_mode(args[index + 1U]);
      if (!parsed.report_mode.has_value()) {
        err << CLI_USAGE;
        return std::nullopt;
      }
      index += 2U;
      continue;
    }
    err << CLI_USAGE;
    return std::nullopt;
  }
  if (parsed.config_path.empty()) {
    err << CLI_USAGE;
    return std::nullopt;
  }
  return parsed;
}

int write_batch_success(const BatchSimulationResult &batch_result,
                        std::ostream &out) {
  const auto failure_count = batch_result.summary.total_case_count -
                             batch_result.summary.succeeded_case_count;
  out << "status=success" << " batch_id=" << batch_result.batch_id
      << " cases=" << batch_result.summary.total_case_count
      << " succeeded=" << batch_result.summary.succeeded_case_count
      << " failed=" << failure_count
      << " start=" << batch_result.start_timestamp_utc
      << " end=" << batch_result.end_timestamp_utc
      << " summary=" << batch_result.outputs.summary_path << '\n';
  return 0;
}

int write_batch_failure(const BatchSimulationResult &batch_result,
                        std::ostream &err) {
  const auto failure_count = batch_result.summary.total_case_count -
                             batch_result.summary.succeeded_case_count;
  const auto exit_code =
      batch_result.status == RunStatus::configuration_error ? 2 : 3;
  err << "status=" << run_status_text(batch_result.status)
      << " batch_id=" << batch_result.batch_id
      << " cases=" << batch_result.summary.total_case_count
      << " succeeded=" << batch_result.summary.succeeded_case_count
      << " failed=" << failure_count
      << " start=" << batch_result.start_timestamp_utc
      << " end=" << batch_result.end_timestamp_utc
      << " summary=" << batch_result.outputs.summary_path << '\n';
  for (const auto &diagnostic : batch_result.diagnostics) {
    err << "diagnostic" << " code=" << diagnostic.code
        << " subsystem=" << diagnostic.subsystem << " path=" << diagnostic.path
        << " message=" << diagnostic.message << '\n';
  }
  for (const auto &case_result : batch_result.case_results) {
    err << "case" << " case_id=" << case_result.case_id
        << " status=" << run_status_text(case_result.run_result.status)
        << " config_id=" << case_result.run_result.metadata.config_id
        << " steps=" << case_result.run_result.summary.executed_step_count
        << '\n';
    for (const auto &diagnostic : case_result.run_result.diagnostics) {
      err << "diagnostic" << " case_id=" << case_result.case_id
          << " code=" << diagnostic.code
          << " subsystem=" << diagnostic.subsystem
          << " path=" << diagnostic.path << " message=" << diagnostic.message
          << '\n';
    }
  }
  return exit_code;
}

int write_single_result(
    const SimulationRunResult &result, std::ostream &out, std::ostream &err,
    const std::optional<RunAnalysisReportMode> &report_mode) {
  if (result.status == RunStatus::success) {
    out << "status=success" << " config_id=" << result.metadata.config_id
        << " version=" << result.metadata.simulator_version
        << " mechanics_backend=" << result.metadata.mechanics_backend_id
        << " integration_backend=" << result.metadata.integration_backend_id
        << " startup=" << result.metadata.startup_status
        << " start=" << result.metadata.start_timestamp_utc
        << " end=" << result.metadata.end_timestamp_utc
        << " steps=" << result.summary.executed_step_count
        << " summary=" << result.outputs.summary_path
        << " time_series=" << result.outputs.time_series_path;
    if (!result.outputs.visualization_path.empty()) {
      out << " visualization=" << result.outputs.visualization_path;
    }
    if (!result.outputs.hdf5_path.empty() && result.outputs.emit_hdf5) {
      out << " hdf5=" << result.outputs.hdf5_path;
    }
    if (!result.outputs.truth_model_export_path.empty()) {
      out << " truth_model=" << result.outputs.truth_model_export_path;
    }
    out << '\n';
    if (report_mode.has_value()) {
      out << '\n' << format_run_analysis_report(result, *report_mode) << '\n';
    }
    return 0;
  }

  const auto exit_code =
      result.status == RunStatus::configuration_error ? 2 : 3;
  err << "status=" << run_status_text(result.status)
      << " version=" << result.metadata.simulator_version
      << " mechanics_backend=" << result.metadata.mechanics_backend_id
      << " integration_backend=" << result.metadata.integration_backend_id
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

} // namespace

/**
 * @design D-014 — Headless CLI argument handling and exit mapping
 * @title Minimal command-line entry wrapper for single-run and batch
 * orchestration
 * @satisfies [A-002]
 * @notes Parses the baseline `--config` CLI contract, dispatches to the
 * shared single-run or batch path, and maps result status to stable process
 * exit codes and concise terminal output.
 */
int run_headless_cli(std::span<const std::string_view> args, std::ostream &out,
                     std::ostream &err, const CliDependencies &dependencies) {
  const auto parsed = parse_cli_args(args, err);
  if (!parsed.has_value()) {
    return 64;
  }

  const auto batch_result = run_batch_simulation_from_config_file(
      std::filesystem::path(parsed->config_path), dependencies.simulation);
  if (!should_fallback_to_single_run(batch_result)) {
    if (parsed->report_mode.has_value()) {
      err << CLI_USAGE;
      return 64;
    }
    if (batch_result.status == RunStatus::success) {
      return write_batch_success(batch_result, out);
    }
    return write_batch_failure(batch_result, err);
  }

  const auto result = run_simulation_from_config_file(
      std::filesystem::path(parsed->config_path), dependencies.simulation);
  return write_single_result(result, out, err, parsed->report_mode);
}

} // namespace project
