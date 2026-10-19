#include <gtest/gtest.h>

#include <filesystem>

#include "utils/TempDir.hpp"

namespace {
class TempDirTest : public ::testing::TestWithParam<int> {};
}  // namespace

// The suite and test names of a parameterised test contain '/'.
TEST_P(TempDirTest, IsADirectChildOfTheSystemTempFolder) {
    std::filesystem::path root;
    {
        const utils::TempDir dir;
        root = dir.path();
        EXPECT_EQ(root.parent_path(), std::filesystem::temp_directory_path());
    }
    EXPECT_FALSE(std::filesystem::exists(root));
}

INSTANTIATE_TEST_SUITE_P(Names, TempDirTest, ::testing::Values(0));
