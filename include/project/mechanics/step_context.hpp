#pragma once

#include "project/core/stroke_command.hpp"
#include "project/mechanics/state.hpp"

namespace project {

struct StepContext {
  double time_s{};
  StrokeActuationCommand stroke_command{};
  MechanicalStateSnapshot state;

  bool operator==(const StepContext &) const = default;
};

} // namespace project
