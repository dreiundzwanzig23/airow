#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

#include "project/orchestrator/simulation_run.hpp"

namespace project {

struct CliDependencies {
  SimulationDependencies simulation;
};

int run_headless_cli(std::span<const std::string_view> args, std::ostream &out,
                     std::ostream &err,
                     const CliDependencies &dependencies = {});

} // namespace project
