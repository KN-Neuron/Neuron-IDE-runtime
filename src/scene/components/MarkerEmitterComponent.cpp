#include "scene/components/MarkerEmitterComponent.hpp"

#include "events/Delegate.hpp"
#include "neuronide.pb.h"
#include "scene/Scene.hpp"
#include "scene/components/ComponentRegistry.hpp"

MarkerEmitterComponent::MarkerEmitterComponent(const std::shared_ptr<SceneObject>& owner,
                                               std::vector<Binding>                bindings)
    : Component(owner), bindings(std::move(bindings)) {}

MarkerEmitterComponent::~MarkerEmitterComponent() {
    for (auto& sub : activeSubscriptions) {
        sub.delegate->unsubscribe(sub.link.get());
    }
}

void MarkerEmitterComponent::update(const Context& context) { (void)context; }

void MarkerEmitterComponent::render(SDL_Renderer* renderer) { (void)renderer; }

void MarkerEmitterComponent::onSceneReady(Scene& scene) {
    for (const auto& binding : bindings) {
        auto& delegate = scene.resolveEvents(binding.targetObject, binding.targetEvent);

        auto link = std::make_unique<MarkerEventLink>(binding.markerName);
        delegate.subscribe(link.get());

        activeSubscriptions.push_back({std::move(link), &delegate});
    }
}

std::unique_ptr<Component> MarkerEmitterComponent::createMarkerEmitter(
    const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner) {
    std::vector<Binding> bindings;
    for (const auto& protoBinding : protoComp.marker_emitter().bindings()) {
        bindings.push_back({protoBinding.target_object(), protoBinding.target_event(),
                            protoBinding.marker_name()});
    }
    return std::make_unique<MarkerEmitterComponent>(owner, std::move(bindings));
}

REGISTER_COMPONENT(NeuronIDE::Component::kMarkerEmitter,
                   MarkerEmitterComponent::createMarkerEmitter)
