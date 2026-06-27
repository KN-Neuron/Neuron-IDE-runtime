#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <memory>
#include <string>

class Scene;
class SceneObject;
class Component;

namespace NeuronIDE {
class SceneObject;
class Component;
}  // namespace NeuronIDE

class Parser {
   public:
    Parser() = default;

    std::shared_ptr<Scene> parse(const std::string& filePath);
    std::shared_ptr<Scene> parseStream(std::istream& stream);

   private:
    static std::shared_ptr<SceneObject> buildSceneObject(const NeuronIDE::SceneObject& protoObj);
    static std::unique_ptr<Component>   buildComponent(const NeuronIDE::Component&         protoComp,
                                                       const std::shared_ptr<SceneObject>& owner);
};

#endif  // PARSER_HPP