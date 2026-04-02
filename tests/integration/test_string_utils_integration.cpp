#include <gtest/gtest.h>

#include "project/string_utils.hpp"

/**
 * @test IT-900
 * @verifies [A-900]
 * @notes Given the bootstrap placeholder component in normal app-style use,
 * when the uppercase API is called, then deterministic uppercase output is
 * produced.
 */
TEST(StringUtilsIntegration, UpperAsciiContract) {
  EXPECT_EQ(project::to_upper_ascii("abcXYZ"), "ABCXYZ");
  EXPECT_EQ(project::to_upper_ascii("already UPPER"), "ALREADY UPPER");
}
