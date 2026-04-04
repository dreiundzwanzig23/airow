#pragma once

#include "project/mechanics/state.hpp"

namespace project {

struct StepContext {
  double time_s{};
  MechanicalStateSnapshot state;

  bool operator==(const StepContext &) const = default;
};

} // namespace project
