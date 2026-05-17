#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <memory>
#include <string>

#include "neuronide.pb.h"
#include "scene/SceneAll.hpp"

class Parser {
   public:
    Parser() = default;

    std::shared_ptr<Scene> parse(const std::string& filePath);

   private:
    static std::shared_ptr<SceneObject> buildSceneObject(const NeuronIDE::SceneObject& protoObj);
    static std::unique_ptr<Component>   buildComponent(const NeuronIDE::Component& protoComp);
};

#endif  // PARSER_HPP