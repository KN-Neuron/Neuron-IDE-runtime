#include "scene/components/ComponentRegistry.hpp"

#include "neuronide.pb.h"
#include "scene/components/Component.hpp"

std::unique_ptr<Component> ComponentRegistry::build(const NeuronIDE::Component& protoComp) {
    auto activeCase = protoComp.component_type_case();

    int typeId = static_cast<int>(activeCase);

    auto it = creators.find(typeId);
    if (it != creators.end()) {
        return it->second(protoComp);
    }

    return nullptr;
}