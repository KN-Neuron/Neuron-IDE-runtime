#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "scene/SceneObject.hpp"
#include "scene/components/TextRenderer.hpp"
#include "utils/SdlTestUtils.hpp"
#include "utils/TempDir.hpp"

namespace {
namespace fs = std::filesystem;

constexpr int           kCanvasWidth  = 240;
constexpr int           kCanvasHeight = 140;
constexpr int           kPixelCount   = kCanvasWidth * kCanvasHeight;
constexpr double        kBoxX         = 40.0;
constexpr double        kBoxY         = 20.0;
constexpr double        kBoxWidth     = 160.0;
constexpr double        kBoxHeight    = 100.0;
constexpr int           kBoxCentreX   = 120;
constexpr int           kBoxCentreY   = 70;
constexpr std::uint32_t kFontSize     = 20;
constexpr int           kTolerance    = 2;
constexpr int           kGlyphSlack   = 10;
constexpr double        kQuarterTurn  = 90.0;
constexpr double        kBeyondInt    = 1e30;
constexpr double        kNarrowBox    = 30.0;
constexpr double        kTinyBox      = 1.0;

constexpr utils::Rgba kBackground{200, 0, 0, 255};
constexpr utils::Rgba kWhite{255, 255, 255, 255};

// U+2588 FULL BLOCK: a solid rectangle, so what is drawn shows exactly where the glyph went.
constexpr std::string_view kBlock       = "\xE2\x96\x88";
constexpr std::string_view kThreeBlocks = "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88";
constexpr std::string_view kTwoLines    = "\xE2\x96\x88\n\xE2\x96\x88";
constexpr std::string_view kTwoWords    = "\xE2\x96\x88 \xE2\x96\x88";
// Three blocks over one: the line of one block has to be centred under the wider one.
constexpr std::string_view kWideOverNarrow = "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\n\xE2\x96\x88";
// U+E000, from the private use area, which the built-in font has no glyph for.
constexpr std::string_view kMissingGlyph = "\xEE\x80\x80";

using Canvas = std::vector<utils::Rgba>;

// The rectangle of pixels that differ from the background. `right` and `bottom` are exclusive.
struct Extent {
    int left   = kCanvasWidth;
    int top    = kCanvasHeight;
    int right  = 0;
    int bottom = 0;

    [[nodiscard]] bool empty() const { return right <= left || bottom <= top; }
    [[nodiscard]] int  width() const { return right - left; }
    [[nodiscard]] int  height() const { return bottom - top; }
    [[nodiscard]] int  centreX() const { return (left + right) / 2; }
    [[nodiscard]] int  centreY() const { return (top + bottom) / 2; }
};

const utils::Rgba& at(const Canvas& canvas, int column, int row) {
    const int index = (row * kCanvasWidth) + column;
    return canvas.at(static_cast<std::size_t>(index));
}

Extent extentOf(const Canvas& canvas) {
    Extent extent;
    for (int row = 0; row < kCanvasHeight; ++row) {
        for (int column = 0; column < kCanvasWidth; ++column) {
            if (at(canvas, column, row) != kBackground) {
                extent.left   = std::min(extent.left, column);
                extent.top    = std::min(extent.top, row);
                extent.right  = std::max(extent.right, column + 1);
                extent.bottom = std::max(extent.bottom, row + 1);
            }
        }
    }
    return extent;
}

class TextRendererTest : public ::testing::Test {
   protected:
    void SetUp() override {
        object = std::make_shared<SceneObject>("Label");
        object->setTransform({kBoxX, kBoxY, kBoxWidth, kBoxHeight, 0.0});
        renderTarget = utils::makeSoftwareTarget(kCanvasWidth, kCanvasHeight);
        utils::clear(renderTarget.get(), kBackground);
    }

    const std::shared_ptr<SceneObject>&  owner() const { return object; }
    const std::shared_ptr<SDL_Renderer>& target() const { return renderTarget; }
    const fs::path&                      dir() const { return tempDir.path(); }

    void clearCanvas() const { utils::clear(renderTarget.get(), kBackground); }

    Canvas canvas() const {
        const std::size_t  channels = utils::Rgba{}.size();
        std::vector<Uint8> bytes(static_cast<std::size_t>(kPixelCount) * channels);
        if (SDL_RenderReadPixels(renderTarget.get(), nullptr, SDL_PIXELFORMAT_RGBA32, bytes.data(),
                                 kCanvasWidth * static_cast<int>(channels)) != 0) {
            throw utils::sdlFailure("SDL_RenderReadPixels");
        }

        Canvas pixels;
        pixels.reserve(static_cast<std::size_t>(kPixelCount));
        for (std::size_t offset = 0; offset < bytes.size(); offset += channels) {
            pixels.push_back(
                {bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]});
        }
        return pixels;
    }

    void drawOnto(TextRenderer& text) const {
        text.onRenderTargetReady(renderTarget);
        text.render(renderTarget.get());
    }

    // What `content` looks like on a freshly cleared canvas.
    Canvas drawn(std::string_view content, const fs::path& font = {}) const {
        clearCanvas();
        TextRenderer text(object, content, font, kFontSize);
        drawOnto(text);
        return canvas();
    }

    Extent drawnExtent(std::string_view content) const { return extentOf(drawn(content)); }

    void expectRejected(double SceneObject::Transform::* field, double value) const {
        const double original    = object->transform.*field;
        object->transform.*field = value;
        EXPECT_THROW(TextRenderer(object, kBlock, "", kFontSize), std::invalid_argument);
        object->transform.*field = original;
    }

   private:
    utils::TempDir                tempDir;
    std::shared_ptr<SceneObject>  object;
    std::shared_ptr<SDL_Renderer> renderTarget;
};
}  // namespace

TEST_F(TextRendererTest, CentresTheTextInTheTransform) {
    const Canvas pixels = drawn(kBlock);
    const Extent extent = extentOf(pixels);

    ASSERT_FALSE(extent.empty());
    EXPECT_NEAR(extent.centreX(), kBoxCentreX, kTolerance);
    EXPECT_NEAR(extent.centreY(), kBoxCentreY, kTolerance);
    EXPECT_EQ(at(pixels, extent.centreX(), extent.centreY()), kWhite);
}

TEST_F(TextRendererTest, FontSizeSetsTheSizeOnScreenWhateverTheBox) {
    const Extent inLargeBox = drawnExtent(kBlock);

    owner()->transform.posX += kBoxWidth / 4;
    owner()->transform.width /= 2;
    const Extent inSmallBox = drawnExtent(kBlock);

    EXPECT_EQ(inLargeBox.width(), inSmallBox.width());
    EXPECT_EQ(inLargeBox.height(), inSmallBox.height());
    EXPECT_NEAR(inLargeBox.height(), kFontSize, kGlyphSlack);
}

TEST_F(TextRendererTest, RotatesClockwiseAboutTheCentreOfTheBox) {
    const Extent flat = drawnExtent(kThreeBlocks);
    ASSERT_GT(flat.width(), flat.height());

    owner()->transform.rotation = kQuarterTurn;
    const Extent turned         = drawnExtent(kThreeBlocks);

    EXPECT_NEAR(turned.height(), flat.width(), kTolerance);
    EXPECT_NEAR(turned.width(), flat.height(), kTolerance);
    EXPECT_NEAR(turned.centreX(), kBoxCentreX, kTolerance);
    EXPECT_NEAR(turned.centreY(), kBoxCentreY, kTolerance);
}

TEST_F(TextRendererTest, LeavesTheBackgroundVisibleBetweenGlyphStrokes) {
    const Canvas pixels = drawn("O");
    const Extent extent = extentOf(pixels);

    ASSERT_FALSE(extent.empty());
    EXPECT_EQ(at(pixels, extent.centreX(), extent.centreY()), kBackground);
}

TEST_F(TextRendererTest, BreaksLinesAtNewlines) {
    const Extent oneLine  = drawnExtent(kBlock);
    const Extent twoLines = drawnExtent(kTwoLines);

    EXPECT_GT(twoLines.height(), oneLine.height() + (oneLine.height() / 2));
    EXPECT_EQ(twoLines.width(), oneLine.width());
}

TEST_F(TextRendererTest, WrapsAtTheWidthOfTheBox) {
    const Extent wide = drawnExtent(kTwoWords);

    owner()->transform.width = kNarrowBox;
    const Extent narrow      = drawnExtent(kTwoWords);

    EXPECT_GT(wide.width(), narrow.width());
    EXPECT_GT(narrow.height(), wide.height() + (wide.height() / 2));
}

TEST_F(TextRendererTest, CentresLinesAgainstEachOther) {
    const Canvas pixels = drawn(kWideOverNarrow);
    const Extent extent = extentOf(pixels);
    ASSERT_FALSE(extent.empty());

    int left  = kCanvasWidth;
    int right = 0;
    for (int column = 0; column < kCanvasWidth; ++column) {
        if (at(pixels, column, extent.bottom - 1) != kBackground) {
            left  = std::min(left, column);
            right = std::max(right, column + 1);
        }
    }

    const int lastLineCentre = (left + right) / 2;
    EXPECT_LT(right - left, extent.width());
    EXPECT_NEAR(lastLineCentre, kBoxCentreX, kTolerance);
}

TEST_F(TextRendererTest, TrailingWhitespaceDrawsNothingExtra) {
    const Canvas plain = drawn("A");

    EXPECT_EQ(drawn("A\n"), plain);
    EXPECT_EQ(drawn("A\r\n"), plain);
    EXPECT_EQ(drawn("A  "), plain);
}

TEST_F(TextRendererTest, CarriageReturnsAndTabsDrawNoMissingGlyphBoxes) {
    EXPECT_EQ(drawn("A\r\nB"), drawn("A\nB"));
    EXPECT_EQ(drawn("A\tB"), drawn("A B"));
}

TEST_F(TextRendererTest, TextWithNothingToDrawLeavesTheCanvasAlone) {
    for (const std::string_view blank : {"", "   ", "\n\n", "\t \r\n", "\xE2\x80\x8B"}) {
        EXPECT_TRUE(drawnExtent(blank).empty()) << "'" << blank << "'";
    }
}

TEST_F(TextRendererTest, BuiltInFontHasEveryPolishLetter) {
    const Canvas missing = drawn(kMissingGlyph);

    for (const std::string_view letter : {"ą", "ć", "ę", "ł", "ń", "ó", "ś", "ź", "ż", "Ą", "Ć",
                                          "Ę", "Ł", "Ń", "Ó", "Ś", "Ź", "Ż"}) {
        EXPECT_NE(drawn(letter), missing) << letter;
    }
}

TEST_F(TextRendererTest, LoadsAFontFromANonAsciiPath) {
    const fs::path font = dir() / fs::path(u8"czcionka_ż.ttf");
    utils::writeFontFile(font);

    EXPECT_EQ(drawn("A", font), drawn("A"));
}

TEST_F(TextRendererTest, DrawsNothingBeforeTheRenderTargetIsReady) {
    TextRenderer text(owner(), kBlock, "", kFontSize);

    text.render(target().get());

    EXPECT_TRUE(extentOf(canvas()).empty());
}

TEST_F(TextRendererTest, ReportsMissingTextureOnlyOnceAndKeepsRunning) {
    TextRenderer text(owner(), kBlock, "", kFontSize);

    EXPECT_NO_THROW(text.render(target().get()));
    EXPECT_NO_THROW(text.render(target().get()));
}

TEST_F(TextRendererTest, NullRendererDrawsNothing) {
    TextRenderer text(owner(), kBlock, "", kFontSize);
    text.onRenderTargetReady(target());

    EXPECT_NO_THROW(text.render(nullptr));
}

// A device reset calls the hook a second time, with the old textures gone.
TEST_F(TextRendererTest, UploadsAgainWhenTheRenderTargetIsReadyAgain) {
    TextRenderer text(owner(), kBlock, "", kFontSize);
    text.onRenderTargetReady(target());
    EXPECT_NO_THROW(text.onRenderTargetReady(target()));

    text.render(target().get());

    EXPECT_FALSE(extentOf(canvas()).empty());
}

TEST_F(TextRendererTest, SetTextReplacesWhatIsDrawn) {
    TextRenderer text(owner(), kBlock, "", kFontSize);
    drawOnto(text);
    const Extent before = extentOf(canvas());

    text.setText(kThreeBlocks);
    clearCanvas();
    text.render(target().get());

    EXPECT_GT(extentOf(canvas()).width(), before.width());
}

TEST_F(TextRendererTest, SetTextBeforeTheRenderTargetIsDrawnOnceReady) {
    TextRenderer text(owner(), "", "", kFontSize);
    text.setText(kBlock);

    drawOnto(text);

    EXPECT_FALSE(extentOf(canvas()).empty());
}

TEST_F(TextRendererTest, SetTextToNothingStopsDrawing) {
    TextRenderer text(owner(), kBlock, "", kFontSize);
    text.onRenderTargetReady(target());

    text.setText("");
    text.render(target().get());

    EXPECT_TRUE(extentOf(canvas()).empty());
}

TEST_F(TextRendererTest, TextThatDoesNotFitItsBoxIsStillDrawn) {
    owner()->transform.width  = kTinyBox;
    owner()->transform.height = kTinyBox;

    TextRenderer text(owner(), kBlock, "", kFontSize);
    drawOnto(text);

    EXPECT_FALSE(extentOf(canvas()).empty());
}

TEST_F(TextRendererTest, ThrowsWhenDrawingFails) {
    TextRenderer text(owner(), kBlock, "", kFontSize);
    text.onRenderTargetReady(target());

    // SDL refuses to copy a texture onto a renderer it was not created on.
    const auto otherTarget = utils::makeSoftwareTarget(kCanvasWidth, kCanvasHeight);
    EXPECT_THROW(text.render(otherTarget.get()), std::runtime_error);
}

TEST_F(TextRendererTest, ThrowsWhenOwnerIsNull) {
    EXPECT_THROW(TextRenderer(nullptr, kBlock, "", kFontSize), std::invalid_argument);
}

TEST_F(TextRendererTest, ThrowsWhenOwnerHasNoArea) {
    expectRejected(&SceneObject::Transform::width, 0.0);
    expectRejected(&SceneObject::Transform::height, 0.0);
}

TEST_F(TextRendererTest, ThrowsWhenTransformIsNotDrawable) {
    using Transform = SceneObject::Transform;
    for (const auto field : {&Transform::posX, &Transform::posY, &Transform::width,
                             &Transform::height, &Transform::rotation}) {
        expectRejected(field, std::numeric_limits<double>::infinity());
        expectRejected(field, std::numeric_limits<double>::quiet_NaN());
    }
    for (const auto field :
         {&Transform::posX, &Transform::posY, &Transform::width, &Transform::height}) {
        expectRejected(field, kBeyondInt);
    }
}

TEST_F(TextRendererTest, ThrowsWhenFontSizeIsOutOfRange) {
    EXPECT_THROW(TextRenderer(owner(), kBlock, "", 0U), std::invalid_argument);
    EXPECT_THROW(TextRenderer(owner(), kBlock, "", TextRenderer::kMaxFontSize + 1),
                 std::invalid_argument);
    EXPECT_THROW(TextRenderer(owner(), kBlock, "", std::numeric_limits<std::uint32_t>::max()),
                 std::invalid_argument);
    EXPECT_NO_THROW(TextRenderer(owner(), "", "", TextRenderer::kMaxFontSize));
}

TEST_F(TextRendererTest, ThrowsWhenFontFileIsMissing) {
    EXPECT_THROW(TextRenderer(owner(), kBlock, dir() / "missing.ttf", kFontSize),
                 std::runtime_error);
}

TEST_F(TextRendererTest, ThrowsWhenFontFileIsNotAFont) {
    const fs::path font = dir() / "broken.ttf";
    std::ofstream(font) << "not a font";

    EXPECT_THROW(TextRenderer(owner(), kBlock, font, kFontSize), std::runtime_error);
}

// The font is what a later setText needs, so a bad one fails now rather than during the run.
TEST_F(TextRendererTest, ThrowsForAMissingFontEvenWithoutText) {
    EXPECT_THROW(TextRenderer(owner(), "", dir() / "missing.ttf", kFontSize), std::runtime_error);
}
