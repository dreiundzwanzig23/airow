#pragma once

#include "project/core/stroke_command.hpp"

namespace project {

struct SimulatorConfig;

StrokeActuationCommand
make_stroke_actuation_command(const SimulatorConfig &config);

} // namespace project
