#include "project/string_utils.hpp"
#include <gtest/gtest.h>

/**
 * @test UT-900
 * @verifies [D-900]
 * @notes Verifies bootstrap placeholder uppercase behavior.
 */
TEST(StringUtils, UpperAscii) {
  EXPECT_EQ(project::to_upper_ascii("abcXYZ"), "ABCXYZ");
  EXPECT_EQ(project::to_upper_ascii("123!"), "123!");
  EXPECT_EQ(project::to_upper_ascii("MiXeD"), "MIXED");
}
