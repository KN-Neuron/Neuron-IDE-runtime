#ifndef PARSER_TEST_UTILS_HPP
#define PARSER_TEST_UTILS_HPP

#include <memory>
#include <sstream>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"

namespace utils {

inline constexpr double kDefaultBlinkFrequencyHz = 1.5;

struct SimpleSceneParams {
    std::string projectName    = "TestProject";
    std::string objectName     = "ObiektA";
    bool        isVisible      = true;
    double      blinkFrequency = kDefaultBlinkFrequencyHz;
};

inline NeuronIDE::Scene buildSimpleScene(const SimpleSceneParams& params = {}) {
    NeuronIDE::Scene scene;
    scene.set_project_name(params.projectName);

    auto* obj = scene.add_scene_objects();
    obj->set_name(params.objectName);
    obj->set_is_visible(params.isVisible);

    auto* comp    = obj->add_components();
    auto* blinker = comp->mutable_blinker();
    blinker->set_blink_frequency_hz(params.blinkFrequency);

    return scene;
}

inline std::shared_ptr<Scene> parseProtoScene(const NeuronIDE::Scene& scene) {
    std::stringstream stream;
    scene.SerializeToOstream(&stream);
    return Parser::parseStream(stream);
}

}  // namespace utils

#endif  // PARSER_TEST_UTILS_HPP
