#ifndef SCRIPTCOMPONENT_HPP
#define SCRIPTCOMPONENT_HPP

#include <filesystem>
#include <memory>

#include "scene/components/Component.hpp"

class ComponentBuildContext;

namespace NeuronIDE {
class Component;
}

// Experiment logic written in Python. The file defines exactly one class deriving from
// neuronide.Script, with either hook, both or none:
//
//     class Trial(neuronide.Script):
//         def on_start(self): ...       # once, after the scene is built
//         def on_update(self, dt): ...  # every frame while the object is active; dt in seconds
//
// The file runs, and its class is instantiated, while the scene is parsed, so a script that does
// not load stops the run before the window opens. on_start runs then too: before the EEG stream is
// awaited and the recording starts, so the markers it emits are held for the first frame. An
// exception that escapes a hook stops the experiment. What a script can reach is the `neuronide`
// module, defined in ScriptComponent.cpp.
//
// Hooks run inside the frame, so they must not block, and print() is logged, so nothing should
// print every frame.
//
// Needs a running PythonInterpreter that outlives the component, used only from the thread that
// runs the render loop.
class ScriptComponent : public Component {
   public:
    // Throws std::invalid_argument for a null owner or an empty path, std::logic_error when no
    // interpreter is running, and std::runtime_error when the script cannot be loaded.
    ScriptComponent(const std::shared_ptr<SceneObject>& owner,
                    const std::filesystem::path&        scriptPath);
    ~ScriptComponent() override;

    ScriptComponent(const ScriptComponent&)            = delete;
    ScriptComponent& operator=(const ScriptComponent&) = delete;
    ScriptComponent(ScriptComponent&&)                 = delete;
    ScriptComponent& operator=(ScriptComponent&&)      = delete;

    void update(const Context& context) override;
    void render(SDL_Renderer* renderer) override;
    // Runs on_start.
    void onSceneReady(Scene& scene) override;

    static std::unique_ptr<Component> createScript(const NeuronIDE::Component&         protoComp,
                                                   const std::shared_ptr<SceneObject>& owner,
                                                   const ComponentBuildContext&        context);

    // Defined in ScriptComponent.cpp, next to the Python bindings that use it.
    struct Host;

   private:
    std::unique_ptr<Host> host;
};

#endif  // SCRIPTCOMPONENT_HPP
