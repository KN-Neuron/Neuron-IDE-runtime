#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/TextRenderer.hpp"
#include "utils/SdlTestUtils.hpp"
#include "utils/TempDir.hpp"

// Text in a .neuroz parsed from disk, the way Runtime loads an experiment.

namespace {
namespace fs = std::filesystem;

constexpr int         kCanvasSize = 60;
constexpr double      kBoxSize    = 40.0;
constexpr double      kBoxOffset  = 10.0;
constexpr int         kBoxCentre  = 30;
constexpr unsigned    kFontSize   = 20;
constexpr utils::Rgba kBackground{0, 0, 0, 255};
constexpr utils::Rgba kWhite{255, 255, 255, 255};
constexpr const char* kFullBlock = "\xE2\x96\x88";

NeuronIDE::Scene sceneWithText(const std::string& fontPath, unsigned fontSize = kFontSize) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Text");
    auto* object = scene.add_scene_objects();
    object->set_name("Label");
    object->set_is_visible(true);
    object->mutable_transform()->set_x(kBoxOffset);
    object->mutable_transform()->set_y(kBoxOffset);
    object->mutable_transform()->set_width(kBoxSize);
    object->mutable_transform()->set_height(kBoxSize);

    auto* text = object->add_components()->mutable_text();
    text->set_text(kFullBlock);
    text->set_font_path(fontPath);
    text->set_font_size(fontSize);
    return scene;
}

class TextSceneTest : public ::testing::Test {
   protected:
    const fs::path& dir() const { return tempDir.path(); }
    fs::path        experimentPath() const { return dir() / "experiment.neuroz"; }

    void writeExperiment(const NeuronIDE::Scene& scene) const {
        std::ofstream out(experimentPath(), std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        ASSERT_TRUE(scene.SerializeToOstream(&out));
    }

    std::shared_ptr<Scene> parseExperiment() const {
        return Parser::parse(experimentPath().string());
    }

   private:
    utils::TempDir tempDir;
};

bool hasText(const Scene& scene) {
    const auto& components = scene.getObjects().at(0)->components;
    return components.size() == 1 &&
           dynamic_cast<TextRenderer*>(components.front().get()) != nullptr;
}
}  // namespace

// What the editor exports today: it has no way to choose a font.
TEST_F(TextSceneTest, EmptyFontPathDrawsInTheBuiltInFont) {
    writeExperiment(sceneWithText(""));
    const auto scene = parseExperiment();
    ASSERT_TRUE(hasText(*scene));

    const auto target = utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
    utils::clear(target.get(), kBackground);
    scene->onRenderTargetReady(target);
    scene->render(target.get());

    EXPECT_EQ(utils::readPixel(target.get(), kBoxCentre, kBoxCentre), kWhite);
}

TEST_F(TextSceneTest, ResolvesRelativeFontPathAgainstTheExperimentFolder) {
    // Otherwise the path would resolve even if it were taken relative to the working directory.
    ASSERT_NE(fs::current_path(), dir());

    fs::create_directories(dir() / "fonts");
    utils::writeFontFile(dir() / "fonts" / "lab.ttf");
    writeExperiment(sceneWithText("fonts/lab.ttf"));

    EXPECT_TRUE(hasText(*parseExperiment()));
}

TEST_F(TextSceneTest, UsesAbsoluteFontPathAsIs) {
    const utils::TempDir elsewhere;
    const fs::path       font = elsewhere.path() / "lab.ttf";
    utils::writeFontFile(font);
    writeExperiment(sceneWithText(font.string()));

    EXPECT_TRUE(hasText(*parseExperiment()));
}

TEST_F(TextSceneTest, MissingFontFailsTheParseAndNamesTheFile) {
    writeExperiment(sceneWithText("fonts/missing.ttf"));
    const std::string expectedPath = (dir() / "fonts" / "missing.ttf").string();

    try {
        parseExperiment();
        FAIL() << "expected the parse to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(expectedPath), std::string::npos) << error.what();
    }
}

TEST_F(TextSceneTest, UnsetFontSizeFailsTheParseAndNamesTheObject) {
    writeExperiment(sceneWithText("", /*fontSize=*/0));

    try {
        parseExperiment();
        FAIL() << "expected the parse to fail";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string(error.what()).find("Label"), std::string::npos) << error.what();
    }
}
