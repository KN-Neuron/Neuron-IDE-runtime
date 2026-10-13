#include "scene/components/BlinkComponent.hpp"

#include <cmath>
#include <numbers>

#include "data_structures/Context.hpp"
#include "neuronide.pb.h"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentRegistry.hpp"

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi;
}  // namespace

void BlinkComponent::setFrequency(double freq) {
    blinkFrequencyHz = freq;
    elapsedTime      = 0.0;
    if (blinkFrequencyHz <= 0.0) {
        if (auto ownerPtr = owner.lock()) {
            ownerPtr->isVisible = true;
        }
    }
}

std::unique_ptr<Component> BlinkComponent::createBlinker(
    const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner) {
    return std::make_unique<BlinkComponent>(owner, protoComp.blinker().blink_frequency_hz());
}

void BlinkComponent::update(const Context& context) {
    if (blinkFrequencyHz <= 0.0) {
        return;
    }
    auto ownerPtr = owner.lock();
    if (ownerPtr == nullptr) {
        return;
    }
    elapsedTime += context.timestamp;
    ownerPtr->isVisible = std::sin(kTwoPi * blinkFrequencyHz * elapsedTime) >= 0.0;
}

void BlinkComponent::render(SDL_Renderer* renderer) {
    (void)renderer;
    // This component does not render anything itself, it only controls visibility of the owner
    // object.
}

REGISTER_COMPONENT(NeuronIDE::Component::kBlinker, BlinkComponent::createBlinker)