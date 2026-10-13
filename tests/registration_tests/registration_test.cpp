#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "utils/SdlTestUtils.hpp"
#include "utils/TempDir.hpp"

// Do not include or name any component class in this file. A component found here was linked in
// only through its REGISTER_COMPONENT static initializer, exactly as in the NeuronIDE executable.

namespace {
constexpr double      kBlinkFrequencyHz = 10.0;
constexpr utils::Rgba kWhite{255, 255, 255, 255};

NeuronIDE::Component* addObjectWithComponent(NeuronIDE::Scene& scene, const std::string& name) {
    auto* object = scene.add_scene_objects();
    object->set_name(name);
    object->set_is_visible(true);
    object->mutable_transform()->set_width(1.0);
    object->mutable_transform()->set_height(1.0);
    return object->add_components();
}
}  // namespace

// One object per implemented component type. Add yours here when you register a new one.
TEST(ComponentRegistrationTest, EveryImplementedComponentIsLinkedIn) {
    const utils::TempDir assets;
    utils::writeTwoPixelPng(assets.path() / "sprite.png", kWhite, kWhite);

    NeuronIDE::Scene proto;
    proto.set_project_name("Registration");
    addObjectWithComponent(proto, "Blinker")
        ->mutable_blinker()
        ->set_blink_frequency_hz(kBlinkFrequencyHz);
    addObjectWithComponent(proto, "MarkerEmitter")->mutable_marker_emitter();
    addObjectWithComponent(proto, "Sprite")->mutable_renderer()->set_img_path("sprite.png");

    std::stringstream stream;
    ASSERT_TRUE(proto.SerializeToOstream(&stream));
    const auto scene =
        Parser::parseStream(stream, ComponentBuildContext{.assetRoot = assets.path()});

    for (const auto& object : scene->getObjects()) {
        EXPECT_EQ(object->components.size(), 1U)
            << object->name << ": its component type is not registered in this binary";
    }
}
