#include <gtest/gtest.h>

#include <filesystem>

#include "scene/components/ComponentBuildContext.hpp"

namespace {
namespace fs = std::filesystem;

// temp_directory_path() is absolute on every platform, unlike a literal such as "/data".
fs::path experimentFolder() { return fs::temp_directory_path() / "experiment"; }
}  // namespace

TEST(ComponentBuildContextTest, ResolvesRelativePathAgainstAssetRoot) {
    const ComponentBuildContext context{.assetRoot = experimentFolder()};

    EXPECT_EQ(context.resolveAsset("stimuli/face.png"), experimentFolder() / "stimuli/face.png");
}

TEST(ComponentBuildContextTest, KeepsAbsolutePathAsIs) {
    const ComponentBuildContext context{.assetRoot = experimentFolder()};
    const fs::path              absolute = fs::temp_directory_path() / "elsewhere" / "face.png";

    EXPECT_EQ(context.resolveAsset(absolute), absolute);
}

TEST(ComponentBuildContextTest, KeepsEmptyPathEmpty) {
    const ComponentBuildContext context{.assetRoot = experimentFolder()};

    EXPECT_TRUE(context.resolveAsset(fs::path{}).empty());
}

TEST(ComponentBuildContextTest, WithoutAssetRootLeavesRelativePathForWorkingDirectory) {
    const ComponentBuildContext context;

    EXPECT_EQ(context.resolveAsset("face.png"), fs::path("face.png"));
}
