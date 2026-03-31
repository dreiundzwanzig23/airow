#include <gtest/gtest.h>

#include "project/string_utils.hpp"

/**
 * @test QT-001
 * @verifies [R-001]
 * @notes Given requirement-level input examples, when the CLI-facing library
 * behavior is exercised, then acceptance outcomes are met.
 */
TEST(StringUtilsSystem, RequirementAcceptanceExamples) {
  EXPECT_EQ(project::to_upper_ascii("abcXYZ"), "ABCXYZ");
  EXPECT_EQ(project::to_upper_ascii("123!"), "123!");
}
