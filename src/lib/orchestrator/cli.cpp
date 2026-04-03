#include "project/orchestrator/cli.hpp"

#include <filesystem>
#include <ostream>
#include <string>

namespace project {

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
  if (args.size() != 2 || args.front() != "--config" || args.back().empty()) {
    err << "usage: project_app --config <path>\n";
    return 64;
  }

  const auto result = run_simulation_from_config_file(
      std::filesystem::path(args.back()), dependencies.simulation);

  if (result.status == RunStatus::success) {
    out << "status=success" << " config_id=" << result.metadata.config_id
        << " version=" << result.metadata.simulator_version
        << " start=" << result.metadata.start_timestamp_utc
        << " end=" << result.metadata.end_timestamp_utc
        << " steps=" << result.summary.executed_step_count << '\n';
    return 0;
  }

  const auto exit_code =
      result.status == RunStatus::configuration_error ? 2 : 3;
  err << "status="
      << (result.status == RunStatus::configuration_error
              ? "configuration_error"
              : "runtime_error")
      << " version=" << result.metadata.simulator_version
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
