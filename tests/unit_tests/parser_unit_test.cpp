#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"
#include "utils/ParserTestUtils.hpp"

namespace {
constexpr int kMultipleObjects  = 5;
constexpr int kLargeObjectCount = 500;

constexpr double kPosX        = 10.5;
constexpr double kPosY        = 20.25;
constexpr double kWidth       = 64.0;
constexpr double kHeight      = 128.0;
constexpr double kRotation    = 45.0;
constexpr double kNegPosX     = -50.0;
constexpr double kNegPosY     = -100.0;
constexpr double kNegRotation = -90.0;

constexpr double kBlinkFreq2Hz = 2.0;
constexpr double kBlinkFreq3Hz = 3.0;
}  // namespace

//  Grupa: Parser -- nazwa projektu (Scene.project_name)
TEST(ParserSceneNameTest, SetsProjectName) {
    auto scene  = utils::buildSimpleScene({.projectName = "MojProjekt"});
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getExperimentName(), "MojProjekt");
}

TEST(ParserSceneNameTest, EmptyProjectNameThrowsError) {
    auto scene = utils::buildSimpleScene({.projectName = ""});
    EXPECT_THROW(utils::parseProtoScene(scene), std::invalid_argument);
}

//  Grupa: Parser -- liczba obiektow sceny
TEST(ParserObjectCountTest, EmptySceneHasNoObjects) {
    NeuronIDE::Scene protoScene;
    protoScene.set_project_name("EmptyScene");
    auto result = utils::parseProtoScene(protoScene);
    EXPECT_TRUE(result->getObjects().empty());
}

TEST(ParserObjectCountTest, SingleObjectIsLoaded) {
    auto scene  = utils::buildSimpleScene({.projectName = "P", .objectName = "Obj1"});
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getObjects().size(), 1U);
}

TEST(ParserObjectCountTest, MultipleObjectsAreAllLoaded) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Multi");

    for (int i = 0; i < kMultipleObjects; ++i) {
        auto* obj = scene.add_scene_objects();
        obj->set_name("Obj" + std::to_string(i));
        obj->set_is_visible(true);
    }

    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(static_cast<int>(result->getObjects().size()), kMultipleObjects);
}

//  Grupa: Parser -- atrybuty SceneObject
TEST(ParserSceneObjectTest, ObjectNameIsCorrect) {
    auto scene  = utils::buildSimpleScene({.projectName = "P", .objectName = "MojaRakieta"});
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getObjects()[0]->name, "MojaRakieta");
}

TEST(ParserSceneObjectTest, ObjectIsVisibleWhenTrue) {
    auto scene =
        utils::buildSimpleScene({.projectName = "P", .objectName = "Obj", .isVisible = true});
    auto result = utils::parseProtoScene(scene);
    EXPECT_TRUE(result->getObjects()[0]->isVisible);
}

TEST(ParserSceneObjectTest, ObjectIsHiddenWhenFalse) {
    auto scene =
        utils::buildSimpleScene({.projectName = "P", .objectName = "Obj", .isVisible = false});
    auto result = utils::parseProtoScene(scene);
    EXPECT_FALSE(result->getObjects()[0]->isVisible);
}

TEST(ParserSceneObjectTest, ObjectsPreserveInsertionOrder) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Order");
    const std::vector<std::string> names = {"Alpha", "Beta", "Gamma"};
    for (const auto& name : names) {
        auto* obj = scene.add_scene_objects();
        obj->set_name(name);
    }
    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        EXPECT_EQ(result->getObjects()[i]->name, names[i]);
    }
}

//  Grupa: Parser -- Transform
TEST(ParserTransformTest, TransformFieldsAreParsedCorrectly) {
    NeuronIDE::Scene scene;
    scene.set_project_name("TransformTest");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Sprite");
    obj->set_is_visible(true);

    auto* tra = obj->mutable_transform();
    tra->set_x(kPosX);
    tra->set_y(kPosY);
    tra->set_width(kWidth);
    tra->set_height(kHeight);
    tra->set_rotation(kRotation);

    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1U);

    const auto& transform = result->getObjects()[0]->transform;
    EXPECT_DOUBLE_EQ(transform.posX, kPosX);
    EXPECT_DOUBLE_EQ(transform.posY, kPosY);
    EXPECT_DOUBLE_EQ(transform.width, kWidth);
    EXPECT_DOUBLE_EQ(transform.height, kHeight);
    EXPECT_DOUBLE_EQ(transform.rotation, kRotation);
}

TEST(ParserTransformTest, DefaultTransformIsZeroWhenNotProvided) {
    NeuronIDE::Scene scene;
    scene.set_project_name("NoTransform");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Bezpozycyjny");
    obj->set_is_visible(true);

    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1U);

    const auto& transform = result->getObjects()[0]->transform;
    EXPECT_DOUBLE_EQ(transform.posX, 0.0);
    EXPECT_DOUBLE_EQ(transform.posY, 0.0);
    EXPECT_DOUBLE_EQ(transform.width, 0.0);
    EXPECT_DOUBLE_EQ(transform.height, 0.0);
    EXPECT_DOUBLE_EQ(transform.rotation, 0.0);
}

TEST(ParserTransformTest, NegativeTransformValuesAreAccepted) {
    NeuronIDE::Scene scene;
    scene.set_project_name("NegTrans");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Ujemny");
    obj->set_is_visible(true);
    auto* tra = obj->mutable_transform();
    tra->set_x(kNegPosX);
    tra->set_y(kNegPosY);
    tra->set_rotation(kNegRotation);

    auto        result    = utils::parseProtoScene(scene);
    const auto& transform = result->getObjects()[0]->transform;
    EXPECT_DOUBLE_EQ(transform.posX, kNegPosX);
    EXPECT_DOUBLE_EQ(transform.posY, kNegPosY);
    EXPECT_DOUBLE_EQ(transform.rotation, kNegRotation);
}

//  Grupa: Parser -- komponenty (BlinkComponent)
TEST(ParserComponentTest, ObjectWithNoComponentsHasEmptyComponentList) {
    NeuronIDE::Scene scene;
    scene.set_project_name("NoComp");
    auto* obj = scene.add_scene_objects();
    obj->set_name("PustyObiekt");

    auto result = utils::parseProtoScene(scene);
    EXPECT_TRUE(result->getObjects()[0]->components.empty());
}

TEST(ParserComponentTest, BlinkComponentIsCreated) {
    auto scene  = utils::buildSimpleScene({.projectName    = "P",
                                           .objectName     = "Mrugacz",
                                           .isVisible      = true,
                                           .blinkFrequency = kBlinkFreq2Hz});
    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1U);
    EXPECT_EQ(result->getObjects()[0]->components.size(), 1U);
    EXPECT_NE(result->getObjects()[0]->components[0], nullptr);
}

TEST(ParserComponentTest, BlinkComponentIsCorrectType) {
    auto  scene  = utils::buildSimpleScene({.projectName    = "P",
                                            .objectName     = "Blinker",
                                            .isVisible      = true,
                                            .blinkFrequency = kBlinkFreq3Hz});
    auto  result = utils::parseProtoScene(scene);
    auto* raw    = result->getObjects()[0]->components[0].get();
    EXPECT_NE(dynamic_cast<BlinkComponent*>(raw), nullptr);
}

TEST(ParserComponentTest, UnknownComponentTypeIsIgnored) {
    NeuronIDE::Scene scene;
    scene.set_project_name("UnknownComp");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Obiekt");
    obj->set_is_visible(true);
    obj->add_components();  // pusty Component, brak oneof

    auto result = utils::parseProtoScene(scene);
    EXPECT_TRUE(result->getObjects()[0]->components.empty());
}

TEST(ParserComponentTest, DuplicateComponentTypeThrows) {
    NeuronIDE::Scene scene;
    scene.set_project_name("DupComp");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Zduplikowany");
    obj->set_is_visible(true);

    obj->add_components()->mutable_blinker()->set_blink_frequency_hz(1.0);
    obj->add_components()->mutable_blinker()->set_blink_frequency_hz(kBlinkFreq2Hz);

    std::stringstream stream;
    scene.SerializeToOstream(&stream);

    EXPECT_THROW(Parser::parseStream(stream), std::runtime_error);
}

TEST(ParserComponentTest, MultipleObjectsEachHaveTheirOwnComponents) {
    NeuronIDE::Scene scene;
    scene.set_project_name("IndepComp");
    for (int i = 0; i < 3; ++i) {
        auto* obj = scene.add_scene_objects();
        obj->set_name("Obj" + std::to_string(i));
        obj->set_is_visible(true);
        obj->add_components()->mutable_blinker()->set_blink_frequency_hz(
            static_cast<double>(i + 1));
    }
    auto result = utils::parseProtoScene(scene);
    for (const auto& obj : result->getObjects()) {
        EXPECT_EQ(obj->components.size(), 1U);
    }
}

//  Grupa: Parser -- scenariusze brzegowe
TEST(ParserEdgeCaseTest, ObjectWithEmptyNameThrowsError) {
    NeuronIDE::Scene scene;
    scene.set_project_name("EmptyObjName");
    auto* obj = scene.add_scene_objects();
    obj->set_name("");
    obj->set_is_visible(true);

    EXPECT_THROW(utils::parseProtoScene(scene), std::invalid_argument);
}

TEST(ParserEdgeCaseTest, LargeNumberOfObjectsIsHandled) {
    NeuronIDE::Scene scene;
    scene.set_project_name("LargeScene");
    for (int i = 0; i < kLargeObjectCount; ++i) {
        auto* obj = scene.add_scene_objects();
        obj->set_name("O" + std::to_string(i));
        obj->set_is_visible(i % 2 == 0);
        obj->add_components()->mutable_blinker()->set_blink_frequency_hz(static_cast<double>(i));
    }
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(static_cast<int>(result->getObjects().size()), kLargeObjectCount);
}

TEST(ParserEdgeCaseTest, BlinkFrequencyZeroIsValid) {
    auto scene = utils::buildSimpleScene(
        {.projectName = "P", .objectName = "ZeroHz", .isVisible = true, .blinkFrequency = 0.0});
    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1U);
    EXPECT_EQ(result->getObjects()[0]->components.size(), 1U);
}

TEST(ParserEdgeCaseTest, ParserReturnsDifferentObjectEachCall) {
    auto              scene = utils::buildSimpleScene();
    std::stringstream ss1;
    scene.SerializeToOstream(&ss1);
    std::stringstream ss2;
    scene.SerializeToOstream(&ss2);

    auto resultA = Parser::parseStream(ss1);
    auto resultB = Parser::parseStream(ss2);
    EXPECT_NE(resultA.get(), resultB.get());
}
