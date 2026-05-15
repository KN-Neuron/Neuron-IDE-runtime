#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <string>
#include <memory>

#include "scene/SceneAll.hpp"
#include "neuronide.pb.h"

class Parser {
public:
    Parser() = default;

    // Parsuje plik protobuff .proto -> natywna Scene 
    std::shared_ptr<::Scene> parse(const std::string& filePath);

private:
    // Konwertery proto -> typy sceny
    static std::shared_ptr<SceneObject> buildSceneObject(const NeuronIDE::SceneObject& protoObj);
    static std::unique_ptr<Component> buildComponent(const NeuronIDE::Component& protoComp);
};

#endif  // PARSER_HPP