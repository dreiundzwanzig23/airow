#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_output.hpp"

namespace {

std::string output_schema_base_json() {
  return R"({
    "config_id": "ut-output-schema-errors",
    "simulation": {
      "duration_s": 1.0,
      "time_step_s": 0.25
    },
    "hull": {
      "mass_kg": 14.0,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.1, 7.8, 8.2],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    },
    "output": __OUTPUT__
  })";
}

template <typename ParseResult>
bool normalized_contains(const ParseResult &parsed, std::string_view key,
                         std::string_view value, std::string_view unit) {
  return std::any_of(
      parsed.normalized_config.begin(), parsed.normalized_config.end(),
      [&](const project::NormalizedConfigEntry &entry) {
        return entry.key == key && entry.value == value && entry.unit == unit;
      });
}

} // namespace

/**
 * @test UT-036
 * @verifies [D-021]
 * @notes Given output settings in JSON configuration, when parsing succeeds,
 * then normalized config metadata includes output paths and the high-frequency
 * output toggle.
 */
TEST(RunOutputsConfig, ParsesOutputSettingsFromConfigSchema) {
  const std::string config_text = R"({
    "config_id": "ut-output-config",
    "simulation": {
      "duration_s": 1.0,
      "time_step_s": 0.25
    },
    "hull": {
      "mass_kg": 14.0,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.1, 7.8, 8.2],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    },
    "output": {
      "summary_path": "results/summary.json",
      "time_series_path": "results/timeseries.json",
      "high_frequency_time_series": true
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_TRUE(parsed.ok());
  ASSERT_TRUE(parsed.config.has_value());
  EXPECT_EQ(parsed.config->output.summary_path, "results/summary.json");
  EXPECT_EQ(parsed.config->output.time_series_path, "results/timeseries.json");
  EXPECT_TRUE(parsed.config->output.high_frequency_time_series);
  EXPECT_TRUE(normalized_contains(parsed, "$.output.summary_path",
                                  "results/summary.json", ""));
  EXPECT_TRUE(normalized_contains(parsed, "$.output.time_series_path",
                                  "results/timeseries.json", ""));
  EXPECT_TRUE(normalized_contains(parsed, "$.output.high_frequency_time_series",
                                  "true", "bool"));
}

/**
 * @test UT-043
 * @verifies [D-021]
 * @notes Given a non-object output block, when config parsing executes, then
 * deterministic type diagnostics point to the top-level output path.
 */
TEST(RunOutputsConfig, RejectsNonObjectOutputSchema) {
  auto config_text = output_schema_base_json();
  config_text.replace(config_text.find("__OUTPUT__"),
                      std::string("__OUTPUT__").size(), "7");
  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output");
}

/**
 * @test UT-150
 * @verifies [D-021]
 * @notes Given a non-string summary path inside the output block, when config
 * parsing executes, then deterministic type diagnostics point to the summary
 * path field.
 */
TEST(RunOutputsConfig, RejectsNonStringSummaryPathInOutputSchema) {
  auto config_text = output_schema_base_json();
  config_text.replace(config_text.find("__OUTPUT__"),
                      std::string("__OUTPUT__").size(),
                      R"({"summary_path": 7})");
  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.summary_path");
}

/**
 * @test UT-151
 * @verifies [D-021]
 * @notes Given a non-boolean high-frequency toggle inside the output block,
 * when config parsing executes, then deterministic type diagnostics point to
 * the high-frequency field.
 */
TEST(RunOutputsConfig, RejectsNonBooleanHighFrequencyOutputToggle) {
  auto config_text = output_schema_base_json();
  config_text.replace(config_text.find("__OUTPUT__"),
                      std::string("__OUTPUT__").size(),
                      R"({"high_frequency_time_series": "yes"})");
  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(parsed.diagnostics.front().path,
            "$.output.high_frequency_time_series");
}

/**
 * @test UT-044
 * @verifies [D-021]
 * @notes Given explicit output-format selection, when parsing succeeds, then
 * JSON and HDF5 format flags plus HDF5 path are normalized deterministically.
 */
TEST(RunOutputsConfig, ParsesOutputFormatSelectionAndHdf5Path) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  const std::string config_text = R"({
    "config_id": "ut-output-format-select",
    "simulation": {
      "duration_s": 1.0,
      "time_step_s": 0.25
    },
    "hull": {
      "mass_kg": 14.0,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.1, 7.8, 8.2],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    },
    "output": {
      "formats": ["json", "hdf5"],
      "hdf5_path": "results/run.h5",
      "high_frequency_time_series": true
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_TRUE(parsed.ok());
  ASSERT_TRUE(parsed.config.has_value());
  EXPECT_TRUE(parsed.config->output.emit_json);
  EXPECT_TRUE(parsed.config->output.emit_hdf5);
  EXPECT_EQ(parsed.config->output.hdf5_path, "results/run.h5");
  EXPECT_TRUE(
      normalized_contains(parsed, "$.output.formats", "[json, hdf5]", ""));
  EXPECT_TRUE(
      normalized_contains(parsed, "$.output.hdf5_path", "results/run.h5", ""));
}

/**
 * @test UT-045
 * @verifies [D-021]
 * @notes Given unknown output formats, when parsing executes, then
 * deterministic format diagnostics reject unsupported values.
 */
TEST(RunOutputsConfig, RejectsUnknownOutputFormats) {
  const std::string config_text = R"({
    "config_id": "ut-output-format-unknown",
    "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
    "hull": {
      "mass_kg": 14.0,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.1, 7.8, 8.2],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    },
    "output": {
      "formats": ["json", "csv"]
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats[1]");
}

/**
 * @test UT-046
 * @verifies [D-021]
 * @notes Given an empty output format list, when parsing executes, then
 * deterministic validation rejects empty format selection.
 */
TEST(RunOutputsConfig, RejectsEmptyOutputFormatList) {
  const std::string config_text = R"({
    "config_id": "ut-output-format-empty",
    "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
    "hull": {
      "mass_kg": 14.0,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.1, 7.8, 8.2],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    },
    "output": {
      "formats": []
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats");
}

/**
 * @test UT-202
 * @verifies [D-021]
 * @notes Given a non-array output formats field, when config parsing
 * executes, then deterministic type diagnostics point to the formats path.
 */
TEST(RunOutputsConfig, RejectsNonArrayOutputFormatsField) {
  auto config_text = output_schema_base_json();
  config_text.replace(config_text.find("__OUTPUT__"),
                      std::string("__OUTPUT__").size(),
                      R"({"formats": "json"})");

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats");
}

/**
 * @test UT-203
 * @verifies [D-021]
 * @notes Given a non-string entry inside the output formats list, when config
 * parsing executes, then deterministic type diagnostics point to the indexed
 * formats path.
 */
TEST(RunOutputsConfig, RejectsNonStringOutputFormatEntry) {
  auto config_text = output_schema_base_json();
  config_text.replace(config_text.find("__OUTPUT__"),
                      std::string("__OUTPUT__").size(),
                      R"({"formats": ["json", 7]})");

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats[1]");
}

/**
 * @test UT-047
 * @verifies [D-021]
 * @notes Given HDF5 output requested on a build without HDF5 support, when
 * parsing executes, then deterministic diagnostics reject unavailable output
 * format selection.
 */
TEST(RunOutputsConfig, RejectsHdf5FormatWhenUnavailable) {
  if (project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support available on this build";
  }

  const std::string config_text = R"({
    "config_id": "ut-output-hdf5-unavailable",
    "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
    "hull": {
      "mass_kg": 14.0,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.1, 7.8, 8.2],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    },
    "output": {
      "formats": ["hdf5"],
      "hdf5_path": "results/run.h5"
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "unsupported_value");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats[0]");
}
