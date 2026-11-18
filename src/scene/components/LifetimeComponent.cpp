#include "scene/components/LifetimeComponent.hpp"

#include <logging/Logging.hpp>
#include <stdexcept>
#include <string>

#include "neuronide.pb.h"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentRegistry.hpp"

namespace {
quill::Logger* logger() { return logging::logger(logging::Subsystem::Scene); }
}  // namespace

LifetimeComponent::LifetimeComponent(const std::shared_ptr<SceneObject>& owner,
                                     std::uint32_t                       milliseconds)
    : Component(owner), duration(milliseconds) {
    if (!owner) {
        throw std::invalid_argument("LifetimeComponent: owner must not be null");
    }
    if (milliseconds == 0 || milliseconds > kMaxDurationMs) {
        throw std::invalid_argument(
            "LifetimeComponent on '" + owner->name + "': duration_ms must be between 1 and " +
            std::to_string(kMaxDurationMs) + ", got " + std::to_string(milliseconds));
    }
    LOG_DEBUG(logger(), "LifetimeComponent on '{}' hides its object after {} ms", owner->name,
              milliseconds);
}

void LifetimeComponent::update(const Context& context) { (void)context; }

void LifetimeComponent::render(SDL_Renderer* renderer) { (void)renderer; }

std::unique_ptr<Component> LifetimeComponent::createLifetime(
    const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner,
    const ComponentBuildContext& /*context*/) {
    return std::make_unique<LifetimeComponent>(owner, protoComp.lifetime().duration_ms());
}

REGISTER_COMPONENT(NeuronIDE::Component::kLifetime, LifetimeComponent::createLifetime)
