#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "scene/Image.hpp"
#include "scene/ImageLibrary.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/SpriteRenderer.hpp"
#include "utils/SdlTestUtils.hpp"
#include "utils/TempDir.hpp"

namespace {
namespace fs = std::filesystem;

constexpr int         kCanvasSize = 4;
constexpr utils::Rgba kBackground{0, 0, 0, 255};
constexpr utils::Rgba kWhite{255, 255, 255, 255};

class ImageLibraryTest : public ::testing::Test {
   protected:
    void SetUp() override {
        utils::writeTwoPixelPng(dir() / "a.png", kWhite, kWhite);
        utils::writeTwoPixelPng(dir() / "b.png", kWhite, kWhite);
    }

    const fs::path& dir() const { return tempDir.path(); }

   private:
    utils::TempDir tempDir;
};
}  // namespace

TEST_F(ImageLibraryTest, LoadsEachFileOnce) {
    ImageLibrary library;

    EXPECT_EQ(library.load(dir() / "a.png"), library.load(dir() / "a.png"));
    EXPECT_EQ(library.load(dir() / "." / "a.png"), library.load(dir() / "a.png"));
    EXPECT_EQ(library.size(), 1U);
}

TEST_F(ImageLibraryTest, KeepsDifferentFilesApart) {
    ImageLibrary library;

    EXPECT_NE(library.load(dir() / "a.png"), library.load(dir() / "b.png"));
    EXPECT_EQ(library.size(), 2U);
}

TEST_F(ImageLibraryTest, MissingFileThrowsAndIsNotRemembered) {
    ImageLibrary library;

    EXPECT_THROW(library.load(dir() / "missing.png"), std::runtime_error);
    EXPECT_THROW(library.load(dir() / "missing.png"), std::runtime_error);
    EXPECT_EQ(library.size(), 0U);
}

TEST_F(ImageLibraryTest, FileThatIsNotAnImageThrows) {
    std::ofstream(dir() / "notes.png") << "not an image";
    ImageLibrary library;

    EXPECT_THROW(library.load(dir() / "notes.png"), std::runtime_error);
}

TEST_F(ImageLibraryTest, UploadGivesEveryImageATexture) {
    ImageLibrary library;
    const auto   first  = library.load(dir() / "a.png");
    const auto   second = library.load(dir() / "b.png");
    ASSERT_EQ(first->texture(), nullptr);

    library.upload(utils::makeSoftwareTarget(kCanvasSize, kCanvasSize));

    EXPECT_NE(first->texture(), nullptr);
    EXPECT_NE(second->texture(), nullptr);
}

TEST_F(ImageLibraryTest, UploadToNullTargetThrows) {
    Image image(dir() / "a.png");

    EXPECT_THROW(image.upload(nullptr), std::invalid_argument);
}

// After a device reset the scene uploads again, so a second upload must leave a drawable texture.
TEST_F(ImageLibraryTest, ImageCanBeUploadedAgain) {
    const auto target = utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
    const auto image  = std::make_shared<Image>(dir() / "a.png");
    const auto owner  = std::make_shared<SceneObject>("Stimulus");
    owner->setTransform({0.0, 0.0, kCanvasSize, kCanvasSize, 0.0});
    SpriteRenderer sprite(owner, image);

    image->upload(target);
    image->upload(target);
    utils::clear(target.get(), kBackground);
    sprite.render(target.get());

    EXPECT_EQ(utils::readPixel(target.get(), 1, 1), kWhite);
}

// SDL frees a renderer's textures along with it, so destroying the texture afterwards would be a
// use-after-free. The texture therefore keeps its renderer alive.
TEST_F(ImageLibraryTest, TextureKeepsRenderTargetAlive) {
    auto image  = std::make_unique<Image>(dir() / "a.png");
    auto target = utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
    image->upload(target);

    const std::weak_ptr<SDL_Renderer> weakTarget = target;
    target.reset();
    EXPECT_FALSE(weakTarget.expired());

    image.reset();
    EXPECT_TRUE(weakTarget.expired());
}
