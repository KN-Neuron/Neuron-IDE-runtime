#ifndef COMPONENTREGISTRY_HPP
#define COMPONENTREGISTRY_HPP

#include <functional>
#include <memory>
#include <unordered_map>

class Component;
namespace NeuronIDE {
class Component;
}

using ComponentCreatorFunc = std::function<std::unique_ptr<Component>(const NeuronIDE::Component&)>;

class ComponentRegistry {
   public:
    ComponentRegistry(const ComponentRegistry&)            = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    ComponentRegistry(ComponentRegistry&&)            = delete;
    ComponentRegistry& operator=(ComponentRegistry&&) = delete;

    static ComponentRegistry& instance() {
        static ComponentRegistry instance;
        return instance;
    }

    void registerCreator(int typeId, ComponentCreatorFunc creator);

    std::unique_ptr<Component> build(const NeuronIDE::Component& protoComp);

   private:
    ComponentRegistry() = default;

    std::unordered_map<int, ComponentCreatorFunc> creators;
};

#endif  // COMPONENTREGISTRY_HPP