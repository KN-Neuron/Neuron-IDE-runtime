#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"

//  Pomocnicze funkcje do tworzenia tymczasowych plików .pb
namespace {

std::string writeTempProto(const NeuronIDE::Scene& scene, const std::string& filename) {
    const std::string path = (std::filesystem::temp_directory_path() / filename).string();
    std::ofstream     out(path, std::ios::binary | std::ios::trunc);
    EXPECT_TRUE(out.is_open()) << "Nie mozna otworzyc pliku tymczasowego: " << path;
    scene.SerializeToOstream(&out);
    return path;
}

NeuronIDE::Scene buildSimpleScene(const std::string& projectName = "TestProject",
                                  const std::string& objectName = "ObiektA", bool isVisible = true,
                                  double blinkFrequency = 1.5) {
                                  NeuronIDE::Scene scene;
                                  scene.set_project_name(projectName
                                );

    auto* obj = scene.add_scene_objects();
    obj->set_name(objectName);
    obj->set_is_visible(isVisible);

    auto* comp    = obj->add_components();
    auto* blinker = comp->mutable_blinker();
    blinker->set_blink_frequency_hz(blinkFrequency);

    return scene;
}

}  // namespace

//  Grupa: Parser -- otwieranie pliku
TEST(ParserFileTest, ThrowsWhenFileDoesNotExist) {
    Parser parser;
    EXPECT_THROW(parser.parse("/nonexistent/path/scene.pb"), std::runtime_error);
}

TEST(ParserFileTest, ReturnsNonNullSceneForValidFile) {
    auto  scene = buildSimpleScene();
    const std::string path  = writeTempProto(scene, "valid_scene.pb");

    Parser parser;
    auto   result = parser.parse(path);
    ASSERT_NE(result, nullptr);
}

//  Grupa: Parser -- nazwa projektu (Scene.project_name)
TEST(ParserSceneNameTest, SetsProjectName) {
    auto  scene = buildSimpleScene("MojProjekt");
    const std::string path  = writeTempProto(scene, "name_test.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_EQ(result->getExperimentName(), "MojProjekt");
}

TEST(ParserSceneNameTest, EmptyProjectNameIsPreserved) {
    auto  scene = buildSimpleScene("");
    const std::string path  = writeTempProto(scene, "empty_name.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_EQ(result->getExperimentName(), "");
}

//  Grupa: Parser -- liczba obiektow sceny
TEST(ParserObjectCountTest, EmptySceneHasNoObjects) {
    NeuronIDE::Scene  protoScene;
    const std::string path = writeTempProto(protoScene, "no_objects.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_TRUE(result->getObjects().empty());
}

TEST(ParserObjectCountTest, SingleObjectIsLoaded) {
    auto  scene = buildSimpleScene("P", "Obj1");
    const std::string path  = writeTempProto(scene, "one_object.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_EQ(result->getObjects().size(), 1u);
}

TEST(ParserObjectCountTest, MultipleObjectsAreAllLoaded) {
    NeuronIDE::Scene scene;
    scene.set_project_name("Multi");
    for (int i = 0; i < 5; ++i) {
        auto* obj = scene.add_scene_objects();
        obj->set_name("Obj" + std::to_string(i));
        obj->set_is_visible(true);
        auto* comp = obj->add_components();
        comp->mutable_blinker()->set_blink_frequency_hz(static_cast<double>(i));
    }
    const std::string path = writeTempProto(scene, "multi_objects.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_EQ(result->getObjects().size(), 5u);
}

//  Grupa: Parser -- atrybuty SceneObject
TEST(ParserSceneObjectTest, ObjectNameIsCorrect) {
    auto  scene = buildSimpleScene("P", "MojaRakieta");
    const std::string path  = writeTempProto(scene, "obj_name.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_EQ(result->getObjects()[0]->name, "MojaRakieta");
}

TEST(ParserSceneObjectTest, ObjectIsVisibleWhenTrue) {
    auto   scene = buildSimpleScene("P", "Obj", true);
    const std::string path  = writeTempProto(scene, "obj_visible_true.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_TRUE(result->getObjects()[0]->isVisible);
}

TEST(ParserSceneObjectTest, ObjectIsHiddenWhenFalse) {
    auto   scene = buildSimpleScene("P", "Obj", false);
    const std::string path  = writeTempProto(scene, "obj_visible_false.pb");

    Parser parser;
    auto   result = parser.parse(path);
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
    const std::string path = writeTempProto(scene, "order_test.pb");

    Parser parser;
    auto   result = parser.parse(path);
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

    const std::string path = writeTempProto(scene, "transform_test.pb");

    Parser parser;
    auto   result = parser.parse(path);
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

    const std::string path = writeTempProto(scene, "no_transform.pb");

    Parser parser;
    auto   result = parser.parse(path);
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

    const std::string path = writeTempProto(scene, "neg_transform.pb");

    Parser      parser;
    auto        result = parser.parse(path);
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

    const std::string path = writeTempProto(scene, "no_components.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_TRUE(result->getObjects()[0]->components.empty());
}

TEST(ParserComponentTest, BlinkComponentIsCreated) {
    auto  scene = buildSimpleScene("P", "Mrugacz", true, 2.0);
    const std::string path  = writeTempProto(scene, "blink_comp.pb");

    Parser parser;
    auto   result = parser.parse(path);
    ASSERT_EQ(result->getObjects().size(), 1u);
    EXPECT_EQ(result->getObjects()[0]->components.size(), 1u);
    EXPECT_NE(result->getObjects()[0]->components[0], nullptr);
}

TEST(ParserComponentTest, BlinkComponentIsCorrectType) {
    auto  scene = buildSimpleScene("P", "Blinker", true, 3.0);
    const std::string path  = writeTempProto(scene, "blink_type.pb");

    Parser parser;
    auto   result = parser.parse(path);
    auto*  raw    = result->getObjects()[0]->components[0].get();
    EXPECT_NE(dynamic_cast<BlinkComponent*>(raw), nullptr);
}

TEST(ParserComponentTest, UnknownComponentTypeIsIgnored) {
    NeuronIDE::Scene scene;
    scene.set_project_name("UnknownComp");
    auto* obj = scene.add_scene_objects();
    obj->set_name("Obiekt");
    obj->set_is_visible(true);
    obj->add_components();  // pusty Component, brak oneof

    const std::string path = writeTempProto(scene, "unknown_comp.pb");

    Parser parser;
    auto   result = parser.parse(path);
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

    const std::string path = writeTempProto(scene, "dup_comp.pb");

    Parser parser;
    EXPECT_THROW(parser.parse(path), std::runtime_error);
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
    const std::string path = writeTempProto(scene, "indep_comp.pb");

    Parser parser;
    auto   result = parser.parse(path);
    for (const auto& obj : result->getObjects()) {
        EXPECT_EQ(obj->components.size(), 1u);
    }
}

//  Grupa: Parser -- scenariusze brzegowe
TEST(ParserEdgeCaseTest, ObjectWithEmptyNameIsParsed) {
    NeuronIDE::Scene scene;
    scene.set_project_name("EmptyObjName");
    auto* obj = scene.add_scene_objects();
    obj->set_name("");
    obj->set_is_visible(true);

    const std::string path = writeTempProto(scene, "empty_obj_name.pb");

    Parser parser;
    auto   result = parser.parse(path);
    ASSERT_EQ(result->getObjects().size(), 1u);
    EXPECT_EQ(result->getObjects()[0]->name, "");
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
    const std::string path = writeTempProto(scene, "large_scene.pb");

    Parser parser;
    auto   result = parser.parse(path);
    EXPECT_EQ(static_cast<int>(result->getObjects().size()), N);
}

TEST(ParserEdgeCaseTest, BlinkFrequencyZeroIsValid) {
    auto  scene = buildSimpleScene("P", "ZeroHz", true, 0.0);
    const std::string path  = writeTempProto(scene, "zero_freq.pb");

    Parser parser;
    auto   result = parser.parse(path);
    ASSERT_EQ(result->getObjects().size(), 1u);
    EXPECT_EQ(result->getObjects()[0]->components.size(), 1u);
}

TEST(ParserEdgeCaseTest, ParseReturnsDifferentObjectEachCall) {
    auto  scene = buildSimpleScene();
    const std::string path  = writeTempProto(scene, "two_calls.pb");

    Parser parser;
    auto   r1 = parser.parse(path);
    auto   r2 = parser.parse(path);
    EXPECT_NE(r1.get(), r2.get());
}