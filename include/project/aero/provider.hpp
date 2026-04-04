#pragma once

#include <string_view>

#include "project/core/geometry.hpp"
#include "project/mechanics/step_context.hpp"

namespace project {

struct AeroLoadSample {
  Vector3 apparent_wind_world_mps;
  Vector3 force_world_n;
  Vector3 moment_world_n_m;

  bool operator==(const AeroLoadSample &) const = default;
};

class AeroProvider {
public:
  virtual ~AeroProvider() = default;

  [[nodiscard]] virtual std::string_view identifier() const noexcept = 0;
  virtual AeroLoadSample sample_load(const StepContext &context,
                                     const Vector3 &ambient_wind_world_mps) = 0;
};

} // namespace project
