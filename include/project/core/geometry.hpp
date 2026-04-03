#pragma once

namespace project {

struct Vector3 {
  double x{};
  double y{};
  double z{};

  bool operator==(const Vector3 &) const = default;
};

struct Quaternion {
  double x{};
  double y{};
  double z{};
  double w{1.0};

  bool operator==(const Quaternion &) const = default;
};

} // namespace project
