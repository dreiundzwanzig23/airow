#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "project/calibration/common.hpp"
#include "project/core/geometry.hpp"

namespace project {

struct MeasurementBundleDiagnostic {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const MeasurementBundleDiagnostic &) const = default;
};

struct MeasurementBundleProvenance {
  std::string source_id;
  std::string artifact_version;
  std::string content_hash;
  std::string schema_id;

  bool operator==(const MeasurementBundleProvenance &) const = default;
};

struct MeasurementBundleBoatParameters {
  double mass_kg{};
  Vector3 center_of_mass_m;
  Vector3 inertia_kg_m2;

  bool operator==(const MeasurementBundleBoatParameters &) const = default;
};

struct MeasurementBundleRiggingSide {
  double inboard_length_m{};
  double outboard_length_m{};
  Vector3 oarlock_position_m;

  bool operator==(const MeasurementBundleRiggingSide &) const = default;
};

struct MeasurementBundleRigging {
  MeasurementBundleRiggingSide port;
  MeasurementBundleRiggingSide starboard;

  bool operator==(const MeasurementBundleRigging &) const = default;
};

struct MeasurementBundleAthleteParameters {
  double rower_mass_kg{};
  Vector3 body_center_of_mass_m;
  double seat_position_to_com_scale{};

  bool operator==(const MeasurementBundleAthleteParameters &) const = default;
};

struct MeasurementBundle {
  MeasurementBundleProvenance provenance;
  ArtifactStateConventions state_conventions;
  ArtifactReferenceContract reference_contract;
  MeasurementBundleBoatParameters boat;
  MeasurementBundleRigging rigging;
  MeasurementBundleAthleteParameters athlete;

  bool operator==(const MeasurementBundle &) const = default;
};

struct LoadMeasurementBundleResult {
  std::optional<MeasurementBundle> bundle;
  std::vector<MeasurementBundleDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return bundle.has_value() && diagnostics.empty();
  }
};

LoadMeasurementBundleResult
parse_measurement_bundle_text(std::string_view json_text,
                              std::string_view source_name = "<memory>");

LoadMeasurementBundleResult
load_measurement_bundle_file(const std::filesystem::path &path);

} // namespace project
