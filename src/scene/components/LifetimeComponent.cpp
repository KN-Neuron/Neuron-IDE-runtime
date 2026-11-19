#include "scene/components/LifetimeComponent.hpp"

#include <logging/Logging.hpp>
#include <stdexcept>
#include <string>

#include "data_structures/Context.hpp"
#include "neuronide.pb.h"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentRegistry.hpp"

namespace {
constexpr double kMillisecondsPerSecond = 1000.0;

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

void LifetimeComponent::update(const Context& context) {
    const auto ownerPtr = owner.lock();
    if (!ownerPtr) {
        return;
    }
    elapsedSeconds += context.timestamp;
    if (elapsedSeconds * kMillisecondsPerSecond < static_cast<double>(duration)) {
        return;
    }
    ownerPtr->isActive = false;
    elapsedSeconds     = 0.0;
    LOG_DEBUG(logger(), "LifetimeComponent on '{}' hid its object after {} ms", ownerPtr->name,
              duration);
    expireEvent.invoke(context);
}

void LifetimeComponent::render(SDL_Renderer* renderer) { (void)renderer; }

Delegate* LifetimeComponent::getEvent(std::string_view eventName) {
    return eventName == "expire" ? &expireEvent : nullptr;
}

std::unique_ptr<Component> LifetimeComponent::createLifetime(
    const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner,
    const ComponentBuildContext& /*context*/) {
    return std::make_unique<LifetimeComponent>(owner, protoComp.lifetime().duration_ms());
}

REGISTER_COMPONENT(NeuronIDE::Component::kLifetime, LifetimeComponent::createLifetime)
