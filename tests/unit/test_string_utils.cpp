#include "project/string_utils.hpp"
#include <gtest/gtest.h>

/**
 * @test UT-001
 * @verifies [D-001]
 * @notes Verifies uppercase transformation behavior.
 */
TEST(StringUtils, UpperAscii) {
  EXPECT_EQ(project::to_upper_ascii("abcXYZ"), "ABCXYZ");
  EXPECT_EQ(project::to_upper_ascii("123!"), "123!");
  EXPECT_EQ(project::to_upper_ascii("MiXeD"), "MIXED");
}
