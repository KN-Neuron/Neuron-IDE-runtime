#ifndef PARSER_HPP
#define PARSER_HPP

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>

class Scene;
class SceneObject;
class Component;
class ComponentBuildContext;

namespace NeuronIDE {
class SceneObject;
class Component;
}  // namespace NeuronIDE

class Parser {
   public:
    Parser() = default;

    // Relative asset paths in the scene (e.g. sprite images) resolve against the folder holding
    // filePath.
    static std::shared_ptr<Scene> parse(const std::string& filePath);
    // Relative asset paths resolve against assetRoot: the working directory by default.
    static std::shared_ptr<Scene> parseStream(std::istream&                stream,
                                              const std::filesystem::path& assetRoot = {});

   private:
    static std::shared_ptr<SceneObject> buildSceneObject(const NeuronIDE::SceneObject& protoObj,
                                                         const ComponentBuildContext&  context);
    static std::unique_ptr<Component>   buildComponent(const NeuronIDE::Component&         protoComp,
                                                       const std::shared_ptr<SceneObject>& owner,
                                                       const ComponentBuildContext&        context);
};

#endif  // PARSER_HPP