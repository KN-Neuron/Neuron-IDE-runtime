#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "scene/SceneObject.hpp"
#include "scene/components/SpriteRenderer.hpp"
#include "utils/SdlTestUtils.hpp"
#include "utils/TempDir.hpp"

namespace {
constexpr int    kCanvasSize   = 40;
constexpr double kSpriteX      = 10.0;
constexpr double kSpriteY      = 20.0;
constexpr double kSpriteWidth  = 20.0;
constexpr double kSpriteHeight = 4.0;
constexpr double kQuarterTurn  = 90.0;

constexpr utils::Rgba kBackground{0, 0, 0, 255};
constexpr utils::Rgba kLeft{255, 0, 0, 255};
constexpr utils::Rgba kRight{0, 0, 255, 255};

// Probe points sit well inside one half of the sprite, away from edges where rounding could go
// either way. Unturned, the sprite covers x 10..30 and y 20..24, its left half x 10..20.
constexpr int kLeftHalfX  = 12;
constexpr int kRightHalfX = 27;
constexpr int kRowY       = 21;
// Turned a quarter clockwise about its centre (20, 22), it covers x 18..22 and y 12..32, with
// what was its left half now on top.
constexpr int kColumnX = 20;
constexpr int kTopY    = 14;
constexpr int kBottomY = 29;
constexpr int kOutside = 5;

class SpriteRendererTest : public ::testing::Test {
   protected:
    void SetUp() override {
        image = tempDir.path() / "sprite.png";
        utils::writeTwoPixelPng(image, kLeft, kRight);

        object = std::make_shared<SceneObject>("Stimulus");
        object->setTransform({kSpriteX, kSpriteY, kSpriteWidth, kSpriteHeight, 0.0});

        renderTarget = utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
        utils::clear(renderTarget.get(), kBackground);
    }

    const std::filesystem::path&         dir() const { return tempDir.path(); }
    const std::filesystem::path&         imagePath() const { return image; }
    const std::shared_ptr<SceneObject>&  owner() const { return object; }
    const std::shared_ptr<SDL_Renderer>& target() const { return renderTarget; }
    void                                 releaseTarget() { renderTarget.reset(); }

    utils::Rgba pixelAt(int column, int row) const {
        return utils::readPixel(renderTarget.get(), column, row);
    }

   private:
    utils::TempDir                tempDir;
    std::filesystem::path         image;
    std::shared_ptr<SceneObject>  object;
    std::shared_ptr<SDL_Renderer> renderTarget;
};
}  // namespace

TEST_F(SpriteRendererTest, StretchesImageOverTransform) {
    SpriteRenderer sprite(owner(), imagePath());
    sprite.onRenderTargetReady(target());
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kLeft);
    EXPECT_EQ(pixelAt(kRightHalfX, kRowY), kRight);
    EXPECT_EQ(pixelAt(kColumnX, kTopY), kBackground);
    EXPECT_EQ(pixelAt(kOutside, kOutside), kBackground);
}

TEST_F(SpriteRendererTest, RotatesClockwiseAboutCentre) {
    owner()->transform.rotation = kQuarterTurn;

    SpriteRenderer sprite(owner(), imagePath());
    sprite.onRenderTargetReady(target());
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kColumnX, kTopY), kLeft);
    EXPECT_EQ(pixelAt(kColumnX, kBottomY), kRight);
    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kBackground);
}

TEST_F(SpriteRendererTest, DrawsNothingBeforeRenderTargetIsReady) {
    SpriteRenderer sprite(owner(), imagePath());
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kBackground);
}

TEST_F(SpriteRendererTest, ThrowsWhenImageIsMissing) {
    EXPECT_THROW(SpriteRenderer(owner(), dir() / "missing.png"), std::runtime_error);
}

TEST_F(SpriteRendererTest, ThrowsWhenFileIsNotAnImage) {
    const auto notAnImage = dir() / "notes.png";
    std::ofstream(notAnImage) << "not an image";

    EXPECT_THROW(SpriteRenderer(owner(), notAnImage), std::runtime_error);
}

TEST_F(SpriteRendererTest, ThrowsWhenImagePathIsEmpty) {
    EXPECT_THROW(SpriteRenderer(owner(), std::filesystem::path{}), std::invalid_argument);
}

TEST_F(SpriteRendererTest, ThrowsWhenOwnerIsNull) {
    EXPECT_THROW(SpriteRenderer(nullptr, imagePath()), std::invalid_argument);
}

TEST_F(SpriteRendererTest, ThrowsWhenRenderTargetIsNull) {
    SpriteRenderer sprite(owner(), imagePath());

    EXPECT_THROW(sprite.onRenderTargetReady(nullptr), std::invalid_argument);
}

TEST_F(SpriteRendererTest, ThrowsWhenOwnerHasNoArea) {
    owner()->transform.width = 0.0;

    EXPECT_THROW(SpriteRenderer(owner(), imagePath()), std::invalid_argument);
}

TEST_F(SpriteRendererTest, RejectsASecondRenderTarget) {
    SpriteRenderer sprite(owner(), imagePath());
    sprite.onRenderTargetReady(target());

    EXPECT_THROW(sprite.onRenderTargetReady(target()), std::logic_error);
}

// SDL frees a renderer's textures along with it, so destroying the texture afterwards would be a
// use-after-free. The texture therefore keeps its renderer alive.
TEST_F(SpriteRendererTest, KeepsRenderTargetAliveWhileTextureExists) {
    auto sprite = std::make_unique<SpriteRenderer>(owner(), imagePath());
    sprite->onRenderTargetReady(target());

    const std::weak_ptr<SDL_Renderer> weakTarget = target();
    releaseTarget();
    EXPECT_FALSE(weakTarget.expired());

    sprite.reset();
    EXPECT_TRUE(weakTarget.expired());
}
