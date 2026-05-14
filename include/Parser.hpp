#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <string>
#include <memory>

#include "Scene.hpp"
#include "SceneObject.hpp"
#include "components/Component.hpp"
#include "neuronide.pb.h"

class Parser {
   public:
    Parser() = default;

    // Parsuje plik protobuff .proto -> natywna Scene 
    auto parse(const std::string& filePath) -> std::shared_ptr<::Scene>;
   private:
    // Konwertery proto -> typy sceny
    static auto buildSceneObject(const NeuronIDE::SceneObject& protoObj) -> std::shared_ptr<SceneObject>;
    static auto buildComponent(const NeuronIDE::Component& protoComp) -> std::unique_ptr<Component>;
};

#endif  // PARSER_HPP