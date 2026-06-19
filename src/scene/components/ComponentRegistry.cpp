#include "scene/components/ComponentRegistry.hpp"

#include <stdexcept>
#include <utility>

#include "neuronide.pb.h"
#include "scene/components/Component.hpp"

void ComponentRegistry::registerCreator(int typeId, ComponentCreatorFunc creator) {
    if (creators.contains(typeId)) {
        throw std::runtime_error("Creator for this typeId is already registered.");
    }
    creators[typeId] = std::move(creator);
}

std::unique_ptr<Component> ComponentRegistry::build(const NeuronIDE::Component&         protoComp,
                                                    const std::shared_ptr<SceneObject>& owner) {
    auto activeCase = protoComp.component_type_case();

    int typeId = static_cast<int>(activeCase);

    auto iter = creators.find(typeId);
    if (iter != creators.end()) {
        return iter->second(protoComp, owner);
    }

    return nullptr;
}