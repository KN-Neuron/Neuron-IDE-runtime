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

//  Grupa: Parser -- nazwa projektu (Scene.project_name)
TEST(ParserSceneNameTest, SetsProjectName) {
    auto scene  = utils::buildSimpleScene("MojProjekt");
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getExperimentName(), "MojProjekt");
}

TEST(ParserSceneNameTest, EmptyProjectNameThrowsError) {
    auto scene = utils::buildSimpleScene("");
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
    auto scene  = utils::buildSimpleScene("P", "Obj1");
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getObjects().size(), 1u);
}

TEST(ParserObjectCountTest, MultipleObjectsAreAllLoaded) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Multi");

    for (int i = 0; i < 5; ++i) {
        auto* obj = scene.add_scene_objects();
        obj->set_name("Obj" + std::to_string(i));
        obj->set_is_visible(true);
    }

    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getObjects().size(), 5u);
}

//  Grupa: Parser -- atrybuty SceneObject
TEST(ParserSceneObjectTest, ObjectNameIsCorrect) {
    auto scene  = utils::buildSimpleScene("P", "MojaRakieta");
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(result->getObjects()[0]->name, "MojaRakieta");
}

TEST(ParserSceneObjectTest, ObjectIsVisibleWhenTrue) {
    auto scene  = utils::buildSimpleScene("P", "Obj", true);
    auto result = utils::parseProtoScene(scene);
    EXPECT_TRUE(result->getObjects()[0]->isVisible);
}

TEST(ParserSceneObjectTest, ObjectIsHiddenWhenFalse) {
    auto scene  = utils::buildSimpleScene("P", "Obj", false);
    auto result = utils::parseProtoScene(scene);
    EXPECT_FALSE(result->getObjects()[0]->isVisible);
}

TEST(ParserSceneObjectTest, ObjectsPreserveInsertionOrder) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Order");
    const std::vector<std::string> names = {"Alpha", "Beta", "Gamma"};
    for (const auto& n : names) {
        auto* obj = scene.add_scene_objects();
        obj->set_name(n);
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
    tra->set_x(10.5);
    tra->set_y(20.25);
    tra->set_width(64.0);
    tra->set_height(128.0);
    tra->set_rotation(45.0);

    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1u);

    const auto& t = result->getObjects()[0]->transform;
    EXPECT_DOUBLE_EQ(t.posX, 10.5);
    EXPECT_DOUBLE_EQ(t.posY, 20.25);
    EXPECT_DOUBLE_EQ(t.width, 64.0);
    EXPECT_DOUBLE_EQ(t.height, 128.0);
    EXPECT_DOUBLE_EQ(t.rotation, 45.0);
}

TEST(ParserTransformTest, DefaultTransformIsZeroWhenNotProvided) {
    NeuronIDE::Scene scene;
    scene.set_project_name("NoTransform");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Bezpozycyjny");
    obj->set_is_visible(true);

    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1u);

    const auto& t = result->getObjects()[0]->transform;
    EXPECT_DOUBLE_EQ(t.posX, 0.0);
    EXPECT_DOUBLE_EQ(t.posY, 0.0);
    EXPECT_DOUBLE_EQ(t.width, 0.0);
    EXPECT_DOUBLE_EQ(t.height, 0.0);
    EXPECT_DOUBLE_EQ(t.rotation, 0.0);
}

TEST(ParserTransformTest, NegativeTransformValuesAreAccepted) {
    NeuronIDE::Scene scene;
    scene.set_project_name("NegTrans");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Ujemny");
    obj->set_is_visible(true);
    auto* tra = obj->mutable_transform();
    tra->set_x(-50.0);
    tra->set_y(-100.0);
    tra->set_rotation(-90.0);

    auto        result = utils::parseProtoScene(scene);
    const auto& t      = result->getObjects()[0]->transform;
    EXPECT_DOUBLE_EQ(t.posX, -50.0);
    EXPECT_DOUBLE_EQ(t.posY, -100.0);
    EXPECT_DOUBLE_EQ(t.rotation, -90.0);
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
    auto scene  = utils::buildSimpleScene("P", "Mrugacz", true, 2.0);
    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1u);
    EXPECT_EQ(result->getObjects()[0]->components.size(), 1u);
    EXPECT_NE(result->getObjects()[0]->components[0], nullptr);
}

TEST(ParserComponentTest, BlinkComponentIsCorrectType) {
    auto  scene  = utils::buildSimpleScene("P", "Blinker", true, 3.0);
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
    obj->add_components()->mutable_blinker()->set_blink_frequency_hz(2.0);

    std::stringstream ss;
    scene.SerializeToOstream(&ss);

    Parser parser;
    EXPECT_THROW(parser.parseStream(ss), std::runtime_error);
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
        EXPECT_EQ(obj->components.size(), 1u);
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
    const int N = 500;
    for (int i = 0; i < N; ++i) {
        auto* obj = scene.add_scene_objects();
        obj->set_name("O" + std::to_string(i));
        obj->set_is_visible(i % 2 == 0);
        obj->add_components()->mutable_blinker()->set_blink_frequency_hz(static_cast<double>(i));
    }
    auto result = utils::parseProtoScene(scene);
    EXPECT_EQ(static_cast<int>(result->getObjects().size()), N);
}

TEST(ParserEdgeCaseTest, BlinkFrequencyZeroIsValid) {
    auto scene  = utils::buildSimpleScene("P", "ZeroHz", true, 0.0);
    auto result = utils::parseProtoScene(scene);
    ASSERT_EQ(result->getObjects().size(), 1u);
    EXPECT_EQ(result->getObjects()[0]->components.size(), 1u);
}

TEST(ParserEdgeCaseTest, ParserReturnsDifferentObjectEachCall) {
    auto              scene = utils::buildSimpleScene();
    std::stringstream ss1;
    scene.SerializeToOstream(&ss1);
    std::stringstream ss2;
    scene.SerializeToOstream(&ss2);

    Parser parser;
    auto   r1 = parser.parseStream(ss1);
    auto   r2 = parser.parseStream(ss2);
    EXPECT_NE(r1.get(), r2.get());
}