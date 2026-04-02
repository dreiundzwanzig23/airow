#include <gtest/gtest.h>

#include "project/string_utils.hpp"

/**
 * @test QT-900
 * @verifies [R-900]
 * @notes Given the bootstrap placeholder utility, when the sample behavior is
 * exercised, then trace and gate wiring stay intact without claiming simulator
 * feature coverage.
 */
TEST(StringUtilsSystem, RequirementAcceptanceExamples) {
  EXPECT_EQ(project::to_upper_ascii("abcXYZ"), "ABCXYZ");
  EXPECT_EQ(project::to_upper_ascii("123!"), "123!");
}
