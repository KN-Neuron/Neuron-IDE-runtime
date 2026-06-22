#ifndef PARSER_TEST_UTILS_HPP
#define PARSER_TEST_UTILS_HPP

#include <memory>
#include <sstream>
#include <string>

#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"

namespace utils {

inline NeuronIDE::Scene buildSimpleScene(const std::string& projectName = "TestProject",
                                         const std::string& objectName  = "ObiektA",
                                         bool isVisible = true, double blinkFrequency = 1.5) {
    NeuronIDE::Scene scene;
    scene.set_project_name(projectName);

    auto* obj = scene.add_scene_objects();
    obj->set_name(objectName);
    obj->set_is_visible(isVisible);

    auto* comp    = obj->add_components();
    auto* blinker = comp->mutable_blinker();
    blinker->set_blink_frequency_hz(blinkFrequency);

    return scene;
}

inline std::shared_ptr<Scene> parseProtoScene(const NeuronIDE::Scene& scene) {
    std::stringstream ss;
    scene.SerializeToOstream(&ss);
    Parser parser;
    return parser.parseStream(ss);
}

}  // namespace utils

#endif  // PARSER_TEST_UTILS_HPP
