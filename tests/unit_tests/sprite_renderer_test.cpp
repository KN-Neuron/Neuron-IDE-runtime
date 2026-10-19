#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>

#include "scene/Image.hpp"
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
constexpr double kBeyondFloat  = 1e39;

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
        const auto path = tempDir.path() / "sprite.png";
        utils::writeTwoPixelPng(path, kLeft, kRight);
        loadedImage = std::make_shared<Image>(path);

        object = std::make_shared<SceneObject>("Stimulus");
        object->setTransform({kSpriteX, kSpriteY, kSpriteWidth, kSpriteHeight, 0.0});

        renderTarget = utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
        utils::clear(renderTarget.get(), kBackground);
    }

    const std::shared_ptr<Image>&        image() const { return loadedImage; }
    const std::shared_ptr<SceneObject>&  owner() const { return object; }
    const std::shared_ptr<SDL_Renderer>& target() const { return renderTarget; }

    utils::Rgba pixelAt(int column, int row) const {
        return utils::readPixel(renderTarget.get(), column, row);
    }

    // Sets one transform field to `value` and reports whether a sprite on the owner is rejected.
    bool rejectsTransformValue(double SceneObject::Transform::*field, double value) const {
        const double original    = object->transform.*field;
        object->transform.*field = value;
        bool rejected            = false;
        try {
            const SpriteRenderer sprite(object, loadedImage);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        object->transform.*field = original;
        return rejected;
    }

   private:
    utils::TempDir                tempDir;
    std::shared_ptr<Image>        loadedImage;
    std::shared_ptr<SceneObject>  object;
    std::shared_ptr<SDL_Renderer> renderTarget;
};
}  // namespace

TEST_F(SpriteRendererTest, StretchesImageOverTransform) {
    SpriteRenderer sprite(owner(), image());
    image()->upload(target());
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kLeft);
    EXPECT_EQ(pixelAt(kRightHalfX, kRowY), kRight);
    EXPECT_EQ(pixelAt(kColumnX, kTopY), kBackground);
    EXPECT_EQ(pixelAt(kOutside, kOutside), kBackground);
}

TEST_F(SpriteRendererTest, RotatesClockwiseAboutCentre) {
    owner()->transform.rotation = kQuarterTurn;

    SpriteRenderer sprite(owner(), image());
    image()->upload(target());
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kColumnX, kTopY), kLeft);
    EXPECT_EQ(pixelAt(kColumnX, kBottomY), kRight);
    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kBackground);
}

TEST_F(SpriteRendererTest, DrawsNothingBeforeImageIsUploaded) {
    SpriteRenderer sprite(owner(), image());
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kBackground);
}

TEST_F(SpriteRendererTest, WithoutImageDrawsNothingAndNeedsNoTransform) {
    owner()->transform.width = 0.0;

    SpriteRenderer sprite(owner(), nullptr);
    sprite.render(target().get());

    EXPECT_EQ(pixelAt(kLeftHalfX, kRowY), kBackground);
}

TEST_F(SpriteRendererTest, ThrowsWhenDrawingFails) {
    SpriteRenderer sprite(owner(), image());
    image()->upload(target());

    // SDL refuses to copy a texture onto a renderer it was not created on.
    const auto otherTarget = utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
    EXPECT_THROW(sprite.render(otherTarget.get()), std::runtime_error);
}

TEST_F(SpriteRendererTest, ThrowsWhenOwnerIsNull) {
    EXPECT_THROW(SpriteRenderer(nullptr, image()), std::invalid_argument);
}

TEST_F(SpriteRendererTest, ThrowsWhenOwnerHasNoArea) {
    owner()->transform.width = 0.0;

    EXPECT_THROW(SpriteRenderer(owner(), image()), std::invalid_argument);
}

TEST_F(SpriteRendererTest, ThrowsWhenTransformIsNotFinite) {
    using Transform = SceneObject::Transform;
    for (const auto field : {&Transform::posX, &Transform::posY, &Transform::width,
                             &Transform::height, &Transform::rotation}) {
        EXPECT_TRUE(rejectsTransformValue(field, std::numeric_limits<double>::infinity()));
        EXPECT_TRUE(rejectsTransformValue(field, std::numeric_limits<double>::quiet_NaN()));
        EXPECT_TRUE(rejectsTransformValue(field, kBeyondFloat));
    }
}
