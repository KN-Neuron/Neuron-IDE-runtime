#include "scene/components/ScriptComponent.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data_structures/Context.hpp"
#include "neuronide.pb.h"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentBuildContext.hpp"
#include "scene/components/ComponentRegistry.hpp"
#include "scene/components/TextRenderer.hpp"

namespace py = pybind11;

namespace script_api {
class ScriptBase;
}

struct ScriptComponent::Host {
    // "ScriptComponent on '<object>' (<path>)", to prefix errors with.
    std::string                where;
    std::weak_ptr<SceneObject> owner;
    std::weak_ptr<Scene>       scene;

    py::object              instance;
    script_api::ScriptBase* base = nullptr;
    py::object              onStart;
    py::object              onUpdate;

    // A Python exception becomes a std::runtime_error here, while the interpreter still runs: an
    // error_already_set that outlived it would crash when destroyed.
    template <typename... Args>
    void call(const py::object& function, std::string_view hook, Args&&... args) const {
        try {
            function(std::forward<Args>(args)...);
        } catch (const py::error_already_set& error) {
            throw std::runtime_error(where + ": " + std::string(hook) + " failed: " + error.what());
        }
    }
};

namespace script_api {

// The C++ side of neuronide.Script. Its ScriptComponent attaches to it once the scene is built,
// so nothing is reachable from a script's __init__.
class ScriptBase {
   public:
    void attach(ScriptComponent::Host* newHost) { host = newHost; }

    [[nodiscard]] ScriptComponent::Host& attached() const {
        if (host == nullptr) {
            throw std::runtime_error(
                "this neuronide.Script is not attached to a scene object: use it from on_start "
                "or later, not from __init__");
        }
        return *host;
    }

   private:
    ScriptComponent::Host* host = nullptr;
};

// neuronide.SceneObject. Weak, so a script that keeps one does not keep the object alive.
class ObjectRef {
   public:
    explicit ObjectRef(std::weak_ptr<SceneObject> object) : object(std::move(object)) {}

    [[nodiscard]] std::shared_ptr<SceneObject> get() const {
        auto locked = object.lock();
        if (!locked) {
            throw std::runtime_error("the scene object no longer exists");
        }
        return locked;
    }

    [[nodiscard]] TextRenderer& text() const {
        const auto locked   = get();
        auto*      renderer = locked->findComponent<TextRenderer>();
        if (renderer == nullptr) {
            throw std::runtime_error("'" + locked->name + "' has no TextRenderer");
        }
        return *renderer;
    }

   private:
    std::weak_ptr<SceneObject> object;
};

// neuronide.Scene
class SceneRef {
   public:
    explicit SceneRef(std::weak_ptr<Scene> scene) : scene(std::move(scene)) {}

    [[nodiscard]] std::shared_ptr<Scene> get() const {
        auto locked = scene.lock();
        if (!locked) {
            throw std::runtime_error("the scene no longer exists");
        }
        return locked;
    }

    [[nodiscard]] ObjectRef find(std::string_view name) const {
        auto object = get()->findObject(name);
        if (!object) {
            throw py::key_error("no scene object named '" + std::string(name) + "'");
        }
        return ObjectRef(object);
    }

   private:
    std::weak_ptr<Scene> scene;
};

}  // namespace script_api

namespace {

// Runs the file and returns the one class in it that derives from neuronide.Script.
py::object loadScriptClass(const std::filesystem::path& scriptPath, const std::string& where) {
    const py::object base = py::module_::import("neuronide").attr("Script");

    py::dict globals;
    globals["__builtins__"] = py::module_::import("builtins");
    globals["__name__"]     = scriptPath.stem().string();
    globals["__file__"]     = scriptPath.string();
    py::eval_file(scriptPath.string(), globals);

    std::vector<py::object> scriptClasses;
    for (const auto& item : globals) {
        const py::handle value = item.second;
        if (py::isinstance<py::type>(value) && !value.is(base) &&
            PyObject_IsSubclass(value.ptr(), base.ptr()) == 1) {
            scriptClasses.push_back(py::reinterpret_borrow<py::object>(value));
        }
    }
    if (scriptClasses.size() != 1) {
        throw std::runtime_error(where + ": defines " + std::to_string(scriptClasses.size()) +
                                 " classes deriving from neuronide.Script; it must define exactly "
                                 "one");
    }
    return scriptClasses.front();
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(neuronide, module) {
    using script_api::ObjectRef;
    using script_api::SceneRef;
    using script_api::ScriptBase;

    module.doc() = "The NeuronIDE runtime, as seen from an experiment script.";

    py::class_<ObjectRef>(module, "SceneObject")
        .def_property_readonly("name", [](const ObjectRef& ref) { return ref.get()->name; })
        .def_property(
            "active", [](const ObjectRef& ref) { return ref.get()->isActive; },
            [](const ObjectRef& ref, bool active) { ref.get()->isActive = active; },
            "Whether the object takes part in the scene. An inactive object is neither drawn nor "
            "updated, and neither is a script on it.")
        .def_property(
            "text", [](const ObjectRef& ref) { return ref.text().getText(); },
            [](const ObjectRef& ref, std::string_view text) { ref.text().setText(text); },
            "The text of the object's TextRenderer.");

    py::class_<SceneRef>(module, "Scene")
        .def("find", &SceneRef::find, py::arg("name"),
             "The scene object called `name`. Raises KeyError if there is none.");

    py::class_<ScriptBase>(module, "Script")
        .def(py::init<>())
        .def_property_readonly(
            "obj", [](const ScriptBase& self) { return ObjectRef(self.attached().owner); },
            "The scene object this script is on.")
        .def_property_readonly(
            "scene", [](const ScriptBase& self) { return SceneRef(self.attached().scene); },
            "The whole scene.")
        .def(
            "emit_marker",
            [](const ScriptBase& self, std::string name) {
                SceneRef(self.attached().scene).get()->emitMarker(std::move(name));
            },
            py::arg("name"),
            "Records a marker, stamped with the time the current frame appears on screen. "
            "Markers emitted in on_start get the first frame's time.");
}

ScriptComponent::ScriptComponent(const std::shared_ptr<SceneObject>& owner,
                                 const std::filesystem::path&        scriptPath)
    : Component(owner), host(std::make_unique<Host>()) {
    if (!owner) {
        throw std::invalid_argument("ScriptComponent: owner must not be null");
    }
    if (scriptPath.empty()) {
        throw std::invalid_argument("ScriptComponent on '" + owner->name +
                                    "': script path is empty");
    }
    host->where = "ScriptComponent on '" + owner->name + "' (" + scriptPath.string() + ")";
    host->owner = owner;

    if (Py_IsInitialized() == 0) {
        throw std::logic_error(host->where + ": no Python interpreter is running");
    }
    if (!std::filesystem::is_regular_file(scriptPath)) {
        throw std::runtime_error(host->where + ": no such file");
    }

    try {
        host->instance = loadScriptClass(scriptPath, host->where)();
        host->base     = &host->instance.cast<script_api::ScriptBase&>();
        host->onStart  = py::getattr(host->instance, "on_start", py::none());
        host->onUpdate = py::getattr(host->instance, "on_update", py::none());
    } catch (const py::error_already_set& error) {
        throw std::runtime_error(host->where + ": " + error.what());
    }
}

// The script's instance outlives this component if Python code kept a reference to it.
ScriptComponent::~ScriptComponent() {
    if (host->base != nullptr) {
        host->base->attach(nullptr);
    }
}

void ScriptComponent::onSceneReady(Scene& scene) {
    host->scene = scene.weak_from_this();
    host->base->attach(host.get());
    if (!host->onStart.is_none()) {
        host->call(host->onStart, "on_start");
    }
}

void ScriptComponent::update(const Context& context) {
    if (!host->onUpdate.is_none()) {
        host->call(host->onUpdate, "on_update", context.timestamp);
    }
}

void ScriptComponent::render(SDL_Renderer* renderer) { (void)renderer; }

std::unique_ptr<Component> ScriptComponent::createScript(const NeuronIDE::Component& protoComp,
                                                         const std::shared_ptr<SceneObject>& owner,
                                                         const ComponentBuildContext& context) {
    return std::make_unique<ScriptComponent>(
        owner, context.resolveAsset(protoComp.script().script_path()));
}

REGISTER_COMPONENT(NeuronIDE::Component::kScript, ScriptComponent::createScript)
