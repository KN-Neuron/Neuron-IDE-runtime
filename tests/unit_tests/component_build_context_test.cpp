#include <gtest/gtest.h>

#include <filesystem>

#include "scene/ImageLibrary.hpp"
#include "scene/components/ComponentBuildContext.hpp"

namespace {
namespace fs = std::filesystem;

// temp_directory_path() is absolute on every platform, unlike a literal such as "/data".
fs::path experimentFolder() { return fs::temp_directory_path() / "experiment"; }

class ComponentBuildContextTest : public ::testing::Test {
   protected:
    ImageLibrary& images() { return library; }

   private:
    ImageLibrary library;
};
}  // namespace

TEST_F(ComponentBuildContextTest, ResolvesRelativePathAgainstAssetRoot) {
    const ComponentBuildContext context(experimentFolder(), images());

    EXPECT_EQ(context.resolveAsset("stimuli/face.png"), experimentFolder() / "stimuli/face.png");
}

TEST_F(ComponentBuildContextTest, KeepsAbsolutePathAsIs) {
    const ComponentBuildContext context(experimentFolder(), images());
    const fs::path              absolute = fs::temp_directory_path() / "elsewhere" / "face.png";

    EXPECT_EQ(context.resolveAsset(absolute), absolute);
}

TEST_F(ComponentBuildContextTest, KeepsEmptyPathEmpty) {
    const ComponentBuildContext context(experimentFolder(), images());

    EXPECT_TRUE(context.resolveAsset(fs::path{}).empty());
}

TEST_F(ComponentBuildContextTest, WithoutAssetRootLeavesRelativePathForWorkingDirectory) {
    const ComponentBuildContext context({}, images());

    EXPECT_EQ(context.resolveAsset("face.png"), fs::path("face.png"));
}
