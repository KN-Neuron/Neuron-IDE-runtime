#include "scene/components/BlinkComponent.hpp"

#include "neuronide.pb.h"
#include "scene/components/ComponentRegistry.hpp"

void BlinkComponent::setFrequency(double freq) { blinkFrequencyHz = freq; }

std::unique_ptr<Component> BlinkComponent::createBlinker(
    const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner) {
    return std::make_unique<BlinkComponent>(owner, protoComp.blinker().blink_frequency_hz());
}

void BlinkComponent::update(const Context& context) {
    // TODO: implement blinking logic based on blinkFrequencyHz and context.timestamp
}

void BlinkComponent::render(SDL_Renderer* renderer) {
    // This component does not render anything itself, it only controls visibility of the owner
    // object.
}

REGISTER_COMPONENT(NeuronIDE::Component::kBlinker, BlinkComponent::createBlinker)