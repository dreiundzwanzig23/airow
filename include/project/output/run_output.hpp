#pragma once

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_result.hpp"

namespace project {
[[nodiscard]] bool hdf5_output_supported() noexcept;

void emit_run_outputs(const SimulatorConfig &config,
                      SimulationRunResult &result);

} // namespace project
