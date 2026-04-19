#pragma once

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_result.hpp"

namespace project {
[[nodiscard]] bool hdf5_output_supported() noexcept;

void emit_run_outputs(const SimulatorConfig &config,
                      SimulationRunResult &result);

void emit_batch_outputs(std::string_view batch_id,
                        std::string_view summary_path,
                        BatchSimulationResult &result);

} // namespace project
