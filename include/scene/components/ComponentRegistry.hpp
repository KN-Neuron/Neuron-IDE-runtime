#ifndef COMPONENTREGISTRY_HPP
#define COMPONENTREGISTRY_HPP

#include <functional>
#include <memory>
#include <unordered_map>

class Component;
namespace NeuronIDE {
class Component;
}
class SceneObject;

using ComponentCreatorFunc = std::function<std::unique_ptr<Component>(
    const NeuronIDE::Component&, const std::shared_ptr<SceneObject>&)>;

class ComponentRegistry {
   public:
    ComponentRegistry(const ComponentRegistry&)            = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    ComponentRegistry(ComponentRegistry&&)            = delete;
    ComponentRegistry& operator=(ComponentRegistry&&) = delete;
    ~ComponentRegistry()                              = default;

    static ComponentRegistry& instance() {
        static ComponentRegistry instance;
        return instance;
    }

    void registerCreator(int typeId, ComponentCreatorFunc creator);

    std::unique_ptr<Component> build(const NeuronIDE::Component&         protoComp,
                                     const std::shared_ptr<SceneObject>& owner);

   private:
    ComponentRegistry() = default;

    std::unordered_map<int, ComponentCreatorFunc> creators;
};

#define COMPONENT_REGISTRATION_CONCAT_IMPL(x, y) x##y
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define COMPONENT_REGISTRATION_CONCAT(x, y) COMPONENT_REGISTRATION_CONCAT_IMPL(x, y)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REGISTER_COMPONENT(typeId, creatorFunc)                                                   \
    namespace {                                                                                   \
    struct COMPONENT_REGISTRATION_CONCAT(ComponentRegistrar_, __LINE__) {                         \
        COMPONENT_REGISTRATION_CONCAT(ComponentRegistrar_, __LINE__)() {                          \
            ComponentRegistry::instance().registerCreator(static_cast<int>(typeId), creatorFunc); \
        }                                                                                         \
    };                                                                                            \
    static const COMPONENT_REGISTRATION_CONCAT(ComponentRegistrar_, __LINE__)                     \
        COMPONENT_REGISTRATION_CONCAT(global_registrar_, __LINE__);                               \
    }

#endif  // COMPONENTREGISTRY_HPP