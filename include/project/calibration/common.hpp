#pragma once

#include <string>

namespace project {

struct ArtifactStateConventions {
  std::string world_frame;
  std::string body_frame;
  std::string orientation;

  bool operator==(const ArtifactStateConventions &) const = default;
};

struct ArtifactReferenceContract {
  std::string boat_id;
  std::string rigging_id;
  std::string athlete_id;

  bool operator==(const ArtifactReferenceContract &) const = default;
};

} // namespace project
