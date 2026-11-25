#include "scene/components/ScriptComponent.hpp"

#include <SDL2/SDL_keyboard.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data_structures/Context.hpp"
#include "data_structures/KeyPress.hpp"
#include "data_structures/Marker.hpp"
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

    struct KeyBinding {
        SDL_Scancode key;
        py::object   callback;
    };
    std::vector<KeyBinding> keyBindings;

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

    static double requirePixel(double value, std::string_view axis) {
        // The renderers draw in float and TextRenderer centres in int, so a position must fit both.
        if (!std::isfinite(value) ||
            std::abs(value) > static_cast<double>(std::numeric_limits<int>::max())) {
            throw py::value_error(std::string(axis) + " must be a finite number of pixels, got " +
                                  std::to_string(value));
        }
        return value;
    }

   private:
    std::weak_ptr<SceneObject> object;
};

// SDL's name for the key (case does not matter) as its scancode, the physical key a KeyPress
// carries.
SDL_Scancode scancodeNamed(const std::string& name) {
    const SDL_Scancode key = SDL_GetScancodeFromName(name.c_str());
    if (key == SDL_SCANCODE_UNKNOWN) {
        throw py::value_error("no key named '" + name +
                              "'; use SDL's key names, such as 'Space', 'Return', 'Left' or 'A'");
    }
    return key;
}

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
    using script_api::scancodeNamed;
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
            "The text of the object's TextRenderer.")
        .def_property(
            "x", [](const ObjectRef& ref) { return ref.get()->transform.posX; },
            [](const ObjectRef& ref, double left) {
                ref.get()->transform.posX = ObjectRef::requirePixel(left, "x");
            },
            "The left edge, in window pixels. A change shows in the frame it is made in.")
        .def_property(
            "y", [](const ObjectRef& ref) { return ref.get()->transform.posY; },
            [](const ObjectRef& ref, double top) {
                ref.get()->transform.posY = ObjectRef::requirePixel(top, "y");
            },
            "The top edge, in window pixels, counted downwards.")
        .def_property_readonly(
            "width", [](const ObjectRef& ref) { return ref.get()->transform.width; },
            "The width in pixels, as the experiment file sets it.")
        .def_property_readonly(
            "height", [](const ObjectRef& ref) { return ref.get()->transform.height; },
            "The height in pixels, as the experiment file sets it.");

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
            [](const ScriptBase& self, std::string name, std::optional<double> time) {
                const auto scene = SceneRef(self.attached().scene).get();
                if (!time) {
                    scene->emitMarker(std::move(name));
                    return;
                }
                if (!std::isfinite(*time)) {
                    throw py::value_error("a marker's time must be finite");
                }
                scene->emitTimedMarker(Marker{.eventName = std::move(name), .timestamp = *time});
            },
            py::arg("name"), py::arg("time") = py::none(),
            "Records a marker. Without `time` it is stamped with the time the current frame "
            "appears on screen, and markers emitted in on_start get the first frame's time. "
            "`time` is on the LSL clock, like the time an on_key callback gets: pass it to record "
            "a response at the moment of its key press.")
        .def(
            "on_key",
            [](const ScriptBase& self, const std::string& key, py::object callback) {
                if (PyCallable_Check(callback.ptr()) == 0) {
                    throw py::type_error("on_key needs a function to call");
                }
                self.attached().keyBindings.push_back({scancodeNamed(key), std::move(callback)});
            },
            py::arg("key"), py::arg("callback"),
            "Calls `callback(time)` whenever `key` goes down, for the rest of the run. `key` is "
            "SDL's name for it, such as 'Space', 'Return', 'Left' or 'A', and means the same "
            "physical key on any keyboard layout. `time` is when the key went down, on the LSL "
            "clock, to within one frame. Callbacks run at the start of the frame after the press, "
            "before on_update, and only while the script's object is active.");
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
    for (const KeyPress& press : context.keyPresses) {
        // By index, and the callback copied: a callback may bind more keys, which moves the rest.
        for (std::size_t index = 0; index < host->keyBindings.size(); ++index) {
            if (host->keyBindings[index].key != press.key) {
                continue;
            }
            const py::object callback = host->keyBindings[index].callback;
            host->call(callback, "a key callback", press.timestamp);
        }
    }
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
