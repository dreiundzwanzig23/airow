#include <gtest/gtest.h>

#include "project/string_utils.hpp"

/**
 * @test IT-001
 * @verifies [A-001]
 * @notes Given a string utility component in normal app-style use, when the
 * uppercase API is called, then deterministic uppercase output is produced.
 */
TEST(StringUtilsIntegration, UpperAsciiContract) {
  EXPECT_EQ(project::to_upper_ascii("abcXYZ"), "ABCXYZ");
  EXPECT_EQ(project::to_upper_ascii("already UPPER"), "ALREADY UPPER");
}
