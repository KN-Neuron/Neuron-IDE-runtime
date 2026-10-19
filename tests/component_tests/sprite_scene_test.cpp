#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/SpriteRenderer.hpp"
#include "utils/SdlTestUtils.hpp"
#include "utils/TempDir.hpp"

// Sprites in a .neuroz parsed from disk, the way Runtime loads an experiment.

namespace {
namespace fs = std::filesystem;

constexpr double      kSpriteSize = 10.0;
constexpr utils::Rgba kLeft{255, 0, 0, 255};
constexpr utils::Rgba kRight{0, 0, 255, 255};

void addSprite(NeuronIDE::Scene& scene, const std::string& imagePath, bool withTransform = true) {
    auto* object = scene.add_scene_objects();
    object->set_name("Stimulus" + std::to_string(scene.scene_objects_size()));
    object->set_is_visible(true);
    if (withTransform) {
        object->mutable_transform()->set_width(kSpriteSize);
        object->mutable_transform()->set_height(kSpriteSize);
    }
    object->add_components()->mutable_renderer()->set_img_path(imagePath);
}

NeuronIDE::Scene sceneWithSprite(const std::string& imagePath, bool withTransform = true) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Sprites");
    addSprite(scene, imagePath, withTransform);
    return scene;
}

class SpriteSceneTest : public ::testing::Test {
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

bool hasSprite(const Scene& scene) {
    const auto& components = scene.getObjects().at(0)->components;
    return components.size() == 1 &&
           dynamic_cast<SpriteRenderer*>(components.front().get()) != nullptr;
}
}  // namespace

TEST_F(SpriteSceneTest, ResolvesRelativeImagePathAgainstTheExperimentFolder) {
    // Otherwise the path would resolve even if it were taken relative to the working directory.
    ASSERT_NE(fs::current_path(), dir());

    fs::create_directories(dir() / "stimuli");
    utils::writeTwoPixelPng(dir() / "stimuli" / "face.png", kLeft, kRight);
    writeExperiment(sceneWithSprite("stimuli/face.png"));

    EXPECT_TRUE(hasSprite(*parseExperiment()));
}

TEST_F(SpriteSceneTest, UsesAbsoluteImagePathAsIs) {
    const utils::TempDir elsewhere;
    const fs::path       image = elsewhere.path() / "face.png";
    utils::writeTwoPixelPng(image, kLeft, kRight);
    writeExperiment(sceneWithSprite(image.string()));

    EXPECT_TRUE(hasSprite(*parseExperiment()));
}

TEST_F(SpriteSceneTest, MissingImageFailsTheParseAndNamesTheFile) {
    writeExperiment(sceneWithSprite("stimuli/missing.png"));
    const std::string expectedPath = (dir() / "stimuli" / "missing.png").string();

    try {
        parseExperiment();
        FAIL() << "expected the parse to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(expectedPath), std::string::npos) << error.what();
    }
}

// Only an image that is named but cannot be loaded is an error.
TEST_F(SpriteSceneTest, EmptyImagePathGivesASpriteWithoutImage) {
    writeExperiment(sceneWithSprite("", /*withTransform=*/false));

    const auto scene = parseExperiment();
    EXPECT_TRUE(hasSprite(*scene));
    EXPECT_EQ(scene->images().size(), 0U);
}

TEST_F(SpriteSceneTest, ObjectsShowingTheSameFileShareOneImage) {
    utils::writeTwoPixelPng(dir() / "face.png", kLeft, kRight);
    NeuronIDE::Scene proto;
    proto.set_project_name("Sprites");
    addSprite(proto, "face.png");
    addSprite(proto, "./face.png");
    writeExperiment(proto);

    EXPECT_EQ(parseExperiment()->images().size(), 1U);
}

// Proto strings are UTF-8, and stimulus names need not be ASCII.
TEST_F(SpriteSceneTest, LoadsImageWithNonAsciiPath) {
    const std::u8string imagePath = u8"bodźce/twarz_ż.png";
    fs::create_directories(dir() / fs::path(u8"bodźce"));
    utils::writeTwoPixelPng(dir() / fs::path(imagePath), kLeft, kRight);
    writeExperiment(sceneWithSprite(std::string(imagePath.begin(), imagePath.end())));

    const auto scene = parseExperiment();
    EXPECT_TRUE(hasSprite(*scene));
    EXPECT_EQ(scene->images().size(), 1U);
}

TEST_F(SpriteSceneTest, SpriteOnObjectWithoutTransformFailsTheParse) {
    utils::writeTwoPixelPng(dir() / "face.png", kLeft, kRight);
    writeExperiment(sceneWithSprite("face.png", /*withTransform=*/false));

    EXPECT_THROW(parseExperiment(), std::invalid_argument);
}
